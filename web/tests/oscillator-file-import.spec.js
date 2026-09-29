import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const bytes=Array.from(readFileSync(new URL('../../resources/data/wavetables/Basic/Sine.wt',import.meta.url)));
async function open(page){
  page.setDefaultTimeout(10000);await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
}
const name=page=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]));
async function picker(page,delayed=false,file={bytes,name:'Imported.wt'}){
  await page.evaluate(({file,delayed})=>{
    const result=()=>[{getFile:async()=>new File([new Uint8Array(file.bytes)],file.name)}];
    window.showOpenFilePicker=delayed?()=>new Promise(resolve=>{globalThis.finishOscPicker=()=>resolve(result());}):async()=>result();
    const call=Module.ccall;globalThis.oscPickerFinished=false;
    Module.ccall=function(name,...args){const value=call.call(this,name,...args);if(name==='surge_file_dialog_complete')oscPickerFinished=true;return value;};
  },{file,delayed});
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Wavetable from File...',exact:true}).dispatchEvent('click');
}
test('oscillator file picker loads exact compatible WT samples',async({page})=>{
  await open(page);await picker(page);await expect.poll(()=>name(page)).toBe('Imported');
  const result=await page.evaluate(()=>({size:Module._surge_browser_wt_size(0),frames:Module._surge_browser_wt_frames(0),samples:Array.from({length:1024},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/64),i%64))}));
  expect(result.size).toBe(64);expect(result.frames).toBe(16);
  const file=Buffer.from(bytes);expect(result.samples).toEqual(Array.from({length:1024},(_,i)=>file.readInt16LE(12+i*2)/16384));
});
test('a delayed oscillator picker preserves a newer wavetable selection',async({page})=>{
  await open(page);await picker(page,true);
  await expect.poll(()=>page.evaluate(()=>typeof finishOscPicker)).toBe('function');
  await page.evaluate(()=>Module.ccall('surge_browser_request_wt','number',['number','string'],[0,'/factory/wavetables/Basic/Triangle.wt']));
  await expect.poll(()=>name(page)).toBe('Triangle');
  await page.evaluate(()=>finishOscPicker());
  await expect.poll(()=>page.evaluate(()=>oscPickerFinished)).toBe(true);
  await expect(page.getByText('The oscillator changed while the wavetable picker was open. The current state was retained.',{exact:true})).toBeAttached();
  expect(await name(page)).toBe('Triangle');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await picker(page);await expect.poll(()=>name(page)).toBe('Imported');
});

const importedScript='function init(wt) wt.name="Imported Script" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=(i-1+(wt.frame-1)*64)/256 end return r end';
function importFile(format){
  if(format==='wtscript')return {name:'Imported.wtscript',bytes:Array.from(Buffer.from(`<wtscript><script lua="${Buffer.from(importedScript).toString('base64')}" frames="2" samples="2"/></wtscript>`))};
  const floating=format==='float wav',dataSize=128*(floating?4:2);
  const wav=Buffer.alloc(60+dataSize);wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVE',8);
  wav.write('fmt ',12);wav.writeUInt32LE(16,16);wav.writeUInt16LE(floating?3:1,20);wav.writeUInt16LE(1,22);
  wav.writeUInt32LE(48000,24);wav.writeUInt32LE(floating?192000:96000,28);wav.writeUInt16LE(floating?4:2,32);wav.writeUInt16LE(floating?32:16,34);
  wav.write('srge',36);wav.writeUInt32LE(8,40);wav.writeUInt32LE(1,44);wav.writeUInt32LE(64,48);
  wav.write('data',52);wav.writeUInt32LE(dataSize,56);for(let i=0;i<128;i++){
    if(floating)wav.writeFloatLE(i/256,60+i*4);else wav.writeInt16LE(i*128,60+i*2);
  }
  return {name:'Imported.wav',bytes:Array.from(wav)};
}
for(const format of ['wav','float wav','wtscript'])test(`oscillator picker imports compatible ${format} frames and samples`,async({page})=>{
  await open(page);await picker(page,false,importFile(format));
  await expect.poll(()=>name(page)).toBe(format==='wtscript'?'Imported Script':'Imported');
  await expect.poll(()=>page.evaluate(()=>[Module._surge_browser_wt_size(0),Module._surge_browser_wt_frames(0)])).toEqual([64,2]);
  const samples=await page.evaluate(()=>Array.from({length:128},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/64),i%64)));
  // Tagged PCM tables retain Surge's native 16384 half-range normalization.
  expect(samples).toEqual(Array.from({length:128},(_,i)=>i/(format==='wav'?128:256)));
  if(format==='wtscript'){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
    await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(importedScript);
  }
});
for(const change of ['scene','patch'])test(`a pending oscillator import preserves a changed ${change}`,async({page})=>{
  await open(page);await picker(page,true);await expect.poll(()=>page.evaluate(()=>typeof finishOscPicker)).toBe('function');
  if(change==='scene'){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+s');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(1);
  }else{
    await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init FM2.fxp']));
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  }
  const before=await page.evaluate(()=>Array.from({length:6},(_,i)=>Module.ccall('surge_browser_wt_name','string',['number'],[i])));
  await page.evaluate(()=>finishOscPicker());await expect.poll(()=>page.evaluate(()=>oscPickerFinished)).toBe(true);
  await expect(page.getByText('The oscillator changed while the wavetable picker was open. The current state was retained.',{exact:true})).toBeAttached();
  expect(await page.evaluate(()=>Array.from({length:6},(_,i)=>Module.ccall('surge_browser_wt_name','string',['number'],[i])))).toEqual(before);
});

