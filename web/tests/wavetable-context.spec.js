import {test,expect} from './fixtures.js';
async function open(page){
  page.setDefaultTimeout(10000);await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
}
async function command(page,name){
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
}
const name=page=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]));
const samples=page=>page.evaluate(()=>({size:Module._surge_browser_wt_size(0),frames:Module._surge_browser_wt_frames(0),values:Array.from({length:Module._surge_browser_wt_frames(0)},(_,f)=>Array.from({length:Module._surge_browser_wt_size(0)},(_,i)=>Module._surge_browser_wt_sample(0,f,i)))}));
for(const active of [false,true])test(`wavetable rename cancellation and persisted patch reload with audio ${active?'running':'inactive'}`,async({page})=>{
  await open(page);
  if(active){await page.getByRole('button',{name:'Enable audio',exact:true}).click();await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);}
  const before=await samples(page),oldName=await name(page);
  await command(page,'Rename Wavetable...');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Cancelled name');
  await page.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');
  expect(await name(page)).toBe(oldName);expect(await samples(page)).toEqual(before);
  await command(page,'Rename Wavetable...');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Renamed – café');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe('Renamed – café');expect(await samples(page)).toEqual(before);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Renamed Table');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Patches/Browser Tests/Renamed Table.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),path);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Renamed Table');
  expect(await name(page)).toBe('Renamed – café');expect(await samples(page)).toEqual(before);
});
test('wavetable context menu opens the original script editor',async({page})=>{
  await open(page);const before=await samples(page);await command(page,'Wavetable Script Editor...');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();expect(await samples(page)).toEqual(before);
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
});
test('refresh wavetable list discovers user files without replacing the current table',async({page})=>{
  await open(page);const before=await samples(page),oldName=await name(page);
  await page.evaluate(()=>{
    const data=new Uint8Array(12+64*4),view=new DataView(data.buffer);
    data.set(new TextEncoder().encode('vawt'));view.setUint32(4,64,true);view.setUint16(8,1,true);
    for(let i=0;i<64;i++)view.setFloat32(12+i*4,(i-32)/64,true);
    Module.FS.mkdirTree('/user/Wavetables/Browser Refresh');Module.FS.writeFile('/user/Wavetables/Browser Refresh/Fresh.wt',data);
  });
  await command(page,'Refresh Wavetable List');expect(await name(page)).toBe(oldName);expect(await samples(page)).toEqual(before);
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Browser Refresh',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Fresh',exact:true}).dispatchEvent('click');await expect.poll(()=>name(page)).toBe('Fresh');
  expect(await samples(page)).toEqual({size:64,frames:1,values:[Array.from({length:64},(_,i)=>(i-32)/64)]});
});

