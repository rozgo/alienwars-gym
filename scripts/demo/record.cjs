/* Record the local filming build through Chrome's DevTools screencast API.
 * Generation, simulation and sensor measurements are the production code.
 * Capture-only camera tracks and overlay weights are recorded in build.json. */
const puppeteer=require('../../.local/demo-tools/node_modules/puppeteer-core');
const fs=require('node:fs');
const path=require('node:path');
const {spawn}=require('node:child_process');
const {once}=require('node:events');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const ROOT=path.resolve(__dirname,'../..');process.chdir(ROOT);
const OUT=path.resolve(ROOT,process.env.DEMO_OUT||'outputs/demo-v3');fs.mkdirSync(path.join(OUT,'raw'),{recursive:true});
const BASE=process.env.DEMO_BASE||'http://127.0.0.1:8769/';
const START='https://rozgo.github.io/alienwars-gym/maplab/?seed=73&sym=1&a=6&b=6&biome=2&tunnels=1&cut=1&sensors=15&unit=7&sensorsAll=1';

async function record(page,name,seconds,action){
  const session=await page.createCDPSession();
  const target=path.join(OUT,'raw',name+'.mp4');
  const ff=spawn('ffmpeg',['-hide_banner','-loglevel','error','-y','-f','image2pipe','-framerate','30','-vcodec','mjpeg','-i','pipe:0','-an','-vf','scale=1920:1080:flags=lanczos','-c:v','libx264','-preset','fast','-crf','17','-threads','4','-pix_fmt','yuv420p','-movflags','+faststart',target]);
  let errors='';ff.stderr.on('data',b=>errors+=b);ff.stdin.on('error',()=>{});
  let previous=null,first=null,frames=0,received=0,queue=Promise.resolve();
  const write=(buffer,count)=>{queue=queue.then(async()=>{for(let i=0;i<count;i++)if(!ff.stdin.write(buffer))await once(ff.stdin,'drain');});};
  let ready;const firstFrame=new Promise(r=>ready=r);
  session.on('Page.screencastFrame',event=>{
    void session.send('Page.screencastFrameAck',{sessionId:event.sessionId}).catch(()=>{});
    const time=event.metadata.timestamp,buffer=Buffer.from(event.data,'base64');received++;
    if(first===null){
      fs.writeFileSync(path.join(OUT,name+'-capture-first.jpg'),buffer);
      console.log('FRAME',name,JSON.stringify(event.metadata));
      first=time;previous=buffer;ready();return;
    }
    const frame=Math.min(Math.round((time-first)*30),Math.round(seconds*30));
    if(frame>frames){write(previous,frame-frames);frames=frame;}
    previous=buffer;
  });
  await session.send('Page.startScreencast',{format:'jpeg',quality:95,maxWidth:1920,maxHeight:1080,everyNthFrame:1});
  await firstFrame;
  const started=performance.now();
  if(action)await action();
  await wait(Math.max(0,seconds*1000-(performance.now()-started)));
  await session.send('Page.stopScreencast');
  write(previous,Math.max(0,Math.round(seconds*30)-frames));
  await queue;ff.stdin.end();const [code]=await once(ff,'exit');
  await session.detach();if(code)throw Error(errors);
  const meta={name,seconds,frames:Math.round(seconds*30),received,bytes:fs.statSync(target).size,fps:await page.$eval('#fps',e=>e.textContent)};
  console.log('RECORDED',JSON.stringify(meta));return meta;
}

