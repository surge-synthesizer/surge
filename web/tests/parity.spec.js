import {test,expect} from './fixtures.js';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {airwindowsInventory,airwindowsFixture,airwindowsActiveParameters,summarizeAirwindows} from '../scripts/airwindows-inventory.mjs';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=path.join(root,'build-reference/src/surge-web/surge-engine-reference');
const fixture=path.join(root,'resources/data/patches_factory/Templates/Init FM2.fxp');
const configuration=fs.readFileSync(path.join(root,'resources/surge-shared/configuration.xml'),'utf8').match(/<fx>([\s\S]*?)<\/fx>/)[1];
const types=new Map([...configuration.matchAll(/<type i="(\d+)" name="([^"]+)"/g)].map(([,id,name])=>[name,Number(id)]));
const coverage=JSON.parse(fs.readFileSync(path.join(root,'web/parity/effects.json'),'utf8'));
if(JSON.stringify([...types.keys()].sort())!==JSON.stringify(Object.keys(coverage.effects).sort()))
  throw Error('Review the changed effect-family parity inventory');
for(const [name,entry] of Object.entries(coverage.effects)){
  if(types.get(name)!==entry.type)throw Error('Effect type changed: '+name);
  if(!['default','convolution','parameters','input','airwindows','pending'].includes(entry.fixture) || (entry.fixture==='pending' && !entry.reason) ||
    (entry.fixture==='parameters' && !entry.parameterFixtures?.length))
    throw Error('Missing effect fixture or pending reason: '+name);
}
const effects=Object.entries(coverage.effects).filter(([,entry])=>entry.fixture==='default').map(([name,entry])=>{
  return {name,type:entry.type};
});
const inputModes=Object.entries(coverage.effects).filter(([,entry])=>entry.fixture==='input').map(([name,entry])=>({name:`${name} stereo input`,type:entry.type,audioInput:true}));
const airwindowsCoverage=JSON.parse(fs.readFileSync(path.join(root,'web/parity/airwindows.json'),'utf8'));
const airwindowsRegistry=airwindowsInventory();
if(airwindowsCoverage.effectOnly || !airwindowsCoverage.complete || !airwindowsCoverage.fullInventory ||
  airwindowsCoverage.cases.length!==airwindowsRegistry.length*2 ||
  JSON.stringify(airwindowsRegistry)!==JSON.stringify(airwindowsCoverage.inventory) ||
  JSON.stringify(summarizeAirwindows(airwindowsCoverage))!==JSON.stringify(airwindowsCoverage.coverage))
  throw Error('Review the changed Airwindows sub-effect inventory');
const airwindowsModes=airwindowsRegistry.flatMap(entry=>{
  const cases=airwindowsCoverage.cases.filter(item=>item.streamingId===entry.id);
  if(cases.length!==2 || ![44100,48000].every(rate=>cases.some(item=>item.rate===rate)))
    throw Error('Missing Airwindows survey rate: '+entry.name);
  const parameters=airwindowsCoverage.activeParameters?airwindowsActiveParameters(entry):[];
  if(cases.some(item=>JSON.stringify(item.parameters||[])!==JSON.stringify(parameters) ||
    JSON.stringify(item.parameter)!==JSON.stringify(airwindowsFixture(entry,airwindowsRegistry))))
    throw Error('Review changed Airwindows parameter fixture: '+entry.name);
  if(![...airwindowsCoverage.coverage.activeFixtures,...airwindowsCoverage.coverage.retiredSilence].includes(entry.name))return [];
  return [{name:`Airwindows ${entry.name}`,type:types.get('Airwindows'),parameter:airwindowsFixture(entry,airwindowsRegistry),parameters,retiredSilence:entry.noOp}];
});
function enumMembersFromHeader(header,name){
  const declaration=header.match(new RegExp(`enum ${name}\\s*\\{([\\s\\S]*?)\\}`));
  if(!declaration)throw Error('Missing fixture parameter enum: '+name);
  const members=declaration[1].replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g,'').split(',').map(value=>value.trim()).filter(Boolean);
  // These parameter enums are sequential from zero. Fail if their layout changes.
  if(members.some((member,index)=>!/^\w+(?:\s*=\s*0)?$/.test(member) || (index>0 && member.includes('='))))
    throw Error('Review fixture parameter enum layout: '+name);
  return members.map(member=>member.split(/\s*=/)[0]);
}
const parameterModes=Object.entries(coverage.effects).flatMap(([name,entry])=>(entry.parameterFixtures || []).map(fixture=>{
  const header=fs.readFileSync(path.join(root,'src/common/dsp/effects',fixture.header),'utf8');
  const index=enumMembersFromHeader(header,fixture.enum).indexOf(fixture.member);
  let value=fixture.value;
  if(fixture.discreteEnum){
    const choices=enumMembersFromHeader(header,fixture.discreteEnum);
    const choice=choices.indexOf(fixture.discreteMember);
    if(choice<0 || choices.length<2)throw Error('Invalid discrete fixture: '+name);
    value=choice/(choices.length-1);
  }
  if(index<0 || !Number.isFinite(value) || value<0 || value>1)
    throw Error('Invalid parameter fixture: '+name);
  return {name:`${name} ${fixture.discreteMember || fixture.member}`,type:entry.type,audioInput:!!fixture.audioInput,parameter:{index,value}};
}));
const ensembleHeader=fs.readFileSync(path.join(root,'src/common/dsp/effects/BBDEnsembleEffect.h'),'utf8');
const enumMembers=name=>[...ensembleHeader.match(new RegExp(`enum ${name}\\s*\\{([\\s\\S]*?)\\}`))[1].matchAll(/^\s*(ens_\w+)/gm)].map(match=>match[1]);
const ensembleParameter=enumMembers('ens_params').indexOf('ens_delay_type');
const ensembleStages=enumMembers('EnsembleStages');
const ensembleDefaults=fs.readFileSync(path.join(root,'src/common/dsp/effects/BBDEnsembleEffect.cpp'),'utf8');
const ensembleDefaultStage=Number(ensembleDefaults.match(/ens_delay_type\]\.val\.i = (\d+);/)[1]);
if(ensembleParameter<0 || ensembleStages.length!==7)throw Error('Review Ensemble fixture metadata');
const ensembleModes=ensembleStages.map((name,index)=>({name:`Ensemble ${name}`,type:types.get('Ensemble'),
  parameter:{index:ensembleParameter,value:index/(ensembleStages.length-1),isDefault:index===ensembleDefaultStage}}));
