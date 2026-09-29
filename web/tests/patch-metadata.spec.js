import {test,expect} from './fixtures.js';
const author='Browser Author Ω',comment='Default notes — café';
const appendLabel='Append Original Author Name to Modified Patches';
async function ready(page){await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);}
async function menu(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
}
async function editDefault(page,kind,value,cancel=false){
  await menu(page);await page.getByRole('menuitem',{name:`Set Default Patch ${kind}...`,exact:true}).dispatchEvent('click');
  const field=page.getByRole('textbox',{name:'Value',exact:true});await field.fill(value);
  await page.getByRole('button',{name:cancel?'Cancel':'OK',exact:true}).dispatchEvent('click');await expect(field).toHaveCount(0);
}
async function checkDefault(page,kind,value){
  await menu(page);await page.getByRole('menuitem',{name:`Set Default Patch ${kind}...`,exact:true}).dispatchEvent('click');
  const field=page.getByRole('textbox',{name:'Value',exact:true});await expect(field).toHaveValue(value);
  await page.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');await expect(field).toHaveCount(0);
}
async function reload(page){await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready(page);}
async function initSine(page){
  expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Sine.fxp']))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
}

test('patch metadata defaults persist, cancel safely and honor original-author attribution on save',async({page})=>{
  test.setTimeout(60000);await page.goto('/surge-xt-browser.html');await ready(page);
  await editDefault(page,'Author',author);await editDefault(page,'Comment',comment);await reload(page);
  await checkDefault(page,'Author',author);await checkDefault(page,'Comment',comment);
  await editDefault(page,'Author','Discarded author',true);await editDefault(page,'Comment','Discarded comment',true);
  await reload(page);await checkDefault(page,'Author',author);await checkDefault(page,'Comment',comment);
  for(const [caseIndex,append] of [true,false,true].entries()){
    await menu(page);
    const checked=page.getByRole('menuitemcheckbox',{name:appendLabel+' (Checked)',exact:true});
    if((await checked.count())!==Number(append)){
      const item=append?page.getByRole('menuitem',{name:appendLabel,exact:true}):checked;
      await item.dispatchEvent('click');
    }else{await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);}
    await reload(page);await menu(page);
    if(append)await expect(page.getByRole('menuitemcheckbox',{name:appendLabel+' (Checked)',exact:true})).toHaveAttribute('aria-checked','true');
    else await expect(page.getByRole('menuitem',{name:appendLabel,exact:true})).toBeAttached();
    await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
    await initSine(page);await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('textbox',{name:'patch author',exact:true})).toHaveValue(author);
    const expectedComment=comment+(append?' (Original patch by Surge Synth Team)':'');
    await expect(page.getByRole('textbox',{name:'patch comment',exact:true})).toHaveValue(expectedComment);
    const license=await page.getByRole('textbox',{name:'patch license',exact:true}).inputValue();
    expect(license).toContain('CC0');
    // Canceling a save must leave the source patch and its metadata untouched.
    await page.getByRole('textbox',{name:'patch author',exact:true}).fill('Unsaved author');
    await page.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('textbox',{name:'patch author',exact:true})).toHaveCount(0);
    await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('textbox',{name:'patch author',exact:true})).toHaveValue(author);
    await expect(page.getByRole('textbox',{name:'patch comment',exact:true})).toHaveValue(expectedComment);
    const name='Metadata '+caseIndex+' '+(append?'Attributed':'Plain');
    await page.getByRole('textbox',{name:'patch name',exact:true}).fill(name);
    await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Metadata Tests');
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    const path='/user/Patches/Metadata Tests/'+name+'.fxp';
    await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
    const actual=await page.evaluate(path=>{
      const bytes=Module.FS.readFile(path),n=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getUint32(64,true);
      const doc=new DOMParser().parseFromString(new TextDecoder().decode(bytes.slice(92,92+n)).replace(/\0+$/,''),'text/xml');
      const meta=doc.querySelector('meta');return Object.fromEntries(['author','comment','license'].map(key=>[key,meta.getAttribute(key)]));
    },path);
    expect(actual).toEqual({author,comment:expectedComment,license});
  }
  await editDefault(page,'Author','');await editDefault(page,'Comment','');await reload(page);
  await checkDefault(page,'Author','');await checkDefault(page,'Comment','');
  await initSine(page);await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'patch author',exact:true})).toHaveValue('Surge Synth Team');
  await expect(page.getByRole('textbox',{name:'patch comment',exact:true})).toHaveValue('');
  await page.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');
});
