import {test,expect} from './fixtures.js';

async function open(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();
  await page.keyboard.press('Alt+w');
  const editor=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await expect(editor).toBeAttached();
  return editor;
}

for(const modifier of ['Meta','Control'])
test(`accessible Lua editor preserves native indentation and duplicate shortcuts with ${modifier}`,async({page})=>{
  const editor=await open(page);
  await editor.fill('local x = 1');
  await editor.press('Meta+a');
  await editor.press('Tab');
  await expect(editor).toHaveValue('    local x = 1');
  await expect(editor).toBeFocused();
  await editor.press('Shift+Tab');
  await expect(editor).toHaveValue('local x = 1');
  await editor.press('Meta+a');
  await editor.press(modifier+'+d');
  await expect(editor).toHaveValue('local x = 1local x = 1');
});

test('accessible Lua editor applies the preview with the native command shortcut',async({page})=>{
  const editor=await open(page);
  await editor.fill('function init(wt) wt.name = "Shortcut Generated" return wt end\nfunction generate(wt) local r = {} for i = 1, wt.sample_count do r[i] = 0.125 end return r end');
  const apply=page.getByRole('button',{name:'Apply',exact:true});
  await expect(apply).toBeEnabled();
  await editor.press('Meta+Enter');
  await expect(apply).toBeDisabled();
  expect(await page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('Sine HQ');
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('Shortcut Generated');
  expect(await page.evaluate(()=>Module._surge_browser_wt_sample(0,0,0))).toBe(0.125);
});

for(const [key,value,selection] of [['f','second',[6,12]],['g','2',[12,12]]])
test(`accessible Lua editor opens native ${key==='f'?'search':'go to line'} and moves the code selection`,async({page})=>{
  const editor=await open(page);
  await editor.fill('first\nsecond\nthird');
  await editor.press('Meta+'+key);
  const field=page.locator('textarea:focus');
  await expect(field).toHaveCount(1);
  await expect(field).toHaveAttribute('aria-label',key==='f'?'Find':'Go to line');
  await field.fill(value);
  if(key==='g')await field.press('Enter');
  await expect.poll(()=>editor.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual(selection);
  await expect(editor).toHaveValue('first\nsecond\nthird');
});

test('accessible Lua editor opens native replace and replaces a found occurrence',async({page})=>{
  const editor=await open(page);
  await editor.fill('first\nsecond\nthird');
  await editor.press('Meta+f');
  await page.locator('textarea:focus').fill('second');
  await expect.poll(()=>editor.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual([6,12]);
  await editor.focus();
  await editor.press('Meta+h');
  const replacement=page.locator('textarea:focus');
  await expect(replacement).toHaveCount(1);
  await expect(replacement).toHaveAttribute('aria-label','Replace');
  await replacement.fill('replacement');
  await replacement.press('Enter');
  await expect(editor).toHaveValue('first\nreplacement\nthird');
});

test('accessible Lua code preserves automatic newline indentation and tab-stop deletion',async({page})=>{
  const editor=await open(page);
  await editor.fill('function test()');
  await editor.press('Enter');
  await expect(editor).toHaveValue('function test()\n    ');
  await editor.press('Backspace');
  await expect(editor).toHaveValue('function test()\n');
});

for(const pair of ['()','[]','{}','""',"''"])
test(`accessible Lua code pairs ${pair}, skips closing delimiters and deletes empty pairs`,async({page})=>{
  const editor=await open(page);
  await editor.fill(' ');
  await editor.press('ArrowLeft');
  await editor.press(pair[0]);
  await expect(editor).toHaveValue(pair+' ');
  await editor.press('Meta+z');
  await expect(editor).toHaveValue(' ');
  await editor.press('Meta+Shift+z');
  await expect(editor).toHaveValue(pair+' ');
  // JUCE redo follows the inserted range to its end. Return inside the pair
  // before checking the native skip-closing-delimiter behavior.
  expect(await editor.evaluate(node=>node.selectionStart)).toBe(2);
  await editor.press('ArrowLeft');
  await editor.press(pair[1]);
  await expect(editor).toHaveValue(pair+' ');
  expect(await editor.evaluate(node=>node.selectionStart)).toBe(2);
  await editor.press('ArrowLeft');
  await editor.press('Backspace');
  await expect(editor).toHaveValue(' ');
  await editor.fill('word');
  await editor.press('Meta+a');
  await editor.press(pair[0]);
  await expect(editor).toHaveValue(pair[0]+'word'+pair[1]);
  expect(await editor.evaluate(node=>[node.selectionStart,node.selectionEnd])).toEqual([1,5]);
});

test('accessible Lua delimiter handling preserves literal replacements and IME composition',async({page})=>{
  const editor=await open(page);
  await editor.fill('literal () [] {} "quoted"');
  await expect(editor).toHaveValue('literal () [] {} "quoted"');
  await editor.fill('original');
  await editor.press('Meta+a');
  const cdp=await page.context().newCDPSession(page);
  await cdp.send('Input.imeSetComposition',{text:'(',selectionStart:1,selectionEnd:1});
  await expect(editor).toHaveValue('(');
  await cdp.send('Input.insertText',{text:'('});
  await expect(editor).toHaveValue('(');
  await editor.press('Meta+z');
  await expect(editor).toHaveValue('original');
  await editor.press('Meta+Shift+z');
  await expect(editor).toHaveValue('(');
});

for(const action of ['Find...','Replace...','Go to Line...'])
test(`Lua context menu ${action} reaches the native editor operation`,async({page})=>{
  const editor=await open(page);
  await editor.fill('first\nsecond\nthird');
  const bounds=await editor.boundingBox();expect(bounds).not.toBeNull();
  await page.mouse.click(bounds.x+50,bounds.y+12,{button:'right'});
  await page.getByRole('menuitem',{name:action,exact:true}).dispatchEvent('click');
  const field=page.getByRole('textbox',{name:action==='Go to Line...'?'Go to line':'Find',exact:true});
  await field.fill(action==='Go to Line...'?'2':'second');
  if(action==='Go to Line...')await field.press('Enter');
  else await expect.poll(()=>editor.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual([6,12]);
  if(action==='Replace...'){
    const replacement=page.getByRole('textbox',{name:'Replace',exact:true});
    await replacement.fill('日本語 replacement');await replacement.press('Enter');
    await expect(editor).toHaveValue('first\n日本語 replacement\nthird');
  }else{
    if(action==='Go to Line...')await expect.poll(()=>editor.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual([12,12]);
    await expect(editor).toHaveValue('first\nsecond\nthird');
  }
});