for(const mode of [{name:'dry'},{name:'stereo convolution',convolution:true},...effects,...ensembleModes,...parameterModes,...inputModes,...airwindowsModes])for(const rate of [44100,48000]) {
  const convolution=!!mode.convolution;
  const parameterArguments=[...(mode.parameter?[mode.parameter]:[]),...(mode.parameters||[])].flatMap(edit=>['--effect-parameter',String(edit.index),String(edit.value)]);
  test(`native/Wasm FM2 with ${mode.name} rendering at ${rate} Hz with variable buffer sizes`,async({page})=>{
    test.skip(!fs.existsSync(harness),'Build the native reference harness before running parity tests.');
    const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'surge-parity-'));
    try {
      const capture=path.join(temporary,'reference.f32');
      execFileSync(harness,[...(mode.audioInput?['--audio-input']:[]),...(convolution?['--convolution']:mode.type!==undefined?['--effect-type',String(mode.type)]:[]),...parameterArguments,path.join(root,'resources/data'),fixture,String(rate),capture]);
      const reference=fs.readFileSync(capture);
      if(mode.audioInput){
        const silentInputCapture=path.join(temporary,'no-input.f32');
        execFileSync(harness,['--effect-type',String(mode.type),...parameterArguments,path.join(root,'resources/data'),fixture,String(rate),silentInputCapture]);
        const baseline=fs.readFileSync(silentInputCapture);
        let delta=0,energy=0;
        for(let i=0;i<reference.length;i+=4){
          const x=reference.readFloatLE(i),y=baseline.readFloatLE(i);
          delta+=(x-y)**2;energy+=x*x;
        }
        expect(Math.sqrt(delta/energy),'External input must contribute to the rendered output').toBeGreaterThan(1e-3);
      }
      if(mode.type!==undefined){
        const dryCapture=path.join(temporary,'dry.f32');
        execFileSync(harness,[path.join(root,'resources/data'),fixture,String(rate),dryCapture]);
        const dry=fs.readFileSync(dryCapture);
        let delta=0,energy=0;
        for(let i=0;i<reference.length;i+=4){
          const x=reference.readFloatLE(i),y=dry.readFloatLE(i);
          delta+=(x-y)**2;energy+=y*y;
        }
        expect(Math.sqrt(delta/energy),'Effect fixture must change the dry signal').toBeGreaterThan(1e-3);
      }
      if(mode.parameter && !mode.parameter.isDefault){
        const defaultCapture=path.join(temporary,'default-effect.f32');
        execFileSync(harness,[...(mode.audioInput?['--audio-input']:[]),'--effect-type',String(mode.type),path.join(root,'resources/data'),fixture,String(rate),defaultCapture]);
        const baseline=fs.readFileSync(defaultCapture);
        let delta=0,energy=0;
        for(let i=0;i<reference.length;i+=4){
          const x=reference.readFloatLE(i),y=baseline.readFloatLE(i);
          delta+=(x-y)**2;energy+=y*y;
        }
        expect(Math.sqrt(delta/energy),'Selected mode must change the default effect output').toBeGreaterThan(1e-3);
      }
      if(mode.parameters?.length){
        const selectedDefault=path.join(temporary,'selected-default.f32');
        execFileSync(harness,['--effect-type',String(mode.type),'--effect-parameter',String(mode.parameter.index),String(mode.parameter.value),path.join(root,'resources/data'),fixture,String(rate),selectedDefault],{timeout:20000});
        const baseline=fs.readFileSync(selectedDefault);
        let delta=0,energy=0;
        for(let i=0;i<reference.length;i+=4){
          const x=reference.readFloatLE(i),y=baseline.readFloatLE(i);
          delta+=(x-y)**2;energy+=y*y;
        }
        expect(Math.sqrt(delta/energy),'Edited sub-effect parameters must change its selected defaults').toBeGreaterThan(1e-3);
      }
      const bytes=[...fs.readFileSync(fixture)];
      await page.goto('/');
      const actual=await page.evaluate(renderEngineFixture,{rate,bytes,convolution,effectType:mode.type,effectParameter:mode.parameter,effectParameters:mode.parameters,audioInput:mode.audioInput});
      expect(actual.length).toBe(reference.length/4);
      let signal=0,error=0;
      for(let i=0;i<actual.length;++i){
        const expected=reference.readFloatLE(i*4);
        if(!Number.isFinite(actual[i]))throw Error(`Non-finite sample ${i}`);
        signal+=expected*expected;error+=(actual[i]-expected)**2;
      }
      if(mode.retiredSilence){
        expect(signal,'The original retired Airwindows placeholder outputs silence').toBe(0);
        expect(error).toBe(0);
      }else{
        expect(signal).toBeGreaterThan(1);
        expect(Math.sqrt(error/signal)).toBeLessThan(1e-5);
      }
    } finally {fs.rmSync(temporary,{recursive:true,force:true});}
  });
}
test('engine parameter metadata and FXP serialization preserve an edit',async({page})=>{
  await page.goto('/');
  const result=await page.evaluate(async()=>{
    const {default:create}=await import('/surge-web.js');
    const m=await create();m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');
    const e=m.ccall('surge_create','number',['number','string'],[48000,'/factory']);
    if(!e)throw Error(m.ccall('surge_error','string',[],[]));
    try {
      const count=m._surge_parameter_count(e);
      const parameters=Array.from({length:count},(_,id)=>JSON.parse(m.ccall('surge_parameter_info','string',['number','number'],[e,id])));
      const volume=parameters.find(p=>p.name==='Global Volume');
      if(!volume)throw Error('Missing native parameter metadata');
      if(!m._surge_set_parameter(e,volume.id,0.5))throw Error('Edit failed');
      if(!m.ccall('surge_save_patch','number',['number','string'],[e,'/roundtrip.fxp']))throw Error(m.ccall('surge_error','string',[],[]));
      m._surge_set_parameter(e,volume.id,0.2);
      if(!m.ccall('surge_load_patch','number',['number','string'],[e,'/roundtrip.fxp']))throw Error(m.ccall('surge_error','string',[],[]));
      return {count,value:JSON.parse(m.ccall('surge_parameter_info','string',['number','number'],[e,volume.id])).value,size:m.FS.stat('/roundtrip.fxp').size};
    } finally {m._surge_destroy(e);}
  });
  expect(result.count).toBeGreaterThan(100);
  expect(result.size).toBeGreaterThan(100);
  expect(result.value).toBeCloseTo(0.5,5);
});
