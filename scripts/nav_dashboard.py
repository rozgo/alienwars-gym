#!/usr/bin/env python3
"""Local read-only dashboard for native PufferLib JSONL and AlienWars playback."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]

def read_records(path):
    records = []
    errors = 0
    for line in path.read_text().splitlines(keepends=True):
        try:
            value = json.loads(line)
            if isinstance(value, dict):
                records.append(value)
        except ValueError:
            # The writer may be partway through its last line.
            errors += line.endswith('\n')
    return records, errors

class Dashboard(SimpleHTTPRequestHandler):
    def __init__(self, *args, logs, evaluations, viewer, **kwargs):
        self.logs, self.evaluations, self.viewer = logs, evaluations, viewer
        super().__init__(*args, directory=str(ROOT/'web/navigation'), **kwargs)

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/runs':
            files = sorted(self.logs.glob('*.jsonl'), key=lambda p: p.stat().st_mtime, reverse=True)
            return self.send_json([{'name':p.name,'modified':p.stat().st_mtime} for p in files])
        if parsed.path in ('/api/run', '/api/evaluations'):
            if parsed.path == '/api/run':
                name = parse_qs(parsed.query).get('name',[''])[0]
                if not name or Path(name).name != name or not name.endswith('.jsonl'):
                    return self.send_error(400)
                path = self.logs/name
                if not path.is_file():
                    return self.send_error(404)
                records, errors = read_records(path)
                # Bound rendering without dropping the final point or provenance.
                metrics = [r for r in records if r.get('type') == 'metrics']
                stride = max(1, len(metrics)//2000)
                plotted = metrics[::stride]
                if metrics and (not plotted or plotted[-1] is not metrics[-1]):
                    plotted.append(metrics[-1])
                return self.send_json({'records':[r for r in records if r.get('type')!='metrics']+plotted,
                                       'modified':path.stat().st_mtime,'samples':len(metrics),'errors':errors})
            results=[]
            for path in sorted(self.evaluations.glob('*.jsonl')):
                records, errors=read_records(path)
                summaries=[r for r in records if r.get('type')=='summary']
                if summaries:
                    results.append({'name':path.name,'metadata':next((r for r in records if r.get('type')=='evaluation'),{}),
                                    'summary':summaries[-1],'errors':errors})
            return self.send_json(results)
        if parsed.path.startswith('/viewer/'):
            relative = parsed.path.removeprefix('/viewer/') or 'index.html'
            target=(self.viewer/relative).resolve()
            if not target.is_relative_to(self.viewer.resolve()) or not target.is_file():
                return self.send_error(404)
            data=target.read_bytes()
            self.send_response(200);self.send_header('Content-Type',self.guess_type(str(target)))
            self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data);return
        if parsed.path == '/':
            self.path='/dashboard.html'
        super().do_GET()

    def send_json(self, value):
        data=json.dumps(value,allow_nan=False).encode()
        self.send_response(200);self.send_header('Content-Type','application/json')
        self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(data)))
        self.end_headers();self.wfile.write(data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log-dir',type=Path,default=ROOT/'logs/alienwars')
    parser.add_argument('--evaluations',type=Path,default=ROOT/'outputs/navigation/evaluations')
    parser.add_argument('--viewer',type=Path,default=ROOT/'build/web/alienwars-nav')
    parser.add_argument('--port',type=int,default=8766)
    args=parser.parse_args()
    handler=partial(Dashboard,logs=args.log_dir,evaluations=args.evaluations,viewer=args.viewer)
    server=ThreadingHTTPServer(('127.0.0.1',args.port),handler)
    print(f'AlienWars training dashboard: http://127.0.0.1:{args.port}',flush=True)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
