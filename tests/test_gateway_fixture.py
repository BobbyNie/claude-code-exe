"""Validate that interrupted-stream fixtures exercise semantic EOF boundaries."""
import importlib.util
import json
import socket
import ssl
from pathlib import Path
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import threading
import tempfile

spec = importlib.util.spec_from_file_location(
    "gateway_fixture", Path(__file__).parent / "ccode/gateway-integration.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class GatewayFixtureTests(unittest.TestCase):
    def test_dns_refusal_requires_nonzero_exit_and_exact_neutral_line(self):
        self.assertTrue(fixture.dns_failure_observed(1, '[E_GATEWAY_DNS: name resolution failed]\n'))
        self.assertFalse(fixture.dns_failure_observed(0, '[E_GATEWAY_DNS: name resolution failed]\n'))
        self.assertFalse(fixture.dns_failure_observed(1, 'private E_GATEWAY_DNS text'))
        self.assertFalse(fixture.dns_failure_observed(1, '[E_ENGINE: incomplete turn; check gateway and configuration]'))

    def test_expired_turn_trusts_only_fixture_ca_without_disabling_tls(self):
        with tempfile.TemporaryDirectory() as folder:
            executable = Path(folder) / 'ccode.exe'
            executable.write_bytes(b'fixture')
            ca = Path(folder) / 'public-test-ca.pem'
            endpoint = SimpleNamespace(server_port=12345, http_requests=0, ca_certificate=ca)
            def run(args, **kwargs):
                self.assertEqual(kwargs['env']['NODE_EXTRA_CA_CERTS'], str(ca))
                self.assertNotIn('NODE_TLS_REJECT_UNAUTHORIZED', kwargs['env'])
                return SimpleNamespace(returncode=1,
                    stdout='[E_GATEWAY_TLS: certificate verification failed]\n', stderr='')
            with patch.object(fixture, 'untrusted_tls_endpoint') as context, patch.object(
                    fixture.subprocess, 'run', side_effect=run), patch.object(
                    fixture, 'tls_rejection_observed', return_value=True), patch.object(
                    fixture, 'validate_failure_diagnostic') as validate, patch('builtins.print'):
                context.return_value.__enter__.return_value = endpoint
                fixture.check_tls_rejection(executable, expired=True)
                context.assert_called_once_with(expired=True)
                self.assertEqual(validate.call_args.args[1:], ('E_GATEWAY_TLS', 1))

    def test_tls12_probe_endpoint_rejects_untrusted_client_and_negotiates_only_tls12(self):
        with fixture.untrusted_tls_endpoint(maximum_version=ssl.TLSVersion.TLSv1_2) as endpoint:
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with self.assertRaises(ssl.SSLCertVerificationError):
                    ssl.create_default_context().wrap_socket(connection, server_hostname='127.0.0.1')
            self.assertTrue(endpoint.handshake_failed.wait(2))
            trusted = ssl.create_default_context(cafile=str(endpoint.certificate))
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with trusted.wrap_socket(connection, server_hostname='127.0.0.1') as secure:
                    self.assertEqual(secure.version(), 'TLSv1.2')
                    secure.sendall(b'GET / HTTP/1.0\r\nHost: localhost\r\n\r\n')
                    self.assertIn(b'503', secure.recv(4096))
            self.assertEqual(endpoint.tls_versions, {'TLSv1.2': 1, 'TLSv1.3': 0, 'other': 0})

    def test_valid_leaf_from_expiry_fixture_issuer_is_accepted(self):
        with fixture.untrusted_tls_endpoint(trusted_chain=True) as endpoint:
            context = ssl.create_default_context(cafile=str(endpoint.ca_certificate))
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with context.wrap_socket(connection, server_hostname='127.0.0.1') as secure:
                    secure.sendall(b'GET / HTTP/1.0\r\nHost: localhost\r\n\r\n')
                    self.assertIn(b'503', secure.recv(4096))
            self.assertEqual(endpoint.http_requests, 1)
            self.assertEqual(endpoint.tls_handshakes_completed, 1)

    def test_expired_endpoint_is_rejected_even_with_approved_fixture_ca(self):
        with fixture.untrusted_tls_endpoint(expired=True) as endpoint:
            context = ssl.create_default_context(cafile=str(endpoint.ca_certificate))
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with self.assertRaises(ssl.SSLCertVerificationError) as caught:
                    context.wrap_socket(connection, server_hostname='127.0.0.1')
            self.assertEqual(caught.exception.verify_code, 10)  # certificate expired
            self.assertTrue(endpoint.handshake_failed.wait(2))
            self.assertEqual(endpoint.http_requests, 0)
            self.assertEqual(endpoint.tls_handshakes_completed, 0)

    def test_unreachable_endpoint_reserves_port_without_accepting_connections(self):
        with fixture.unreachable_endpoint() as address:
            self.assertEqual(address[0], "127.0.0.1")
            with socket.socket() as client:
                client.settimeout(2)
                self.assertNotEqual(client.connect_ex(address), 0)
            with socket.socket() as competitor:
                with self.assertRaises(OSError):
                    competitor.bind(address)

    def test_unreachable_turn_requires_fresh_exact_failure_report(self):
        with tempfile.TemporaryDirectory() as folder:
            executable = Path(folder) / 'ccode.exe'
            executable.write_bytes(b'fixture')
            def run(args, **kwargs):
                self.assertIn('--diagnostics', args)
                report = Path(args[args.index('--diagnostics') + 1])
                self.assertFalse(report.exists())
                return SimpleNamespace(returncode=1, stdout='[E_ENGINE: turn failed]\n', stderr='')
            with patch.object(fixture.subprocess, 'run', side_effect=run), patch.object(
                    fixture, 'validate_failure_diagnostic') as validate, patch('builtins.print'):
                fixture.check_unreachable(executable)
                validate.assert_called_once()
                self.assertEqual(validate.call_args.args[1:], ('E_ENGINE', 1))
                self.assertEqual(validate.call_args.kwargs, {'category': 'local'})

    def test_tls_turn_requires_fresh_exact_tls_failure_report(self):
        with tempfile.TemporaryDirectory() as folder:
            executable = Path(folder) / 'ccode.exe'
            executable.write_bytes(b'fixture')
            endpoint = SimpleNamespace(server_port=12345, http_requests=0)
            def run(args, **kwargs):
                self.assertIn('--diagnostics', args)
                report = Path(args[args.index('--diagnostics') + 1])
                self.assertFalse(report.exists())
                return SimpleNamespace(returncode=1,
                    stdout='[E_GATEWAY_TLS: certificate verification failed]\n', stderr='')
            with patch.object(fixture, 'untrusted_tls_endpoint') as context, patch.object(
                    fixture.subprocess, 'run', side_effect=run), patch.object(
                    fixture, 'tls_rejection_observed', return_value=True), patch.object(
                    fixture, 'validate_failure_diagnostic') as validate, patch('builtins.print'):
                context.return_value.__enter__.return_value = endpoint
                fixture.check_tls_rejection(executable)
                validate.assert_called_once()
                self.assertEqual(validate.call_args.args[1:], ('E_GATEWAY_TLS', 1))

    def test_tls_endpoint_rejects_untrusted_certificate_but_accepts_explicit_trust(self):
        with fixture.untrusted_tls_endpoint() as endpoint:
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with self.assertRaises(ssl.SSLCertVerificationError):
                    ssl.create_default_context().wrap_socket(connection, server_hostname="127.0.0.1")
            self.assertTrue(endpoint.handshake_failed.wait(2), "No actual failed TLS handshake observed")
            trusted = ssl.create_default_context(cafile=str(endpoint.certificate))
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with trusted.wrap_socket(connection, server_hostname="127.0.0.1") as secure:
                    negotiated = secure.version()
                    secure.sendall(b"GET / HTTP/1.0\r\nHost: localhost\r\n\r\n")
                    self.assertIn(b"503", secure.recv(4096))
            self.assertEqual(endpoint.http_requests, 1)
            self.assertEqual(endpoint.tls_handshakes_completed, 1)
            self.assertEqual(endpoint.tls_versions, {
                "TLSv1.2": int(negotiated == "TLSv1.2"),
                "TLSv1.3": int(negotiated == "TLSv1.3"), "other": 0})

    def test_tls_endpoint_records_failed_connection_without_http_or_private_details(self):
        with fixture.untrusted_tls_endpoint() as endpoint:
            with socket.create_connection(endpoint.address, timeout=2):
                pass
            self.assertTrue(endpoint.handshake_failed.wait(3))
            self.assertEqual(endpoint.connections, 1)
            self.assertEqual(endpoint.http_requests, 0)
            self.assertEqual(len(endpoint.handshake_errors), 1)
            error = endpoint.handshake_errors[0]
            self.assertIn(error['kind'], ('tls', 'transport'))
            self.assertTrue(error['errno'] is None or isinstance(error['errno'], int))
            self.assertEqual(set(error), {'kind', 'errno'})

    def test_tls_prefix_classification_never_returns_received_content(self):
        cases = [(b"\x16\x03\x01", "tls-record"),
                 (b"\x16\x03\x03", "tls-record"),
                 (b"GET /secret", "other"), (b"", "closed"),
                 (b"\x16", "other"), (b"\x16\x02\x01", "other")]
        for prefix, expected in cases:
            self.assertEqual(fixture.classify_tls_prefix(prefix), expected)

    def test_transport_reset_requires_engine_tls_classification(self):
        failed = threading.Event()
        failed.set()
        endpoint = SimpleNamespace(handshake_failed=failed, connections=1, http_requests=0)
        self.assertFalse(fixture.tls_rejection_observed(endpoint, 1, 'E_ENGINE_API'))
        self.assertTrue(fixture.tls_rejection_observed(endpoint, 1, 'E_GATEWAY_TLS'))
        self.assertFalse(fixture.tls_rejection_observed(endpoint, 0, 'E_GATEWAY_TLS'))
        endpoint.http_requests = 1
        self.assertFalse(fixture.tls_rejection_observed(endpoint, 1, 'E_GATEWAY_TLS'))
        endpoint.http_requests = 0
        failed.clear()
        self.assertFalse(fixture.tls_rejection_observed(endpoint, 1, 'E_GATEWAY_TLS'))

    def test_tls_evidence_waits_for_delayed_server_observation_without_relaxing_classification(self):
        failed = threading.Event()
        endpoint = SimpleNamespace(handshake_failed=failed, connections=1, http_requests=0)
        timer = threading.Timer(0.02, failed.set)
        timer.start()
        try:
            self.assertTrue(fixture.tls_rejection_observed(
                endpoint, 1, 'E_GATEWAY_TLS', handshake_timeout=1))
            self.assertFalse(fixture.tls_rejection_observed(
                endpoint, 1, 'E_ENGINE', handshake_timeout=0))
        finally:
            timer.join()
        failed.clear()
        self.assertFalse(fixture.tls_rejection_observed(
            endpoint, 1, 'E_GATEWAY_TLS', handshake_timeout=0.01))

    def test_tls_terminal_evidence_accepts_only_canonical_neutral_lines(self):
        flags = fixture.tls_terminal_evidence(
            '[E_GATEWAY_RETRY: automatic retry refused]\n'
            'private E_GATEWAY_TLS token\n[E_ENGINE: turn failed]\n')
        self.assertEqual(flags, {'tls': False, 'retry': True, 'engine': True})
        self.assertTrue(fixture.tls_terminal_evidence(
            '[E_GATEWAY_TLS: certificate verification failed]\n')['tls'])

    def test_gateway_failure_report_requires_exact_private_free_schema_and_cause(self):
        report = {'schemaVersion': 1, 'product': 'ccode', 'platform': 'windows',
                  'architecture': 'x64', 'status': 'error',
                  'operationId': 'd425fe8e-13f4-42da-9ec0-1779df26c445',
                  'errorCode': 'E_GATEWAY_AUTH', 'category': 'network', 'exitCode': 1,
                  'privacy': dict.fromkeys(('argumentsCaptured', 'environmentValuesCaptured',
                                           'promptOrContentCaptured', 'credentialsCaptured'), False)}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'failure.json'
            path.write_text(json.dumps(report), encoding='utf-8')
            fixture.validate_failure_diagnostic(path, 'E_GATEWAY_AUTH', 1)
            for replacement in (dict(report, errorCode='E_ENGINE'),
                                dict(report, prompt='private'),
                                dict(report, operationId='private'),
                                dict(report, exitCode=0),
                                dict(report, privacy={'credentialsCaptured': False}),
                                dict(report, privacy=dict.fromkeys(report['privacy'], 0))):
                path.write_text(json.dumps(replacement), encoding='utf-8')
                with self.assertRaises(AssertionError):
                    fixture.validate_failure_diagnostic(path, 'E_GATEWAY_AUTH', 1)

    def test_stream_failure_report_uses_only_canonical_failure_lines(self):
        self.assertEqual(fixture.stream_failure_diagnostic(
            'private E_GATEWAY_RETRY text\n[E_MISSING_RESULT: incomplete turn]\n'),
            ('E_MISSING_RESULT', 'protocol'))
        self.assertEqual(fixture.stream_failure_diagnostic(
            '[E_GATEWAY_RETRY: automatic retry refused]\n'),
            ('E_GATEWAY_RETRY', 'network'))
        with self.assertRaises(AssertionError):
            fixture.stream_failure_diagnostic('private E_MISSING_RESULT text')

    def test_native_network_failure_takes_precedence_over_engine_adapter_error(self):
        self.assertEqual(fixture.stream_failure_diagnostic(
            '[E_ENGINE: request failed]\n[E_NETWORK: gateway request failed]\n'),
            ('E_NETWORK', 'network'))
        with self.assertRaises(AssertionError):
            fixture.stream_failure_diagnostic('private [E_NETWORK: gateway request failed] text')

    def test_complete_arguments_still_lack_block_and_message_termination(self):
        events = fixture.unfinished_tool_events("fixture-model", "target.txt", "marker", True)
        self.assertEqual([kind for kind, _ in events],
                         ["message_start", "content_block_start", "content_block_delta"])
        start = events[1][1]["content_block"]
        self.assertEqual((start["type"], start["name"], start["input"]),
                         ("tool_use", "Write", {}))
        arguments = json.loads(events[2][1]["delta"]["partial_json"])
        self.assertEqual(arguments, {"file_path": "target.txt", "content": "marker"})
        self.assertIsNone(events[0][1]["message"]["stop_reason"])

    def test_existing_incomplete_argument_case_remains_invalid_json(self):
        events = fixture.unfinished_tool_events("fixture-model", "target.txt", "marker", False)
        self.assertEqual(len(events), 3)
        with self.assertRaises(json.JSONDecodeError):
            json.loads(events[2][1]["delta"]["partial_json"])


if __name__ == '__main__':
    unittest.main()
