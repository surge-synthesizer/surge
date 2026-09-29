import {test,expect} from './fixtures.js';
import {formulaCases,formulaFixture} from '../scripts/formula-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=path.join(root,'build-reference/src/surge-web/surge-engine-reference');

for(const fixture of formulaCases)for(const rate of [44100,48000])
test(`formula ${fixture.name} matches native audio at ${rate} Hz`,async({page})=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-formula-audio-'));
  try{
    const bytes=formulaFixture(fixture),patch=path.join(directory,'formula.fxp'),dry=path.join(directory,'dry.fxp');
    writeFileSync(patch,bytes);writeFileSync(dry,formulaFixture(fixture,{modulated:false}));
    const render=(file,name)=>{
      const output=path.join(directory,name+'.f32');
      execFileSync(harness,[path.join(root,'resources/data'),file,String(rate),output],{timeout:20000});
      return readFileSync(output);
    };
    const reference=render(patch,'reference'),repeat=render(patch,'repeat'),baseline=render(dry,'dry');
    expect(repeat.equals(reference),'Native formula fixture must repeat exactly').toBe(true);
    await page.goto('/');
    const actual=await page.evaluate(renderEngineFixture,{rate,bytes:[...bytes],verifyFormulaPreparation:true});
    expect(actual.length).toBe(reference.length/4);
    let energy=0,error=0,modulation=0;
    for(let i=0;i<actual.length;i++){
      const expected=reference.readFloatLE(i*4);
      if(!Number.isFinite(actual[i]))throw Error(`Non-finite formula sample ${i}`);
      energy+=expected*expected;error+=(expected-actual[i])**2;
      modulation+=(expected-baseline.readFloatLE(i*4))**2;
    }
    expect(energy).toBeGreaterThan(1);
    expect(Math.sqrt(modulation/energy),'Formula routing must affect the signal').toBeGreaterThan(1e-3);
    expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    await test.info().attach('formula-audio-comparison',{contentType:'application/json',body:JSON.stringify({name:fixture.name,rate,relativeRms:Math.sqrt(error/energy),modulationDelta:Math.sqrt(modulation/energy)})});
  }finally{rmSync(directory,{recursive:true,force:true});}
});
