"""Validate that interrupted-stream fixtures exercise semantic EOF boundaries."""
import importlib.util
import json
import socket
import ssl
from pathlib import Path
import unittest
from types import SimpleNamespace
import threading

spec = importlib.util.spec_from_file_location(
    "gateway_fixture", Path(__file__).parent / "ccode/gateway-integration.py")
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class GatewayFixtureTests(unittest.TestCase):
    def test_unreachable_endpoint_reserves_port_without_accepting_connections(self):
        with fixture.unreachable_endpoint() as address:
            self.assertEqual(address[0], "127.0.0.1")
            with socket.socket() as client:
                client.settimeout(2)
                self.assertNotEqual(client.connect_ex(address), 0)
            with socket.socket() as competitor:
                with self.assertRaises(OSError):
                    competitor.bind(address)

    def test_tls_endpoint_rejects_untrusted_certificate_but_accepts_explicit_trust(self):
        with fixture.untrusted_tls_endpoint() as endpoint:
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with self.assertRaises(ssl.SSLCertVerificationError):
                    ssl.create_default_context().wrap_socket(connection, server_hostname="127.0.0.1")
            self.assertTrue(endpoint.handshake_failed.wait(2), "No actual failed TLS handshake observed")
            trusted = ssl.create_default_context(cafile=str(endpoint.certificate))
            with socket.create_connection(endpoint.address, timeout=2) as connection:
                with trusted.wrap_socket(connection, server_hostname="127.0.0.1") as secure:
                    secure.sendall(b"GET / HTTP/1.0\r\nHost: localhost\r\n\r\n")
                    self.assertIn(b"503", secure.recv(4096))
            self.assertEqual(endpoint.http_requests, 1)

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