for(const format of ['wt','wtscript'])for(const edit of ['draft','applied script'])test(`pending ${format} import retains a newer ${edit}`,async({page})=>{
  await open(page);await picker(page,true,format==='wt'?{bytes,name:'Imported.wt'}:importFile(format));
  await expect.poll(()=>page.evaluate(()=>typeof finishOscPicker)).toBe('function');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  const draft=(await code.inputValue())+'\n-- preserve this newer draft';
  await code.fill(draft);
  if(edit==='applied script'){
    const apply=page.getByRole('button',{name:'Apply',exact:true});
    await apply.dispatchEvent('click');await expect(apply).toBeDisabled();
  }
  const previousName=await name(page);
  await page.evaluate(()=>finishOscPicker());await expect.poll(()=>page.evaluate(()=>oscPickerFinished)).toBe(true);
  await expect(page.getByText('The oscillator changed while the wavetable picker was open. The current state was retained.',{exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(code).toHaveValue(draft);expect(await name(page)).toBe(previousName);
});


test('opening an unedited Lua editor does not invalidate a pending script import',async({page})=>{
  await open(page);await picker(page,true,importFile('wtscript'));
  await expect.poll(()=>page.evaluate(()=>typeof finishOscPicker)).toBe('function');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
  await page.evaluate(()=>finishOscPicker());
  await expect.poll(()=>name(page)).toBe('Imported Script');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(importedScript);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(2);
});

for(const edited of [false,true])test(`an already open editor ${edited?'retains newer edits':'allows its requested import'}`,async({page})=>{
  await open(page);await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  const baseline=(await code.inputValue())+'\n-- existing draft';await code.fill(baseline);
  await picker(page,true,importFile('wtscript'));await expect.poll(()=>page.evaluate(()=>typeof finishOscPicker)).toBe('function');
  const draft=baseline+'\n-- edited after picker opened';if(edited)await code.fill(draft);
  await page.evaluate(()=>finishOscPicker());await expect.poll(()=>page.evaluate(()=>oscPickerFinished)).toBe(true);
  if(edited){
    await expect(page.getByText('The oscillator changed while the wavetable picker was open. The current state was retained.',{exact:true})).toBeAttached();
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');await expect(code).toHaveValue(draft);
  }else{
    await expect.poll(()=>name(page)).toBe('Imported Script');await expect(code).toHaveValue(importedScript);
  }
});

async function samples(page){return page.evaluate(()=>{
  const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
  return {size,frames,values:Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/size),i%size))};
});}
for(const format of ['wt','wav','wtscript'])test(`invalid oscillator ${format} import retains samples and editor source and permits retry`,async({page})=>{
  await open(page);await picker(page);await expect.poll(()=>name(page)).toBe('Imported');
  const before=await samples(page);
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true}),source=await code.inputValue();
  await picker(page,false,{name:'Invalid.'+format,bytes:Array.from(Buffer.from(format==='wtscript'?'<wtscript><broken/></wtscript>':'invalid'))});
  if(format==='wtscript')await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  else await expect(page.locator('#file-status')).toContainText('could not be decoded');
  const ok=page.getByRole('button',{name:'OK',exact:true});if(await ok.count())await ok.dispatchEvent('click');
  expect(await name(page)).toBe('Imported');expect(await samples(page)).toEqual(before);await expect(code).toHaveValue(source);
  await picker(page,false,{name:'Retry.wt',bytes});await expect.poll(()=>name(page)).toBe('Retry');
  expect(await samples(page)).toEqual(before);
});
for(const failure of ['AbortError','NotAllowedError'])test(`${failure} from the oscillator picker retains current state`,async({page})=>{
  await open(page);const before=await samples(page),previousName=await name(page);
  await page.evaluate(failure=>{window.showOpenFilePicker=async()=>{throw new DOMException(failure==='AbortError'?'Canceled':'Permission denied',failure);};},failure);
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Wavetable from File...',exact:true}).dispatchEvent('click');
  if(failure==='NotAllowedError')await expect(page.locator('#file-status')).toContainText('Permission denied');
  else await expect(page.getByRole('menuitem')).toHaveCount(0);
  expect(await name(page)).toBe(previousName);expect(await samples(page)).toEqual(before);
  await picker(page);await expect.poll(()=>name(page)).toBe('Imported');
});
