import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
import {airwindowsInventory} from '../scripts/airwindows-inventory.mjs';
import {sourceIds} from '../scripts/mseg-fixtures.mjs';
const root=new URL('../../',import.meta.url),inventory=airwindowsInventory();
const type=Number(readFileSync(new URL('resources/surge-shared/configuration.xml',root),'utf8').match(/<type i="(\d+)" name="Airwindows"/)[1]);
const original=readFileSync(new URL('resources/data/patches_factory/Templates/Init FM2.fxp',root));
const size=original.readUInt32LE(64),header=Buffer.from(original.subarray(0,92)),tail=original.subarray(92+size);
const source=sourceIds.find(x=>x.name==='ms_slfo1').id;
const xml=original.subarray(92,92+size).toString().replace(/\0+$/,'').replace('</parameters>',
  `<fx1_type type="0" value="${type}"/><fx1_p0 type="0" value="${inventory.find(x=>x.name==='DeRez').id}"/><fx1_p1 type="2" value="1"><modrouting source="${source}" source_scene="1" source_index="0" depth="0.05" muted="1"/></fx1_p1></parameters>`);
const payload=Buffer.from(xml);header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);header.writeUInt32BE(84+payload.length+tail.length,4);
const bytes=[...Buffer.concat([header,payload,tail])];
async function save(page,name,selectorKey,cancel=false){
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill(name);
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  if(selectorKey){
    // Submit the edit and save in one task, before control-side preparation.
    const actions=await page.evaluate(({key,cancel})=>{
      const actions=[],original=Module._surge_accessibility_action;
      Module._surge_accessibility_action=(...args)=>{const result=original(...args);actions.push({action:args[1],result});return result;};
      try{
      document.querySelector('[role="slider"][aria-label="FX A1 FX - Type"]')
        .dispatchEvent(new KeyboardEvent('keydown',{key,bubbles:true}));
      const dialog=document.querySelector('[role="group"][aria-label="Save Patch"]');
      dialog.querySelector('[role="button"][aria-label="OK"]').click();
      if(cancel) dialog.querySelector('[role="button"][aria-label="Cancel"]').click();
      }finally{Module._surge_accessibility_action=original;}
      return actions;
    },{key:selectorKey,cancel});
    expect(actions.filter(a=>a.action===4)).toEqual([{action:4,result:1}]);
    expect(actions.filter(a=>a.action===0)).toEqual(Array.from({length:cancel?2:1},()=>({action:0,result:1})));
  }else await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  if(cancel) return;
  const path='/user/Patches/Browser Tests/'+name+'.fxp';
  await expect.poll(()=>page.evaluate(p=>Module.FS.analyzePath(p).exists,path)).toBe(true);
  return page.evaluate(path=>{
    const b=Module.FS.readFile(path),n=new DataView(b.buffer,b.byteOffset,b.byteLength).getUint32(64,true);
    const xml=new DOMParser().parseFromString(new TextDecoder().decode(b.slice(92,92+n)).replace(/\0+$/,''),'text/xml');
    return {selector:Number(xml.querySelector('fx1_p0').getAttribute('value')),
      values:[1,2,3,4].map(i=>Number(xml.querySelector('fx1_p'+i).getAttribute('value'))),
      routes:[...xml.querySelectorAll('modrouting')].map(r=>({parameter:r.parentElement.tagName,attrs:[...r.attributes].map(a=>[a.name,a.value]).sort()}))};
  },path);
}
for(const rate of [44100,48000])test(`live Airwindows selector, undo and MIDI learn use prepared adoption at ${rate} Hz`,async({page})=>{
  test.setTimeout(60000);
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  page.on('console',message=>{if(message.text().includes('Surge Error [Database Loading Favorites]'))errors.push(message.text());});
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'Airwindows routes.fxp'));
    const r=canvas.getBoundingClientRect();canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:r.left+10,clientY:r.top+10}));
  },bytes);
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Airwindows routes');
  const before=await save(page,'AW before');expect(before.routes).toHaveLength(1);
  await page.getByRole('button',{name:'Add to Favorites',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  await page.evaluate(()=>{const {context,node}=SurgeAudioInput.input.graph;node.disconnect();const mute=context.createGain();mute.gain.value=0;node.connect(mute);mute.connect(context.destination);globalThis.selectionMute=mute;Module._surge_browser_midi(0x90,60,100,0);});
  const selector=page.getByRole('slider',{name:'FX A1 FX - Type',exact:true});
  const adoptions=()=>page.evaluate(()=>Module._surge_browser_airwindows_adoptions());
  const constructions=await page.evaluate(()=>Module._surge_browser_airwindows_realtime_constructions());
  const immediate=await save(page,'AW immediate','Home');
  expect(immediate.selector).toBe(0);expect(immediate.values).toEqual([0,0.5,0.5,0]);
  expect(immediate.routes).toEqual(before.routes);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','DeRez');
  const cancelCount=await adoptions();
  await save(page,'AW canceled','Home',true);
  await expect.poll(adoptions).toBeGreaterThan(cancelCount);
  expect(await page.evaluate(()=>Module.FS.analyzePath('/user/Patches/Browser Tests/AW canceled.fxp').exists)).toBe(false);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','DeRez');
  // Both original UI actions run in one main-thread task: the control loop
  // cannot prepare the selection before Undo captures its redo value.
  await selector.evaluate(node=>{
    node.dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));
    document.querySelector('[role="button"][aria-label="Undo"]').click();
  });
  await expect(selector).toHaveAttribute('aria-valuetext','DeRez');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','DeRez');
  let count=await adoptions();await selector.focus();await page.keyboard.press('Home');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');await expect.poll(adoptions).toBeGreaterThan(count);
  const selected=await save(page,'AW selected');expect(selected.selector).toBe(0);expect(selected.routes).toEqual(before.routes);
  count=await adoptions();await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','DeRez');await expect.poll(adoptions).toBeGreaterThan(count);
  count=await adoptions();await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');await expect.poll(adoptions).toBeGreaterThan(count);
  await selector.focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'MIDI Learn...',exact:true}).dispatchEvent('click');
  // CC 74 is reserved by Surge; use an assignable controller for MIDI learn.
  count=await adoptions();const blocks=await page.evaluate(()=>{Module._surge_browser_midi(0xb0,21,127,0);return Module._surge_browser_audio_blocks();});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(blocks+10);
  await page.evaluate(()=>Module._surge_browser_midi(0xb0,21,127,0));
  await expect(selector).toHaveAttribute('aria-valuetext',inventory.at(-1).name);
  await expect.poll(adoptions).toBeGreaterThan(count);
  const midi=await save(page,'AW midi');expect(midi.selector).toBe(inventory.length-1);expect(midi.routes).toEqual(before.routes);
  await page.evaluate(()=>SurgeAudioInput.input.graph.context.suspend());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
  const stoppedBlocks=await page.evaluate(()=>Module._surge_browser_audio_blocks());
  const stoppedSave=await save(page,'AW stopped immediate','Home');
  expect(stoppedSave.selector).toBe(0);expect(stoppedSave.values).toEqual([0,0.5,0.5,0]);
  expect(stoppedSave.routes).toEqual(before.routes);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext',inventory.at(-1).name);
  count=await adoptions();await selector.focus();await page.keyboard.press('Home');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');
  await expect(page.getByRole('slider',{name:'FX A1 Boost - AD Clip',exact:true})).toBeVisible();
  await expect.poll(adoptions).toBeGreaterThan(count);
  const suspended=await save(page,'AW suspended');expect(suspended.selector).toBe(0);expect(suspended.routes).toEqual(before.routes);
  expect(suspended.values).toEqual([0,0.5,0.5,0]); // ADClip7 constructor defaults.
  count=await adoptions();await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext',inventory.at(-1).name);
  await expect.poll(adoptions).toBeGreaterThan(count);
  count=await adoptions();await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');
  await expect.poll(adoptions).toBeGreaterThan(count);
  await selector.evaluate(node=>{
    node.dispatchEvent(new KeyboardEvent('keydown',{key:'End',bubbles:true}));
    document.querySelector('[role="button"][aria-label="Undo"]').click();
  });
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext',inventory.at(-1).name);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(selector).toHaveAttribute('aria-valuetext','AD Clip');
  expect(await page.evaluate(()=>Module._surge_browser_audio_blocks())).toBe(stoppedBlocks);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(stoppedBlocks+10);
  expect(await page.evaluate(()=>Module._surge_browser_airwindows_realtime_constructions())).toBe(constructions);
  await page.evaluate(()=>Module._surge_browser_panic());
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['"AW before"'])),{timeout:30000}).toBe(1);
  const favorites=page.getByRole('button',{name:/^(Add to|Remove from) Favorites$/});
  await favorites.focus();await page.keyboard.press('Shift+F10');
  await expect(page.getByRole('menuitem',{name:'AW before',exact:true})).toBeAttached();
  await page.keyboard.press('Escape');
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['"AW before"'])),{timeout:30000}).toBe(1);
  await favorites.focus();await page.keyboard.press('Shift+F10');
  await expect(page.getByRole('menuitem',{name:'AW before',exact:true})).toBeAttached();
  expect(errors).toEqual([]);
});

