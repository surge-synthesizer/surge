import {test,expect} from './fixtures.js';
const scale='! saved.scl\nSaved Five\n5\n!\n240.0\n480.0\n720.0\n960.0\n2/1\n';
const mapping='! saved.kbm\n0\n0\n127\n60\n69\n432.0\n0\n';
async function start(page){
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
}
async function importTuning(page,extension,text){
  await page.evaluate(({extension,text})=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([text],'saved.'+extension)}];},{extension,text});
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Tuning',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:extension==='scl'?'Load .scl Tuning...':'Load .kbm Keyboard Mapping...',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menu')).toHaveCount(0);
}
async function openFiles(page){
  await page.getByRole('button',{name:'User files',exact:true}).click();
  const dialog=page.getByRole('dialog',{name:'User files',exact:true});await expect(dialog).toBeVisible();return dialog;
}
for(const includeTuning of [false,true])test(`saved patch exports and reimports with embedded tuning ${includeTuning}`,async({page})=>{
  await start(page);await importTuning(page,'scl',scale);await importTuning(page,'kbm',mapping);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Saved Tuning');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Export Tests');
  await page.getByRole('textbox',{name:'patch author',exact:true}).fill('Browser Tester');
  await page.getByRole('textbox',{name:'patch comment',exact:true}).fill('Preserve tuning — round trip');
  const toggle=page.getByRole('checkbox',{name:'Save Tuning',exact:true});
  if((await toggle.getAttribute('aria-checked'))!==String(includeTuning))await toggle.dispatchEvent('click');
  await expect(toggle).toHaveAttribute('aria-checked',String(includeTuning));
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Patches/Export Tests/Saved Tuning.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const saved=await page.evaluate(path=>Array.from(Module.FS.readFile(path)),path);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await page.evaluate(()=>{
    window.exported=null;window.exportSuggestion=null;
    window.showSaveFilePicker=async options=>{exportSuggestion=options.suggestedName;return {createWritable:async()=>({write:async bytes=>{window.exported=Array.from(bytes);},close:async()=>{},abort:async()=>{}})};};
  });
  const dialog=await openFiles(page);
  await dialog.getByRole('button',{name:'Open folder Patches',exact:true}).click();
  await dialog.getByRole('button',{name:'Open folder Export Tests',exact:true}).click();
  await dialog.getByRole('button',{name:'Download Saved Tuning.fxp',exact:true}).click();
  await expect(dialog.getByRole('status')).toHaveText('Downloaded Saved Tuning.fxp');
  expect(await page.evaluate(()=>exportSuggestion)).toBe('Saved Tuning.fxp');
  expect(await page.evaluate(()=>exported)).toEqual(saved);
  const metadata=await page.evaluate(()=>{
    const bytes=new Uint8Array(exported),size=new DataView(bytes.buffer).getUint32(64,true);
    const xml=new DOMParser().parseFromString(new TextDecoder().decode(bytes.slice(92,92+size)).replace(/\0+$/,''),'text/xml');
    const tuning=xml.querySelector('patchTuning'),meta=xml.querySelector('meta');
    return {author:meta.getAttribute('author'),comment:meta.getAttribute('comment'),tuning:tuning?{scale:atob(tuning.getAttribute('v')),mapping:atob(tuning.getAttribute('m'))}:null};
  });
  expect(metadata.author).toBe('Browser Tester');expect(metadata.comment).toBe('Preserve tuning — round trip');
  expect(metadata.tuning).toEqual(includeTuning?{scale,mapping}:null);
  await dialog.getByRole('button',{name:'Close user files',exact:true}).click();
  for(const kind of ['Tuning','Mapping']){
    await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Tuning on Patch Load',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:`Override With Embedded ${kind} if Available`,exact:true}).dispatchEvent('click');
  }
  await page.evaluate(()=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(exported)],'Exported Roundtrip.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  });
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Exported Roundtrip');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
  const current=await page.getByRole('textbox',{name:'Scala Scale',exact:true}).inputValue();
  if(includeTuning){
    expect(current).toContain('Saved Five');
    await expect(page.getByRole('textbox',{name:'Keyboard Mapping',exact:true})).toHaveValue(/432\.0/);
  }else expect(current).not.toContain('Saved Five');
});

test('user file export cancellation and write failure retain bytes and allow retry',async({page})=>{
  await start(page);
  await page.evaluate(()=>{Module.FS.mkdirTree('/user/Export Checks');Module.FS.writeFile('/user/Export Checks/retained.txt','Keep this work');});
  const dialog=await openFiles(page);await dialog.getByRole('button',{name:'Open folder Export Checks',exact:true}).click();
  const download=dialog.getByRole('button',{name:'Download retained.txt',exact:true});
  await page.evaluate(()=>{window.showSaveFilePicker=async()=>{throw new DOMException('Canceled','AbortError');};});
  await download.click();await expect(dialog.getByRole('status')).toContainText('Download canceled');
  await page.evaluate(()=>{window.aborted=false;window.showSaveFilePicker=async()=>({createWritable:async()=>({write:async()=>{throw Error('Disk full');},close:async()=>{},abort:async()=>{aborted=true;}})});});
  await download.click();await expect(dialog.getByRole('alert')).toContainText('Disk full');expect(await page.evaluate(()=>aborted)).toBe(true);
  expect(await page.evaluate(()=>Module.FS.readFile('/user/Export Checks/retained.txt',{encoding:'utf8'}))).toBe('Keep this work');
  await page.evaluate(()=>{window.exported='';window.showSaveFilePicker=async()=>({createWritable:async()=>({write:async bytes=>{exported=new TextDecoder().decode(bytes);},close:async()=>{},abort:async()=>{}})});});
  await download.click();await expect(dialog.getByRole('status')).toHaveText('Downloaded retained.txt');
  expect(await page.evaluate(()=>exported)).toBe('Keep this work');await expect(dialog.getByRole('alert')).toBeEmpty();
  await dialog.getByRole('button',{name:'Parent folder',exact:true}).click();await expect(dialog.getByRole('button',{name:'Parent folder',exact:true})).toBeDisabled();
  await page.keyboard.press('Escape');await expect(dialog).toHaveCount(0);await expect(page.getByRole('button',{name:'User files',exact:true})).toBeFocused();
});

test('export captures selected bytes before the asynchronous picker and reports permission denial',async({page})=>{
  await start(page);
  await page.evaluate(()=>{
    Module.FS.writeFile('/user/version.txt','Selected version');
    window.showSaveFilePicker=()=>new Promise(resolve=>{window.finishPicker=()=>resolve({createWritable:async()=>({write:async bytes=>{window.exported=new TextDecoder().decode(bytes);},close:async()=>{},abort:async()=>{}})});});
  });
  const dialog=await openFiles(page),download=dialog.getByRole('button',{name:'Download version.txt',exact:true});
  await download.click();await expect(download).toBeDisabled();
  await page.evaluate(()=>{Module.FS.writeFile('/user/version.txt','Newer version');finishPicker();});
  await expect(dialog.getByRole('status')).toHaveText('Downloaded version.txt');
  expect(await page.evaluate(()=>exported)).toBe('Selected version');
  expect(await page.evaluate(()=>Module.FS.readFile('/user/version.txt',{encoding:'utf8'}))).toBe('Newer version');
  await page.evaluate(()=>{window.showSaveFilePicker=async()=>{throw new DOMException('Permission denied','NotAllowedError');};});
  await download.click();await expect(dialog.getByRole('alert')).toContainText('Permission denied');
  expect(await page.evaluate(()=>Module.FS.readFile('/user/version.txt',{encoding:'utf8'}))).toBe('Newer version');
});
