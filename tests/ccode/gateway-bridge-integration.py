"""Real Windows native transport, no engine substitute and no private logs."""
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import argparse
import http.client
import importlib.util
import json
from pathlib import Path
import subprocess
import ssl
import socket
import socketserver
import select
import os
import sys
import threading
import time
from urllib.parse import urlsplit


@contextmanager
def bridge(executable, upstream, ca=None, *, env=None):
    command = [str(executable), upstream] + ([str(ca)] if ca else [])
    process = subprocess.Popen(command, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
    try:
        # This test-only capability never goes to the public CI log.
        with ThreadPoolExecutor(max_workers=1) as pool:
            try:
                line = pool.submit(process.stdout.readline).result(timeout=15).strip()
            except TimeoutError:
                process.kill()
                raise AssertionError("Native transport startup exceeded deadline") from None
        url = urlsplit(line)
        assert url.scheme == "http" and url.hostname == "127.0.0.1" and url.port
        yield process, url
    finally:
        if process.poll() is None:
            try:
                process.communicate("stop\n", timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                raise AssertionError("Native transport cleanup exceeded deadline") from None


def proxy_environment(uri, bypass=""):
    env = {key: value for key, value in os.environ.items()
           if key.lower() not in {"http_proxy", "https_proxy", "all_proxy", "no_proxy"}}
    env.update(HTTP_PROXY=uri, HTTPS_PROXY=uri, NO_PROXY=bypass)
    return env


@contextmanager
def connect_proxy(target, *, deny=False, authorization="", authority_host=None, stall=False, tls_context=None):
    """Local test proxy: fixed target only, connection counts, no private logs."""
    authority = f"{authority_host or target[0]}:{target[1]}"

    class Handler(socketserver.BaseRequestHandler):
        def finish(self):
            self.request.close()

        def handle(self):
            self.server.connections += 1
            self.request.settimeout(5)
            if tls_context is not None:
                try:
                    self.request = tls_context.wrap_socket(self.request, server_side=True)
                    self.server.tls_handshakes += 1
                except (ssl.SSLError, ConnectionError):
                    self.server.tls_failures += 1
                    return
            header = bytearray()
            while not header.endswith(b"\r\n\r\n"):
                byte = self.request.recv(1)
                if not byte:
                    return
                header.extend(byte)
                if len(header) > 65536:
                    raise AssertionError("Proxy request header limit exceeded")
            lines = bytes(header).decode("ascii").split("\r\n")
            valid = lines[0] == f"CONNECT {authority} HTTP/1.1"
            credentials = [line.partition(":")[2].strip() for line in lines[1:]
                           if line.partition(":")[0].lower() == "proxy-authorization"]
            valid &= credentials == ([authorization] if authorization else [])
            self.server.started.set()
            if stall:
                assert self.server.stopped.wait(10), "Proxy stall fixture did not clean up"
                return
            if deny or not valid:
                self.request.sendall(b"HTTP/1.1 407 Proxy Authentication Required\r\nContent-Length: 0\r\n\r\n")
                return
            with socket.create_connection(target, timeout=5) as upstream:
                self.server.tunnels += 1
                self.request.sendall(b"HTTP/1.1 200 Connection established\r\n\r\n")
                peers = (self.request, upstream)
                while not self.server.stopped.is_set():
                    readable, _, _ = select.select(peers, [], [], 0.1)
                    for source in readable:
                        try:
                            chunk = source.recv(16384)
                            if not chunk:
                                return
                            (upstream if source is self.request else self.request).sendall(chunk)
                        except ConnectionError:
                            return  # Expected when the bridge rejects TLS or cancels a request.

    class Server(socketserver.ThreadingTCPServer):
        def handle_error(self, *_):
            self.handler_failed = True  # Never print an exception containing peer data.

    server = Server(("127.0.0.1", 0), Handler)
    server.connections = server.tunnels = server.tls_handshakes = server.tls_failures = 0
    server.handler_failed = False
    server.started, server.stopped = threading.Event(), threading.Event()
    server.worker = threading.Thread(target=server.serve_forever, daemon=True)
    server.worker.start()
    try:
        yield server
    finally:
        server.stopped.set()
        server.shutdown()
        server.server_close()
        server.worker.join(timeout=5)
        assert not server.worker.is_alive() and not server.handler_failed, "Proxy fixture failed or did not clean up"


def request(url, method="POST", suffix="/echo", body=b'{"test":"private-body"}', *, authorized=True):
    connection = http.client.HTTPConnection(url.hostname, url.port, timeout=10)
    try:
        path = (url.path if authorized else "/wrong-capability") + suffix
        connection.request(method, path, body=body, headers={
            "Authorization": "Bearer private-fixture-token", "Content-Type": "application/json"})
        response = connection.getresponse()
        return response.status, response.read(), response.getheader("Content-Type")
    finally:
        connection.close()


def request_wire(url, suffix):
    with socket.create_connection((url.hostname, url.port), timeout=5) as wire:
        wire.sendall(("POST " + url.path + suffix + " HTTP/1.1\r\nHost: localhost\r\n"
                      "Content-Length: 0\r\nConnection: close\r\n\r\n").encode("ascii"))
        response_bytes = bytearray()
        while True:
            part = wire.recv(4096)
            if not part:
                break
            response_bytes.extend(part)
            assert len(response_bytes) < 65536
        return bytes(response_bytes).split(b"\r\n\r\n", 1)


def cancel_request(process, pending, request_started):
    assert request_started.wait(5), "Upstream request did not start before cancellation"
    start = time.monotonic()
    stdout, stderr = process.communicate("stop\n", timeout=5)
    assert time.monotonic() - start < 5 and process.returncode == 0
    assert not stdout.strip() and not stderr.strip()
    try:
        pending.result(timeout=5)
    except TimeoutError:
        raise  # A stuck worker is not an expected connection closure.
    except (ConnectionError, http.client.HTTPException):
        pass  # Expected: cancellation closes the loopback connection.


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--include-deferred-https-proxy', action='store_true',
                        help='Also validate deferred HTTPS-proxy to HTTPS-origin support')
    return parser.parse_args(argv)


def proxy_schemes(include_deferred=False):
    return ('http', 'https') if include_deferred else ('http',)


def main(executable, *, include_deferred_https_proxy=False):
    records = []
    release_stream, request_started = threading.Event(), threading.Event()

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *_):
            pass

        def do_POST(self):
            if self.headers.get("Proxy-Authorization"):
                self.server.proxy_credentials_seen = True
            body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
            records.append((self.path, body, self.headers.get("Authorization")))
            if self.path.endswith("/redirect"):
                self.send_response(307)
                self.send_header("Location", f"http://127.0.0.1:{self.server.server_port}/never")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            if self.path.endswith("/stall"):
                request_started.set()
                release_stream.wait(15)
                return
            if self.path.endswith(("/trailers", "/forbidden-trailers")):
                self.send_response(200)
                self.send_header("Transfer-Encoding", "chunked")
                self.send_header("Trailer", "X-Checksum")
                self.end_headers()
                self.wfile.write(b"3\r\nabc\r\n0\r\n")
                self.wfile.flush()
                self.wfile.write(b"Content-Length: 99\r\n\r\n" if self.path.endswith("/forbidden-trailers")
                                 else b"X-Checksum: fixture\r\nServer-Timing: total;dur=1\r\n\r\n")
                self.wfile.flush()
                return
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream" if self.path.endswith("/stream") else "application/json")
            self.send_header("Content-Length", "12" if self.path.endswith("/stream") else str(len(body)))
            self.end_headers()
            if self.path.endswith("/stream"):
                self.wfile.write(b"data: 1\n\n")
                self.wfile.flush()
                release_stream.wait(15)
                self.wfile.write(b"end")
            else:
                self.wfile.write(body)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        upstream = f"http://127.0.0.1:{server.server_port}/base/"
        # Verify WinHTTP named-proxy rejection does not fall back to origin.
        with connect_proxy(("127.0.0.1", server.server_port), deny=True) as proxy:
            env = proxy_environment(f"http://127.0.0.1:{proxy.server_address[1]}")
            with bridge(executable, upstream, env=env) as (process, url):
                status = request(url)[0]
                assert status == 407, "HTTP proxy denial mismatch: " + json.dumps({
                    "status": status, "proxy_connections": proxy.connections,
                    "proxy_tunnels": proxy.tunnels, "origin_requests": len(records)})
                assert proxy.connections == 1 and proxy.tunnels == 0 and not records
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
            env = proxy_environment(f"http://127.0.0.1:{proxy.server_address[1]}",
                                    f"127.0.0.1:{server.server_port}")
            with bridge(executable, upstream, env=env) as (process, url):
                assert request(url)[0] == 200 and proxy.connections == 1
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
            assert len(records) == 1
            records.clear()
        with bridge(executable, upstream) as (process, url):
            assert request(url, authorized=False)[0] == 400
            assert not records, "Unauthorized loopback request reached upstream"
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=5)
            try:
                connection.putrequest("POST", "/wrong-capability/echo")
                connection.putheader("Content-Length", "1024")
                connection.endheaders()
                time.sleep(0.05)  # Body is deliberately still in flight when rejected.
                connection.send(b"x" * 1024)
                response = connection.getresponse()
                assert response.status == 400 and response.read() == b""
                assert not records, "Fragmented unauthorized request reached upstream"
            finally:
                connection.close()
            assert request(url) == (200, b'{"test":"private-body"}', "application/json")
            assert records == [("/base/echo", b'{"test":"private-body"}', "Bearer private-fixture-token")]
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=5)
            try:
                connection.request("POST", url.path + "/echo", body=iter([b"abc", b"defg"]),
                                   headers={"Authorization": "Bearer private-fixture-token"}, encode_chunked=True)
                response = connection.getresponse()
                assert response.status == 200 and response.read() == b"abcdefg"
                assert records[-1] == ("/base/echo", b"abcdefg", "Bearer private-fixture-token")
            finally:
                connection.close()
            with ThreadPoolExecutor(max_workers=4) as pool:
                results = list(pool.map(lambda _: request(url), range(4)))
            assert all(result[0] == 200 for result in results)
            before = len(records)
            assert request(url, suffix="/redirect")[0] == 307
            assert len(records) == before + 1, "Native transport followed a redirect"
            connection = http.client.HTTPConnection(url.hostname, url.port, timeout=5)
            connection.request("POST", url.path + "/stream", body=b"{}")
            response = connection.getresponse()
            assert response.read(9) == b"data: 1\n\n", "Transport buffered the stream"
            release_stream.set()
            assert response.read() == b"end"
            connection.close()
            stdout, stderr = process.communicate("stop\n", timeout=5)
            assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
        with bridge(executable, "http://ccode-native-dns.invalid") as (process, url):
            assert request(url)[0] == 502
            stdout, stderr = process.communicate("stop\n", timeout=5)
            assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_DNS" and not stderr.strip()
        fixture_spec = importlib.util.spec_from_file_location(
            "gateway_tls_fixture", Path(__file__).with_name("gateway-integration.py"))
        fixture = importlib.util.module_from_spec(fixture_spec)
        fixture_spec.loader.exec_module(fixture)
        with fixture.untrusted_tls_endpoint() as endpoint:
            with bridge(executable, f"https://127.0.0.1:{endpoint.server_port}") as (process, url):
                assert request(url)[0] == 502
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_TLS" and not stderr.strip()
                assert endpoint.handshake_failed.wait(5), "Untrusted TLS handshake was not rejected: " + json.dumps({
                    "connections": endpoint.connections,
                    "completed": endpoint.tls_handshakes_completed,
                    "errors": endpoint.handshake_errors,
                    "versions": endpoint.tls_versions,
                    "protocol": endpoint.protocol_observations,
                    "http": endpoint.http_requests,
                })
                assert endpoint.http_requests == 0 and endpoint.tls_handshakes_completed == 0
        with bridge(executable, "https://ccode-native-dns.invalid") as (process, url):
            assert request(url)[0] == 502
            stdout, stderr = process.communicate("stop\n", timeout=5)
            assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_DNS" and not stderr.strip()
        # Positive control must use the actual native TLS connection and scoped
        # CA, not merely a certificate-policy helper or an engine substitute.
        with fixture.untrusted_tls_endpoint(trusted_chain=True) as endpoint:
            with bridge(executable, f"https://127.0.0.1:{endpoint.server_port}", endpoint.ca_certificate) as (process, url):
                assert request(url)[0] == 503
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
                assert endpoint.http_requests == 1 and endpoint.tls_handshakes_completed == 1
        for expired, hostname in ((True, "127.0.0.1"), (False, "localhost")):
            with fixture.untrusted_tls_endpoint(expired=expired) as endpoint:
                with bridge(executable, f"https://{hostname}:{endpoint.server_port}", endpoint.ca_certificate) as (process, url):
                    assert request(url)[0] == 502
                    stdout, stderr = process.communicate("stop\n", timeout=5)
                    assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_TLS" and not stderr.strip()
                    assert endpoint.handshake_failed.wait(5)
                    assert endpoint.http_requests == 0 and endpoint.tls_handshakes_completed == 0
        tls_server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        tls_server.daemon_threads = True
        tls_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        certificates = Path(__file__).parent / "fixtures"
        tls_context.load_cert_chain(certificates / "valid-test-cert.pem", certificates / "untrusted-test-key.pem")
        tls_server.socket = tls_context.wrap_socket(tls_server.socket, server_side=True)
        tls_thread = threading.Thread(target=tls_server.serve_forever, daemon=True)
        tls_thread.start()
        try:
            target = ("127.0.0.1", tls_server.server_port)
            for scheme, deny in ((scheme, deny) for scheme in proxy_schemes(include_deferred_https_proxy)
                                 for deny in (False, True)):
                with connect_proxy(target, deny=deny, authorization="Basic dXNlcjpwQHNz",
                                   tls_context=tls_context if scheme == "https" else None) as proxy:
                    env = proxy_environment(f"{scheme}://user:p%40ss@127.0.0.1:{proxy.server_address[1]}")
                    before = len(records)
                    with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                                certificates / "expired-test-ca.pem", env=env) as (process, url):
                        result = request(url)
                        assert result[0] == (502 if deny else 200), "Proxy transport mismatch: " + json.dumps({
                            "scheme": scheme, "denied": deny, "status": result[0],
                            "proxy_connections": proxy.connections, "proxy_tls": proxy.tls_handshakes,
                            "proxy_tunnels": proxy.tunnels, "origin_requests": len(records) - before})
                        assert proxy.tls_handshakes == (1 if scheme == "https" else 0)
                        assert proxy.connections == 1 and proxy.tunnels == (0 if deny else 1)
                        assert len(records) == before + (0 if deny else 1), "Proxy denial fell back to origin"
                        assert not getattr(tls_server, "proxy_credentials_seen", False), "Proxy credentials reached origin"
                        stdout, stderr = process.communicate("stop\n", timeout=5)
                        assert process.returncode == 0 and stdout.strip() == ("E_NETWORK" if deny else "") and not stderr.strip()
            if include_deferred_https_proxy:
                for certificate, proxy_host in (("valid-test-cert.pem", "localhost"),
                                                ("expired-test-cert.pem", "127.0.0.1"),
                                                ("untrusted-test-cert.pem", "127.0.0.1")):
                    rejected_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                    rejected_context.load_cert_chain(certificates / certificate,
                                                     certificates / "untrusted-test-key.pem")
                    before = len(records)
                    with connect_proxy(target, tls_context=rejected_context) as proxy:
                        env = proxy_environment(f"https://user:p%40ss@{proxy_host}:{proxy.server_address[1]}")
                        with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                                    certificates / "expired-test-ca.pem", env=env) as (process, url):
                            assert request(url)[0] == 502
                            stdout, stderr = process.communicate("stop\n", timeout=5)
                            assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_TLS" and not stderr.strip()
                    assert proxy.connections == proxy.tls_failures == 1
                    assert proxy.tls_handshakes == proxy.tunnels == 0 and not proxy.started.is_set()
                    assert len(records) == before, "Invalid proxy TLS contacted origin"
            with connect_proxy(target) as proxy:
                env = proxy_environment(f"http://127.0.0.1:{proxy.server_address[1]}",
                                        f"127.0.0.1:{tls_server.server_port}")
                with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                            certificates / "expired-test-ca.pem", env=env) as (process, url):
                    assert request(url)[0] == 200 and proxy.connections == 0
                    stdout, stderr = process.communicate("stop\n", timeout=5)
                    assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
            with connect_proxy(target, authority_host="localhost") as proxy:
                before = len(records)
                env = proxy_environment(f"http://127.0.0.1:{proxy.server_address[1]}")
                with bridge(executable, f"https://localhost:{tls_server.server_port}/base/",
                            certificates / "expired-test-ca.pem", env=env) as (process, url):
                    assert request(url)[0] == 502
                    assert proxy.connections == proxy.tunnels == 1 and len(records) == before
                    stdout, stderr = process.communicate("stop\n", timeout=5)
                    assert process.returncode == 0 and stdout.strip() == "E_GATEWAY_TLS" and not stderr.strip()
            for scheme in proxy_schemes(include_deferred_https_proxy):
                with connect_proxy(target, stall=True,
                                   tls_context=tls_context if scheme == "https" else None) as proxy:
                    env = proxy_environment(f"{scheme}://127.0.0.1:{proxy.server_address[1]}")
                    with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                                certificates / "expired-test-ca.pem", env=env) as (process, url):
                        with ThreadPoolExecutor(max_workers=1) as pool:
                            pending = pool.submit(request, url)
                            cancel_request(process, pending, proxy.started)
            if not include_deferred_https_proxy:
                print("DEFERRED (not a pass): HTTPS proxy to HTTPS API; enable --include-deferred-https-proxy to test")
            print("PASS: actual HTTPS CONNECT, proxy auth isolation, denial without fallback, scoped bypass, TLS rejection and cancellation")
            with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                        certificates / "expired-test-ca.pem") as (process, url):
                assert request(url) == (200, b'{"test":"private-body"}', "application/json")
                assert records[-1] == ("/base/echo", b'{"test":"private-body"}', "Bearer private-fixture-token")
                with ThreadPoolExecutor(max_workers=4) as pool:
                    responses = list(pool.map(lambda _: request(url), range(4)))
                assert responses == [(200, b'{"test":"private-body"}', "application/json")] * 4
                # Inspect the actual loopback wire: metadata stays in the trailer
                # section, not merged into headers or discarded by the bridge.
                header, body = request_wire(url, "/trailers")
                assert b"X-Checksum: fixture" not in header
                assert body == b"3\r\nabc\r\n0\r\nX-Checksum: fixture\r\nServer-Timing: total;dur=1\r\n\r\n"
                release_stream.clear()
                connection = http.client.HTTPConnection(url.hostname, url.port, timeout=5)
                try:
                    connection.request("POST", url.path + "/stream", body=b"{}")
                    response = connection.getresponse()
                    assert response.read(9) == b"data: 1\n\n", "Scoped TLS transport buffered the stream"
                    release_stream.set()
                    assert response.read() == b"end"
                finally:
                    connection.close()
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and not stdout.strip() and not stderr.strip()
            with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                        certificates / "expired-test-ca.pem") as (process, url):
                header, body = request_wire(url, "/forbidden-trailers")
                assert b"Content-Length: 99" not in body
                assert header.startswith(b"HTTP/1.1 502 ") or (
                    header.startswith(b"HTTP/1.1 200 ") and not body.endswith(b"\r\n\r\n"))
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert process.returncode == 0 and stdout.strip() == "E_NETWORK" and not stderr.strip()
            release_stream.clear()
            request_started.clear()
            with bridge(executable, f"https://127.0.0.1:{tls_server.server_port}/base/",
                        certificates / "expired-test-ca.pem") as (process, url):
                with ThreadPoolExecutor(max_workers=1) as pool:
                    pending = pool.submit(request, url, suffix="/stall")
                    cancel_request(process, pending, request_started)
        finally:
            release_stream.set()
            tls_server.shutdown()
            tls_server.server_close()
            tls_thread.join()
        release_stream.clear()
        request_started.clear()
        with bridge(executable, upstream) as (process, url):
            with ThreadPoolExecutor(max_workers=1) as pool:
                pending = pool.submit(request, url, suffix="/stall")
                cancel_request(process, pending, request_started)
    finally:
        release_stream.set()
        server.shutdown()
        server.server_close()
        thread.join()
    print("PASS: native HTTP/HTTPS request forwarding, capability rejection, concurrency, streaming, no redirect, real DNS cause, cancellation cleanup")


if __name__ == "__main__":
    args = parse_arguments()
    main(args.executable.resolve(), include_deferred_https_proxy=args.include_deferred_https_proxy)
