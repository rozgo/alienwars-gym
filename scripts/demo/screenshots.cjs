/* Capture the shipped viewer for README and the public showcase page. */
const puppeteer=require('../../.local/demo-tools/node_modules/puppeteer-core');
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const ROOT=path.resolve(__dirname,'../..');process.chdir(ROOT);
const BASE=process.env.DEMO_BASE||'http://127.0.0.1:8781/maplab/';
const OUT=path.resolve('docs/demo/images');
const only=process.argv.find(a=>a.startsWith('--scene='))?.split('=')[1];
const scenes=[
 {name:'temperate',query:'seed=73&sym=1&a=6&b=6&biome=1&sensors=0',caption:'Temperate terrain with mirrored bases, lakes and ocean.'},
 {name:'desert',query:'seed=175847449&sym=0&a=2&b=9&biome=2&sensors=0',caption:'Asymmetric desert with independently placed bases.'},
 {name:'frozen',query:'seed=2279248715&sym=1&a=6&b=6&biome=3&sensors=0',caption:'Frozen terrain with connected roads, cliffs and water.'},
 {name:'tunnels',query:'seed=2279248715&sym=1&a=6&b=6&biome=3&sensors=0&isolate=1',caption:'Underground network isolated from the surface.',frameTunnels:true},
 {name:'sensors',query:'seed=73&sym=1&a=6&b=6&biome=1&sensors=15&sensorsAll=1&unit=7',caption:'Live LiDAR, sonar, RF and depth-camera overlays across the fleet.'},
 {name:'explorer',query:'seed=73&sym=1&a=6&b=6&biome=1&sensors=15&unit=7&explorer=1',caption:'Official Flecs Explorer inspecting live unit components.',explorer:true},
 {name:'submarine',query:'seed=73&sym=1&a=6&b=6&biome=1&sensors=2&unit=11',caption:'Heavy submarine with sonar and visibility through water.',focus:true}
];
(async()=>{
 fs.mkdirSync(OUT,{recursive:true});
 if(only&&!scenes.some(s=>s.name===only))throw Error('Unknown scene: '+only);
 const browser=await puppeteer.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true,defaultViewport:{width:1600,height:1000,deviceScaleFactor:1}});
 try {
  const page=await browser.newPage();page.setDefaultTimeout(120000);
  const errors=[],images=only?JSON.parse(fs.readFileSync(path.join(OUT,'manifest.json'),'utf8')).images.filter(i=>i.file!==only+'.webp'):[];page.on('pageerror',e=>errors.push(e.message));
  for(const scene of scenes){
   if(only&&scene.name!==only)continue;
   await page.goto(BASE+'?'+scene.query,{waitUntil:'domcontentloaded'});
   await page.waitForFunction(()=>window.lastFleetReport?.trained && document.querySelector('#loading').hidden);
   // Use production camera controls; do not change rendering or sensor equipment.
   if(scene.frameTunnels)await page.evaluate(()=>Module._aw_camera_control(0));
   if(scene.focus){
    await page.click('#sensor-focus');
    await page.click('#follow');
    await page.evaluate(()=>{for(let i=0;i<5;i++)Module._aw_camera_control(1);});
   }
   if(scene.explorer){
    const handle=await page.$('#explorer-frame');const frame=await handle.contentFrame();
    await frame.waitForSelector('.entity-inspector-component');
   }
   await page.$eval('aside',e=>e.scrollTop=0);
   await new Promise(r=>setTimeout(r,1800));
   const file=scene.name+'.webp';
   await page.screenshot({path:path.join(OUT,file),type:'webp',quality:88});
   const blob=fs.readFileSync(path.join(OUT,file));
   images.push({file,caption:scene.caption,url:'https://rozgo.github.io/alienwars-gym/maplab/?'+scene.query,hash:await page.$eval('#map-hash',e=>e.textContent),bytes:blob.length,sha256:crypto.createHash('sha256').update(blob).digest('hex')});
   console.log('Captured',file,blob.length);
  }
  if(errors.length)throw Error(errors.join('\n'));
  const build=JSON.parse(fs.readFileSync('docs/maplab/build.json','utf8'));
  fs.writeFileSync(path.join(OUT,'manifest.json'),JSON.stringify({generator_version:build.generator_version,viewer_source:build.source_commit,images},null,2)+'\n');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
