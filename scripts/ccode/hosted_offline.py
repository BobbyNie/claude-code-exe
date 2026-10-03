"""Bounded real disconnect trial, only on disposable GitHub-hosted Windows runners.

Never run on a developer workstation or self-hosted runner. A separate recovery
process restores initially-Up adapters if the controller stalls. This is not a
claim that recovery survives forced destruction of the entire runner VM.
"""
import argparse
import base64
import json
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid


def require_host(platform, environment):
    if (platform != 'win32' or environment.get('GITHUB_ACTIONS') != 'true'
            or environment.get('RUNNER_ENVIRONMENT') != 'github-hosted'):
        raise RuntimeError('E_HOSTED_OFFLINE_HOST')


def adapter_ids(values):
    if not isinstance(values, list) or not values:
        raise ValueError('E_HOSTED_OFFLINE_ADAPTERS')
    result = []
    for value in values:
        if not isinstance(value, str) or str(uuid.UUID(value)) != value.lower():
            raise ValueError('E_HOSTED_OFFLINE_ADAPTERS')
        result.append(value.lower())
    if len(set(result)) != len(result):
        raise ValueError('E_HOSTED_OFFLINE_ADAPTERS')
    return result


def isolated_trial(disconnect, trial, restore):
    try:
        disconnect()
        trial()
    except Exception:
        raise RuntimeError('E_HOSTED_OFFLINE_TRIAL') from None
    finally:
        try:
            restore()
        except Exception:
            raise RuntimeError('E_HOSTED_OFFLINE_RESTORE') from None


def powershell(script):
    encoded = base64.b64encode(("$ErrorActionPreference='Stop'; " + script).encode('utf-16le')).decode('ascii')
    result = subprocess.run(['pwsh', '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded],
                            capture_output=True, text=True, timeout=45)
    if result.returncode:
        raise RuntimeError('E_HOSTED_OFFLINE_NETWORK')
    return result.stdout


def network(ids, enable):
    ids = adapter_ids(ids)
    literal = ','.join("'" + value + "'" for value in ids)
    selected = (f"$ids=@({literal}); $adapters=@(Get-NetAdapter -IncludeHidden | "
                "Where-Object { $ids -contains $_.InterfaceGuid.ToString().ToLowerInvariant() }); "
                "if ($adapters.Count -ne $ids.Count) { throw 'missing adapter' }; ")
    if enable:
        script = (selected + "$adapters | Enable-NetAdapter -Confirm:$false; "
                  "$deadline=[DateTime]::UtcNow.AddSeconds(20); do { "
                  "$up=@(Get-NetAdapter -IncludeHidden | Where-Object { "
                  "$ids -contains $_.InterfaceGuid.ToString().ToLowerInvariant() -and $_.Status -eq 'Up' }); "
                  "if ($up.Count -eq $ids.Count) { exit 0 }; Start-Sleep -Milliseconds 250 "
                  "} while ([DateTime]::UtcNow -lt $deadline); throw 'restore incomplete'")
    else:
        script = selected + "$adapters | Disable-NetAdapter -Confirm:$false"
    powershell(script)


def recover(state_path):
    state = json.loads(state_path.read_text(encoding='utf-8'))
    ids = adapter_ids(state['adapters'])
    ready = state_path.with_suffix('.ready')
    done = state_path.with_suffix('.done')
    ready.touch()
    deadline = time.monotonic() + 220
    while time.monotonic() < deadline:
        if done.exists():
            return
        if state_path.with_suffix('.restore-now').exists():
            break
        time.sleep(0.25)
    # Mark any watchdog intervention as a failed trial, even if restoration succeeds.
    state_path.with_suffix('.recovered').touch()
    network(ids, True)


def execute(args):
    require_host(sys.platform, os.environ)
    if args.recover:
        recover(args.recover)
        return
    powershell("$p=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent()); "
               "if (-not $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'admin required' }")
    raw = powershell("$a=@(Get-NetAdapter -IncludeHidden | Where-Object { "
                     "$_.Status -eq 'Up' -and $_.ifIndex -ne 1 -and "
                     "$_.Name -notmatch '(?i)loopback' -and $_.InterfaceDescription -notmatch '(?i)loopback' } | "
                     "ForEach-Object { $_.InterfaceGuid.ToString().ToLowerInvariant() }); ConvertTo-Json -InputObject $a -Compress")
    ids = adapter_ids(json.loads(raw))
    evidence = args.evidence.resolve()
    evidence.parent.mkdir(parents=True, exist_ok=True)
    if evidence.exists():
        raise RuntimeError('E_HOSTED_OFFLINE_STALE_EVIDENCE')
    with tempfile.TemporaryDirectory(prefix='ccode-offline-control-') as temporary:
        state = Path(temporary) / 'state.json'
        state.write_text(json.dumps({'adapters': ids}), encoding='utf-8')
        watchdog = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--recover', str(state)],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        restored = False
        try:
            deadline = time.monotonic() + 15
            while not state.with_suffix('.ready').exists():
                if watchdog.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('E_HOSTED_OFFLINE_RECOVERY_START')
                time.sleep(0.1)
            def trial():
                harness = Path(__file__).with_name('accept-offline-windows11-x64.ps1')
                # Existing native-tested Job Object runner starts suspended and
                # reaps the complete process tree before network restoration.
                probe_path = Path(__file__).resolve().parents[2] / 'tests/ccode/tls-probe.py'
                spec = importlib.util.spec_from_file_location('offline_containment', probe_path)
                containment = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(containment)
                result = containment.run_contained(['pwsh', '-NoProfile', '-NonInteractive', '-File', str(harness),
                    '-Executable', str(args.executable.resolve()), '-ExpectedEngineVersion', args.version,
                    '-AdapterRevision', args.revision, '-AcceptanceTarget', 'github-hosted',
                    '-EvidencePath', str(evidence)], cwd=Path(__file__).resolve().parents[2],
                    env=os.environ.copy(), input='', timeout=150)
                if result.returncode or not evidence.is_file():
                    raise RuntimeError('E_HOSTED_OFFLINE_ACCEPTANCE')
                report = json.loads(evidence.read_text(encoding='utf-8-sig'))
                if report['acceptance']['passed'] is not True:
                    raise RuntimeError('E_HOSTED_OFFLINE_ACCEPTANCE')
            def restore():
                nonlocal restored
                network(ids, True)
                restored = True
            isolated_trial(lambda: network(ids, False), trial, restore)
            if state.with_suffix('.recovered').exists() or watchdog.poll() is not None:
                raise RuntimeError('E_HOSTED_OFFLINE_RECOVERY_INTERVENED')
            print('PASS: isolated hosted offline trial and adapter restoration')
        finally:
            state.with_suffix('.done' if restored else '.restore-now').touch()
            try:
                if watchdog.wait(timeout=50) != 0:
                    raise RuntimeError('E_HOSTED_OFFLINE_RECOVERY_FAILED')
            except subprocess.TimeoutExpired:
                watchdog.kill()
                watchdog.wait()
                raise RuntimeError('E_HOSTED_OFFLINE_RECOVERY_CLEANUP') from None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recover', type=Path, help=argparse.SUPPRESS)
    parser.add_argument('--executable', type=Path)
    parser.add_argument('--version')
    parser.add_argument('--revision')
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    if not args.recover and not all((args.executable, args.version, args.revision, args.evidence)):
        parser.error('executable, version, revision and evidence are required')
    try:
        execute(args)
    except Exception:
        # Raw command errors can include network identities or local paths.
        print('E_HOSTED_OFFLINE: isolation, trial or restoration failed', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
