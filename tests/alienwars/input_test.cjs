const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const code=fs.readFileSync('web/maplab/shell.html','utf8').match(/<script id="map-input">([\s\S]*?)<\/script>/)[1];
// Model the backend's canvas-only mouse listeners. Real capture retargeting
// and the camera behavior are checked separately in Chrome.
class Target {
  listeners={};
  addEventListener(type,fn){(this.listeners[type]??=[]).push(fn);}
  dispatchEvent(event){event.target??=this;for(const fn of this.listeners[event.type]??[])fn(event);return true;}
}
class MouseEvent {
  constructor(type,init={}){this.type=type;Object.assign(this,init);}
  preventDefault(){this.prevented=true;}
  stopImmediatePropagation(){this.stopped=true;}
}
const window=new Target(),document=new Target(),canvas=new Target(),ui=new Target();
let captured=null,focuses=0,enabled=true,zooms=[];const backend=new Set();
Object.assign(canvas,{clientHeight:900,focus(){focuses++;},setPointerCapture(id){captured=id;},hasPointerCapture(id){return captured===id;},releasePointerCapture(){captured=null;}});
vm.runInNewContext(code+'\ninstallMapInput(canvas,delta=>zooms.push(delta),()=>enabled());',{window,document,canvas,MouseEvent,zooms,enabled:()=>enabled});
canvas.addEventListener('mousedown',e=>backend.add(e.button));
canvas.addEventListener('mouseup',e=>backend.delete(e.button));
const emit=(target,type,init={})=>{const e=new MouseEvent(type,init);target.dispatchEvent(e);return e;};
function down(button=0){emit(canvas,'pointerdown',{isPrimary:true,pointerType:'mouse',pointerId:1});emit(canvas,'mousedown',{button,clientX:200,clientY:200});assert.equal(captured,1);}
down();emit(canvas,'mouseup',{button:0});emit(canvas,'lostpointercapture',{pointerId:1});assert.equal(backend.size,0);
down();emit(window,'mouseup',{target:ui,button:0,buttons:0});assert.equal(backend.size,0);emit(canvas,'lostpointercapture',{pointerId:1});
for(const type of ['pointercancel','lostpointercapture']){down();emit(canvas,type,{pointerId:1});assert.equal(backend.size,0);assert.equal(captured,null);}
down();emit(window,'blur');assert.equal(backend.size,0);assert.equal(captured,null);
down();document.hidden=true;emit(document,'visibilitychange');assert.equal(backend.size,0);document.hidden=false;
down();emit(canvas,'mousemove',{buttons:0,clientX:100,clientY:100});assert.equal(backend.size,0);
down();emit(canvas,'mousedown',{button:2});emit(window,'mouseup',{target:ui,button:0,buttons:2});assert.deepEqual([...backend],[2]);emit(window,'blur');assert.equal(backend.size,0);
emit(window,'mouseup',{target:ui,button:0});assert.equal(backend.size,0);
for(let i=0;i<100;i++)emit(canvas,'wheel',{deltaY:1,deltaMode:0});
const small=zooms.reduce((a,b)=>a+b,0);zooms.length=0;
let event=emit(canvas,'wheel',{deltaY:100,deltaMode:0});assert(Math.abs(zooms[0]-small)<1e-12);assert(event.prevented&&event.stopped);
emit(canvas,'wheel',{deltaY:1,deltaMode:1});assert.equal(zooms.at(-1),.024);
emit(canvas,'wheel',{deltaY:1,deltaMode:2});assert.equal(zooms.at(-1),.35);
emit(canvas,'wheel',{deltaY:-2,deltaMode:0,ctrlKey:true});assert.equal(zooms.at(-1),-.01);
const count=zooms.length;emit(canvas,'wheel',{deltaY:NaN});enabled=false;emit(canvas,'wheel',{deltaY:20});assert.equal(zooms.length,count);
assert(focuses>0);
console.log('INPUT_TEST release_outside=PASS capture/cancel=PASS blur/hidden=PASS chorded_buttons=PASS stale_recovery=PASS trackpad/line/page/pinch=PASS no_double_wheel=PASS');
