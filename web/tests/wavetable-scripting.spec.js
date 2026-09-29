import {test,expect} from './fixtures.js';
const script=`function init(wt)
  wt.name = "Browser Generated"
  return wt
end
function generate(wt)
  local result = {}
  for i = 1, wt.sample_count do
    result[i] = 0.25 * sin(2 * pi * (i - 1) / wt.sample_count)
  end
  return result
end`;
async function open(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();
  await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
}
const name=page=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]));
const snapshot=page=>page.evaluate(()=>{
  const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
  return {size,frames,samples:Array.from({length:size},(_,i)=>Module._surge_browser_wt_sample(0,0,i))};
});
async function generate(page,text){
  const editor=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await editor.fill(text);
  await expect(editor).toHaveValue(text);
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
}
test('the original Lua wavetable editor generates exact samples through staged publication',async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page);
  expect(table.size).toBeGreaterThan(32);expect(table.frames).toBeGreaterThan(0);
  for(let i=0;i<table.size;i++)expect(table.samples[i]).toBeCloseTo(0.25*Math.sin(2*Math.PI*i/table.size),6);
});
test('script generation errors retain the current live wavetable',async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const before=await snapshot(page);
  await generate(page,'function generate(wt) error("deliberate generation failure") end');
  await expect(page.getByText(/deliberate generation failure/).first()).toBeAttached();
  expect(await snapshot(page)).toEqual(before);
  expect(await name(page)).toBe('Browser Generated');
});

for(const sampleRate of [44100,48000])test(`scripted wavetable publication preserves live audio at ${sampleRate} Hz`,async({page})=>{
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },sampleRate);
  await open(page);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;
    node.disconnect();const analyser=context.createAnalyser(),silence=context.createGain();
    silence.gain.value=0;node.connect(analyser);analyser.connect(silence);silence.connect(context.destination);
    globalThis.scriptProbe=analyser;
    Module._surge_browser_midi(0x90,60,100,0);
  });
  const level=()=>page.evaluate(()=>{
    const samples=new Float32Array(scriptProbe.fftSize);scriptProbe.getFloatTimeDomainData(samples);
    return Math.max(...samples.map(Math.abs));
  });
  await expect.poll(level).toBeGreaterThan(0.001);
  await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  await expect.poll(level).toBeGreaterThan(0.001);
  expect(await page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  await page.evaluate(()=>SurgeAudioInput.input.graph.context.suspend());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
  const table=await snapshot(page);
  expect(table.samples[Math.floor(table.size/4)]).toBeCloseTo(0.25,6);
});

test('the original script editor exports a compatible WT file through the browser picker',async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page);
  await page.evaluate(()=>{
    globalThis.exportedWavetable=null;
    window.showSaveFilePicker=async()=>({name:'generated.wt',createWritable:async()=>({
      write:async bytes=>{exportedWavetable=Array.from(bytes);},close:async()=>{},abort:async()=>{}
    })});
  });
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:/export as \.wt/i}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>exportedWavetable?.length||0)).toBeGreaterThan(12);
  const bytes=Buffer.from(await page.evaluate(()=>exportedWavetable));
  expect(bytes.subarray(0,4).toString()).toBe('vawt');
  expect(bytes.readUInt32LE(4)).toBe(table.size);
  expect(bytes.readUInt16LE(8)).toBe(table.frames);
  expect(bytes.readUInt16LE(10)&4).toBe(0); // floating-point samples
  expect(bytes.readUInt16LE(10)&0x10).toBe(0x10); // embedded script metadata
  for(let i=0;i<table.size*table.frames;i++)
    expect(bytes.readFloatLE(12+i*4)).toBeCloseTo(0.25*Math.sin(2*Math.PI*(i%table.size)/table.size),6);
  const metadata=bytes.subarray(12+table.size*table.frames*4).toString();
  expect(Buffer.from(metadata.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(script);
});

