# Local HTTP server for the job and storage tests (tests/harness): /delay/<s>, /status/<code>, /flaky/<id> (503 once,
# then 200), /flaky400/<id> (400 once, then 200), /retryafter/<s>/<id> (429 with Retry-After: <s> once, then 200),
# /redirect -> /final, /file/<name> (some bytes), /image/<w>x<h> (a png), /xorimage/<w>x<h>/<hexkey> (the png
# xor-encrypted), /lastheaders (the headers of the last image request), /headers (the headers of this request).
# HEAD requests get the headers of the GET answer. /site/... is a small manga site for the definitions of sources
# (see site()).
import http.server, struct, sys, time, zlib
flaky = set()
lastheaders = ['']

def site(path):
    """The pages of a manga site: a paged directory (/site/dir/<n>: 2 mangas on pages 1-3, 1 on page 4, then
    none), the whole list (/site/all, cut off the first time) with popular pages (/site/popular/<n>, 1 and 2), the
    info of manga 1 and of a licensed one (gone), a chapter with a long strip (ch/3) and chapters with pages."""
    if path.startswith('/dir/'):
        n = int(path.split('/')[2])
        ids = {1: [1, 2], 2: [3, 4], 3: [5, 6], 4: [7]}.get(n, [])
        # a title with a < (no tag in an attribute)
        return 200, ''.join(f'<a class="m" href="/site/manga/{i}" title="Manga {i}{" <Special>" if i == 7 else ""}">'
                            f'<img></a> Views: {i * 10}<br>' for i in ids)
    if path == '/all':
        mangas = ''.join(f'<li><a href="/site/manga/{i}">Manga  {i}</a></li>' for i in [1, 2, 3, 4, 5, 2])
        if path not in flaky:
            flaky.add(path)
            return 200, mangas[:40]
        return 200, mangas + '<footer>'
    if path.startswith('/popular/'):
        n = int(path.split('/')[2])
        if n > 2:
            return 404, ''
        return 200, f'<b><a href="/site/manga/{2 * n + 1}">Manga {2 * n + 1}</a></b>'
    if path == '/manga/1':
        return 200, ('<div class="info"><img src="//example.com/cover.jpg"></div>'
                     '<li><b>Author:</b><a href="/a">A &amp; B</a>, <a href="/b">[Add]</a>, <a href="/c">A &amp; B</a></li>'
                     '<li><b>Genres:</b><a>Action</a>, <a>Drama</a></li>'
                     '<p class="sum">Text [junk]more\r\nSecond line</p>'
                     '<ul class="chapters"><li><a href="/site/ch/3">Ch 3</a> <span>Finale</span></li>'
                     '<li><a href="/site/ch/2">Ch 2</a></li><li><a href="/site/ch/2">Ch 2 again</a></li>'
                     '<li><a href="../ch/1">Ch  1</a></li></ul><p class="msg">not this</p>')
    if path == '/manga/gone':
        return 200, '<div class="info"></div><p class="msg">It has been licensed</p>'
    if path == '/ch/3':
        return 200, ('<div id="viewer"><img src="/image/1x1" class="strip"><img src="/image/2x2" class="strip">'
                     '</div><div class="end"><img src="/image/3x3" class="strip">')
    if path.startswith('/ch/') and path.endswith('.html'):
        page = int(path.split('/')[-1][:-5])
        image = '<img src="/image/1x1" id="image">' if page == 1 else '<img src="/image/2x2">'
        return 200, f'<img src="/logo.png"><div id="viewer">{image}</div>'
    if path.startswith('/ch/'):
        return 200, '<div id="viewer"></div><script>var none = 0; var total = 2;</script>'
    return 404, ''

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
        elif p.startswith('/site/'):
            code, body = site(p[5:])
            self.send(code, body.encode())
        else:
            self.send(404)
    do_HEAD = do_GET
http.server.ThreadingHTTPServer(('127.0.0.1', int(sys.argv[1])), H).serve_forever()
