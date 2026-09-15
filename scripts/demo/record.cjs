/* Record the published Map Lab through Chrome's DevTools screencast API.
 * No simulation, terrain, sensor or rendering code is replaced. */
const puppeteer=require('../../.local/demo-tools/node_modules/puppeteer-core');
const fs=require('node:fs');
const path=require('node:path');
const {spawn}=require('node:child_process');
const {once}=require('node:events');
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const ROOT=path.resolve(__dirname,'../..');process.chdir(ROOT);
const OUT=path.join(ROOT,'outputs/demo');fs.mkdirSync(path.join(OUT,'raw'),{recursive:true});
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
    await page.goto(url);await page.waitForFunction(()=>document.querySelector('#loading').hidden);await wait(3000);
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
    worlds.push({url,hash,build});console.log('WORLD',hash);
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
    await page.keyboard.down('Shift');await page.mouse.move(900,460);await page.mouse.down();
    const start=performance.now(),steps=Math.round(duration*30);
    for(let i=1;i<=steps;i++){await page.mouse.move(900+dx*i/steps,460+dy*i/steps);await wait(Math.max(0,start+i*duration*1000/steps-performance.now()));}
    await page.mouse.up();await page.keyboard.up('Shift');
  }
  const results=[];
  const only=process.argv.find(a=>a.startsWith('--shot='))?.split('=')[1];
  const shot=async(name,seconds,setup,action)=>{if(only&&only!==name)return;await setup();await page.screenshot({path:path.join(OUT,name+'.png')});results.push(await record(page,name,seconds,action));};
  await load(START);
  if(process.argv.includes('--probe')){
    await caption('ALIENWARS / DEVELOPMENT SHOWCASE','A world of possibilities');
    results.push(await record(page,'probe2',3,()=>orbit(3,-24)));await page.screenshot({path:path.join(OUT,'probe2.png')});
  }else{
    await shot('01-opening',4,async()=>{await caption('PROCEDURAL ENVIRONMENTS','ALIENWARS GYM');},()=>orbit(4,-24));
    await shot('02-terrain',6,async()=>{await caption('01 / WORLD GENERATION','Procedural worlds · WFC terrain');await page.$eval('#sensor-unit',e=>e.scrollIntoView({block:'center'}));},async()=>{await mask(0,true);await orbit(4.5,-36,-8);});
    await shot('03-lidar',3,async()=>{await checkbox('sensor-all',false);await mask(1);await focus(7,-.35);await caption('02 / UNIT PERCEPTION','LiDAR','Measured returns across the terrain');},()=>orbit(3,-12));
    await shot('04-rf',3,async()=>{await mask(4);await focus(7,-.15);await caption('02 / UNIT PERCEPTION','RF sensing','Connections between units');},()=>orbit(3,12));
    await shot('05-depth',4,async()=>{await mask(8);await focus(7,-.35);await caption('02 / UNIT PERCEPTION','Depth camera','Live range image + 3D field of view');},()=>orbit(4,-15));
    await shot('06-ground',3,async()=>{await clean(true);await mask(0);await focus(1,-.7);await caption('03 / MULTI-DOMAIN UNITS','Ground · Sea · Air','Ground traversal');},()=>orbit(3,-18,-5));
    await shot('07-naval',4,async()=>{await mask(2);await focus(4,-.7);await caption('03 / MULTI-DOMAIN UNITS','Ground · Sea · Air','Naval patrol + sonar');},()=>orbit(4,20));
    await shot('08-entrance',4,async()=>{await mask(1);await select('sensor-unit',0);await click('inspect-entrance');await checkbox('scout-pause',false);await checkbox('follow',true);await caption('04 / UNDERGROUND TRAVERSAL','Into the terrain');},()=>wait(4000));
    await shot('09-tunnel',7,async()=>{await click('inspect-tunnel');await checkbox('scout-pause',false);await checkbox('follow',true);await select('cutaway',0);await caption('04 / UNDERGROUND TRAVERSAL','Traversal above and below ground');},async()=>{await wait(1200);await select('cutaway',1);await wait(5800);});
    await shot('10-isolation',7,async()=>{await checkbox('follow',false);await mask(0);await clean(false);await page.evaluate(()=>Module._aw_camera_control(0));await page.$eval('#isolate-tunnels',e=>e.scrollIntoView({block:'center'}));await caption('05 / CONNECTED SPACES','Explore the underground network');await wait(650);},async()=>{await wait(1300);await checkbox('isolate-tunnels',true,true);await wait(2800);await checkbox('isolate-tunnels',false,true);await wait(2200);});
    if(!only||only==='11-bridge'){
      await load('https://rozgo.github.io/alienwars-gym/maplab/?seed=2438762858&sym=1&a=6&b=6&biome=2&tunnels=1&sensors=0&cut=1');
      await shot('11-bridge',3,async()=>{await clean(true);await click('inspect-bridge');await checkbox('follow',true);await caption('06 / WORLD VARIETY · SEED 2438762858','Different seeds. Different challenges.');},()=>orbit(3,-12));
    }
    if(!only||only==='12-variety'){
      await load('https://rozgo.github.io/alienwars-gym/maplab/?seed=2026&sym=0&a=2&b=9&biome=1&tunnels=1&sensors=0&cut=1');
      await shot('12-variety',3,async()=>{await clean(true);await caption('06 / WORLD VARIETY · SEED 2026','Different seeds. Different challenges.','Asymmetric terrain · Independent base heights');},()=>orbit(3,-24));
    }
    if(!only||only==='13-closing'){
      await load(START);
      await shot('13-closing',6,async()=>{await clean(true);await mask(5);await page.evaluate(()=>document.body.classList.add('film-end'));await caption('ALIEN WARS / TRAINING ENVIRONMENTS','ALIENWARS GYM','A procedural world for training Alien Wars agents.<span class="credit">Built on PufferLib 5<br>Rendered with Raylib · WebAssembly Runtime</span><span class="url">rozgo.github.io/alienwars-gym</span>');},()=>orbit(6,-28));
    }
  }
  fs.writeFileSync(path.join(OUT,only?only+'-capture.json':'capture.json'),JSON.stringify({sourceURL:START,browser:await browser.version(),worlds,errors,shots:results},null,2)+'\n');
  if(errors.length)throw Error('Browser errors: '+errors.join('; '));
  } finally { await browser.close(); }
}
main().catch(e=>{console.error(e);process.exit(1);});
