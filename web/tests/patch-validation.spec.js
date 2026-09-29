import {test,expect} from './fixtures.js';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
const root=fileURLToPath(new URL('../../',import.meta.url));
const template=name=>fs.readFileSync(path.join(root,'resources/data/patches_factory/Templates/'+name+'.fxp'));
function embeddedTuningPatch(attribute){
  const original=template('Init FM2'),size=original.readUInt32LE(64);
  const xml=original.subarray(92,92+size).toString().replace(/\0+$/,'');
  if(xml.includes('<patchTuning'))throw Error('Review tuning validation fixture');
  const payload=Buffer.from(xml.replace('</patch>',`<patchTuning ${attribute}="${Buffer.from('invalid tuning data\n').toString('base64')}"/></patch>`));
  const header=Buffer.from(original.subarray(0,92)),tail=original.subarray(92+size);
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}

test('native preflight accepts every shipped factory patch',()=>{
  const harness=path.join(root,'build-reference/src/surge-web/surge-engine-reference');
  test.skip(!fs.existsSync(harness),'Build the native reference harness first.');
  const output=execFileSync(harness,['--validate-factory',path.join(root,'resources/data')],{encoding:'utf8'});
  expect(output).toContain('Validated 3561 factory patches; 0 rejected');
});

test('malformed FXP imports leave the current serialized patch unchanged',async({page})=>{
  const good=template('Init FM2');
  const badXml=Buffer.from(good);badXml[92]=33;
  const badLength=Buffer.from(good);badLength.writeUInt32BE(0xffffffff,56);
  const badBlock=Buffer.from(good);badBlock.writeUInt32LE(0xffffffff,68);
  const badTable=template('Init Wavetable');
  badTable.writeUInt32LE(0xffffffff,92+badTable.readUInt32LE(64)+4);
  const invalid=[Buffer.from([1,2,3]),good.subarray(0,good.length-1),badXml,badLength,badBlock,badTable,embeddedTuningPatch('v'),embeddedTuningPatch('m')];
  await page.goto('/');
  const result=await page.evaluate(async({good,invalid})=>{
    const {default:create}=await import('/surge-web.js');
    const m=await create();m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');
    const engine=m.ccall('surge_create','number',['number','string'],[48000,'/factory']);
    if(!engine)throw Error(m.ccall('surge_error','string',[],[]));
    try {
      m.FS.writeFile('/valid.fxp',new Uint8Array(good));
      if(!m.ccall('surge_load_patch','number',['number','string'],[engine,'/valid.fxp']))throw Error('Valid fixture failed');
      const volume=Array.from({length:m._surge_parameter_count(engine)},(_,id)=>JSON.parse(m.ccall('surge_parameter_info','string',['number','number'],[engine,id])))
        .find(p=>p.name==='Global Volume');
      m._surge_set_parameter(engine,volume.id,0.37);
      m.ccall('surge_save_patch','number',['number','string'],[engine,'/before.fxp']);
      const before=m.FS.readFile('/before.fxp');
      const checks=[];
      const corruptExtension=new Uint8Array(before);
      const header=new DataView(corruptExtension.buffer);
      let extension=92+header.getUint32(64,true);
      for(let i=0;i<6;++i)extension+=header.getUint32(68+i*4,true);
      if(extension>=corruptExtension.length)throw Error('Saved fixture has no extension');
      corruptExtension[extension]=0;
      invalid.push(Array.from(corruptExtension));
      for(const bytes of invalid){
        m.FS.writeFile('/invalid.fxp',new Uint8Array(bytes));
        const loaded=m.ccall('surge_load_patch','number',['number','string'],[engine,'/invalid.fxp']);
        m.ccall('surge_save_patch','number',['number','string'],[engine,'/after.fxp']);
        const after=m.FS.readFile('/after.fxp');
        checks.push({loaded,unchanged:before.length===after.length&&before.every((v,i)=>v===after[i])});
      }
      return checks;
    } finally {m._surge_destroy(engine);}
  },{good:[...good],invalid:invalid.map(bytes=>[...bytes])});
  expect(result).toEqual(Array.from({length:invalid.length+1},()=>({loaded:0,unchanged:true})));
});

test('browser import rejects a malformed patch before exposing it to JUCE',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  const result=await page.evaluate(async()=>{
    let error='';
    try {await SurgeBrowser.importFiles([new File(['invalid'],'Broken.fxp')]);} catch(e){error=e.message;}
    return {error,name:Module.ccall('surge_browser_patch_name','string',[],[]),files:Module.FS.readdir('/user/imports').filter(p=>p!=='.'&&p!=='..')};
  });
  expect(result.error).toContain('Truncated FXP header');
  expect(result.name).toBe('Init Saw');
  expect(result.files).toEqual([]);
});

for(const activeAudio of [false,true])test(`dropped patch uses its filename without the temporary path with audio ${activeAudio?'running':'inactive'}`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  if(activeAudio){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  // Desktop file imports use the filename stem, which may differ from XML metadata.
  await page.evaluate(bytes=>{
    const dataTransfer=new DataTransfer();
    dataTransfer.items.add(new File([new Uint8Array(bytes)],'Renamed Sine.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{dataTransfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },[...template('Init Sine')]);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Renamed Sine');
});

for(const attribute of ['v','m'])test(`malformed embedded tuning ${attribute} is rejected before browser patch import`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  const result=await page.evaluate(async bytes=>{
    let error='';
    try{await SurgeBrowser.importFiles([new File([new Uint8Array(bytes)],'Invalid Embedded Tuning.fxp')]);}catch(e){error=e.message;}
    return {error,name:Module.ccall('surge_browser_patch_name','string',[],[]),files:Module.FS.readdir('/user/imports').filter(p=>p!=='.'&&p!=='..')};
  },[...embeddedTuningPatch(attribute)]);
  expect(result.error).toContain('Invalid embedded tuning');
  expect(result.name).toBe('Init Saw');expect(result.files).toEqual([]);
});
