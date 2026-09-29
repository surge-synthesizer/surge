import {test,expect} from './fixtures.js';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {airwindowsInventory,airwindowsFixture} from '../scripts/airwindows-inventory.mjs';
import {readFileSync,mkdtempSync,rmSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=join(root,'build-reference/src/surge-web/surge-engine-reference');
const patch=join(root,'resources/data/patches_factory/Templates/Init FM2.fxp');
const inventory=airwindowsInventory();
const configuration=readFileSync(join(root,'resources/surge-shared/configuration.xml'),'utf8');
const type=Number(configuration.match(/<type i="(\d+)" name="Airwindows"/)[1]);
const modes=[{name:'bypass'},...['AD Clip','Dust Bunny','To Tape'].map(name=>{
  const entry=inventory.find(entry=>entry.name===name);
  if(!entry)throw Error('Missing isolated effect fixture: '+name);
  return {name,type,parameter:airwindowsFixture(entry,inventory)};
})];
for(const mode of modes)for(const rate of [44100,48000]){
  test(`isolated ${mode.name} matches native processing of identical stereo input at ${rate} Hz`,async({page})=>{
    test.skip(!existsSync(harness),'Build the native reference harness.');
    const temporary=mkdtempSync(join(tmpdir(),'surge-isolated-'));
    try{
      const output=join(temporary,'reference.f32');
      execFileSync(harness,['--effect-only',...(mode.type===undefined?[]:['--effect-type',String(mode.type),
        '--effect-parameter',String(mode.parameter.index),String(mode.parameter.value)]),join(root,'resources/data'),patch,String(rate),output],{timeout:20000});
      const reference=readFileSync(output);
      await page.goto('/');
      const actual=await page.evaluate(renderEngineFixture,{rate,bytes:[...readFileSync(patch)],effectOnly:true,effectType:mode.type,effectParameter:mode.parameter});
      expect(actual.length).toBe(rate*3*2);
      expect(reference.length).toBe(actual.length*4);
      let energy=0,error=0,change=0;
      for(let i=0;i<actual.length;++i){
        const expected=reference.readFloatLE(i*4),frame=Math.floor(i/2);
        const input=frame>=rate?0:(i%2?((frame*29)%193-96)/256:((frame*17)%257-128)/256);
        if(!Number.isFinite(actual[i]) || !Number.isFinite(expected))throw Error(`Non-finite sample ${i}`);
        if(mode.type===undefined){
          if(actual[i]!==input || expected!==input)throw Error(`Bypass changed input sample ${i}`);
        }
        energy+=expected*expected;error+=(actual[i]-expected)**2;change+=(expected-input)**2;
      }
      expect(energy).toBeGreaterThan(1);
      if(mode.type!==undefined)expect(Math.sqrt(change/energy),'Effect must change the input').toBeGreaterThan(1e-3);
      expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    }finally{rmSync(temporary,{recursive:true,force:true});}
  });
}
