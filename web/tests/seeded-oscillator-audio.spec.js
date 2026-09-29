import {test,expect} from './fixtures.js';
import {oscillatorTypes,oscillatorFixture} from '../scripts/oscillator-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=join(root,'build-reference/src/surge-web/surge-engine-reference');
const families=['ot_shnoise','ot_string'].map(symbol=>{
  const type=oscillatorTypes.find(t=>t.symbol===symbol);if(!type)throw Error('Missing seeded oscillator family');return type;
});
for(const type of families)for(const rate of [44100,48000])for(const seed of [1,17,123456789])
test(`${type.name} seeded sequence ${seed} matches native at ${rate} Hz`,async({page})=>{
  const directory=mkdtempSync(join(tmpdir(),'surge-seeded-oscillator-'));
  try{
    const bytes=oscillatorFixture(),fixture=join(directory,'fixture.fxp');writeFileSync(fixture,bytes);
    const renderNative=(s,name)=>{
      const output=join(directory,name+'.f32');
      execFileSync(harness,['--oscillator-type',String(type.id),'--storage-seed',String(s),join(root,'resources/data'),fixture,String(rate),output],{timeout:20000});
      return readFileSync(output);
    };
    const reference=renderNative(seed,'reference'),repeatNative=renderNative(seed,'repeat'),other=renderNative(seed+1,'other');
    expect(repeatNative.equals(reference),'Native seeded repeatability').toBe(true);
    const render=async()=>{
      await page.goto('/');
      return page.evaluate(renderEngineFixture,{rate,bytes:[...bytes],oscillatorType:type.id,storageSeed:seed});
    };
    const actual=await render(),repeat=await render();
    expect(actual.length).toBe(reference.length/4);expect(repeat.length).toBe(actual.length);
    let energy=0,error=0,seedDelta=0;
    for(let i=0;i<actual.length;i++){
      const target=reference.readFloatLE(i*4),different=other.readFloatLE(i*4);
      if(!Number.isFinite(target)||!Number.isFinite(different)||!Number.isFinite(actual[i])||actual[i]!==repeat[i])
        throw Error('Non-finite or non-repeatable seeded audio at '+i);
      energy+=target*target;error+=(target-actual[i])**2;seedDelta+=(target-different)**2;
    }
    expect(energy).toBeGreaterThan(1);
    expect(Math.sqrt(seedDelta/energy),'The seed must materially affect the noise/excitation').toBeGreaterThan(1e-3);
    expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    await test.info().attach('seeded-oscillator-comparison',{contentType:'application/json',body:JSON.stringify({name:type.name,rate,seed,relativeRMS:Math.sqrt(error/energy),seedDelta:Math.sqrt(seedDelta/energy)})});
  }finally{rmSync(directory,{recursive:true,force:true});}
});
