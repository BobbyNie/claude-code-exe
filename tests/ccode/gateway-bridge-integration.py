"""Real Windows native transport, no engine substitute and no private logs."""
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import http.client
from pathlib import Path
import subprocess
import sys
import threading
import time
from urllib.parse import urlsplit


@contextmanager
def bridge(executable, upstream):
    process = subprocess.Popen([str(executable), upstream], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
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


def main(executable):
    records = []
    release_stream, request_started = threading.Event(), threading.Event()

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *_):
            pass

        def do_POST(self):
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
        with bridge(executable, upstream) as (process, url):
            assert request(url, authorized=False)[0] == 400
            assert not records, "Unauthorized loopback request reached upstream"
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
        release_stream.clear()
        with bridge(executable, upstream) as (process, url):
            with ThreadPoolExecutor(max_workers=1) as pool:
                pending = pool.submit(request, url, suffix="/stall")
                assert request_started.wait(5)
                start = time.monotonic()
                stdout, stderr = process.communicate("stop\n", timeout=5)
                assert time.monotonic() - start < 5 and process.returncode == 0
                assert not stdout.strip() and not stderr.strip()
                try:
                    pending.result(timeout=5)
                except (OSError, http.client.HTTPException):
                    pass  # Expected: cancellation closes the loopback connection.
    finally:
        release_stream.set()
        server.shutdown()
        server.server_close()
        thread.join()
    print("PASS: native HTTP request forwarding, capability rejection, concurrency, streaming, no redirect, real DNS cause, cancellation cleanup")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve())
