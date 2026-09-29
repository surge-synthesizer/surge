import {test,expect} from './fixtures.js';
import fs from 'node:fs';
import {fileURLToPath} from 'node:url';
const base='/factory/wavetables/Basic/';
const source=fs.readFileSync(fileURLToPath(new URL('../../resources/data/wavetables/Basic/Sine.wt',import.meta.url)));
async function start(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
}
async function select(page,name){
  expect(await page.evaluate(path=>Module.ccall('surge_browser_request_wt','number',['number','string'],[0,path]),base+name+'.wt')).toBe(1);
}
async function name(page){return page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]));}
async function snapshot(page){return page.evaluate(()=>{
  const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
  return {size,frames,samples:Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/size),i%size))};
});}
test('factory wavetable selection prepares and publishes exact samples on demand',async({page})=>{
  const downloads=[];page.on('request',r=>{if(r.url().includes('/library/objects/'))downloads.push(r.url())});
  await start(page);
  expect(await page.evaluate(()=>Module._surge_browser_wt_count())).toBe(931);
  const before=downloads.length;
  await select(page,'Sine');await expect.poll(()=>name(page)).toBe('Sine');
  expect(downloads).toHaveLength(before+1);
  const table=await snapshot(page);
  expect(table.size).toBe(64);expect(table.frames).toBe(16);
  expect(table.samples).toEqual(Array.from({length:1024},(_,i)=>source.readInt16LE(12+i*2)/16384));
});
test('failed and malformed wavetable loads preserve the live sample data',async({page})=>{
  await start(page);await select(page,'Sine');await expect.poll(()=>name(page)).toBe('Sine');
  const before=await snapshot(page);
  await page.route('**/library/objects/**',route=>route.fulfill({status:503,body:'Unavailable'}));
  await select(page,'Sine To Sawtooth');
  await expect(page.locator('#file-status')).toContainText('current table retained');
  expect(await snapshot(page)).toEqual(before);expect(await name(page)).toBe('Sine');
  await page.unroute('**/library/objects/**');
  await page.evaluate(()=>{Module.FS.writeFile('/user/invalid.wt','bad');Module.ccall('surge_browser_request_wt_file','number',['number','string'],[0,'/user/invalid.wt']);});
  await expect(page.locator('#file-status')).toContainText('could not be decoded');
  expect(await snapshot(page)).toEqual(before);expect(await name(page)).toBe('Sine');
});
test('older wavetable downloads cannot replace a newer selection',async({page})=>{
  await start(page);
  const first=await page.evaluate(path=>SurgeFactory.library.entries.get(path).url,'wavetables/Basic/Sine To Sawtooth.wt');
  let release,seen;const held=new Promise(r=>{release=r}),requested=new Promise(r=>{seen=r});
  await page.route('**/library/'+first,async route=>{seen();await held;await route.continue()});
  await select(page,'Sine To Sawtooth');await requested;
  await select(page,'Sine');await expect.poll(()=>name(page)).toBe('Sine');
  release();await expect.poll(()=>page.evaluate(()=>SurgeFactory.library.pending.size)).toBe(0);
  expect(await name(page)).toBe('Sine');
});
test('wavetable reslicing preserves source samples through the prepared-data handoff',async({page})=>{
  await start(page);await select(page,'Sine');await expect.poll(()=>name(page)).toBe('Sine');
  const before=await snapshot(page);
  expect(await page.evaluate(()=>Module._surge_browser_reslice_wt(0,128,8))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_size(0))).toBe(128);
  const after=await snapshot(page);expect(after.frames).toBe(8);expect(after.samples).toEqual(before.samples);
});
for(const active of [false,true])test(`wavetable selection undo restores a resliced table without its factory file with audio ${active?'running':'inactive'}`,async({page})=>{
  await start(page);await select(page,'Sine');await expect.poll(()=>name(page)).toBe('Sine');
  if(active){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  expect(await page.evaluate(()=>Module._surge_browser_reslice_wt(0,128,8))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_size(0))).toBe(128);
  const before=await snapshot(page);expect(before.frames).toBe(8);
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Basic',exact:true}).or(page.getByRole('menuitemcheckbox',{name:'Basic (Checked)',exact:true})).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Triangle',exact:true}).dispatchEvent('click');
  await expect.poll(()=>name(page)).toBe('Triangle');
  const after=await snapshot(page);expect(after).not.toEqual(before);
  await page.evaluate(()=>Module.FS.unlink('/factory/wavetables/Basic/Sine.wt'));
  const downloads=[];
  await page.route('**/library/objects/**',route=>{downloads.push(route.request().url());return route.abort();});
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>snapshot(page)).toEqual(before);
  await expect.poll(()=>name(page)).toBe('Sine');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>snapshot(page)).toEqual(after);
  await expect.poll(()=>name(page)).toBe('Triangle');
  expect(downloads).toEqual([]);
  expect(await page.evaluate(()=>Module.FS.analyzePath('/factory/wavetables/Basic/Sine.wt').exists)).toBe(false);
});
test('a patch change invalidates a pending wavetable replacement',async({page})=>{
  await start(page);
  const asset=await page.evaluate(()=>SurgeFactory.library.entries.get('wavetables/Basic/Sine To Sawtooth.wt').url);
  let release,seen;const held=new Promise(r=>{release=r}),requested=new Promise(r=>{seen=r});
  await page.route('**/library/'+asset,async route=>{seen();await held;await route.continue()});
  await select(page,'Sine To Sawtooth');await requested;
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init FM2.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  const before=await snapshot(page);
  release();await expect.poll(()=>page.evaluate(()=>SurgeFactory.library.pending.size)).toBe(0);
  expect(await snapshot(page)).toEqual(before);
});
