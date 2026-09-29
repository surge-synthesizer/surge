import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const manifest=JSON.parse(readFileSync(new URL('../../build-web/web/library/manifest.json',import.meta.url)));
const impulses=manifest.entries.filter(entry=>/^(impulses_factory|impulses_3rdparty)\//.test(entry.path)&&entry.extension==='.flac');
const entry=name=>impulses.find(entry=>entry.path===`impulses_3rdparty/Airwindows/${name}.flac`);
async function start(page){
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'FX Type',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  for(const [role,name] of [['button','FX Type'],['menuitem','Convolution'],['menuitem','Init (Send)']])
    await page.getByRole(role,{name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'No impulse response loaded!',exact:true})).toBeAttached();
}
async function choose(page,current,next){
  await page.getByRole('button',{name:current,exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Airwindows',exact:true})
    .or(page.getByRole('menuitemcheckbox',{name:/^Airwindows/})).dispatchEvent('click');
  await page.getByRole('menuitem',{name:next,exact:true}).dispatchEvent('click');
}
async function selected(page,name){await expect(page.getByRole('button',{name,exact:true})).toBeAttached();}
for(const sampleRate of [44100,48000])test(`edits made during IR preparation survive the live reload at ${sampleRate} Hz`,async({page})=>{
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },sampleRate);
  await start(page);await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  await page.evaluate(()=>{
    Module._surge_browser_midi(0x90,60,100,0);
    const requests=SurgeFactory.impulseRequests,original=requests.get.bind(requests);
    globalThis.pendingIrEdit={before:Module._surge_browser_convolution_reload_applied(0)};
    requests.get=id=>{
      const status=original(id);
      if(status>0){
        requests.get=original;
        // This microtask runs after the native IR selector has decoded and
        // queued the response. The 18-second factory IR keeps preparation busy;
        // the counter assertions below verify the timing instead of assuming it.
        queueMicrotask(()=>{
          const state=globalThis.pendingIrEdit;
          state.wasPending=Module._surge_browser_convolution_reload_applied(0)===state.before;
          state.expected=[];
          for(const [prefix,key] of [['FX A1 Size','End'],['FX A1 Mix','Home']]){
            const node=[...document.querySelectorAll('[role="slider"]')]
              .find(node=>node.getAttribute('aria-label')?.startsWith(prefix));
            if(!node)throw Error('Missing parameter '+prefix);
            state.expected.push({label:node.getAttribute('aria-label'),value:String(key==='End'?node.juceData.max:node.juceData.min)});
            node.dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true}));
          }
          state.wasPending&&=Module._surge_browser_convolution_reload_applied(0)===state.before;
          state.edited=true;
        });
      }
      return status;
    };
  });
  await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
  for(const name of ['Reverbs','Dropped Spring Verb'])
    await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>pendingIrEdit.edited)).toBe(true);
  expect(await page.evaluate(()=>pendingIrEdit.wasPending)).toBe(true);
  await selected(page,'Dropped Spring Verb');
  for(const expected of await page.evaluate(()=>pendingIrEdit.expected))
    await expect(page.getByRole('slider',{name:expected.label,exact:true})).toHaveAttribute('aria-valuenow',expected.value);
  await page.evaluate(()=>Module._surge_browser_panic());
});
for(const sampleRate of [44100,48000])test(`live convolution IR reloads and parameter edits publish worker kernels at ${sampleRate} Hz`,async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },sampleRate);
  await start(page);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;
    node.disconnect();const analyser=context.createAnalyser(),silent=context.createGain();
    silent.gain.value=0;node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.convolutionProbe=analyser;
    Module._surge_browser_midi(0x90,60,100,0);
  });
  const peak=()=>page.evaluate(()=>{
    const samples=new Float32Array(convolutionProbe.fftSize);
    convolutionProbe.getFloatTimeDomainData(samples);return Math.max(...samples.map(Math.abs));
  });
  await expect.poll(peak).toBeGreaterThan(.00001);
  let current='No impulse response loaded!';
  for(const next of ['Plate Small','Plate Medium','Plate Small']){
    const before=await page.evaluate(()=>Module._surge_browser_convolution_applied());
    await choose(page,current,next);await selected(page,next);
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_convolution_applied())).toBeGreaterThan(before);
    await expect.poll(peak).toBeGreaterThan(.00001);
    current=next;
  }
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await selected(page,'Plate Medium');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await selected(page,'Plate Small');
  for(const [name,key] of [[/^FX A1 Size/,'End'],[/^FX A1 Size/,'Home'],[/^FX A1 Start/,'End']]){
    const before=await page.evaluate(()=>({applied:Module._surge_browser_convolution_applied(),blocks:Module._surge_browser_audio_blocks()}));
    await page.getByRole('slider',{name}).focus();await page.keyboard.press(key);
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_convolution_applied())).toBeGreaterThan(before.applied);
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before.blocks+32);
    await expect.poll(peak).toBeGreaterThan(.00001);
  }
  await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Remove Impulse Response',exact:true}).dispatchEvent('click');
  await selected(page,'No impulse response loaded!');await expect.poll(peak).toBeGreaterThan(.00001);
  const beforeReload=await page.evaluate(()=>Module._surge_browser_convolution_applied());
  await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_convolution_applied())).toBeGreaterThan(beforeReload);
  await page.getByRole('button',{name:'FX Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Copy FX Preset',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem',{name:'Copy FX Preset',exact:true})).toHaveCount(0);
  const beforePaste=await page.evaluate(()=>Module._surge_browser_convolution_applied());
  await page.getByRole('button',{name:'FX Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Paste FX Preset',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_convolution_applied())).toBeGreaterThan(beforePaste);
  // Exercise replacement after submitting another edit; the worker must never
  // publish a completion through an obsolete effect pointer.
  await page.getByRole('slider',{name:/^FX A1 Size/}).focus();await page.keyboard.press('End');
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
  for(const name of ['Reverb 2','Back Room'])await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true})).toBeAttached();
  await expect.poll(peak).toBeGreaterThan(.00001);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],
    ['/factory/patches_factory/Templates/Init FM2.fxp']))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  await page.evaluate(()=>Module._surge_browser_midi(0x90,64,100,0));await expect.poll(peak).toBeGreaterThan(.00001);
  await page.evaluate(()=>Module._surge_browser_panic());
  expect(errors).toEqual([]);
});
test('impulse catalog is lazy and the original picker loads verified FLAC data',async({page})=>{
  const downloads=[];page.on('request',request=>downloads.push(request.url()));await start(page);
  expect(impulses).toHaveLength(449);
  expect(await page.evaluate(entries=>entries.every(entry=>Module.FS.stat('/factory/'+entry.path).size===0),impulses)).toBe(true);
  for(const entry of impulses)expect(downloads.some(url=>url.endsWith(entry.url))).toBe(false);
  await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  const target=entry('Plate Small');
  expect(downloads.filter(url=>url.endsWith(target.url))).toHaveLength(1);
  const hash=await page.evaluate(async path=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/factory/'+path))),b=>b.toString(16).padStart(2,'0')).join(''),target.path);
  expect(hash).toBe(target.sha256);
  await page.reload();await start(page);await page.route('**/'+target.url,route=>route.abort());
  await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
});
for(const failure of ['missing','corrupt'])test(`${failure} impulse download retains the current response and allows retry`,async({page})=>{
  await start(page);await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  const target=entry('Plate Medium');
  await page.route('**/'+target.url,route=>route.fulfill({status:failure==='missing'?503:200,body:'invalid impulse'}));
  await choose(page,'Plate Small','Plate Medium');await expect(page.locator('#file-status')).toContainText('Impulse download failed');
  await selected(page,'Plate Small');
  expect(await page.evaluate(path=>Module.FS.stat('/factory/'+path).size,target.path)).toBe(0);
  await page.unroute('**/'+target.url);await choose(page,'Plate Small','Plate Medium');await selected(page,'Plate Medium');
});
for(const action of ['select','clear','replace'])test(`delayed impulse cannot override a later ${action} action`,async({page})=>{
  await start(page);await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  let release,arrive;const held=new Promise(resolve=>release=resolve),requested=new Promise(resolve=>arrive=resolve);
  const target=entry('Plate Large');await page.route('**/'+target.url,async route=>{arrive();await held;await route.continue();});
  await choose(page,'Plate Small','Plate Large');await requested;
  if(action==='select'){await choose(page,'Plate Small','Plate Medium');await selected(page,'Plate Medium');}
  if(action==='clear'){
    await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Remove Impulse Response',exact:true}).dispatchEvent('click');
    await selected(page,'No impulse response loaded!');
  }
  if(action==='replace'){
    // The dropdown edits presets of the current effect. The slot context menu
    // is the native command for choosing a different effect type.
    await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();
    await page.keyboard.press('Shift+F10');
    for(const [role,name] of [['menuitem','Reverb 2'],['menuitem','Back Room']])
      await page.getByRole(role,{name,exact:true}).dispatchEvent('click');
    await expect(page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true})).toBeAttached();
  }
  release();await expect.poll(()=>page.evaluate(()=>SurgeFactory.library.pending.size)).toBe(0);
  expect(await page.evaluate(()=>SurgeFactory.impulseRequests.size)).toBe(0);
  await expect(page.getByRole('button',{name:'Plate Large',exact:true})).toHaveCount(0);
  if(action==='select')await selected(page,'Plate Medium');
  if(action==='clear')await selected(page,'No impulse response loaded!');
  if(action==='replace')await expect(page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true})).toHaveAttribute('aria-valuetext','90.00 %');
});
for(const rate of [44100,48000])test(`loaded convolution response produces a tail in offline rendering at ${rate} Hz`,async({page})=>{
  await start(page);await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  const renderTail=()=>page.evaluate(rate=>{
    const call=(name,types=[],args=[])=>Module.ccall(name,'number',types,args);
    if(!call('surge_browser_offline_begin',['number'],[rate]))throw Error('Offline rendering unavailable');
    try{
      call('surge_browser_midi',['number','number','number','number'],[0x90,60,100,0]);
      call('surge_browser_midi',['number','number','number','number'],[0x80,60,0,4096]);
      call('surge_browser_offline_render',['number'],[16384]);
      return call('surge_browser_offline_render',['number'],[16384]);
    }finally{call('surge_browser_offline_end');}
  },rate);
  const energy=await renderTail();
  expect(Number.isFinite(energy)).toBe(true);expect(energy).toBeGreaterThan(1e-8);
  await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Remove Impulse Response',exact:true}).dispatchEvent('click');
  await selected(page,'No impulse response loaded!');
  expect(await renderTail()).toBeLessThan(1e-8);
});
test('invalid IR picker input retains the response and a valid retry loads',async({page})=>{
  await start(page);await choose(page,'No impulse response loaded!','Plate Small');await selected(page,'Plate Small');
  await page.evaluate(()=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File(['bad flac'],'Invalid.flac')}];});
  await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Impulse Response...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');await selected(page,'Plate Small');
  const bytes=Array.from(readFileSync(new URL('../../resources/data/'+entry('Plate Medium').path,import.meta.url)));
  await page.evaluate(bytes=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([new Uint8Array(bytes)],'Imported Plate.flac')}];},bytes);
  await page.getByRole('button',{name:'Plate Small',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Impulse Response...',exact:true}).dispatchEvent('click');
  await selected(page,'Imported Plate');
});
test('an IR picker completing after effect replacement cannot modify the replacement',async({page})=>{
  await start(page);
  const bytes=Array.from(readFileSync(new URL('../../resources/data/'+entry('Plate Small').path,import.meta.url)));
  await page.evaluate(bytes=>{
    window.showOpenFilePicker=()=>new Promise(resolve=>{globalThis.finishIRPicker=()=>resolve([{getFile:async()=>new File([new Uint8Array(bytes)],'Late.flac')}]);});
    const call=Module.ccall;globalThis.irPickerCallbackFinished=false;
    Module.ccall=function(name,...args){const result=call.call(this,name,...args);if(name==='surge_file_dialog_complete')irPickerCallbackFinished=true;return result;};
  },bytes);
  await page.getByRole('button',{name:'No impulse response loaded!',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Impulse Response...',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>typeof finishIRPicker)).toBe('function');
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Reverb 2',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Back Room',exact:true}).dispatchEvent('click');
  const damping=page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true});await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.evaluate(()=>finishIRPicker());await expect.poll(()=>page.evaluate(()=>irPickerCallbackFinished)).toBe(true);
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await expect(page.getByRole('button',{name:'Late',exact:true})).toHaveCount(0);
});
