// SPDX-License-Identifier: GPL-3.0-or-later
// Measurements of default-family fixtures, not a full oscillator parity gate.
import {chromium} from '@playwright/test';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {parseArgs} from 'node:util';
import {oscillatorTypes,oscillatorFixture} from './oscillator-fixtures.mjs';
import {renderEngineFixture} from '../tests/helpers/engine-fixture.js';
const root=fileURLToPath(new URL('../../',import.meta.url));
const {values,positionals}=parseArgs({allowPositionals:true,options:{'storage-seed':{type:'string'}}});
if(positionals.length>1)throw Error('Expected one output path');
const storageSeed=values['storage-seed']===undefined?undefined:Number(values['storage-seed']);
if(storageSeed!==undefined&&(!/^\d+$/.test(values['storage-seed'])||!Number.isInteger(storageSeed)||storageSeed<0||storageSeed>0xffffffff))throw Error('Invalid storage seed');
const output=resolve(positionals[0]||'/tmp/surge-oscillator-survey.json');
const native=join(root,'build-reference/src/surge-web/surge-engine-reference');
const wasm=join(root,'build-web/web/surge-web.wasm');
const hash=b=>createHash('sha256').update(b).digest('hex');
const bytes=oscillatorFixture(),temporary=mkdtempSync(join(tmpdir(),'surge-oscillators-'));
const fixture=join(temporary,'embedded.fxp');writeFileSync(fixture,bytes);
const report={schema:1,status:'diagnostic',complete:false,inventory:oscillatorTypes,rates:[44100,48000],relativeRmsTolerance:1e-5,
  scope:'Scene A oscillator 1, original Init Wavetable patch with embedded table and retrigger enabled; original defaults when changing family; Wavetable retains factory template settings; three-second C4 note/release. Audio Input receives identical stereo integer-period saws. Other oscillators receive no external input. Variable browser buffer sizes. Parameter modes, unison, modulation and live performance remain outside this fixture.',
  createdAt:new Date().toISOString(),binaries:{native:hash(readFileSync(native)),wasm:hash(readFileSync(wasm))},fixtureSha256:hash(bytes),cases:[]};
const browser=await chromium.launch({channel:'chrome',headless:true});report.chrome=browser.version();
if(storageSeed!==undefined){report.storageSeed=storageSeed;report.scope+=' Storage RNG is explicitly seeded immediately before the note; other RNGs are unchanged.';}
const samples=b=>Array.from({length:b.length/4},(_,i)=>b.readFloatLE(i*4));
function compare(a,b){
  if(a.length!==b.length)throw Error('Render length mismatch');let energy=0,error=0,peak=0;
  for(let i=0;i<a.length;i++){
    if(!Number.isFinite(a[i])||!Number.isFinite(b[i]))throw Error('Non-finite sample');
    energy+=a[i]*a[i];error+=(a[i]-b[i])**2;peak=Math.max(peak,Math.abs(a[i]));
  }
  return {energy,error,peak,relativeRMS:energy?Math.sqrt(error/energy):null};
}
function reference(rate,type,input){
  const file=join(temporary,'audio.f32');
  execFileSync(native,[...(input?['--audio-input']:[]),'--oscillator-type',String(type),...(storageSeed===undefined?[]:['--storage-seed',String(storageSeed)]),join(root,'resources/data'),fixture,String(rate),file],{timeout:20000});
  return samples(readFileSync(file));
}
async function browserRender(rate,type,input){
  const page=await browser.newPage();
  try{
    await page.goto(process.env.SURGE_TEST_URL||'http://127.0.0.1:8080');
    return await page.evaluate(renderEngineFixture,{rate,bytes:[...bytes],oscillatorType:type,audioInput:input,storageSeed});
  }finally{await page.close();}
}
try{
  for(const rate of report.rates)for(const type of oscillatorTypes){
    const input=type.symbol==='ot_audioinput',item={...type,rate,audioInput:input};
    try{
      const a=reference(rate,type.id,input),b=await browserRender(rate,type.id,input);
      item.comparison=compare(a,b);item.nativeRepeat=compare(a,reference(rate,type.id,input));
      item.wasmRepeat=compare(b,await browserRender(rate,type.id,input));
      if(input)item.inputControl=compare(a,reference(rate,type.id,false));
      item.status=item.comparison.energy<=1?'needs-fixture':
        item.nativeRepeat.error!==0||item.wasmRepeat.error!==0?'needs-repeatability-investigation':
        item.comparison.relativeRMS<report.relativeRmsTolerance?'matched-default-fixture':'needs-numerical-investigation';
      if(input && item.inputControl.relativeRMS<.001)item.status='needs-fixture';
    }catch(error){item.status='error';item.error=String(error);}
    report.cases.push(item);writeFileSync(output,JSON.stringify(report,null,2)+'\n');
    console.log(`${rate} ${type.name}: ${item.status}; relative RMS ${item.comparison?.relativeRMS}`);
  }
  // Detect local artifact changes across this capture; the dev server must serve
  // build-web/web from this checkout for these local binary hashes to apply.
  if(report.binaries.native!==hash(readFileSync(native))||report.binaries.wasm!==hash(readFileSync(wasm)))throw Error('A render binary changed during the survey');
  report.complete=true;writeFileSync(output,JSON.stringify(report,null,2)+'\n');
}finally{await browser.close();rmSync(temporary,{recursive:true,force:true});}