for(const [label,resolution] of [['Export as .wav',null],['Export for Serum',2048],['Export for VCV Rack',256]])
test(`script editor ${label} preserves WAV samples, format and Lua metadata`,async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page),size=resolution||table.size;
  await page.evaluate(()=>{
    globalThis.exportedWavetable=null;
    window.showSaveFilePicker=async()=>({name:'generated.wav',createWritable:async()=>({
      write:async bytes=>{exportedWavetable=Array.from(bytes);},close:async()=>{},abort:async()=>{}
    })});
  });
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:label+'...',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>exportedWavetable?.length||0)).toBeGreaterThan(44);
  const bytes=Buffer.from(await page.evaluate(()=>exportedWavetable));
  expect(bytes.subarray(0,4).toString()).toBe('RIFF');
  expect(bytes.subarray(8,12).toString()).toBe('WAVE');
  expect(bytes.readUInt32LE(4)).toBe(bytes.length-8);
  const chunks=new Map();let offset=12;
  while(offset<bytes.length){
    expect(offset+8).toBeLessThanOrEqual(bytes.length);
    const id=bytes.subarray(offset,offset+4).toString(),length=bytes.readUInt32LE(offset+4);
    expect(offset+8+length).toBeLessThanOrEqual(bytes.length);
    chunks.set(id,bytes.subarray(offset+8,offset+8+length));
    offset+=8+length+(length%2);
  }
  expect(offset).toBe(bytes.length);
  const format=chunks.get('fmt ');
  expect(format.length).toBe(18);
  expect(format.readUInt16LE(16)).toBe(0);
  expect(chunks.get('fact').readUInt32LE(0)).toBe(size*table.frames);
  expect(format.readUInt16LE(0)).toBe(3);
  expect(format.readUInt16LE(2)).toBe(1);
  expect(format.readUInt16LE(14)).toBe(32);
  expect(format.readUInt16LE(12)).toBe(4);
  expect(format.readUInt32LE(8)).toBe(format.readUInt32LE(4)*4);
  expect(chunks.get('srge').readUInt32LE(4)).toBe(size);
  const data=chunks.get('data');expect(data.length).toBe(size*table.frames*4);
  for(let i=0;i<size*table.frames;i++)
    expect(data.readFloatLE(i*4)).toBeCloseTo(0.25*Math.sin(2*Math.PI*(i%size)/size),6);
  const metadata=chunks.get('wtmd').toString();
  expect(Buffer.from(metadata.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(script);
  if(label==='Export for Serum')expect(chunks.get('clm ').toString()).toContain('<!>2048 ');
});

async function directoryPicker(page,fail=false){
  await page.evaluate(fail=>{
    globalThis.frameFiles={};globalThis.frameAborted=false;globalThis.frameClosed=0;
    const directories=new Set(['','Browser Generated - Frames']);
    frameFiles['Browser Generated - Frames/keep.txt']=[1,2,3];
    const directory=prefix=>({name:'frames',kind:'directory',
      async *keys(){
        const entries=new Set();
        for(const path of [...directories,...Object.keys(frameFiles)]){
          if(!path.startsWith(prefix))continue;
          const rest=path.slice(prefix.length);if(rest)entries.add(rest.split('/')[0]);
        }
        yield* entries;
      },
      async getDirectoryHandle(name){const path=prefix+name;directories.add(path);return directory(path+'/');},
      async getFileHandle(name){return {createWritable:async()=>({
        write:async bytes=>{if(fail)throw new DOMException('Frame write interrupted','AbortError');frameFiles[prefix+name]=Array.from(bytes);},
        close:async()=>{frameClosed++;},abort:async()=>{frameAborted=true;}
      })};}
    });
    window.showDirectoryPicker=async options=>{globalThis.framePickerMode=options.mode;return directory('');};
  },fail);
}
async function exportFrames(page){
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:/Export Frames as \.wav/i}).dispatchEvent('click');
}
test('frame-directory export preserves existing files and writes every generated frame',async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page);await directoryPicker(page);await exportFrames(page);
  await expect.poll(()=>page.evaluate(()=>frameClosed)).toBe(table.frames);
  expect(await page.evaluate(()=>framePickerMode)).toBe('readwrite');
  const files=await page.evaluate(()=>frameFiles);
  expect(files['Browser Generated - Frames/keep.txt']).toEqual([1,2,3]);
  for(let frame=0;frame<table.frames;frame++){
    const path=`Browser Generated - Frames 2/Browser Generated ${String(frame+1).padStart(3,'0')}.wav`;
    const bytes=Buffer.from(files[path]);
    expect(bytes.subarray(0,4).toString()).toBe('RIFF');
    expect(bytes.readUInt32LE(4)).toBe(bytes.length-8);
    let offset=12,data;
    while(offset<bytes.length){
      const id=bytes.subarray(offset,offset+4).toString(),length=bytes.readUInt32LE(offset+4);
      if(id==='data')data=bytes.subarray(offset+8,offset+8+length);
      offset+=8+length+(length%2);
    }
    expect(data.length).toBe(table.size*4);
    for(let i=0;i<table.size;i++)expect(data.readFloatLE(i*4)).toBeCloseTo(table.samples[i],6);
  }
  await exportFrames(page);await expect.poll(()=>page.evaluate(()=>frameClosed)).toBe(table.frames*2);
  expect(await page.evaluate(()=>Object.keys(frameFiles).filter(x=>x.startsWith('Browser Generated - Frames 3/')).length)).toBe(table.frames);
});
test('interrupted frame export reports failure and retains generated files across reload',async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page);await directoryPicker(page,true);await exportFrames(page);
  await expect(page.locator('#file-status')).toContainText('Frame write interrupted');
  expect(await page.evaluate(()=>frameAborted)).toBe(true);
  const retained=await page.evaluate(()=>{
    const FS=Module.FS,result=[];
    function visit(path){for(const name of FS.readdir(path)){if(name==='.'||name==='..')continue;
      const child=path+'/'+name;if(FS.isDir(FS.stat(child).mode))visit(child);else result.push(child);
    }}visit('/user/directories');return result;
  });
  expect(retained.length).toBe(table.frames);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.locator('canvas').first()).toBeVisible();await page.evaluate(()=>SurgeBrowser.ready);
  expect(await page.evaluate(paths=>paths.every(path=>Module.FS.readFile(path).length>44),retained)).toBe(true);
});

