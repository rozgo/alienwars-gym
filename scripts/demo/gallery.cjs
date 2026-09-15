/* Actual browser renders for two six-world gallery boards. */
const p=require('../../.local/demo-tools/node_modules/puppeteer-core');
const fs=require('node:fs'),path=require('node:path');
const ROOT=path.resolve(__dirname,'../..');process.chdir(ROOT);
const OUT=path.resolve(process.env.DEMO_OUT||'outputs/demo-v3');
const BASE=process.env.DEMO_BASE||'http://127.0.0.1:8769/';
const worlds=[
  [3416066543,0,2,9,1,'a506196e'],[1013133171,1,3,3,1,'c0a7e7bd'],[1952225827,1,9,9,1,'4dff4d6c'],
  [175847449,0,2,9,2,'416f652a'],[3406710268,1,3,3,2,'bd1c2bd8'],[1214459856,1,6,6,2,'54e13a07'],
  [2279248715,1,6,6,3,'0169f7aa'],[331413409,0,2,9,3,'e9238a32'],[2320390597,1,3,3,3,'c9e5993a'],
  [326278506,0,8,3,0,'9ba303b4'],[1659481049,1,6,6,0,'65805b80'],[1120395591,1,9,9,0,'97d17dc9']
];
const wait=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
fs.mkdirSync(path.join(OUT,'gallery'),{recursive:true});
const browser=await p.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true,defaultViewport:{width:1280,height:720,deviceScaleFactor:1}});
try{
 const page=await browser.newPage();page.setDefaultTimeout(120000);
 const errors=[],results=[];page.on('pageerror',e=>errors.push(e.message));
 for(const [seed,sym,a,b,biome,expected] of worlds){
  const query=`?seed=${seed}&sym=${sym}&a=${a}&b=${b}&biome=${biome}&tunnels=1&cut=1&sensors=0`;
  await page.goto(BASE+query);await page.waitForFunction(()=>document.querySelector('#loading').hidden);
  await page.addStyleTag({content:'aside,header,.view-controls,.map-heading,.viewport-footer{display:none!important}.workspace{height:100vh;grid-template-columns:1fr!important}'});
  await page.evaluate(()=>Module._aw_demo_frame(.55,1.05,164,64,7,64));await wait(1200);
  const hash=await page.$eval('#map-hash',e=>e.textContent);if(hash!==expected)throw Error(`World hash mismatch: ${seed} ${hash}`);
  const image=path.join('gallery',`${seed}.jpg`);await page.screenshot({path:path.join(OUT,image),quality:96});
  const build=await page.evaluate(async()=>await(await fetch('build.json')).json());
  results.push({seed,sym,a,b,biome,hash,image,build,url:'https://rozgo.github.io/alienwars-gym/maplab/'+query});
  console.log('GALLERY',seed,hash);
 }
 if(errors.length)throw Error(errors.join('; '));
 fs.writeFileSync(path.join(OUT,'gallery.json'),JSON.stringify(results,null,2)+'\n');
}finally{await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
