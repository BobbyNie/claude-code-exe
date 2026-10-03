"""Proxy fixture fails closed and never records request credentials or bodies."""
import importlib.util
from pathlib import Path
import socket
import unittest

spec = importlib.util.spec_from_file_location('bridge_proxy_fixture',
    Path(__file__).parent / 'ccode/gateway-bridge-integration.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class ProxyFixtureTests(unittest.TestCase):
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