for(const change of ['patch','generation'])test(`an open export picker retains the selected scripted table after a ${change} change`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const table=await snapshot(page);
  await page.evaluate(()=>{
    globalThis.exportedWavetable=null;
    window.showSaveFilePicker=()=>new Promise(resolve=>{globalThis.finishWavetablePicker=()=>resolve({
      name:'selected.wt',createWritable:async()=>({write:async bytes=>{exportedWavetable=Array.from(bytes);},close:async()=>{},abort:async()=>{}})
    });});
  });
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:/export as \.wt/i}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>typeof finishWavetablePicker)).toBe('function');
  if(change==='patch'){
    await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Saw.fxp']));
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  }else{
    await generate(page,script.replace('Browser Generated','Replacement').replace('0.25 *','0.5 *'));
    await expect.poll(()=>name(page)).toBe('Replacement');
  }
  await page.evaluate(()=>finishWavetablePicker());
  await expect.poll(()=>page.evaluate(()=>exportedWavetable?.length||0)).toBeGreaterThan(12);
  const bytes=Buffer.from(await page.evaluate(()=>exportedWavetable));
  expect(bytes.readUInt32LE(4)).toBe(table.size);expect(bytes.readUInt16LE(8)).toBe(table.frames);
  for(let i=0;i<table.size*table.frames;i++)
    expect(bytes.readFloatLE(12+i*4)).toBeCloseTo(0.25*Math.sin(2*Math.PI*(i%table.size)/table.size),6);
  const metadata=bytes.subarray(12+table.size*table.frames*4).toString();
  expect(Buffer.from(metadata.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(script);
  if(change==='patch')expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  else {expect(await name(page)).toBe('Replacement');const current=await snapshot(page);expect(current.samples[current.size/4]).toBeCloseTo(0.5,6);}
});

