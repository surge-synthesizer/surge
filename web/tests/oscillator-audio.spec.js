import {test,expect} from './fixtures.js';
import {oscillatorTypes,oscillatorFixture} from '../scripts/oscillator-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {readFileSync,writeFileSync,mkdtempSync,rmSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=join(root,'build-reference/src/surge-web/surge-engine-reference');
const evidence=JSON.parse(readFileSync(new URL('../parity/oscillators.json',import.meta.url)));
const fixtureBytes=oscillatorFixture();
if(!evidence.complete||JSON.stringify(evidence.inventory)!==JSON.stringify(oscillatorTypes)||
   JSON.stringify(evidence.rates)!=='[44100,48000]'||evidence.relativeRmsTolerance!==1e-5||
   evidence.fixtureSha256!==createHash('sha256').update(fixtureBytes).digest('hex')||evidence.cases.length!==oscillatorTypes.length*2)
  throw Error('Review changed oscillator fixtures/inventory');
for(const type of oscillatorTypes)for(const rate of evidence.rates){
  const cases=evidence.cases.filter(c=>c.id===type.id&&c.rate===rate);
  if(cases.length!==1||!['matched-default-fixture','needs-repeatability-investigation'].includes(cases[0].status))
    throw Error('Missing oscillator measurement or unresolved fixture error');
}
for(const item of evidence.cases.filter(c=>c.status==='matched-default-fixture'))
test(`${item.name} oscillator fixture matches native at ${item.rate} Hz`,async({page})=>{
  const directory=mkdtempSync(join(tmpdir(),'surge-oscillator-test-'));
  try{
    const fixture=join(directory,'fixture.fxp'),muted=join(directory,'muted.fxp');
    writeFileSync(fixture,fixtureBytes);writeFileSync(muted,oscillatorFixture({muted:true}));
    const native=(patch,name,input=item.audioInput)=>{
      const file=join(directory,name+'.f32');
      execFileSync(harness,[...(input?['--audio-input']:[]),'--oscillator-type',String(item.id),join(root,'resources/data'),patch,String(item.rate),file],{timeout:20000});
      return readFileSync(file);
    };
    const reference=native(fixture,'reference');
    expect(native(fixture,'repeat').equals(reference),'Native repeatability').toBe(true);
    const silence=native(muted,'muted');
    for(let i=0;i<silence.length;i+=4)if(silence.readFloatLE(i)!==0)throw Error('Muted oscillator fixture still produces sound');
    const render=async()=>{
      await page.goto('/');
      return page.evaluate(renderEngineFixture,{rate:item.rate,bytes:[...fixtureBytes],oscillatorType:item.id,audioInput:item.audioInput});
    };
    const actual=await render(),repeat=await render();
    expect(actual.length).toBe(reference.length/4);expect(repeat.length).toBe(actual.length);
    let energy=0,error=0;
    for(let i=0;i<actual.length;i++){
      const target=reference.readFloatLE(i*4);
      if(!Number.isFinite(target)||!Number.isFinite(actual[i])||actual[i]!==repeat[i])throw Error('Non-finite or non-repeatable audio at '+i);
      energy+=target*target;error+=(target-actual[i])**2;
    }
    expect(energy).toBeGreaterThan(1);expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    if(item.audioInput){
      const absent=native(fixture,'no-input',false);let absentEnergy=0;
      for(let i=0;i<absent.length;i+=4)absentEnergy+=absent.readFloatLE(i)**2;
      expect(absentEnergy,'Audio Input must depend on the supplied input').toBe(0);
    }
    await test.info().attach('oscillator-comparison',{contentType:'application/json',body:JSON.stringify({name:item.name,rate:item.rate,relativeRMS:Math.sqrt(error/energy),energy})});
  }finally{rmSync(directory,{recursive:true,force:true});}
});
