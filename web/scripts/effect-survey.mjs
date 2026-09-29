#!/usr/bin/env node
// Diagnostic evidence, not a parity gate: unresolved cases are recorded explicitly.
import {chromium} from '@playwright/test';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {renderEngineFixture} from '../tests/helpers/engine-fixture.js';
import {airwindowsInventory,airwindowsFixture,airwindowsActiveParameters,summarizeAirwindows} from './airwindows-inventory.mjs';
const root=fileURLToPath(new URL('../../',import.meta.url));
const output=resolve(process.argv[2] || '/tmp/surge-effect-survey.json');
const native=join(root,'build-reference/src/surge-web/surge-engine-reference');
const fixture=join(root,'resources/data/patches_factory/Templates/Init FM2.fxp');
const configuration=readFileSync(join(root,'resources/surge-shared/configuration.xml'),'utf8');
const section=configuration.match(/<fx>([\s\S]*?)<\/fx>/)[1];
let families=[...section.matchAll(/<type i="(\d+)" name="([^"]+)"/g)]
  .map(([,id,name])=>({type:Number(id),name,convolution:name==='Convolution'}));
if(families.length!==31)throw Error('Review changed effect-family inventory');
const airwindows=process.argv.slice(3).includes('--airwindows');
const effectOnly=process.argv.slice(3).includes('--effect-only');
const activeParameters=process.argv.slice(3).includes('--active-parameters');
if(activeParameters && !airwindows)throw Error('--active-parameters requires --airwindows');
const inventory=airwindows?airwindowsInventory():null;
if(airwindows){
  const type=families.find(family=>family.name==='Airwindows').type;
  families=inventory.map(entry=>({...entry,type,parameter:airwindowsFixture(entry,inventory),parameters:activeParameters?airwindowsActiveParameters(entry):[]}));
}
const familyCount=families.length;
const requested=process.argv.slice(3).filter(value=>!['--airwindows','--effect-only','--active-parameters'].includes(value));
if(requested.some(name=>!families.some(family=>family.name===name)))throw Error('Unknown effect family');
if(requested.length)families=families.filter(family=>requested.includes(family.name));
const digest=file=>createHash('sha256').update(readFileSync(file)).digest('hex');
const report={schema:1,effectOnly,activeParameters,
  scope:(airwindows?(activeParameters?'Registered Airwindows effects with source-derived active parameter edits. ':'Registered Airwindows effects; AD Clip uses full Boost, others use selected defaults. '):'Original effect families. ')+
    (effectOnly?'Isolated 32-frame effect blocks with bit-identical stereo integer-period saws for one second, followed by two seconds of silence; voices, routing and modulation are bypassed. ':'Init FM2 three-second C4 note/release, with variable browser buffer sizes. ')+
    '44.1/48 kHz. Retired NoOp entries deliberately output silence. Diagnostic only, not complete parameter or real-time parity.',
  selectedFamilies:families.map(family=>family.name),fullInventory:families.length===familyCount,inventory,
  createdAt:new Date().toISOString(),binaries:{native:digest(native),wasm:digest(join(root,'build-web/web/surge-web.wasm'))},
  fixture:digest(fixture),configuration:digest(join(root,'resources/surge-shared/configuration.xml')),cases:[]};
if(airwindows)report.registrySHA256=digest(join(root,'libs/airwindows/src/AirWinBaseClass_pluginRegistry.cpp'));
const temporary=mkdtempSync(join(tmpdir(),'surge-effect-survey-'));
const browser=await chromium.launch({channel:'chrome',headless:true});
report.chrome=browser.version();
function samples(buffer){return Array.from({length:buffer.length/4},(_,i)=>buffer.readFloatLE(i*4));}
function compare(a,b){
  if(a.length!==b.length)throw Error('Different render lengths');
  let energy=0,error=0,peak=0;
  for(let i=0;i<a.length;i++){
    if(!Number.isFinite(a[i])||!Number.isFinite(b[i]))throw Error('Non-finite audio');
    energy+=a[i]*a[i];error+=(a[i]-b[i])**2;peak=Math.max(peak,Math.abs(a[i]));
  }
  return {energy,peak,relativeRMS:energy?Math.sqrt(error/energy):null,error};
}
function reference(rate,mode={}){
  const file=join(temporary,'reference.f32');
  const options=mode.convolution?['--convolution']:mode.type===undefined?[]:['--effect-type',String(mode.type)];
  execFileSync(native,[...(effectOnly?['--effect-only']:[]),...options,...[...(mode.parameter?[mode.parameter]:[]),...(mode.parameters||[])].flatMap(edit=>['--effect-parameter',String(edit.index),String(edit.value)]),join(root,'resources/data'),fixture,String(rate),file],{timeout:20000});
  return samples(readFileSync(file));
}
async function wasm(rate,mode){
  const page=await browser.newPage();
  try{
    await page.goto(process.env.SURGE_TEST_URL || 'http://127.0.0.1:8080');
    return await page.evaluate(renderEngineFixture,{rate,bytes:[...readFileSync(fixture)],convolution:mode.convolution,
      effectType:mode.convolution?undefined:mode.type,effectParameter:mode.parameter,effectParameters:mode.parameters,effectOnly});
  }finally{await page.close();}
}
try{
  for(const rate of [44100,48000]){
    const dry=reference(rate);
    for(const mode of families){
      const item={name:mode.name,type:mode.type,rate,...(airwindows?{streamingId:mode.id,parameter:mode.parameter,parameters:mode.parameters,noOp:mode.noOp}:{})};
      try{
        const a=reference(rate,mode),b=await wasm(rate,mode);
        item.comparison=compare(a,b);
        item.dry=compare(dry,a);
        item.nativeRepeat=compare(a,reference(rate,mode));
        item.wasmRepeat=compare(b,await wasm(rate,mode));
        item.status=mode.noOp && item.comparison.energy===0 && item.comparison.error===0 && item.nativeRepeat.error===0 && item.wasmRepeat.error===0?'matched-retired-silence':item.comparison.energy<=1 || item.dry.relativeRMS<1e-3?'needs-fixture':
          item.comparison.relativeRMS<1e-5 && item.nativeRepeat.relativeRMS<1e-5 && item.wasmRepeat.relativeRMS<1e-5
            ?(mode.parameters?.length?'matched-parameter-fixture':'matched-default-fixture'):'needs-investigation';
      }catch(error){item.status='error';item.error=String(error);}
      report.cases.push(item);
      writeFileSync(output,JSON.stringify(report,null,2)+'\n');
      console.log(`${rate} ${mode.name}: ${item.status}, error=${item.comparison?.relativeRMS}`);
    }
  }
  report.complete=report.cases.length===families.length*2;
  if(airwindows)report.coverage=summarizeAirwindows(report);
  writeFileSync(output,JSON.stringify(report,null,2)+'\n');
  console.log(`Evidence saved to ${output}. This survey is not a full parity gate.`);
}finally{await browser.close();rmSync(temporary,{recursive:true,force:true});}