async function scriptMenu(page,label){
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
}
async function saveScript(page,filename){
  await scriptMenu(page,'Save as .wtscript...');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill(filename);
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
}
test('wavetable script save persists compatible Unicode source and reloads through the native picker',async({page})=>{
  await open(page);
  const source=script+'\n-- Ω 日本語';
  await generate(page,source);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const before=await snapshot(page),path='/user/Wavetables/Scripted/Browser Tests/Unicode.wtscript';
  await saveScript(page,'Browser Tests/Unicode');
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const bytes=await page.evaluate(path=>Array.from(Module.FS.readFile(path)),path);
  const metadata=await page.evaluate(bytes=>{
    const doc=new DOMParser().parseFromString(new TextDecoder().decode(new Uint8Array(bytes)),'text/xml');
    const script=doc.querySelector('wtscript > script');
    return {source:new TextDecoder().decode(Uint8Array.from(atob(script.getAttribute('lua')),c=>c.charCodeAt(0))),frames:Number(script.getAttribute('frames')),samples:Number(script.getAttribute('samples'))};
  },bytes);
  expect(metadata.source).toBe(source);expect(metadata.frames).toBe(before.frames);expect(32*2**(metadata.samples-1)).toBe(before.size);
  await page.evaluate(()=>SurgeBrowser.flush());
  await open(page);
  expect(await page.evaluate(path=>Array.from(Module.FS.readFile(path)),path)).toEqual(bytes);
  await page.evaluate(bytes=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([new Uint8Array(bytes)],'Unicode.wtscript')}];},bytes);
  await scriptMenu(page,'Load .wtscript...');
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(source);
  expect(await snapshot(page)).toEqual(before);
  await page.evaluate(()=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File(['<wtscript><broken/>'],'Invalid.wtscript')}];});
  await scriptMenu(page,'Load .wtscript...');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(source);
  expect(await snapshot(page)).toEqual(before);
});

for(const failure of ['write','commit'])
test(`failed wavetable script ${failure} retains the saved file and permits retry`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const path='/user/Wavetables/Scripted/Retained.wtscript';
  await saveScript(page,'Retained');
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const saved=()=>page.evaluate(path=>Array.from(Module.FS.readFile(path)),path);
  const original=await saved();
  const replacement=script+'\n-- replacement';
  await generate(page,replacement);
  await saveScript(page,'Retained');
  await expect(page.getByText(/already exists.*overwrite/)).toBeAttached();
  await page.getByRole('button',{name:'No',exact:true}).dispatchEvent('click');
  expect(await saved()).toEqual(original);
  await page.evaluate(failure=>{
    const FS=Module.FS,write=FS.write,rename=FS.rename;globalThis.scriptWriteFailed=false;
    globalThis.restoreScriptWrite=()=>{FS.write=write;FS.rename=rename;};
    FS.write=function(stream,...args){
      if(failure==='write'&&stream.path.includes('/Wavetables/Scripted/')){scriptWriteFailed=true;throw new FS.ErrnoError(28);}
      return write.call(this,stream,...args);
    };
    FS.rename=function(from,to){
      if(failure==='commit'&&to.endsWith('/Retained.wtscript')){scriptWriteFailed=true;throw new FS.ErrnoError(28);}
      return rename.call(this,from,to);
    };
  },failure);
  await saveScript(page,'Retained');
  await expect(page.getByText(/already exists.*overwrite/)).toBeAttached();
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(page.getByText(failure==='write'?'Failed to write file.':'Failed to replace the saved script. The previous file was retained.',{exact:true})).toBeAttached();
  expect(await page.evaluate(()=>scriptWriteFailed)).toBe(true);
  await page.evaluate(()=>restoreScriptWrite());
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await saved()).toEqual(original);
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(replacement);
  await saveScript(page,'Retained');
  await expect(page.getByText(/already exists.*overwrite/)).toBeAttached();
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(saved).not.toEqual(original);
  const retried=Buffer.from(await saved()).toString();
  expect(Buffer.from(retried.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(replacement);
  expect(await page.evaluate(()=>Module.FS.readdir('/user/Wavetables/Scripted').filter(name=>name!=='.'&&name!=='..'))).toEqual(['Retained.wtscript']);
});

