import {test,expect} from './fixtures.js';
const script=(name,value)=>`function init(wt) wt.name="${name}" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=${value} end return r end`;
const table=(page,index)=>page.evaluate(index=>{
  const size=Module._surge_browser_wt_size(index),frames=Module._surge_browser_wt_frames(index);
  return {name:Module.ccall('surge_browser_wt_name','string',['number'],[index]),size,frames,
    samples:Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(index,Math.floor(i/size),i%size))};
},index);
async function sceneMenu(page,scene){
  const canvas=page.locator('canvas').first(),y=scene==='A'?22:43;
  await canvas.click({position:{x:25,y}});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(scene==='A'?0:1);
  await canvas.click({position:{x:25,y},button:'right'});
}
async function action(page,scene,label){
  await sceneMenu(page,scene);await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
}
async function load(page,index,name,value){
  const xml=`<wtscript><script lua="${Buffer.from(script(name,value)).toString('base64')}" frames="3" samples="3"/></wtscript>`;
  await page.evaluate(({xml,name})=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([xml],name+'.wtscript')}];},{xml,name});
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load Wavetable from File...',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,index)).toEqual({name,size:128,frames:3,samples:Array(384).fill(value)});
}
async function editor(page){
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
}
async function closeEditor(page){
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveCount(0);
}
for(const active of [false,true])test(`scene paste preserves scripted wavetable and captured inputs through undo with audio ${active?'running':'inactive'}`,async({page})=>{
  test.setTimeout(60000);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  if(active){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  await load(page,0,'Source',0.125);const source=await table(page,0);
  await editor(page);
  for(const slot of [1,2]){
    await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
    await page.getByRole('group',{name:`Snapshot ${slot}`,exact:true}).getByRole('button',{name:'Capture Scene A Osc 1',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitem')).toHaveCount(0);
  }
  await closeEditor(page);
  await action(page,'A','Copy Scene');await action(page,'B','Paste Scene');
  await expect.poll(()=>table(page,3)).toEqual(source);
  await load(page,3,'Target',0.25);const target=await table(page,3);
  await page.waitForTimeout(225);
  await action(page,'B','Paste Scene');
  await expect.poll(()=>table(page,3)).toEqual(source);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,3)).toEqual(target);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,3)).toEqual(source);
  expect(await table(page,0)).toEqual(source);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('checkbox',{name:'Save Wavetable Script Snapshots',exact:true})).not.toBeChecked();
  await page.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');
  await editor(page);
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await expect(code).toHaveValue(script('Source',0.125));
  await code.fill('function init(wt) wt.name="Copied inputs" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=wt.snapshot[1][wt.frame][i]+wt.snapshot[2][wt.frame][i] end return r end');
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(()=>table(page,3)).toEqual({name:'Copied inputs',size:128,frames:3,samples:Array(384).fill(0.25)});
});

for(const includeInputs of [false,true])test(`patch persistence ${includeInputs?'includes':'omits'} captured wavetable inputs according to export preference`,async({page})=>{
  test.setTimeout(60000);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await load(page,0,'Persisted table',0.125);const original=await table(page,0);
  await editor(page);
  for(const slot of [1,2]){
    await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
    await page.getByRole('group',{name:`Snapshot ${slot}`,exact:true}).getByRole('button',{name:'Capture Scene A Osc 1',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitem')).toHaveCount(0);
  }
  await closeEditor(page);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  const setting=page.getByRole('checkbox',{name:'Save Wavetable Script Snapshots',exact:true});
  await expect(setting).not.toBeChecked();
  if(includeInputs){await setting.dispatchEvent('click');await expect(setting).toBeChecked();}
  const patchName=includeInputs?'With inputs':'Without inputs';
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill(patchName);
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path=`/user/Patches/Browser Tests/${patchName}.fxp`;
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const bytes=await page.evaluate(path=>Array.from(Module.FS.readFile(path)),path);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await page.evaluate(path=>Array.from(Module.FS.readFile(path)),path)).toEqual(bytes);
  expect(await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),path)).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe(patchName);
  await expect.poll(()=>table(page,0)).toEqual(original);
  await editor(page);
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await expect(code).toHaveValue(script('Persisted table',0.125));
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem',{name:/^Clear Snapshot [12]/})).toHaveCount(includeInputs?2:0);
  await page.keyboard.press('Escape');
  if(includeInputs){
    await code.fill('function init(wt) wt.name="Persisted inputs" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=wt.snapshot[1][wt.frame][i]+wt.snapshot[2][wt.frame][i] end return r end');
    await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
    await expect.poll(()=>table(page,0)).toEqual({name:'Persisted inputs',size:128,frames:3,samples:Array(384).fill(0.25)});
    await closeEditor(page);
    await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
    await expect(setting).toBeChecked();
  }
});
