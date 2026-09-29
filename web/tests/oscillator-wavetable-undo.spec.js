import {test,expect} from './fixtures.js';
const table=(page,index)=>page.evaluate(index=>{
  const size=Module._surge_browser_wt_size(index),frames=Module._surge_browser_wt_frames(index);
  return {name:Module.ccall('surge_browser_wt_name','string',['number'],[index]),size,frames,
    samples:Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(index,Math.floor(i/size),i%size))};
},index);
const script=name=>`function init(wt) wt.name="${name}" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=${name==='Sine'?0.125:0.25} end return r end`;
async function load(page,index,name,scripted){
  if(scripted){
    const xml=`<wtscript><script lua="${Buffer.from(script(name)).toString('base64')}" frames="${name==='Sine'?2:3}" samples="${name==='Sine'?2:3}"/></wtscript>`;
    await page.evaluate(({xml,name})=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([xml],name+'.wtscript')}];},{xml,name});
    await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Load Wavetable from File...',exact:true}).dispatchEvent('click');
    await expect.poll(()=>table(page,index).then(t=>[t.name,t.size,t.frames])).toEqual([name,name==='Sine'?64:128,name==='Sine'?2:3]);
    return;
  }
  expect(await page.evaluate(({index,name})=>Module.ccall('surge_browser_request_wt','number',['number','string'],
    [index,`/factory/wavetables/Basic/${name}.wt`]),{index,name})).toBe(1);
  await expect.poll(()=>table(page,index).then(t=>t.name)).toBe(name);
}
async function menu(page,label){
  await page.getByRole('group',{name:'Oscillator Select',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
}
async function editor(page){
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
}
async function closeEditor(page){
  await page.getByRole('group',{name:'Osc 2 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
}
for(const scripted of [false,true])for(const active of [false,true])test(`oscillator paste undo restores every ${scripted?'scripted':'factory'} wavetable sample with audio ${active?'running':'inactive'}`,async({page})=>{
  test.setTimeout(60000);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  if(active){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  await load(page,0,'Sine',scripted);const source=await table(page,0);
  await menu(page,'Copy from Osc 1');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+2');
  await expect(page.getByRole('group',{name:'Oscillator Select',exact:true})).toHaveAttribute('aria-valuenow','1');
  await menu(page,'Paste to Osc 2');
  await expect.poll(()=>table(page,1)).toEqual(source);
  await load(page,1,'Triangle',scripted);const before=await table(page,1);
  if(scripted){
    await editor(page);
    for(const slot of [1,2]){
      await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
      await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
      await page.getByRole('group',{name:`Snapshot ${slot}`,exact:true}).getByRole('button',{name:'Capture Scene A Osc 2',exact:true}).dispatchEvent('click');
      await expect(page.getByRole('menuitem')).toHaveCount(0);
    }
    await closeEditor(page);
  }
  expect(before.samples).not.toEqual(source.samples);
  await page.waitForTimeout(225); // Separate setup from the native undo coalescing window.
  await menu(page,'Paste to Osc 2');
  await expect.poll(()=>table(page,1)).toEqual(source);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,1)).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,1)).toEqual(source);
  expect(await table(page,0)).toEqual(source);
  if(active){
    await page.evaluate(()=>SurgeAudioInput.input.graph.context.suspend());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
    // The inactive-audio control loop can publish while suspended. Alternating
    // edits must still restore the final requested table on resume.
    for(const [action,name] of [['Undo','Triangle'],['Redo','Sine'],['Undo','Triangle']]){
      await page.getByRole('button',{name:action,exact:true}).dispatchEvent('click');
      await expect.poll(()=>table(page,1).then(t=>t.name)).toBe(name);
    }
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    await expect.poll(()=>table(page,1)).toEqual(before);
    await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
    await expect.poll(()=>table(page,1)).toEqual(source);
  }
  if(scripted){
    await editor(page);
    await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(script('Sine'));
    await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitem',{name:/^Clear Snapshot [12]/})).toHaveCount(0);
    await page.keyboard.press('Escape');await closeEditor(page);
    await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
    await expect.poll(()=>table(page,1)).toEqual(before);
    await editor(page);
    const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
    await expect(code).toHaveValue(script('Triangle'));
    await code.fill('function init(wt) wt.name="Restored inputs" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=wt.snapshot[1][wt.frame][i]+wt.snapshot[2][wt.frame][i] end return r end');
    await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
    await expect.poll(()=>table(page,1).then(t=>t.name)).toBe('Restored inputs');
    await expect.poll(()=>table(page,1)).toEqual({name:'Restored inputs',size:128,frames:3,samples:Array(128*3).fill(0.5)});
  }
});
