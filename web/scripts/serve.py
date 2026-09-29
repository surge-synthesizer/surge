#!/usr/bin/env python3
"""Development-only static server with the shared-memory isolation headers."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

class Handler(SimpleHTTPRequestHandler):
    # Reuse connections for the bootstrap scripts and worker pool instead of
    # closing a socket for every resource in Chrome's parallel startup burst.
    protocol_version = 'HTTP/1.1'
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Resource-Policy', 'same-origin')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()

class Server(ThreadingHTTPServer):
    request_queue_size = 128

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', default='build-web/web')
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    Server(('127.0.0.1', args.port), partial(Handler, directory=args.directory)).serve_forever()
