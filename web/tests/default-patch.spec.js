import {test,expect} from './fixtures.js';
const name=page=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]));
const pitch=page=>page.getByRole('slider',{name:'Scene A Osc 1 Pitch',exact:true});
async function setDefault(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Set Current Patch as Default',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menu')).toHaveCount(0);
  await page.evaluate(()=>SurgeBrowser.flush());
}
async function select(page,path){
  expect(await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),path)).toBe(1);
}
async function ready(page){await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);}

test('startup default distinguishes same-named factory and user patches and survives reload',async({page})=>{
  test.setTimeout(60000);await page.goto('/surge-xt-browser.html');await ready(page);
  const factory='/factory/patches_factory/Templates/Init Sine.fxp';
  await select(page,factory);await expect.poll(()=>name(page)).toBe('Init Sine');
  await expect(pitch(page)).toHaveAttribute('aria-valuenow','0.5');
  await setDefault(page);await page.reload();await ready(page);
  await expect.poll(()=>name(page)).toBe('Init Sine');await expect(pitch(page)).toHaveAttribute('aria-valuenow','0.5');
  await pitch(page).focus();await page.keyboard.press('End');await expect(pitch(page)).toHaveAttribute('aria-valuenow','1');
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Init Sine');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Templates');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const user='/user/Patches/Templates/Init Sine.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,user)).toBe(true);
  await select(page,factory);await expect(pitch(page)).toHaveAttribute('aria-valuenow','0.5');
  // Select the indexed saved patch explicitly before choosing it as default.
  await select(page,user);await expect(pitch(page)).toHaveAttribute('aria-valuenow','1');
  await setDefault(page);await page.reload();await ready(page);
  await expect.poll(()=>name(page)).toBe('Init Sine');await expect(pitch(page)).toHaveAttribute('aria-valuenow','1');
  await select(page,factory);await expect(pitch(page)).toHaveAttribute('aria-valuenow','0.5');
  await setDefault(page);await page.reload();await ready(page);
  await expect.poll(()=>name(page)).toBe('Init Sine');await expect(pitch(page)).toHaveAttribute('aria-valuenow','0.5');
  // Restore the original factory default using the same native command.
  await select(page,'/factory/patches_factory/Templates/Init Saw.fxp');await expect.poll(()=>name(page)).toBe('Init Saw');
  await setDefault(page);await page.reload();await ready(page);await expect.poll(()=>name(page)).toBe('Init Saw');
});
