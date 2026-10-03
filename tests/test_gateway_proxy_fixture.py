"""Proxy fixture fails closed and never records request credentials or bodies."""
import importlib.util
from pathlib import Path
import socket
import http.client
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import ssl
import gc
import warnings
from concurrent.futures import ThreadPoolExecutor
import unittest

spec = importlib.util.spec_from_file_location('bridge_proxy_fixture',
    Path(__file__).parent / 'ccode/gateway-bridge-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ProxyFixtureTests(unittest.TestCase):
    def test_http_forwarding_requires_auth_and_strips_proxy_credentials(self):
        seen = []
        class Origin(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass
            def do_POST(self):
                body = self.rfile.read(int(self.headers['Content-Length']))
                seen.append((self.path, body, self.headers.get('Authorization'),
                             self.headers.get('Proxy-Authorization')))
                self.send_response(200)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)
        origin = ThreadingHTTPServer(('127.0.0.1', 0), Origin)
        worker = threading.Thread(target=origin.serve_forever)
        worker.start()
        try:
            with fixture.forward_proxy(origin.server_address, authorization='Basic fixture') as proxy:
                target = f'http://127.0.0.1:{origin.server_port}/base/echo?q=1'
                for uri, auth, status in ((target, '', 407),
                                          ('http://other.invalid/echo', 'Basic fixture', 407),
                                          (target, 'Basic fixture', 200)):
                    connection = http.client.HTTPConnection(*proxy.server_address, timeout=5)
                    try:
                        connection.request('POST', uri, body=b'fixture', headers={
                            'Proxy-Authorization': auth, 'Authorization': 'Bearer fixture'})
                        response = connection.getresponse()
                        self.assertEqual(response.status, status)
                        body = response.read()
                        if status == 200:
                            self.assertEqual(body, b'fixture')
                    finally:
                        connection.close()
                self.assertEqual(proxy.forwarded, 1)
                self.assertEqual(seen, [('/base/echo?q=1', b'fixture', 'Bearer fixture', None)])
            self.assertFalse(proxy.worker.is_alive())
        finally:
            origin.shutdown()
            origin.server_close()
            worker.join(timeout=5)
        self.assertFalse(worker.is_alive())

    def test_native_suite_requires_authenticated_http_forwarding(self):
        import inspect
        source = inspect.getsource(fixture.main)
        self.assertIn('with forward_proxy(', source)
        self.assertIn('assert proxy.forwarded == 1', source)
        self.assertIn('assert not server.proxy_credentials_seen', source)

    def test_release_proxy_scope_retains_http_and_requires_explicit_https_opt_in(self):
        args = fixture.parse_arguments(['probe.exe'])
        self.assertEqual(args.executable, Path('probe.exe'))
        self.assertEqual(fixture.proxy_schemes(args.include_deferred_https_proxy), ('http',))
        args = fixture.parse_arguments(['probe.exe', '--include-deferred-https-proxy'])
        self.assertEqual(fixture.proxy_schemes(args.include_deferred_https_proxy), ('http', 'https'))

    def test_http_denial_consumes_delayed_body_before_closing_connection(self):
        # A close with unread POST bytes can reset TCP and hide the 407 on Windows.
        with fixture.connect_proxy(('127.0.0.1', 9), deny=True) as proxy:
            with socket.create_connection(proxy.server_address, timeout=3) as connection:
                connection.sendall(b'POST http://127.0.0.1:9/ HTTP/1.1\r\n'
                                   b'Host: 127.0.0.1:9\r\nContent-Length: 7\r\n\r\n')
                connection.settimeout(0.2)
                with self.assertRaises(socket.timeout):
                    connection.recv(4096)
                connection.sendall(b'fixture')
                connection.settimeout(3)
                self.assertTrue(connection.recv(4096).startswith(b'HTTP/1.1 407 '))
            self.assertEqual(proxy.connections, 1)
            self.assertEqual(proxy.tunnels, 0)
        self.assertFalse(proxy.worker.is_alive())

    def test_denial_observes_one_connect_and_does_not_open_upstream(self):
        with fixture.connect_proxy(('127.0.0.1', 9), deny=True) as proxy:
            with socket.create_connection(proxy.server_address, timeout=3) as connection:
                connection.sendall(b'CONNECT 127.0.0.1:9 HTTP/1.1\r\nHost: 127.0.0.1:9\r\n\r\n')
                self.assertTrue(connection.recv(4096).startswith(b'HTTP/1.1 407 '))
            self.assertEqual(proxy.connections, 1)
            self.assertEqual(proxy.tunnels, 0)
        self.assertFalse(proxy.worker.is_alive())

    def test_bad_authority_is_rejected_without_contacting_target(self):
        with fixture.connect_proxy(('127.0.0.1', 9)) as proxy:
            with socket.create_connection(proxy.server_address, timeout=3) as connection:
                connection.sendall(b'CONNECT other.invalid:443 HTTP/1.1\r\n\r\n')
                self.assertTrue(connection.recv(4096).startswith(b'HTTP/1.1 407 '))
            self.assertEqual(proxy.tunnels, 0)

    def test_proxy_connect_headers_travel_inside_verified_tls(self):
        certificates = Path(__file__).parent / 'ccode/fixtures'
        server_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        server_context.load_cert_chain(certificates / 'valid-test-cert.pem',
                                       certificates / 'untrusted-test-key.pem')
        client_context = ssl.create_default_context(cafile=str(certificates / 'expired-test-ca.pem'))
        with fixture.connect_proxy(('127.0.0.1', 9), deny=True, tls_context=server_context) as proxy:
            with socket.create_connection(proxy.server_address, timeout=3) as raw:
                with client_context.wrap_socket(raw, server_hostname='127.0.0.1') as connection:
                    connection.sendall(b'CONNECT 127.0.0.1:9 HTTP/1.1\r\n\r\n')
                    self.assertTrue(connection.recv(4096).startswith(b'HTTP/1.1 407 '))
            self.assertEqual(proxy.connections, 1)
            self.assertEqual(proxy.tls_handshakes, 1)
            self.assertEqual(proxy.tunnels, 0)

    def test_tls_proxy_closes_wrapped_socket(self):
        with warnings.catch_warnings(record=True) as seen:
            warnings.simplefilter('always', ResourceWarning)
            self.test_proxy_connect_headers_travel_inside_verified_tls()
            gc.collect()
        self.assertFalse(any(issubclass(item.category, ResourceWarning) for item in seen),
                         'TLS proxy fixture leaked a socket')

    def test_tls_proxy_relays_payload_to_only_the_fixed_target(self):
        certificates = Path(__file__).parent / 'ccode/fixtures'
        server_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        server_context.load_cert_chain(certificates / 'valid-test-cert.pem',
                                       certificates / 'untrusted-test-key.pem')
        client_context = ssl.create_default_context(cafile=str(certificates / 'expired-test-ca.pem'))
        payload = b'fixture-' * 8193
        with socket.socket() as target:
            target.bind(('127.0.0.1', 0))
            target.listen(1)
            target.settimeout(5)
            def receive():
                with target.accept()[0] as peer:
                    peer.settimeout(5)
                    received = bytearray()
                    while len(received) < len(payload):
                        part = peer.recv(4096)
                        if not part:
                            break
                        received.extend(part)
                    peer.sendall(b'accepted')
                    return len(received), received == payload
            with ThreadPoolExecutor(max_workers=1) as pool:
                pending = pool.submit(receive)
                with fixture.connect_proxy(target.getsockname(), tls_context=server_context) as proxy:
                    with socket.create_connection(proxy.server_address, timeout=5) as raw:
                        with client_context.wrap_socket(raw, server_hostname='127.0.0.1') as connection:
                            connection.sendall(f'CONNECT 127.0.0.1:{target.getsockname()[1]} HTTP/1.1\r\n\r\n'.encode())
                            header = bytearray()
                            while not header.endswith(b'\r\n\r\n'):
                                part = connection.recv(1)
                                self.assertTrue(part)
                                header.extend(part)
                                self.assertLess(len(header), 4096)
                            self.assertTrue(header.startswith(b'HTTP/1.1 200 '))
                            connection.sendall(payload)
                            response = bytearray()
                            while len(response) < 8:
                                part = connection.recv(8 - len(response))
                                self.assertTrue(part)
                                response.extend(part)
                            self.assertEqual(response, b'accepted')
                    self.assertEqual(pending.result(timeout=5), (len(payload), True))
                    self.assertEqual((proxy.connections, proxy.tls_handshakes, proxy.tunnels), (1, 1, 1))