async function main(){
  const browser=await puppeteer.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true,defaultViewport:{width:1920,height:1080,deviceScaleFactor:1},userDataDir:'.local/demo-chrome',args:['--remote-debugging-port=9223','--window-size=1920,1080','--disable-background-timer-throttling','--disable-renderer-backgrounding','--disable-backgrounding-occluded-windows']});
  try {
  const page=await browser.newPage();page.setDefaultTimeout(120000);
  await page.emulateMediaFeatures([{name:'prefers-reduced-motion',value:'no-preference'}]);
  const errors=[],worlds=[];page.on('pageerror',e=>errors.push(e.message));
  console.log('BROWSER',await browser.version());
  async function load(url){
    const captureURL=BASE+new URL(url).search;
    await page.goto(captureURL);await page.waitForFunction(()=>document.querySelector('#loading').hidden);await wait(3000);
    await page.waitForFunction(()=>typeof Module._aw_demo_orbit==='function');
    await page.addStyleTag({content:`
      .map-heading,.viewport-footer{display:none!important}
      canvas:focus-visible{outline:none!important}
      #film-caption{position:fixed;left:52px;bottom:48px;z-index:1000;pointer-events:none;color:#eeefea;font-family:Menlo,monospace;text-shadow:0 2px 16px #000;max-width:1460px}
      #film-caption small{display:block;color:#9bc9bd;font-size:16px;letter-spacing:3px;margin-bottom:12px}
      #film-caption strong{display:block;font-size:36px;line-height:1.25;letter-spacing:-1.5px}
      #film-caption p{font-size:18px;line-height:1.6;margin-top:12px;color:#b9c1be}
      #film-shade{position:fixed;inset:65% 288px 0 0;background:linear-gradient(transparent,#101719c9);z-index:999;pointer-events:none}
      body.film-clean aside,body.film-clean header,body.film-clean .view-controls{display:none!important}
      body.film-clean .workspace{height:100vh;grid-template-columns:1fr!important}
      body.film-clean #film-shade{right:0}
      body.film-end #film-shade{inset:0;background:linear-gradient(90deg,#101719ed,#101719b0 62%,#10171930)}
      body.film-end #film-caption{bottom:112px;left:104px;max-width:1500px}
      body.film-end #film-caption strong{font-size:78px;letter-spacing:-4px}
      body.film-end #film-caption p{font-size:26px;line-height:1.75}
      #film-caption .credit{display:block;margin-top:28px;color:#d6e5df;font-size:22px;line-height:1.75}
      #film-caption .url{color:#91c7b8;font-size:23px;margin-top:30px;display:block}
      .film-click{outline:2px solid #b3e4d4!important;box-shadow:0 0 16px #91d7bc66!important}
    `});
    await page.evaluate(()=>{for(const id of ['film-shade','film-caption']){const e=document.createElement('div');e.id=id;document.body.append(e);}});
    const hash=await page.$eval('#map-hash',e=>e.textContent);
    const build=await page.evaluate(async()=>await (await fetch('build.json',{cache:'no-store'})).json());
    worlds.push({url,captureURL,hash,build});console.log('WORLD',hash);
  }
  async function caption(kicker,title,body=''){await page.evaluate(({kicker,title,body})=>{document.querySelector('#film-caption').innerHTML=`<small>${kicker}</small><strong>${title}</strong>${body?`<p>${body}</p>`:''}`;},{kicker,title,body});}
  async function clean(value){await page.evaluate(v=>document.body.classList.toggle('film-clean',v),value);await wait(650);}
  async function checkbox(id,value,visible=false){
    if(visible){await page.$eval('#'+id,e=>e.scrollIntoView({block:'center'}));await page.$eval('#'+id,e=>e.classList.add('film-click'));}
    await page.$eval('#'+id,(e,v)=>{if(e.checked!==v)e.click();},value);
    if(visible){await wait(250);await page.$eval('#'+id,e=>e.classList.remove('film-click'));}
  }
  async function select(id,value){await page.select('#'+id,String(value));}
  async function click(id){await page.$eval('#'+id,e=>e.click());}
  async function mask(value,visible=false){for(let i=0;i<4;i++)await checkbox('sensor-show-'+i,!!(value&(1<<i)),visible);}
  async function focus(unit,zoomDelta=0){await select('sensor-unit',unit);await click('sensor-focus');if(zoomDelta)await page.evaluate(d=>Module._aw_camera_zoom(d),zoomDelta);await wait(650);}
  async function orbit(duration,dx=-50,dy=0){
    await page.evaluate(({duration,dx,dy})=>Module._aw_demo_orbit(duration,dx,dy),{duration,dx,dy});
    await wait(duration*1000);
  }
  async function frame(scale=145,yaw=.65,pitch=1.02){
    await checkbox('follow',false);
    await page.evaluate(({scale,yaw,pitch})=>Module._aw_demo_frame(yaw,pitch,scale,64,7,64),{scale,yaw,pitch});
  }
  const results=[];
  const only=process.argv.find(a=>a.startsWith('--shot='))?.split('=')[1];
  const world=(seed,sym,a,b,biome)=>`https://rozgo.github.io/alienwars-gym/maplab/?seed=${seed}&sym=${sym}&a=${a}&b=${b}&biome=${biome}&tunnels=1&cut=1&sensors=0`;
  const worldsByName={
    temperate:world(1952225827,1,9,9,1),
    desert:world(175847449,0,2,9,2),
    frozen:world(2279248715,1,6,6,3),
    forest:world(3416066543,0,2,9,1),
    asymmetric:world(326278506,0,8,3,0),
  };
  // Adjacent takes can share a world; every setup also works as an independent retake.
  let currentURL=null;
  async function shot(name,seconds,url,setup,action){
    if(only&&only!==name)return;
    if(currentURL!==url){await load(url);currentURL=url;}
    await setup();await wait(500);
    await page.screenshot({path:path.join(OUT,name+'.png')});
    const controls=await page.evaluate(()=>({allUnits:document.querySelector('#sensor-all').checked,
      sensorMask:[0,1,2,3].reduce((mask,i)=>mask|(document.querySelector('#sensor-show-'+i).checked?1<<i:0),0)}));
    results.push({...await record(page,name,seconds,action),url,controls});
  }
  function gallery(name,index){
    if(only&&only!==name)return;
    const examples=JSON.parse(fs.readFileSync(path.join(OUT,'gallery.json'))).slice(index*6,index*6+6);
    worlds.push(...examples);
    results.push({name,seconds:4,frames:120,kind:'gallery',worlds:examples.map(w=>w.hash)});
  }
  async function landscape(kicker,title,body,dx=0,dy=0){
    await clean(true);await checkbox('sensor-all',false);await mask(0);
    await page.evaluate(()=>Module._aw_camera_control(0));
    if(dx||dy)await orbit(.4,dx,dy);
    await caption(kicker,title,body);
  }
  async function isolation(title,body,frame=false){
    await clean(true);await checkbox('sensor-all',false);await mask(0);
    await checkbox('follow',false);
    if(frame){
      await checkbox('isolate-tunnels',true);
      await page.evaluate(()=>{Module._aw_camera_control(0);Module._aw_camera_zoom(-.28);});
    }else await page.evaluate(()=>Module._aw_camera_control(0));
    await orbit(.4,100,55);
    await caption('PROCEDURAL TUNNEL NETWORKS',title,body);
  }
  if(process.argv.includes('--probe')){
    await load(START);await caption('ALIENWARS / DEVELOPMENT SHOWCASE','A world of possibilities');
    results.push(await record(page,'probe',3,()=>orbit(3,-24)));
  }else{
    await shot('01-opening',4,START,async()=>{
      await caption('PROCEDURAL ENVIRONMENTS','ALIENWARS GYM');
    },()=>orbit(4,-32));
    gallery('02-world-generation',0);
    gallery('03-world-variety',1);
    await shot('04-lidar',4,worldsByName.forest,async()=>{
      await clean(false);await checkbox('sensor-all',true);await mask(1);await select('sensor-unit',7);await frame(132);
      await page.$eval('#sensor-unit',e=>e.scrollIntoView({block:'center'}));
      await caption('UNIT PERCEPTION / ALL UNITS','Laser range sensing','Live measurements across the terrain');
    },()=>orbit(4,-16));
    await shot('05-radio',4,worldsByName.forest,async()=>{
      await clean(false);await checkbox('sensor-all',true);await mask(4);await select('sensor-unit',7);
      if(only)await frame(132,.73);
      await page.$eval('#sensor-unit',e=>e.scrollIntoView({block:'center'}));
      await caption('UNIT PERCEPTION / ALL UNITS','Radio sensing','Range + bearing between nearby units');
    },()=>orbit(4,-16));
    await shot('06-depth',4,worldsByName.forest,async()=>{
      await clean(false);await checkbox('sensor-all',true);await mask(8);await focus(7,.10);
      await page.$eval('#sensor-unit',e=>e.scrollIntoView({block:'center'}));
      await caption('AIR / UNIT PERCEPTION','Depth camera','Live range image + 3D field of view');
    },()=>orbit(4,-16));
    await shot('07-ground',4,worldsByName.temperate,async()=>{
      await clean(true);await mask(0);await click('inspect-bridge');await checkbox('follow',true);
      await caption('GROUND / BRIDGE CROSSING','Ground · Sea · Air','Navigate crossings and changing elevations');
    },()=>orbit(4,-25));
    await shot('08-naval',4,worldsByName.desert,async()=>{
      await clean(true);await checkbox('sensor-all',true);await mask(2);await focus(4,-.10);
      await caption('SEA / SONAR','Ground · Sea · Air','Naval patrol · Live range returns');
    },()=>orbit(4,24));
    await shot('09-entrance',4,worldsByName.frozen,async()=>{
      await clean(true);await checkbox('sensor-all',true);await select('sensor-unit',0);await mask(1);
      await click('inspect-entrance');await checkbox('scout-pause',false);await checkbox('follow',true);
      await caption('UNDERGROUND TRAVERSAL','Into the terrain','Ramp entrances · Continuous terrain');
    },()=>wait(4000));
    await shot('10-tunnel',6,worldsByName.frozen,async()=>{
      await clean(true);await checkbox('sensor-all',true);await select('sensor-unit',0);await mask(1);
      await click('inspect-tunnel');await checkbox('scout-pause',false);await checkbox('follow',true);await select('cutaway',0);
      await caption('UNDERGROUND TRAVERSAL','See through the terrain','Automatic cutaway · Live sensor returns');
    },async()=>{await wait(900);await select('cutaway',1);await wait(5100);});
    await shot('11-network-symmetric',5,worldsByName.frozen,async()=>{
      await isolation('Explore the underground network','Symmetric · Four chambers + mountain passages');
    },async()=>{await wait(800);await checkbox('isolate-tunnels',true);await wait(3200);await checkbox('isolate-tunnels',false);await wait(1000);});
    await shot('12-network-asymmetric',4,worldsByName.asymmetric,async()=>{
      await isolation('Different routes. Different depths.','Asymmetric · Four chambers + mountain passage',true);
    },async()=>{await orbit(3,36);await checkbox('isolate-tunnels',false);await wait(1000);});
    await shot('13-closing',6,worldsByName.forest,async()=>{
      await clean(true);await mask(5);await checkbox('sensor-all',true);
      await page.evaluate(()=>document.body.classList.add('film-end'));
      await caption('ALIENWARS / TRAINING ENVIRONMENTS','ALIENWARS GYM','A procedural world for training AlienWars agents.<span class="credit">Built on PufferLib 5<br>Rendered with Raylib · WebAssembly Runtime</span><span class="url">rozgo.github.io/alienwars-gym</span>');
    },()=>orbit(6,-40));

  }
  const captureName=only?only+'-capture.json':process.argv.includes('--probe')?'probe-capture.json':'capture.json';
  fs.writeFileSync(path.join(OUT,captureName),JSON.stringify({revision:3,sourceURL:START,browser:await browser.version(),worlds,errors,shots:results},null,2)+'\n');
  if(errors.length)throw Error('Browser errors: '+errors.join('; '));
  } finally { await browser.close(); }
}
main().catch(e=>{console.error(e);process.exit(1);});
