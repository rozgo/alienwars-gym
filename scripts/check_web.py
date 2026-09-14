#!/usr/bin/env python3
"""Verify the deployed artifact inputs and run actual WASM policy inference."""
import hashlib
from html.parser import HTMLParser
import json
import math
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[1]
docs = root / 'docs'
manifest = json.loads((docs / 'web-build.json').read_text())
for name, expected in manifest['artifacts'].items():
    blob = (docs / name).read_bytes()
    assert len(blob) == expected['bytes'], name
    assert hashlib.sha256(blob).hexdigest() == expected['sha256'], name
    assert str(root).encode() not in blob, f'Local workspace path in {name}'
assert (docs / '.nojekyll').is_file()
html = (docs / 'index.html').read_text()
assert '{{{ SCRIPT }}}' not in html
class ScriptSources(HTMLParser):
    def __init__(self):
        super().__init__()
        self.sources = []

    def handle_starttag(self, tag, attrs):
        if tag == 'script':
            self.sources.append(dict(attrs).get('src'))

parser = ScriptSources()
parser.feed(html)
assert 'game.js' in parser.sources, 'Missing relative runtime script URL'
subprocess.run(['node', '--check', 'game.js'], cwd=docs, check=True)
result = subprocess.run(
    ['node', 'game.js', '--headless', '--base.eval_episodes=10'],
    cwd=docs, capture_output=True, text=True, timeout=30, check=True)
print(result.stdout, end='')
lines = [line for line in result.stdout.splitlines() if line.startswith('CPU_EVAL ')]
assert len(lines) == 1, 'Missing completed WASM evaluation'
values = dict(re.findall(r'(\w+)=([^\s]+)', lines[0]))
assert values['untrained'] == '0', 'A trained policy must be loaded'
assert values['match'] == '1', 'Checkpoint architecture must match'
assert values['n'] == '10', 'Evaluation must finish ten episodes'
assert int(values['file_floats']) == 16192, 'Unexpected policy weight count'
assert math.isfinite(float(values['score'])) and float(values['score']) > 0
assert math.isfinite(float(values['perf']))
report = {
    'check': 'Native WASM module evaluated headlessly in Node.js; browser rendering checked separately',
    'node': subprocess.check_output(['node', '--version'], text=True).strip(),
    'evaluation': values,
    'artifact_hashes_match': True,
    'stderr': result.stderr,
}
(root / 'build').mkdir(exist_ok=True)
(root / 'build' / 'web-check.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: artifact hashes, JavaScript syntax, and ten episodes of trained WASM inference')
