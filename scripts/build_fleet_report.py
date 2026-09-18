#!/usr/bin/env -S uv run
"""Publish recorded fleet measurements without changing deployed checkpoints."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def build_report(source: Path, name: str = 'fleet'):
    assert name and all(c.isalnum() or c in '-_' for c in name)
    data = source.read_bytes()
    report = json.loads(data)
    assert report['contract'] in (2, 3)
    page = (ROOT / 'web/training/fleet.html').read_text()
    assert '__FLEET_RESULT_VERSION__' in page
    page = page.replace('__FLEET_RESULT_VERSION__', hashlib.sha256(data).hexdigest()[:16])
    page = page.replace('fleet.json', f'{name}.json')
    output = ROOT / 'docs/training'
    output.mkdir(parents=True, exist_ok=True)
    (output / f'{name}.html').write_text(page)
    (output / f'{name}.json').write_bytes(data)
    manifest_path = output / 'manifest.json'
    if manifest_path.exists():
        manifest = json.loads(manifest_path.read_text())
        for filename in (f'{name}.html', f'{name}.json'):
            manifest['files'][filename] = hashlib.sha256((output / filename).read_bytes()).hexdigest()
        manifest[f'{name}_source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        manifest_path.write_text(json.dumps(manifest, separators=(',', ':')) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('--name', default='fleet')
    args = parser.parse_args()
    build_report(args.source, args.name)
