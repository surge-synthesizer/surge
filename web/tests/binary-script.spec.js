import {test,expect} from './fixtures.js';
import {mkdtempSync,rmSync,readFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
async function drop(page,bytes){
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'snapshot.wtscript'));
    const rect=canvas.getBoundingClientRect();
    canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:rect.left+10,clientY:rect.top+10}));
  },Array.from(bytes));
}
for(const active of [false,true])test(`desktop binary scripts retain inputs through selection undo, export and reload with audio ${active?'running':'inactive'}`,async({page})=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-binary-script-'));
  let bytes;
  try{
    const harness=fileURLToPath(new URL('../../build-reference/src/surge-web/surge-engine-reference',import.meta.url));
    const output=execFileSync(harness,['--check-script-import',directory],{encoding:'utf8'});
    expect(output).toContain('Binary script import retained exact snapshot samples');
    bytes=readFileSync(path.join(directory,'snapshot.wtscript'));
  }finally{rmSync(directory,{recursive:true,force:true});}
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await drop(page,bytes);
  const name=()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]));
  await expect.poll(name).toBe('Snapshot import');
  if(active){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  const samples=()=>page.evaluate(()=>Array.from({length:64},(_,i)=>Module._surge_browser_wt_sample(0,0,i)));
  const expected=Array(64).fill(0);expected[16]=0.5;
  expect(await samples()).toEqual(expected);
  await drop(page,Buffer.from('<wtscript><broken/>'));
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  const editor=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await expect(editor).toHaveValue(/wt.snapshot\[1\]\[1\]/);
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Sine to Triangle',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(50);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(name).toBe('Snapshot import');
  await expect(editor).toHaveValue(/wt.snapshot\[1\]\[1\]/);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(50);
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Import Wavetable Data',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem',{name:/^Clear Snapshot [12]/})).toHaveCount(0);
  await page.keyboard.press('Escape');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(name).toBe('Snapshot import');
  await expect(editor).toHaveValue(/wt.snapshot\[1\]\[1\]/);
  // A new name proves the old snapshot inputs were used by a fresh generation.
  await editor.fill((await editor.inputValue()).replace('Snapshot import','Regenerated snapshot'));
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(name).toBe('Regenerated snapshot');
  expect(await samples()).toEqual(expected);
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Save as .wtscript...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Binary Snapshot');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Wavetables/Scripted/Binary Snapshot.wtscript').exists)).toBe(true);
  const saved=await page.evaluate(()=>Array.from(Module.FS.readFile('/user/Wavetables/Scripted/Binary Snapshot.wtscript')));
  expect(Buffer.from(saved).subarray(0,4).toString()).toBe('wts1');
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await page.evaluate(()=>Array.from(Module.FS.readFile('/user/Wavetables/Scripted/Binary Snapshot.wtscript')))).toEqual(saved);
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await page.evaluate(bytes=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([new Uint8Array(bytes)],'Binary Snapshot.wtscript')}];},saved);
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Load .wtscript...',exact:true}).dispatchEvent('click');
  await expect.poll(name).toBe('Regenerated snapshot');
  expect(await samples()).toEqual(expected);
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(/wt.snapshot\[1\]\[1\]/);
});