for(const change of ['patch','script','draft'])test(`a delayed script picker retains a newer ${change}`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const imported=script.replace('Browser Generated','Late import'),xml=`<wtscript><script lua="${Buffer.from(imported).toString('base64')}" frames="1" samples="2"/></wtscript>`;
  await page.evaluate(xml=>{
    window.showOpenFilePicker=()=>new Promise(resolve=>{globalThis.finishScriptPicker=()=>resolve([{getFile:async()=>new File([xml],'Late.wtscript')}]);});
    const call=Module.ccall;globalThis.scriptPickerFinished=false;
    Module.ccall=function(name,...args){const result=call.call(this,name,...args);if(name==='surge_file_dialog_complete')scriptPickerFinished=true;return result;};
  },xml);
  await scriptMenu(page,'Load .wtscript...');
  await expect.poll(()=>page.evaluate(()=>typeof finishScriptPicker)).toBe('function');
  if(change==='patch'){
    await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init FM2.fxp']));
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
    await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  }else if(change==='draft'){
    await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(script+'\n-- retain this unapplied edit');
  }else{
    await generate(page,script.replace('Browser Generated','Newer script'));
    await expect.poll(()=>name(page)).toBe('Newer script');
  }
  const before=await snapshot(page),beforeName=await name(page);
  await page.evaluate(()=>finishScriptPicker());
  await expect.poll(()=>page.evaluate(()=>scriptPickerFinished)).toBe(true);
  await expect(page.getByText('The oscillator changed while the script picker was open. The current state was retained.',{exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await snapshot(page)).toEqual(before);expect(await name(page)).toBe(beforeName);
  if(change==='draft')await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(script+'\n-- retain this unapplied edit');
  if(!await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).count()){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  }
  await page.evaluate(xml=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([xml],'Retry.wtscript')}];},xml);
  await scriptMenu(page,'Load .wtscript...');
  await expect.poll(()=>name(page)).toBe('Late import');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(imported);
});

