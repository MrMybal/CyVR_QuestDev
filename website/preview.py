# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Serve only the generated showcase on loopback, including a static subpath."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent


class Preview(SimpleHTTPRequestHandler):
    def do_GET(self):
        self._serve()

    def do_HEAD(self):
        self._serve(head=True)

    def _serve(self, head=False):
        path = unquote(urlsplit(self.path).path)
        prefix = self.server.prefix
        if not path.startswith(prefix):
            self.send_error(404)
            return
        relative = path[len(prefix):]
        candidate = (Path(self.directory) / relative).resolve()
        if not candidate.is_relative_to(Path(self.directory).resolve()):
            self.send_error(403)
            return
        if candidate.is_dir():
            candidate /= 'index.html'
        if not candidate.is_file():
            self.send_error(404)
            return
        content = candidate.read_bytes()
        self.send_response(200)
        self.send_header('Content-Type', self.guess_type(str(candidate)) + ('; charset=utf-8' if candidate.suffix in {'.html', '.css', '.js', '.md', '.txt'} else ''))
        self.send_header('Content-Length', str(len(content)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        if not head:
            self.wfile.write(content)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=ROOT / 'dist/website')
    parser.add_argument('--port', type=int, default=50870)
    parser.add_argument('--prefix', default='/questdev/')
    args = parser.parse_args()
    if not (args.directory / 'showcase-build.json').is_file():
        parser.error('Run python website/build.py first')
    if not args.prefix.startswith('/') or not args.prefix.endswith('/') or '..' in args.prefix or '?' in args.prefix:
        parser.error('Prefix must be a path beginning and ending with /')
    server = ThreadingHTTPServer(('127.0.0.1', args.port), partial(Preview, directory=str(args.directory.resolve())))
    server.prefix = args.prefix
    print(f'PREVIEW_URL=http://127.0.0.1:{server.server_port}{server.prefix}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
