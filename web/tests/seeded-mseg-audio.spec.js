import {test,expect} from './fixtures.js';
import {msegTypes,msegFixture,msegFixtureInfo} from '../scripts/mseg-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=join(root,'build-reference/src/surge-web/surge-engine-reference');
const type=msegTypes.find(t=>t.name==='BROWNIAN');if(!type)throw Error('Review Brownian MSEG fixture');
for(const rate of [44100,48000])for(const seed of [1,17,123456789])
test(`Brownian MSEG pitch modulation seed ${seed} matches native at ${rate} Hz`,async({page})=>{
  const directory=mkdtempSync(join(tmpdir(),'surge-seeded-mseg-'));
  try{
    const bytes=msegFixture(type.id,{introHold:true}),fixture=join(directory,'fixture.fxp'),dry=join(directory,'dry.fxp');
    writeFileSync(fixture,bytes);writeFileSync(dry,msegFixture(type.id,{modulated:false,introHold:true}));
    const renderNative=(patch,s,name)=>{
      const output=join(directory,name+'.f32');
      execFileSync(harness,['--mseg-seed',String(s),join(root,'resources/data'),patch,String(rate),output],{timeout:20000});
      return readFileSync(output);
    };
    const reference=renderNative(fixture,seed,'reference'),repeatNative=renderNative(fixture,seed,'repeat');
    const other=renderNative(fixture,seed+1,'other'),unmodulated=renderNative(dry,seed,'dry');
    expect(repeatNative.equals(reference),'Native seeded repeatability').toBe(true);
    const render=async()=>{
      await page.goto('/');
      return page.evaluate(renderEngineFixture,{rate,bytes:[...bytes],msegSeed:seed,verifyMseg:{...msegFixtureInfo,type:type.id,segmentTypes:[msegTypes.find(t=>t.name==='HOLD').id,type.id,type.id,type.id]}});
    };
    const actual=await render(),repeat=await render();
    expect(actual.length).toBe(reference.length/4);expect(repeat.length).toBe(actual.length);
    let energy=0,error=0,seedDelta=0,modulation=0;
    for(let i=0;i<actual.length;i++){
      const target=reference.readFloatLE(i*4),different=other.readFloatLE(i*4),drySample=unmodulated.readFloatLE(i*4);
      if(!Number.isFinite(target)||!Number.isFinite(different)||!Number.isFinite(drySample)||!Number.isFinite(actual[i])||actual[i]!==repeat[i])
        throw Error('Non-finite or non-repeatable seeded audio at '+i);
      energy+=target*target;error+=(target-actual[i])**2;seedDelta+=(target-different)**2;modulation+=(target-drySample)**2;
    }
    expect(energy).toBeGreaterThan(1);
    expect(Math.sqrt(seedDelta/energy),'Seed must affect the Brownian modulation').toBeGreaterThan(1e-3);
    expect(Math.sqrt(modulation/energy),'Modulation routing must affect the signal').toBeGreaterThan(1e-3);
    const relativeRMS=Math.sqrt(error/energy);
    await test.info().attach('seeded-mseg-comparison',{contentType:'application/json',body:JSON.stringify({rate,seed,relativeRMS,seedDelta:Math.sqrt(seedDelta/energy),modulationDelta:Math.sqrt(modulation/energy)})});
    expect(relativeRMS).toBeLessThan(1e-5);
  }finally{rmSync(directory,{recursive:true,force:true});}
});