for(const rate of [44100,48000])test(`offline Airwindows API resolves selectors before edits and saves at ${rate} Hz`,async({page})=>{
  await page.goto('/');
  const derez=inventory.find(x=>x.name==='DeRez').id;
  const result=await page.evaluate(async({rate,bytes,derez,count})=>{
    const {default:create}=await import('/surge-web.js');const m=await create();
    m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');
    m.FS.writeFile('/source.fxp',new Uint8Array(bytes));
    const engine=m.ccall('surge_create','number',['number','string'],[rate,'/factory']);
    const check=value=>{if(!value)throw Error(m.ccall('surge_error','string',[],[]));};check(engine);
    const read=()=>{
      check(m.ccall('surge_save_patch','number',['number','string'],[engine,'/saved.fxp']));
      const b=m.FS.readFile('/saved.fxp'),n=new DataView(b.buffer,b.byteOffset,b.byteLength).getUint32(64,true);
      const xml=new DOMParser().parseFromString(new TextDecoder().decode(b.slice(92,92+n)).replace(/\0+$/,''),'text/xml');
      return [0,1,2,3,4].map(i=>Number(xml.querySelector('fx1_p'+i).getAttribute('value')));
    };
    try{
      check(m.ccall('surge_load_patch','number',['number','string'],[engine,'/source.fxp']));
      const parameters=Array.from({length:m._surge_parameter_count(engine)},(_,id)=>JSON.parse(m.ccall('surge_parameter_info','string',['number','number'],[engine,id])));
      const selector=parameters.find(p=>p.display==='DeRez');if(!selector)throw Error('Missing DeRez selector');
      check(m._surge_set_parameter(engine,selector.id,0));
      const generic=read();
      check(m._surge_set_effect_parameter(engine,0,0,derez/(count-1)));
      check(m._surge_set_effect_parameter(engine,0,1,0.8));
      return {generic,edited:read()};
    }finally{m._surge_destroy(engine);}
  },{rate,bytes,derez,count:inventory.length});
  expect(result.generic).toEqual([0,0,0.5,0.5,0]);
  expect(result.edited[0]).toBe(derez);expect(result.edited[1]).toBeCloseTo(0.8,5);
});
