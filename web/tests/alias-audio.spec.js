import {test,expect} from './fixtures.js';
import {oscillatorFixture,oscillatorTypes} from '../scripts/oscillator-fixtures.mjs';
import {renderEngineFixture} from './helpers/engine-fixture.js';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));

// Change only the saved coefficients; preserve parameters, embedded assets and
// extension data so the control comparison isolates the custom editor state.
function fundamentalOnly(input){
  const bytes=Buffer.from(input),size=bytes.readUInt32LE(64);
  const xml=bytes.subarray(92,92+size).toString('utf8');
  let count=0;
  const changed=xml.replace(/<osc_extra_sc0_osc0\b[^>]*>/,tag=>tag.replace(/extra_data_(\d+)="[^"]*"/g,(_,i)=>{
    count++;return `extra_data_${i}="${Number(i)===0?1:0}"`;
  }));
  if(count!==16)throw Error('Review Alias extra-data serialization');
  const payload=Buffer.from(changed),header=Buffer.from(bytes.subarray(0,92));
  header.writeUInt32LE(payload.length,64);
  header.writeUInt32BE(header.readUInt32BE(56)+payload.length-size,56);
  // Native Surge saves the optional outer byteSize as zero.
  if(header.readUInt32BE(4)!==0)header.writeUInt32BE(header.readUInt32BE(4)+payload.length-size,4);
  return Buffer.concat([header,payload,bytes.subarray(92+size)]);
}

for(const rate of [44100,48000])test(`Alias saved additive editor coefficients match native audio at ${rate} Hz`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(bytes=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'Isolated Alias.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{
      dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },[...oscillatorFixture()]);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Isolated Alias');
  await page.getByRole('button',{name:'Oscillator Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Alias',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Scene A Osc 1 Wrap',exact:true})).toBeAttached();
  await page.getByRole('slider',{name:'Scene A Osc 1 Shape',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await page.getByRole('slider',{name:'Harmonic 1',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Triangle',exact:true}).dispatchEvent('click');
  await page.getByRole('slider',{name:'Harmonic 2',exact:true}).press('End');
  await page.getByRole('slider',{name:'Harmonic 4',exact:true}).press('Shift+ArrowUp');
  const coefficients=await page.getByRole('slider',{name:/^Harmonic \d+$/}).evaluateAll(nodes=>nodes.map(n=>Number(n.getAttribute('aria-valuenow'))));
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Additive audio');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const saved='/user/Patches/Browser Tests/Additive audio.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,saved)).toBe(true);
  const state=await page.evaluate(path=>{
    const bytes=Module.FS.readFile(path),size=new DataView(bytes.buffer,bytes.byteOffset).getUint32(64,true);
    const xml=new DOMParser().parseFromString(new TextDecoder().decode(bytes.slice(92,92+size)).replace(/\0+$/,''),'text/xml');
    const extra=xml.querySelector('extraoscdata [scene="0"][osc="0"]');
    return {bytes:Array.from(bytes),type:Number(xml.querySelector('a_osc1_type').getAttribute('value')),
      retrigger:Number(xml.querySelector('a_osc1_retrigger').getAttribute('value')),
      coefficients:Array.from({length:Number(extra.getAttribute('extra_n'))},(_,i)=>Number(extra.getAttribute('extra_data_'+i)))};
  },saved);
  expect(state.type).toBe(oscillatorTypes.find(t=>t.symbol==='ot_alias').id);
  expect(state.retrigger).toBe(1);expect(state.coefficients).toHaveLength(16);
  for(let i=0;i<16;i++)expect(state.coefficients[i]).toBeCloseTo(coefficients[i],6);
  const directory=mkdtempSync(join(tmpdir(),'surge-alias-audio-'));
  try{
    const fixture=join(directory,'additive.fxp'),control=join(directory,'fundamental.fxp');
    writeFileSync(fixture,Buffer.from(state.bytes));writeFileSync(control,fundamentalOnly(state.bytes));
    const native=(name,patch=fixture)=>{
      const output=join(directory,name+'.f32');
      execFileSync(join(root,'build-reference/src/surge-web/surge-engine-reference'),
        [join(root,'resources/data'),patch,String(rate),output],{timeout:20000});
      return readFileSync(output);
    };
    const reference=native('reference');expect(native('repeat').equals(reference)).toBe(true);
    const baseline=native('baseline',control);
    await page.goto('/');
    const actual=await page.evaluate(renderEngineFixture,{rate,bytes:state.bytes});
    await page.goto('/');
    const repeat=await page.evaluate(renderEngineFixture,{rate,bytes:state.bytes});
    expect(actual.length).toBe(reference.length/4);
    expect(repeat.length).toBe(actual.length);
    let energy=0,error=0,coefficientDelta=0;
    for(let i=0;i<actual.length;i++){
      const target=reference.readFloatLE(i*4);
      if(!Number.isFinite(target)||!Number.isFinite(actual[i]))throw Error('Non-finite audio at '+i);
      if(actual[i]!==repeat[i])throw Error('Non-repeatable audio at '+i);
      energy+=target*target;error+=(target-actual[i])**2;
      coefficientDelta+=(target-baseline.readFloatLE(i*4))**2;
    }
    expect(energy).toBeGreaterThan(1);expect(Math.sqrt(error/energy)).toBeLessThan(1e-5);
    expect(Math.sqrt(coefficientDelta/energy),'Saved coefficients must affect the rendered signal').toBeGreaterThan(1e-3);
    await test.info().attach('alias-additive-audio',{contentType:'application/json',body:JSON.stringify({rate,coefficients,energy,relativeRMS:Math.sqrt(error/energy),coefficientDelta:Math.sqrt(coefficientDelta/energy)})});
  }finally{rmSync(directory,{recursive:true,force:true});}
});
