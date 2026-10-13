# Local HTTP server for the job and storage tests (tests/harness): /delay/<s>, /status/<code>, /flaky/<id> (503 once,
# then 200), /flaky400/<id> (400 once, then 200), /retryafter/<s>/<id> (429 with Retry-After: <s> once, then 200),
# /redirect -> /final, /file/<name> (some bytes), /image/<w>x<h> (a png), /xorimage/<w>x<h>/<hexkey> (the png
# xor-encrypted), /lastheaders (the headers of the last image request), /headers (the headers of this request).
# HEAD requests get the headers of the GET answer.
import http.server, struct, sys, time, zlib
flaky = set()
lastheaders = ['']

def png(w, h):
    raw = b''.join(b'\x00' + bytes((x * 7 + y * 3) % 256 for x in range(w)) for y in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 0, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def send(self, code, body=b'', headers=()):
        self.send_response(code)
        for k, v in headers: self.send_header(k, v)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        if self.command != 'HEAD':
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass  # the client stopped waiting (timeout and cancel tests)

    def do_GET(self):
        p = self.path
        if p.startswith('/delay/'):
            time.sleep(float(p.split('/')[2])); self.send(200, b'late')
        elif p.startswith('/status/'):
            self.send(int(p.split('/')[2]), b'status')
        elif p.startswith('/flaky/'):
            if p in flaky: self.send(200, b'ok after retry')
            else: flaky.add(p); self.send(503, b'busy')
        elif p.startswith('/flaky400/'):
            if p in flaky: self.send(200, b'ok after 400')
            else: flaky.add(p); self.send(400, b'bad request')
        elif p.startswith('/retryafter/'):
            if p in flaky: self.send(200, b'ok after retry-after')
            else: flaky.add(p); self.send(429, b'slow down', [('Retry-After', p.split('/')[2])])
        elif p == '/redirect':
            self.send(302, headers=[('Location', '/final')])
        elif p == '/final':
            self.send(200, b'final')
        elif p.startswith('/file/'):
            self.send(200, b'x' * 10000)
        elif p.startswith('/image/') or p.startswith('/xorimage/'):
            lastheaders[0] = '\n'.join(f'{k}: {v}' for k, v in self.headers.items())
            parts = p.split('/')
            w, h = (int(v) for v in parts[2].split('x'))
            data = png(w, h)
            if p.startswith('/xorimage/'):
                key = bytes.fromhex(parts[3])
                data = bytes(b ^ key[i % len(key)] for i, b in enumerate(data))
            self.send(200, data, [('Content-Type', 'image/png')])
        elif p == '/lastheaders':
            self.send(200, lastheaders[0].encode())
        elif p == '/headers':
            self.send(200, '\n'.join(f'{k}: {v}' for k, v in self.headers.items()).encode())
        else:
            self.send(404)
    do_HEAD = do_GET
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