function wave(bytes){
  expect(bytes.subarray(0,4).toString()).toBe('RIFF');expect(bytes.subarray(8,12).toString()).toBe('WAVE');expect(bytes.readUInt32LE(4)).toBe(bytes.length-8);
  const chunks=new Map();let offset=12;
  while(offset<bytes.length){
    expect(offset+8).toBeLessThanOrEqual(bytes.length);
    const id=bytes.subarray(offset,offset+4).toString(),length=bytes.readUInt32LE(offset+4);
    expect(offset+8+length).toBeLessThanOrEqual(bytes.length);
    chunks.set(id,bytes.subarray(offset+8,offset+8+length));offset+=8+length+(length%2);
  }
  expect(offset).toBe(bytes.length);expect(chunks.get('fmt ').readUInt16LE(0)).toBe(3);expect(chunks.get('fmt ').readUInt16LE(2)).toBe(1);expect(chunks.get('fmt ').readUInt16LE(14)).toBe(32);
  return chunks;
}
for(const label of ['Export as .wt...','Export as .wav...','Export Frames as .wav...','Export for Serum...','Export for VCV Rack...'])
test(`oscillator context submenu ${label} exports the selected scripted table`,async({page})=>{
  await open(page);await command(page,'Wavetable Script Editor...');
  const source='function init(wt) wt.name="Context Export" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=0.125 end return r end';
  await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(source);await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe('Context Export');const before=await samples(page);
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await page.evaluate(()=>{
    globalThis.contextExports={};globalThis.contextExportClosed=0;
    const writer=path=>({write:async bytes=>{contextExports[path]=Array.from(bytes);},close:async()=>{contextExportClosed++;},abort:async()=>{}});
    window.showSaveFilePicker=async()=>({name:'context.wav',createWritable:async()=>writer('single')});
    const directory=prefix=>({name:'frames',kind:'directory',async *keys(){},getDirectoryHandle:async name=>directory(prefix+name+'/'),getFileHandle:async name=>({createWritable:async()=>writer(prefix+name)})});
    window.showDirectoryPicker=async()=>directory('');
  });
  await command(page,'Export Wavetable');await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
  const frames=label==='Export Frames as .wav...';await expect.poll(()=>page.evaluate(()=>contextExportClosed)).toBe(frames?before.frames:1);
  const files=await page.evaluate(()=>contextExports),size=label==='Export for Serum...'?2048:label==='Export for VCV Rack...'?256:before.size;
  if(label==='Export as .wt...'){
    const bytes=Buffer.from(files.single);expect(bytes.subarray(0,4).toString()).toBe('vawt');expect(bytes.readUInt32LE(4)).toBe(size);expect(bytes.readUInt16LE(8)).toBe(before.frames);expect(bytes.readUInt16LE(10)&4).toBe(0);
    for(let i=0;i<size*before.frames;i++)expect(bytes.readFloatLE(12+i*4)).toBe(0.125);
  }else{
    expect(Object.keys(files)).toHaveLength(frames?before.frames:1);
    for(const bytes of Object.values(files)){
      const chunks=wave(Buffer.from(bytes)),data=chunks.get('data');expect(data.length).toBe(size*(frames?1:before.frames)*4);
      for(let i=0;i<data.length;i+=4)expect(data.readFloatLE(i)).toBeCloseTo(0.125,6);
      if(!frames){expect(chunks.get('srge').readUInt32LE(4)).toBe(size);expect(Buffer.from(chunks.get('wtmd').toString().match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(source);}
      if(label==='Export for Serum...')expect(chunks.get('clm ').toString()).toContain('<!>2048 ');
    }
  }
  expect(await samples(page)).toEqual(before);expect(await name(page)).toBe('Context Export');
});

test('non-scripted wavetable context exports keep script-only formats hidden',async({page})=>{
  await open(page);await command(page,'Export Wavetable');
  for(const label of ['Export as .wt...','Export as .wav...','Export Frames as .wav...'])
    await expect(page.getByRole('menuitem',{name:label,exact:true})).toHaveCount(1);
  for(const label of ['Export for Serum...','Export for VCV Rack...'])
    await expect(page.getByRole('menuitem',{name:label,exact:true})).toHaveCount(0);
});

for(const audio of ['inactive','running','suspended'])test(`factory wavetable export after undo retains its snapshot across redo with audio ${audio}`,async({page})=>{
  await open(page);
  await page.evaluate(()=>Module.ccall('surge_browser_request_wt','number',['number','string'],[0,'/factory/wavetables/Basic/Sine.wt']));
  await expect.poll(()=>name(page)).toBe('Sine');
  if(audio!=='inactive'){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  expect(await page.evaluate(()=>Module._surge_browser_reslice_wt(0,128,8))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_size(0))).toBe(128);
  const expected=await samples(page);
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Basic',exact:true}).or(page.getByRole('menuitemcheckbox',{name:'Basic (Checked)',exact:true})).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Triangle',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe('Triangle');
  if(audio==='suspended'){
    await page.evaluate(()=>SurgeAudioInput.input.graph.context.suspend());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
  }
  await page.evaluate(()=>{
    globalThis.exportAfterUndo=null;globalThis.completeUndoExport=null;
    window.showSaveFilePicker=()=>new Promise(resolve=>{
      completeUndoExport=()=>resolve({name:'Restored.wt',createWritable:async()=>({
        write:async bytes=>{exportAfterUndo=Array.from(bytes);},close:async()=>{},abort:async()=>{}
      })});
    });
  });
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await command(page,'Export Wavetable');
  await page.getByRole('menuitem',{name:'Export as .wt...',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>typeof completeUndoExport)).toBe('function');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe('Triangle');
  await page.evaluate(()=>completeUndoExport());
  await expect.poll(()=>page.evaluate(()=>exportAfterUndo?.length||0)).toBeGreaterThan(12);
  const bytes=Buffer.from(await page.evaluate(()=>exportAfterUndo));
  expect(bytes.subarray(0,4).toString()).toBe('vawt');
  expect(bytes.readUInt32LE(4)).toBe(expected.size);expect(bytes.readUInt16LE(8)).toBe(expected.frames);
  const integer=!!(bytes.readUInt16LE(10)&4);
  const actual=Array.from({length:expected.size*expected.frames},(_,i)=>integer?bytes.readInt16LE(12+i*2)/16384:bytes.readFloatLE(12+i*4));
  expect(actual).toEqual(expected.values.flat());expect(await name(page)).toBe('Triangle');
});
