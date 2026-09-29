// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic measurements, not a declaration of complete filter parity.
import {chromium} from '@playwright/test';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {parseArgs} from 'node:util';
import {filterFixture,inputOscillator} from './filter-fixtures.mjs';
import {filterInventory,filterModes,filterTypes} from './filter-inventory.mjs';
import {renderEngineFixture} from '../tests/helpers/engine-fixture.js';
const {values,positionals}=parseArgs({allowPositionals:true,options:{type:{type:'string'},rate:{type:'string'}}});
if(positionals.length!==1)throw Error('Usage: node web/scripts/filter-survey.mjs OUTPUT.json [--type ID] [--rate 44100|48000]');
const type=values.type===undefined?undefined:Number(values.type),rate=values.rate===undefined?undefined:Number(values.rate);
if(values.type!==undefined&&(!/^\d+$/.test(values.type)||!filterTypes[type]))throw Error('Invalid filter type');
if(values.rate!==undefined&&!['44100','48000'].includes(values.rate))throw Error('Invalid rate');
const modes=filterModes.filter(m=>type===undefined||m.type===type),rates=rate===undefined?[44100,48000]:[rate];
const root=fileURLToPath(new URL('../../',import.meta.url)),output=resolve(positionals[0]);
const native=join(root,'build-reference/src/surge-web/surge-engine-reference'),wasm=join(root,'build-web/web/surge-web.wasm');
const hash=b=>createHash('sha256').update(b).digest('hex');
const binaries={native:hash(readFileSync(native)),wasm:hash(readFileSync(wasm))};
const url=process.env.SURGE_TEST_URL||'http://127.0.0.1:8080';
const response=await fetch(new URL('/surge-web.wasm',url));
if(!response.ok||hash(Buffer.from(await response.arrayBuffer()))!==binaries.wasm)throw Error('Server Wasm does not match local build');
const report={schema:1,status:'diagnostic',complete:false,selectionComplete:false,inventory:filterInventory,
  selection:{modes,rates},binaries,createdAt:new Date().toISOString(),relativeRmsTolerance:1e-5,
  scope:'Serial 1, filter 1 only, cutoff 3, resonance 0.35, deterministic stereo Audio Input. Repeatability, mute and bypass controls. No claim about other routing, modulation, parameter extremes or real-time deadlines.',cases:[]};
const temporary=mkdtempSync(join(tmpdir(),'surge-filters-'));
const browser=await chromium.launch({channel:'chrome',headless:true});report.chrome=browser.version();
const flush=()=>writeFileSync(output,JSON.stringify(report,null,2)+'\n');
function compare(a,b){
  if(a.length!==b.length)throw Error('Sample count mismatch');let energy=0,error=0;
  for(let i=0;i<a.length;i++){if(!Number.isFinite(a[i])||!Number.isFinite(b[i]))throw Error('Non-finite sample');energy+=a[i]*a[i];error+=(a[i]-b[i])**2;}
  return {energy,error,relativeRMS:energy?Math.sqrt(error/energy):null};
}
function reference(bytes,rate){
  const patch=join(temporary,'fixture.fxp'),file=join(temporary,'audio.f32');writeFileSync(patch,bytes);
  execFileSync(native,['--audio-input','--oscillator-type',String(inputOscillator),join(root,'resources/data'),patch,String(rate),file],{timeout:20000});
  const data=readFileSync(file);return Array.from({length:data.length/4},(_,i)=>data.readFloatLE(i*4));
}
async function render(bytes,rate,mode){
  const page=await browser.newPage();let deadline;
  const work=async()=>{
    await page.goto(url);
    // Verify the loader retained the requested filter and routing before measuring.
    await page.evaluate(async({bytes,rate,mode})=>{
      const {default:create}=await import('/surge-web.js'),m=await create();
      m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');m.FS.writeFile('/filter.fxp',new Uint8Array(bytes));
      const e=m.ccall('surge_create','number',['number','string'],[rate,'/factory']);
      if(!e)throw Error('Unable to create verification engine');
      try{
        if(!m.ccall('surge_load_patch','number',['number','string'],[e,'/filter.fxp'])||!m.ccall('surge_save_patch','number',['number','string'],[e,'/verify.fxp']))throw Error('Filter round trip failed');
        const data=m.FS.readFile('/verify.fxp'),size=new DataView(data.buffer,data.byteOffset,data.byteLength).getUint32(64,true);
        const xml=new DOMParser().parseFromString(new TextDecoder().decode(data.slice(92,92+size)).replace(/\0+$/,''),'text/xml');
        if(xml.querySelector('parsererror'))throw Error('Invalid saved filter XML');
        for(const [name,value] of Object.entries({a_filter1_type:mode.type,a_filter1_subtype:mode.subtype,a_filter2_type:0,a_fb_config:0,a_route_o1:0,a_ws_type:0,a_filter1_cutoff:3,a_filter1_resonance:0.35,a_filter1_envmod:0,a_filter1_keytrack:0,a_feedback:0})){
          const raw=xml.querySelector(name)?.getAttribute('value'),actual=Number(raw);
          if(raw===null||raw===undefined||!Number.isFinite(actual)||Math.abs(actual-value)>1e-6)throw Error('Filter loader changed '+name);
        }
        if(xml.querySelector('a_filter1_type')?.getAttribute('deactivated')!=='0')throw Error('Filter is deactivated');
      }finally{m._surge_destroy(e);}
    },{bytes:[...bytes],rate,mode});
    return await page.evaluate(renderEngineFixture,{bytes:[...bytes],rate,oscillatorType:inputOscillator,audioInput:true});
  };
  try{return await Promise.race([work(),new Promise((_,reject)=>{
    deadline=setTimeout(()=>reject(Error('Browser filter render exceeded 30 seconds')),30000);
  })]);}finally{clearTimeout(deadline);await page.close();}
}
try{
  for(const rate of rates){
    const bypass=reference(filterFixture({type:0}),rate);
    for(const mode of modes){
      const item={...mode,name:filterTypes[mode.type].name,rate};
      try{
        const bytes=filterFixture(mode);item.fixtureSha256=hash(bytes);
        const a=reference(bytes,rate),b=await render(bytes,rate,mode);
        item.comparison=compare(a,b);item.nativeRepeat=compare(a,reference(bytes,rate));item.wasmRepeat=compare(b,await render(bytes,rate,mode));
        const muted=filterFixture({...mode,muted:true});
        const na=reference(muted,rate),wa=await render(muted,rate,mode);
        item.mute={nativeEnergy:compare(na,na).energy,wasmEnergy:compare(wa,wa).energy};
        item.bypass=compare(bypass,a);
        item.status=item.mute.nativeEnergy!==0||item.mute.wasmEnergy!==0||item.comparison.energy<=0||(mode.type!==0&&item.bypass.relativeRMS<1e-3)?'needs-fixture':
          item.nativeRepeat.error!==0||item.wasmRepeat.error!==0?'needs-repeatability-investigation':
          item.comparison.relativeRMS<report.relativeRmsTolerance?'matched-fixture':'needs-numerical-investigation';
      }catch(error){item.status='error';item.error=String(error);}
      report.cases.push(item);flush();console.log(`${rate} ${item.name}/${mode.subtype}: ${item.status}; RMS ${item.comparison?.relativeRMS}`);
    }
  }
  if(hash(readFileSync(native))!==binaries.native||hash(readFileSync(wasm))!==binaries.wasm)throw Error('Render binaries changed during capture');
  report.selectionComplete=true;report.complete=report.cases.length===filterModes.length*2;flush();
}finally{await browser.close();rmSync(temporary,{recursive:true,force:true});}
