import {test,expect} from './fixtures.js';
import {msegTypes,msegFixture,msegFixtureInfo} from '../scripts/mseg-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const harness=path.join(root,'build-reference/src/surge-web/surge-engine-reference');
const coverage=JSON.parse(readFileSync(new URL('../parity/mseg.json',import.meta.url)));
if(JSON.stringify(msegTypes)!==JSON.stringify(Object.entries(coverage.types).map(([name,t])=>({name,id:t.id}))) ||
   JSON.stringify(coverage.rates)!=='[44100,48000]' || coverage.relativeRmsTolerance!==1e-5 ||
   Object.values(coverage.types).some(t=>!['verified-fixture','pending-statistical'].includes(t.status) || (t.status==='pending-statistical'&&!t.reason)))
  throw Error('Review the changed MSEG audio inventory');

for(const rate of coverage.rates)
for(const {name,id} of msegTypes.filter(t=>coverage.types[t.name].status==='verified-fixture'))
test(`MSEG ${name} pitch modulation matches native audio at ${rate}`,async({page})=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-mseg-audio-'));
  try{
    const fixture=path.join(directory,'mseg.fxp'),dry=path.join(directory,'dry.fxp'),other=path.join(directory,'other.fxp');
    writeFileSync(fixture,msegFixture(id));writeFileSync(dry,msegFixture(id,{modulated:false}));
    writeFileSync(other,msegFixture(msegTypes.find(t=>t.name===(name==='HOLD'?'LINEAR':'HOLD')).id));
    const render=(patch,name)=>{
      const output=path.join(directory,name+'.f32');
      execFileSync(harness,[path.join(root,'resources/data'),patch,String(rate),output],{timeout:20000});
      return readFileSync(output);
    };
    const reference=render(fixture,'reference'),repeat=render(fixture,'repeat'),baseline=render(dry,'dry'),differentShape=render(other,'shape');
    expect(repeat.equals(reference),'Native fixture must repeat exactly').toBe(true);
    await page.goto('/');
    const actual=await page.evaluate(renderEngineFixture,{rate,bytes:[...readFileSync(fixture)],verifyMseg:{...msegFixtureInfo,type:id}});
    expect(actual.length).toBe(reference.length/4);
    let energy=0,error=0,modulation=0,shapeDelta=0;
    for(let i=0;i<actual.length;i++){
      const target=reference.readFloatLE(i*4),unmodulated=baseline.readFloatLE(i*4);
      if(!Number.isFinite(actual[i]))throw Error(`Non-finite sample ${i}`);
      energy+=target*target;error+=(target-actual[i])**2;modulation+=(target-unmodulated)**2;
      shapeDelta+=(target-differentShape.readFloatLE(i*4))**2;
    }
    expect(energy).toBeGreaterThan(1);
    expect(Math.sqrt(modulation/energy),'Routing must audibly change the fixture').toBeGreaterThan(1e-3);
    expect(Math.sqrt(shapeDelta/energy),'Changing MSEG segment type must affect the signal').toBeGreaterThan(1e-3);
    expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    await test.info().attach('mseg-audio-comparison',{contentType:'application/json',body:JSON.stringify({name,rate,relativeRms:Math.sqrt(error/energy),modulationDelta:Math.sqrt(modulation/energy),shapeDelta:Math.sqrt(shapeDelta/energy)})});
  }finally{rmSync(directory,{recursive:true,force:true});}
});
