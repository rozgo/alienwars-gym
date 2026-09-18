#!/usr/bin/env python3
"""Package the pinned official Flecs Explorer with a read-only live-world bridge."""
from pathlib import Path
import hashlib,json,re,shutil,tarfile
ROOT=Path(__file__).resolve().parents[1]
src=ROOT/'vendor/flecs-explorer';out=ROOT/'docs/explorer'
version=json.loads((src/'version.json').read_text())
assert hashlib.sha256((src/'frontend.tar.gz').read_bytes()).hexdigest()==version['archive_sha256']
out.mkdir(parents=True,exist_ok=True)
with tarfile.open(src/'frontend.tar.gz') as tar:tar.extractall(out,filter='data')
for name in ['alienwars_live.js','bootstrap.js']:shutil.copyfile(ROOT/'web/explorer'/name,out/name)
shutil.copyfile(src/'LICENSE',out/'LICENSE')
shutil.copyfile(src/'README.md',out/'NOTICES.md')
shutil.copytree(src/'licenses',out/'licenses',dirs_exist_ok=True)
def patch(name,old,new):
    path=out/name;text=path.read_text();assert old in text,name;path.write_text(text.replace(old,new))
patch('js/components/widgets/menu-bar/menu-button.vue','<div :class="buttonCss" v-on:click="onClick">','<div :class="buttonCss" role="button" tabindex="0" :aria-label="name" v-on:click="onClick" @keydown.enter.prevent="onClick" @keydown.space.prevent="onClick">')
patch('index.html','<head>','<head>\n    <script src="bootstrap.js"></script>')
patch('index.html',"['entities', 'queries', 'stats', 'commands', 'rest', 'internals', 'info', 'docs']", "['entities', 'queries', 'internals', 'info', 'docs']")
patch('js/components/widgets/inspector/entity-inspector-component.vue','return props.base !== undefined;','return true; // AlienWars live inspection is read-only.')
# Remove mutation affordances; the C bridge independently rejects every write.
p=out/'js/components/widgets/inspector/entity-inspector-container.vue'
s=p.read_text();s,n=re.subn(r'<icon-button\s+(?::src="disabledIcon"|src="trash")\s+@click.stop="emit\(\'(?:disable|delete)\'\)">\s*</icon-button>','',s);assert n==2;p.write_text(s)
with (out/'css/style.css').open('a') as f:f.write('\n/* Live viewer: inspection only. */\n.component-delete-icon,.entity-tree-item-actions,.entity-tree-new,.pane-inspector-actions {display:none!important}\n')
# Upstream source remains byte-identical in the archive; normalize published text.
for p in out.rglob('*'):
    if p.suffix in {'.js','.vue','.css','.html','.svg'}:
        p.write_text('\n'.join(line.rstrip() for line in p.read_text().splitlines()).rstrip()+'\n')
manifest={**version,'source_commit':__import__('subprocess').check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'files':{}}
for p in sorted(out.rglob('*')):
    if p.is_file() and p.name!='build.json':manifest['files'][str(p.relative_to(out))]=hashlib.sha256(p.read_bytes()).hexdigest()
(out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Built official Flecs Explorer:',len(manifest['files']),'files')
