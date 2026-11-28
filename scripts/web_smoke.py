#!/usr/bin/env python3
"""Linux integration smoke checks for .env and Web dashboard path guards.

Run after building: QBT_BUILD_DIR=build python3 scripts/web_smoke.py
Only owns/kills its spawned processes and writes fixtures inside a private temp dir.
"""
from pathlib import Path
import os, sys, tempfile, socket, subprocess, json, time, urllib.request, urllib.error, urllib.parse, hashlib, re, shutil
root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get('QBT_BUILD_DIR', str(root / 'build'))).resolve()
work = Path(tempfile.mkdtemp(prefix='qbt-web-validation-'))
evidence = root / 'docs/evidence'
evidence.mkdir(parents=True, exist_ok=True)
checks = []
processes = []
completed = False

def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]

def request(port, path, body=None):
    raw = body if isinstance(body, bytes) else None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(f'http://127.0.0.1:{port}' + path, data=raw, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=3) as r:
            return (r.status, r.read().decode())
    except urllib.error.HTTPError as e:
        return (e.code, e.read().decode())

def ready(port, path):
    for _ in range(100):
        try:
            code, body = request(port, path)
            if code == 200:
                return json.loads(body)
        except (OSError, ValueError):
            pass
        time.sleep(0.1)
    raise AssertionError('service failed to start: ' + str(port))

def record(name, condition, details=''):
    checks.append({'check': name, 'result': 'PASS' if condition else 'FAIL', 'details': details})
    assert condition, (name, details)
try:
    tracker_port = free_port()
    web_port = free_port()
    url = f'http://127.0.0.1:{tracker_port}'
    env = os.environ.copy()
    keys = ['TRACKER_PORT', 'TRACKER_URL', 'PEER_PORT', 'PEER_HOST', 'PEER_DATA_DIR', 'WEBUI_PORT', 'MAX_ACTIVE_DOWNLOADS', 'MAX_ACTIVE_UPLOADS', 'PIPELINE_DEPTH', 'IO_TIMEOUT_MS']
    for key in keys:
        env.pop(key, None)
    (work / '.env').write_text(f'TRACKER_PORT=1\nTRACKER_URL={url}\nPEER_PORT=0\nPEER_HOST=127.0.0.1\nPEER_DATA_DIR={work}/state\nWEBUI_PORT={web_port}\n')

    def start(name, binary, args):
        log = (evidence / (name + '.log')).open('w')
        p = subprocess.Popen([str(build / binary), *args], cwd=work, env=env, stdout=log, stderr=subprocess.STDOUT)
        processes.append((p, log))
        return p
    start('web-smoke-tracker', 'src/tracker/TrackerApp', ['--port', str(tracker_port)])
    ready(tracker_port, '/health')
    record('tracker CLI overrides .env port', True)
    start('web-smoke-peer', 'src/peer/PeerApp', ['--webui'])
    status = ready(web_port, '/api/status')
    record('peer reads .env URL, auto port, data dir and web port', status['tracker_url'] == url and status['port'] > 0 and (work / 'state/peer.db').exists())
    payload = b'DS02 web validation payload\n' * 1024
    code, body = request(web_port, '/api/upload_seed?filename=safe.bin', payload)
    record('valid browser upload can seed a file', code == 200 and (work / 'state/shared/safe.bin').read_bytes() == payload, str(code))
    for name in ['../escape.bin', 'bad\\escape.bin', '/tmp/escape.bin', '..', '']:
        code, _ = request(web_port, '/api/upload_seed?' + urllib.parse.urlencode({'filename': name}), b'not-allowed')
        record('reject unsafe upload filename ' + repr(name), code == 400, str(code))
    for path, body in [('/api/seed', {'file_path': '/etc/hosts'}), ('/api/download', {'torrent_path': '/etc/hosts'}), ('/api/upload_torrent', {'file_name': '../outside.bin'})]:
        code, _ = request(web_port, path, body)
        record('reject escaped path at ' + path, code == 400, str(code))
    for chunk_size in ['0', '1', '999999999999', '-1', 'abc']:
        upload_path = '/api/upload_seed?' + urllib.parse.urlencode({
            'filename': 'invalid-chunk-size.bin',
            'chunk_size': chunk_size,
        })
        code, body = request(web_port, upload_path, b'invalid chunk size')
        error = json.loads(body).get('error') if body else None
        no_file_written = not (work / 'state/shared/invalid-chunk-size.bin').exists()
        record('reject invalid upload chunk_size ' + repr(chunk_size),
               code == 400 and error == 'chunk_size must be between 1024 and 67108864 bytes' and no_file_written,
               str(code))
    for chunk_size in [0, 1, 999999999999, -5, 'abc']:
        code, body = request(web_port, '/api/seed', {
            'file_path': 'shared/safe.bin',
            'chunk_size': chunk_size,
        })
        error = json.loads(body).get('error') if body else None
        record('reject invalid seed chunk_size ' + repr(chunk_size),
               code == 400 and error == 'chunk_size must be between 1024 and 67108864 bytes',
               str(code))
    code, _ = request(web_port, '/api/status')
    record('web service remains usable after invalid chunk_size requests', code == 200, str(code))
    status = ready(web_port, '/api/status')
    meta = json.loads((work / 'state/shared/safe.bin.torrent.json').read_text())
    record('upload has trusted metadata and peer registration', meta['file_hash'] == hashlib.sha256(payload).hexdigest())
    peers = ready(tracker_port, '/peers?file_id=' + meta['file_hash'])
    record('tracker discovers seeded peer', any((p['peer_id'] == status['peer_id'] for p in peers)))
    meta['file_name'] = '../../outside.bin'
    (work / 'malicious.torrent.json').write_text(json.dumps(meta))
    child = subprocess.run([str(build / 'src/peer/PeerApp'), '--tracker', url, '--port', '0', '--data-dir', str(work / 'leech-state'), '--download', str(work / 'malicious.torrent.json'), '--output-dir', str(work / 'sandbox/output'), '--exit-after-download'], cwd=work, env=env, capture_output=True, text=True, timeout=20)
    (evidence / 'web-smoke-download.log').write_text(child.stdout + child.stderr)
    downloaded = work / 'sandbox/output/download.bin'
    record('metadata filename is sanitized and download remains correct', child.returncode == 0 and downloaded.exists() and (downloaded.read_bytes() == payload) and (not (work / 'outside.bin').exists()), str(child.returncode))
    code, _ = request(web_port, '/api/status')
    record('web service remains usable after invalid requests', code == 200)
    completed = True
finally:
    for proc, log in reversed(processes):
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=7)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        log.close()
    (evidence / 'web-smoke.json').write_text(json.dumps({'checks': checks, 'all_passed': completed and bool(checks) and all((c['result'] == 'PASS' for c in checks))}, ensure_ascii=False, indent=2) + '\n')
    shutil.rmtree(work)
print(json.dumps({'checks': len(checks), 'passed': sum((c['result'] == 'PASS' for c in checks))}))
