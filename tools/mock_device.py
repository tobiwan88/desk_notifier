"""In-process mock of the device API for tests."""
from __future__ import annotations

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TOKEN = "testtoken"


class MockDevice:
    def __init__(self):
        self.clips: dict[str, bytes] = {}
        self.eye_cmds: list[dict] = []
        self.play_cmds: list[dict] = []
        self.stops = 0
        mock = self

        class H(BaseHTTPRequestHandler):
            def log_message(self, *a):  # silence
                pass

            def _send(self, code, body=None):
                data = json.dumps(body).encode() if body is not None else b""
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def _err(self, code, name, msg):
                self._send(code, {"error": {"code": name, "message": msg}})

            def _auth(self):
                if self.headers.get("Authorization") != f"Bearer {TOKEN}":
                    self._err(401, "E_UNAUTHORIZED", "wrong token")
                    return False
                return True

            def _body(self):
                return self.rfile.read(int(self.headers.get("Content-Length", 0)))

            def do_GET(self):
                if self.path == "/api/status":
                    return self._send(200, {"version": "mock", "hostname": "eyes-mock", "fps": 30.0, "uptime_s": 1,
                                            "heap_internal": 100000, "heap_min": 90000, "frame_ms_max": 20,
                                            "source": {"left": "eyes", "right": "eyes"}, "fs_free": 3000000,
                                            "clip_max_bps": 1500000, "spi_hz": 20000000, "ip": "127.0.0.1", "rssi": -50})
                if not self._auth():
                    return
                if self.path == "/api/clips":
                    return self._send(200, {"clips": [{"name": n, "size": len(d), "frames": 1, "fps": 10.0} for n, d in mock.clips.items()],
                                            "free": 3000000})
                self._err(404, "E_NOT_FOUND", "no route")

            def do_POST(self):
                if not self._auth():
                    return
                body = self._body()
                if self.path == "/api/eye":
                    mock.eye_cmds.append(json.loads(body))
                    return self._send(202, {"queued": True})
                if self.path == "/api/play":
                    mock.play_cmds.append(json.loads(body))
                    return self._send(202, {"queued": True})
                if self.path == "/api/stop":
                    mock.stops += 1
                    return self._send(202, {"queued": True})
                self._err(404, "E_NOT_FOUND", "no route")

            def do_PUT(self):
                if not self._auth():
                    return
                body = self._body()
                name = self.path.rsplit("/", 1)[-1]
                if body[:4] != b"EYCL":
                    return self._err(422, "E_BAD_CLIP", "invalid clip")
                mock.clips[name] = body
                self._send(201, {"name": name, "size": len(body), "frames": 1})

            def do_DELETE(self):
                if not self._auth():
                    return
                name = self.path.rsplit("/", 1)[-1]
                if mock.clips.pop(name, None) is None:
                    return self._err(404, "E_NOT_FOUND", "no such clip")
                self._send(204)

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)

    @property
    def host(self) -> str:
        return f"127.0.0.1:{self.server.server_address[1]}"

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *a):
        self.server.shutdown()
        self.server.server_close()
