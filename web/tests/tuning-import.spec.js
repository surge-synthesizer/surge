import {test,expect} from './fixtures.js';
const scale='! retained.scl\nBrowser retained scale\n5\n!\n240.0\n480.0\n720.0\n960.0\n2/1\n';
const mapping='! retained.kbm\n0\n0\n127\n60\n69\n432.0\n0\n';
async function importFile(page,extension,text){
  await page.evaluate(({extension,text})=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([text],'test.'+extension)}];},{extension,text});
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Tuning',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:extension==='scl'?'Load .scl Tuning...':'Load .kbm Keyboard Mapping...',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menu')).toHaveCount(0);
}
async function state(page){
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
  const scale=page.getByRole('textbox',{name:'Scala Scale',exact:true});
  await expect(scale).toBeAttached();
  const result={scale:await scale.inputValue(),mapping:await page.getByRole('textbox',{name:'Keyboard Mapping',exact:true}).inputValue()};
  await page.getByRole('button',{name:'Export HTML',exact:true}).dispatchEvent('click');
  result.report=await page.frameLocator('#surge-report iframe').locator('body').innerText();
  await page.getByRole('button',{name:'Close report',exact:true}).click();
  await page.getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  // The JUCE accessibility snapshot is refreshed asynchronously. Do not let
  // reopening the editor resolve controls from its retired instance.
  await expect(scale).toHaveCount(0);
  return result;
}
for(const extension of ['scl','kbm'])test(`invalid ${extension} import preserves the current scale, mapping and pitch report`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await importFile(page,'scl',scale);await importFile(page,'kbm',mapping);
  const before=await state(page);expect(before.scale).toContain('Browser retained scale');expect(before.mapping).toContain('432');
  await importFile(page,extension,'invalid tuning data\n');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await state(page)).toEqual(before);
});

async function drop(page,extension,text,fail=false){
  await page.locator('canvas').first().evaluate((canvas,{extension,text,fail})=>{
    const transfer=new DataTransfer();const file=new File([text],'dropped.'+extension);
    if(fail)file.arrayBuffer=async()=>{throw Error('Dropped file unreadable');};
    transfer.items.add(file);const bounds=canvas.getBoundingClientRect();
    canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:bounds.left+10,clientY:bounds.top+10}));
  },{extension,text,fail});
}
for(const extension of ['scl','kbm'])test(`invalid dropped ${extension} retains the current tuning through the JUCE file-drop path`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await importFile(page,'scl',scale);await importFile(page,'kbm',mapping);
  const before=await state(page);
  await drop(page,extension,'invalid tuning data\n');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await state(page)).toEqual(before);
});
test('valid dropped tuning files reach the original editor',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await drop(page,'scl',scale);
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
  await expect(page.getByRole('textbox',{name:'Scala Scale',exact:true})).toHaveValue(/Browser retained scale/);
  await page.getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await drop(page,'kbm',mapping);
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
  await expect(page.getByRole('textbox',{name:'Keyboard Mapping',exact:true})).toHaveValue(/432.0/);
});
test('file-drop read failures report an error without changing tuning',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  const before=await state(page);await drop(page,'scl',scale,true);
  await expect(page.locator('#file-status')).toContainText('Dropped file unreadable');
  expect(await state(page)).toEqual(before);
});