for(const dialog of ['filename','overwrite'])test(`patch changes cancel ${dialog} script saves without modifying files`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const path='/user/Wavetables/Scripted/Pending Save.wtscript';
  if(dialog==='overwrite'){
    await saveScript(page,'Pending Save');
    await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  }
  const requested=script+'\n-- requested save Ω';
  await generate(page,requested);
  await scriptMenu(page,'Save as .wtscript...');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Pending Save');
  if(dialog==='overwrite'){
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    await expect(page.getByText(/already exists.*overwrite/)).toBeAttached();
  }
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init FM2.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  await expect(page.getByRole('textbox',{name:'Value',exact:true})).toHaveCount(0);
  await expect(page.getByText(/already exists.*overwrite/)).toHaveCount(0);
  if(dialog==='filename')expect(await page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(false);
  else {
    const original=await page.evaluate(path=>new TextDecoder().decode(Module.FS.readFile(path)),path);
    expect(Buffer.from(original.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(script);
  }
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await generate(page,requested);await expect.poll(()=>name(page)).toBe('Browser Generated');
  await saveScript(page,'Pending Save');
  if(dialog==='overwrite')await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(path=>{
    if(!Module.FS.analyzePath(path).exists)return null;
    const doc=new DOMParser().parseFromString(new TextDecoder().decode(Module.FS.readFile(path)),'text/xml');
    const encoded=doc.querySelector('script')?.getAttribute('lua');
    return encoded?new TextDecoder().decode(Uint8Array.from(atob(encoded),c=>c.charCodeAt(0))):'';
  },path)).toBe(requested);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
});

test('oscillator wavetable menu saves the current scripted source',async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
  await page.getByRole('button',{name:'Wavetable: Browser Generated',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Save as .wtscript...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Oscillator Menu');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Wavetables/Scripted/Oscillator Menu.wtscript';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const saved=await page.evaluate(path=>new TextDecoder().decode(Module.FS.readFile(path)),path);
  expect(Buffer.from(saved.match(/lua="([^"]+)"/)[1],'base64').toString()).toBe(script);
  expect(await name(page)).toBe('Browser Generated');
});

const dimensionScript='function init(wt) wt.name = "Dimensions " .. wt.sample_count .. "x" .. wt.frame_count return wt end\nfunction generate(wt) local r = {} for i=1,wt.sample_count do r[i]=0.5*wt.frame/wt.frame_count end return r end';
async function dimensionMenu(page,label){
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  const control=page.getByLabel(label,{exact:true}),bounds=await control.boundingBox();
  expect(bounds).not.toBeNull();
  await page.mouse.click(bounds.x+bounds.width/2,bounds.y+bounds.height/2,{button:'right'});
  await expect(page.getByRole('menuitem').first()).toBeAttached();
}
async function dimensionStop(page,label,value){
  await dimensionMenu(page,label);
  const item=page.getByRole('menuitem',{name:String(value),exact:true}).or(page.getByRole('menuitemcheckbox',{name:String(value)+' (Checked)',exact:true}));
  await item.dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
}
async function checkDimensions(page,size,frames){
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe(`Dimensions ${size}x${frames}`);
  const result=await page.evaluate(()=>{
    const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
    return {size,frames,edges:Array.from({length:frames},(_,f)=>[Module._surge_browser_wt_sample(0,f,0),Module._surge_browser_wt_sample(0,f,size-1)])};
  });
  expect(result.size).toBe(size);expect(result.frames).toBe(frames);
  for(let f=0;f<frames;f++)for(const sample of result.edges[f])expect(sample).toBeCloseTo(0.5*(f+1)/frames,6);
}
for(const family of ['Samples','Frames'])test(`wavetable ${family} context menu generates every offered dimension`,async({page})=>{
  test.setTimeout(90000);
  await open(page);await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(dimensionScript);
  await dimensionStop(page,'Samples',32);await dimensionStop(page,'Frames',1);
  const options=family==='Samples'?[32,64,128,256,512,1024,2048,4096]:[1,2,4,5,8,10,16,20,32,50,64,100,128,200,256];
  await dimensionMenu(page,family);
  const labels=await page.locator('[role=menuitem],[role=menuitemcheckbox]').evaluateAll(nodes=>nodes.map(node=>(node.getAttribute('aria-label')||node.textContent).replace(/ \(Checked\)$/,'')));
  expect(labels.filter(s=>/^\d+$/.test(s))).toEqual(options.map(String));
  await page.keyboard.press('Escape');
  for(const option of options){
    const before=await snapshot(page);
    await dimensionStop(page,family,option);
    expect(await snapshot(page)).toEqual(before);
    await checkDimensions(page,family==='Samples'?option:32,family==='Frames'?option:1);
  }
});

test('wavetable frame-count type-in rejects out-of-range values and generates non-preset counts',async({page})=>{
  await open(page);await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(dimensionScript);
  await dimensionStop(page,'Samples',32);await dimensionStop(page,'Frames',1);
  await checkDimensions(page,32,1);
  let currentFrames=1;
  const enter=async()=>{
    await dimensionMenu(page,'Frames');
    await page.getByRole('menuitem',{name:'Edit Value: '+currentFrames,exact:true}).dispatchEvent('click');
    const field=page.getByRole('textbox',{name:'New Value',exact:true});await expect(field).toHaveValue(String(currentFrames));return field;
  };
  for(const invalid of ['0','257','-1','invalid']){
    const field=await enter();await field.fill(invalid);await field.press('Enter');
    await expect(field).toHaveValue(invalid);
    expect((await snapshot(page)).frames).toBe(1);
    await field.press('Escape');await expect(field).toHaveCount(0);
  }
  for(const frames of [3,127,255]){
    const before=await snapshot(page),field=await enter();
    await field.fill(String(frames));await field.press('Enter');await expect(field).toHaveCount(0);
    expect(await snapshot(page)).toEqual(before);
    await checkDimensions(page,32,frames);currentFrames=frames;
  }
});

for(const slot of [1,2])test(`snapshot ${slot} captures and clears a live oscillator table`,async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const original=await snapshot(page);
  await scriptMenu(page,'Import Wavetable Data');
  const row=page.getByRole('group',{name:`Snapshot ${slot}`,exact:true});
  await expect(row.getByRole('button',{name:'Capture Scene A Osc 1',exact:true})).toBeAttached();
  await row.getByRole('button',{name:'Capture Scene A Osc 1',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  const copied=`function init(wt) wt.name="Snapshot ${slot} copy" return wt end\nfunction generate(wt) return wt.snapshot[${slot}][1] end`;
  await generate(page,copied);
  await expect.poll(()=>name(page)).toBe(`Snapshot ${slot} copy`);
  expect(await snapshot(page)).toEqual(original);
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('menuitem',{name:new RegExp(`^Clear Snapshot ${slot} `)}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  await scriptMenu(page,'Import Wavetable Data');
  await expect(page.getByRole('group',{name:`Snapshot ${slot}`,exact:true})).toBeAttached();
  await expect(page.getByRole('menuitem',{name:new RegExp(`^Clear Snapshot ${slot}`)})).toHaveCount(0);
  await page.keyboard.press('Escape');
  expect(await snapshot(page)).toEqual(original);
});

test('invalid snapshot import retains the previous captured samples',async({page})=>{
  await open(page);await generate(page,script);
  await expect.poll(()=>name(page)).toBe('Browser Generated');
  const original=await snapshot(page);
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('group',{name:'Snapshot 1',exact:true}).getByRole('button',{name:'Capture Scene A Osc 1',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  await page.evaluate(()=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File(['invalid wavetable'],'Invalid.wt')}];});
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('group',{name:/^Snapshot 1 \(/}).getByRole('button',{name:'Load .wav/.wt',exact:true}).dispatchEvent('click');
  await expect(page.getByText(/The previous snapshot was retained/).first()).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await generate(page,'function init(wt) wt.name="Retained snapshot" return wt end\nfunction generate(wt) return wt.snapshot[1][1] end');
  await expect.poll(()=>name(page)).toBe('Retained snapshot');
  expect(await snapshot(page)).toEqual(original);
});

function snapshotFile(){
  const bytes=Buffer.alloc(12+64*4);bytes.write('vawt');bytes.writeUInt32LE(64,4);bytes.writeUInt16LE(1,8);
  for(let i=0;i<64;i++)bytes.writeFloatLE(i/128,12+i*4);
  return Array.from(bytes);
}
async function snapshotPicker(page,delayed=false){
  await page.evaluate(({bytes,delayed})=>{
    const result=()=>[{getFile:async()=>new File([new Uint8Array(bytes)],'Snapshot.wt')}];
    window.showOpenFilePicker=delayed?()=>new Promise(resolve=>{globalThis.finishSnapshotPicker=()=>resolve(result());}):async()=>result();
    const call=Module.ccall;globalThis.snapshotPickerFinished=false;
    Module.ccall=function(name,...args){const value=call.call(this,name,...args);if(name==='surge_file_dialog_complete')snapshotPickerFinished=true;return value;};
  },{bytes:snapshotFile(),delayed});
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('group',{name:/^Snapshot 1(?: \(|$)/}).getByRole('button',{name:'Load .wav/.wt',exact:true}).dispatchEvent('click');
}
test('snapshot WT picker imports exact samples for script generation',async({page})=>{
  await open(page);await snapshotPicker(page);
  await expect.poll(()=>page.evaluate(()=>snapshotPickerFinished)).toBe(true);
  await dimensionStop(page,'Samples',64);
  await generate(page,'function init(wt) wt.name="Imported snapshot" return wt end\nfunction generate(wt) return wt.snapshot[1][1] end');
  await expect.poll(()=>name(page)).toBe('Imported snapshot');
  const result=await snapshot(page);expect(result.size).toBe(64);
  for(let i=0;i<64;i++)expect(result.samples[i]).toBeCloseTo(i/128,6);
});
for(const change of ['draft','generation','snapshot'])test(`a delayed snapshot picker preserves newer ${change}`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  await snapshotPicker(page,true);
  await expect.poll(()=>page.evaluate(()=>typeof finishSnapshotPicker)).toBe('function');
  let draft=script;
  if(change==='draft'){
    draft+='\n-- newer snapshot workflow';
    await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(draft);
  }else if(change==='generation'){
    draft=script.replace('Browser Generated','Newer snapshot source');
    await generate(page,draft);await expect.poll(()=>name(page)).toBe('Newer snapshot source');
  }else{
    await scriptMenu(page,'Import Wavetable Data');
    await page.getByRole('group',{name:'Snapshot 1',exact:true}).getByRole('button',{name:'Capture Scene A Osc 1',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitem')).toHaveCount(0);
  }
  const before=await snapshot(page);
  await page.evaluate(()=>finishSnapshotPicker());
  await expect.poll(()=>page.evaluate(()=>snapshotPickerFinished)).toBe(true);
  await expect(page.getByText('The oscillator changed while the snapshot picker was open. The current state was retained.',{exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(draft);
  expect(await snapshot(page)).toEqual(before);
});

for(const change of ['patch','closed editor'])test(`a delayed snapshot picker cannot modify a ${change}`,async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  await snapshotPicker(page,true);
  await expect.poll(()=>page.evaluate(()=>typeof finishSnapshotPicker)).toBe('function');
  if(change==='patch'){
    for(const patch of ['Init FM2','Init Wavetable']){
      await page.evaluate(patch=>Module.ccall('surge_browser_request_patch','number',['string'],[`/factory/patches_factory/Templates/${patch}.fxp`]),patch);
      await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe(patch);
    }
  }else{
    await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
  }
  const before=await snapshot(page);
  await page.evaluate(()=>finishSnapshotPicker());
  await expect.poll(()=>page.evaluate(()=>snapshotPickerFinished)).toBe(true);
  const canceled=page.getByText('The oscillator changed while the snapshot picker was open. The current state was retained.',{exact:true});
  if(await canceled.count())await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await snapshot(page)).toEqual(before);
  if(!await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).count()){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  }
  await scriptMenu(page,'Import Wavetable Data');
  await expect(page.getByRole('group',{name:'Snapshot 1',exact:true})).toBeAttached();
});

test('snapshot WAV picker restores all samples from a compatible export',async({page})=>{
  await open(page);await generate(page,script);await expect.poll(()=>name(page)).toBe('Browser Generated');
  const original=await snapshot(page);
  await page.evaluate(()=>{
    globalThis.snapshotWav=null;
    window.showSaveFilePicker=async()=>({name:'Snapshot.wav',createWritable:async()=>({write:async bytes=>{snapshotWav=Array.from(bytes);},close:async()=>{}})});
  });
  await scriptMenu(page,'Export as .wav...');
  await expect.poll(()=>page.evaluate(()=>snapshotWav?.length||0)).toBeGreaterThan(44);
  await page.evaluate(()=>{
    window.showOpenFilePicker=async()=>[{getFile:async()=>new File([new Uint8Array(snapshotWav)],'Snapshot.wav')}];
    const call=Module.ccall;globalThis.snapshotPickerFinished=false;
    Module.ccall=function(name,...args){const value=call.call(this,name,...args);if(name==='surge_file_dialog_complete')snapshotPickerFinished=true;return value;};
  });
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('group',{name:'Snapshot 2',exact:true}).getByRole('button',{name:'Load .wav/.wt',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>snapshotPickerFinished)).toBe(true);
  await generate(page,'function init(wt) wt.name="WAV snapshot" return wt end\nfunction generate(wt) return wt.snapshot[2][wt.frame] end');
  await expect.poll(()=>name(page)).toBe('WAV snapshot');
  expect(await snapshot(page)).toEqual(original);
});

for(const scene of [0,1])for(const source of [0,1,2])test(`snapshot capture uses Scene ${scene?'B':'A'} oscillator ${source+1}`,async({page})=>{
  await open(page);
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
  if(scene){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+s');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(scene);
  }
  const index=scene*3+source;
  await page.evaluate(({index,bytes})=>{
    Module.FS.writeFile('/user/CaptureSource.wt',new Uint8Array(bytes));
    Module.ccall('surge_browser_request_wt_file','number',['number','string'],[index,'/user/CaptureSource.wt']);
  },{index,bytes:snapshotFile()});
  await expect.poll(()=>page.evaluate(index=>Module.ccall('surge_browser_wt_name','string',['number'],[index]),index)).toBe('CaptureSource');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
  await scriptMenu(page,'Import Wavetable Data');
  await page.getByRole('group',{name:'Snapshot 1',exact:true}).getByRole('button',{name:`Capture Scene ${scene?'B':'A'} Osc ${source+1}`,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  await dimensionStop(page,'Samples',64);
  await generate(page,'function init(wt) wt.name="Captured source" return wt end\nfunction generate(wt) return wt.snapshot[1][1] end');
  await expect.poll(()=>page.evaluate(index=>Module.ccall('surge_browser_wt_name','string',['number'],[index]),scene*3)).toBe('Captured source');
  const samples=await page.evaluate(index=>Array.from({length:64},(_,i)=>Module._surge_browser_wt_sample(index,0,i)),scene*3);
  for(let i=0;i<64;i++)expect(samples[i]).toBeCloseTo(i/128,6);
});
