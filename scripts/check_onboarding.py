#!/usr/bin/env -S uv run
"""Check onboarding links, skill metadata and doctor failure reporting; no SDK/GPU."""
from html.parser import HTMLParser
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from unittest.mock import patch
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


class Page(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.links = []
        self.ids = set()
        self.copy_targets = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, f"Duplicate element id: {attrs['id']}"
            self.ids.add(attrs['id'])
        self.links.extend(attrs[k] for k in ('href', 'src') if k in attrs)
        if 'data-copy' in attrs:
            self.copy_targets.append(attrs['data-copy'])


def links():
    markdown = [ROOT / name for name in (
        'AGENTS.md', 'CONTRIBUTING.md', 'README.md', 'web/AGENTS.md',
        'ocean/alienwars/AGENTS.md', 'docs/START_HERE.md',
        'docs/ENGINEERING_INVARIANTS.md', 'docs/lessons/sensor-equipment.md',
        '.agents/skills/alienwars-start/SKILL.md')]
    html = [ROOT / 'docs/learn/index.html', ROOT / 'docs/demo/index.html']
    count = 0
    for path in markdown + html:
        text = path.read_text()
        if path.suffix == '.html':
            page = Page(text)
            targets = page.links
            assert set(page.copy_targets) <= page.ids, f'Broken copy target in {path}'
        else:
            # Skip fenced examples; normal Markdown links are all relative or HTTPS.
            text = re.sub(r'```.*?```', '', text, flags=re.S)
            targets = re.findall(r'\]\(([^\s)]+)\)', text)
        for target in targets:
            url = urlsplit(target)
            if url.scheme or url.netloc:
                continue
            dest = (path.parent / unquote(url.path)).resolve() if url.path else path
            assert dest.is_relative_to(ROOT), f'Link leaves repository: {path}: {target}'
            if dest.is_dir():
                dest = dest / 'index.html'
            assert dest.is_file(), f'Broken link: {path.relative_to(ROOT)}: {target}'
            if url.fragment and dest.suffix == '.html':
                assert unquote(url.fragment) in Page(dest.read_text()).ids, f'Broken anchor: {target}'
            if url.fragment and dest.suffix == '.md':
                headings = re.findall(r'^#+\s+(.+)$', dest.read_text(), flags=re.M)
                slugs = {re.sub(r'[^\w\- ]', '', h.lower()).replace(' ', '-') for h in headings}
                assert unquote(url.fragment) in slugs, f'Broken Markdown anchor: {target}'
            count += 1
    print(f'PASS: {count} repository links, anchors, unique HTML IDs and copy targets')


def skill():
    path = ROOT / '.agents/skills/alienwars-start/SKILL.md'
    parts = path.read_text().split('---', 2)
    assert len(parts) == 3 and not parts[0].strip()
    fields = dict(line.split(': ', 1) for line in parts[1].strip().splitlines())
    assert fields['name'] == path.parent.name
    assert 0 < len(fields['name']) <= 64 and re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', fields['name'])
    assert 0 < len(fields['description']) <= 1024
    print('PASS: onboarding skill metadata')


def doctor():
    spec = importlib.util.spec_from_file_location('alienwars_doctor', ROOT / 'scripts/doctor.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    with tempfile.TemporaryDirectory(prefix='alienwars-onboarding-') as directory:
        root = Path(directory)
        (root / '.python-version').write_text(module.platform.python_version())
        (root / 'docs/maplab').mkdir(parents=True)
        (root / 'docs/learn').mkdir()
        (root / 'docs/learn/index.html').write_text('preview fixture')
        artifacts = {}
        for name in ('index.html', 'maplab.js', 'maplab.wasm', 'maplab.data'):
            payload = ('fixture-' + name).encode()
            (root / 'docs/maplab' / name).write_bytes(payload)
            artifacts[name] = {'sha256': module.hashlib.sha256(payload).hexdigest()}
        (root / 'docs/maplab/build.json').write_text(json.dumps({'artifacts': artifacts}))
        with patch.object(module, 'ROOT', root):
            assert module.inspect('preview')['ok']
            (root / 'docs/maplab/maplab.wasm').write_bytes(b'corrupted')
            broken = module.inspect('preview')
            assert not broken['ok']
            assert any('Hash mismatch' in c['detail'] and c['fix'] for c in broken['checks'])
            with patch.object(module.shutil, 'which', return_value=None):
                missing = module.inspect('web')
                assert not missing['ok']
                assert any(c['name'] == 'clang' and not c['ok'] and c['fix'] for c in missing['checks'])
                assert any(c['name'] == 'Emscripten' and not c['ok'] for c in missing['checks'])
    result = subprocess.run([sys.executable, str(ROOT / 'scripts/doctor.py'), '--json'],
                            capture_output=True, text=True, check=True)
    assert json.loads(result.stdout)['ok']
    print('PASS: preview preflight, corrupt artifact detection, missing-tool guidance and JSON CLI')


if __name__ == '__main__':
    links()
    skill()
    doctor()
