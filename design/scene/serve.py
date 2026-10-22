"""Serves the scene and saves what it renders.

    python3 serve.py            (then open http://localhost:8767/spread-scene.html?only=...)

The page POSTs each finished image as a data URL to /save?name=<file>; it lands in out/.
"""
import base64, http.server, os, urllib.parse

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "out")

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ROOT, **k)

    def do_POST(self):
        query = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
        name = os.path.basename(query.get("name", ["image.png"])[0])
        body = self.rfile.read(int(self.headers["Content-Length"])).decode()
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(base64.b64decode(body.split(",", 1)[1]))
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(b"ok")

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

http.server.ThreadingHTTPServer(("127.0.0.1", 8767), Handler).serve_forever()
