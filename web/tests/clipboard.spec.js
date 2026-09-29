import {test,expect} from './fixtures.js';
const pageErrors=new WeakMap();
test.beforeEach(({page})=>{const errors=[];pageErrors.set(page,errors);page.on('pageerror',error=>errors.push(error.message));});
test.afterEach(({page})=>expect(pageErrors.get(page)).toEqual([]));
const editorText=(page,code)=>page.evaluate(code=>Module.ccall('surge_check_editor_text','string',['number'],[code]),code);
async function open(page,code) {
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_timer_callbacks())).toBeGreaterThan(2);
  await page.mouse.click(code?450:120,280);
  await page.keyboard.press('Control+a');
}
for(const code of [0,1]) {
  test(`JUCE ${code?'code':'text'} editor pastes Unicode and preserves undo/redo`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=> 'local value = "日本語 ✓"',writeText:async text=>{globalThis.copiedText=text}}}));
    await open(page,code);
    const before=await editorText(page,code);
    await page.keyboard.press('Control+v');
    await expect.poll(()=>editorText(page,code)).toBe('local value = "日本語 ✓"');
    await page.keyboard.press('Control+z');
    await expect.poll(()=>editorText(page,code)).toBe(before);
    await page.keyboard.press('Control+Shift+z');
    await expect.poll(()=>editorText(page,code)).toBe('local value = "日本語 ✓"');
    await page.keyboard.press('Control+a');await page.keyboard.press('Control+c');
    await expect.poll(()=>page.evaluate(()=>globalThis.copiedText)).toBe('local value = "日本語 ✓"');
    await page.keyboard.press('Control+x');
    await expect.poll(()=>editorText(page,code)).toBe('');
    await page.keyboard.press('Control+z');
    await expect.poll(()=>editorText(page,code)).toBe('local value = "日本語 ✓"');
  });
  test(`JUCE ${code?'code':'text'} clipboard failure retains text and supports retry`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=>{throw Error('Permission denied')},writeText:async()=>{throw Error('Permission denied')}}}));
    await open(page,code);const before=await editorText(page,code);
    await page.keyboard.press('Control+x');
    await expect(page.locator('#clipboard-status')).toContainText('Unable to copy');
    expect(await editorText(page,code)).toBe(before);
    await page.keyboard.press('Control+v');
    await expect(page.locator('#clipboard-status')).toContainText('Unable to paste');
    expect(await editorText(page,code)).toBe(before);
    await page.evaluate(()=>navigator.clipboard.readText=async()=> 'retry succeeded');
    await page.keyboard.press('Control+v');
    await expect.poll(()=>editorText(page,code)).toBe('retry succeeded');
  });
  test(`JUCE ${code?'code':'text'} editor rejects paste after an intervening edit`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:()=>new Promise(resolve=>{globalThis.finishPaste=resolve})}}));
    await open(page,code);await page.keyboard.press('Control+v');
    await expect.poll(()=>page.evaluate(()=>typeof finishPaste)).toBe('function');
    await page.keyboard.type('keep my edit');
    await page.evaluate(()=>finishPaste('stale paste'));
    await expect(page.locator('#clipboard-status')).toContainText('editor changed');
    expect(await editorText(page,code)).toBe('keep my edit');
  });
}
for(const code of [0,1]) {
  test(`JUCE ${code?'code':'text'} editor defers cut until write succeeds and retains newer edits`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{writeText:()=>new Promise(resolve=>{globalThis.finishCopy=resolve})}}));
    await open(page,code);const before=await editorText(page,code);
    await page.keyboard.press('Control+x');
    await expect.poll(()=>page.evaluate(()=>typeof finishCopy)).toBe('function');
    expect(await editorText(page,code)).toBe(before);
    await page.keyboard.type('new edit');
    await page.evaluate(()=>finishCopy());
    await expect(page.locator('#clipboard-status')).toContainText('cut canceled');
    expect(await editorText(page,code)).toBe('new edit');
  });
  test(`JUCE ${code?'code':'text'} editor discards a paste if made read-only while awaiting permission`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:()=>new Promise(resolve=>{globalThis.finishPaste=resolve})}}));
    await open(page,code);const before=await editorText(page,code);
    await page.keyboard.press('Control+v');
    await expect.poll(()=>page.evaluate(()=>typeof finishPaste)).toBe('function');
    await page.evaluate(code=>{Module._surge_check_editor_readonly(code,1);finishPaste('must not insert')},code);
    await expect(page.locator('#clipboard-status')).toContainText('editor changed');
    expect(await editorText(page,code)).toBe(before);
  });
}
test('large clipboard scripts use heap transfer and remain one undo transaction',async({page})=>{
  const script='local value = "✓"\n'.repeat(5000);
  await page.addInitScript(script=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=>script}}),script);
  await open(page,1);await page.keyboard.press('Control+v');
  await expect.poll(()=>editorText(page,1)).toBe(script);
  await page.keyboard.press('Control+z');
  await expect.poll(()=>editorText(page,1)).toBe('return 1');
});
for(const code of [0,1]) {
  test(`JUCE ${code?'code':'text'} context menu pastes through the asynchronous clipboard`,async({page})=>{
    await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=> 'context menu paste'}}));
    await open(page,code);
    const x=code?450:120;
    await page.mouse.click(x,280,{button:'right'});
    await expect(page.locator('canvas')).toHaveCount(2);
    await page.waitForTimeout(350); // JUCE popup release guard.
    await page.mouse.click(x+35,336);
    await expect.poll(()=>editorText(page,code)).toBe('context menu paste');
    await expect(page.locator('canvas')).toHaveCount(1);
  });
}
test('clipboard paste in the original Surge search field finds and loads a factory patch',async({page})=>{
  await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=> 'Init FM2'}}));
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
  await page.mouse.click(165,20);
  await page.keyboard.press('Control+v');
  // The query opens Surge's original results list underneath the search field.
  await expect(page.getByRole('textbox',{name:'Patch select',exact:true})).toHaveValue('Init FM2');
  await expect(page.getByRole('list',{name:'Patch select',exact:true}).getByRole('listitem')).toHaveCount(1);
  await expect(page.getByRole('listitem',{name:'Init FM2 in Templates',exact:true})).toBeAttached();
  await page.mouse.click(250,50);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
});
test('a failed search-result download retains the patch and allows the same result to be retried',async({page})=>{
  await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{readText:async()=> 'Init FM2'}}));
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
  await page.route('**/library/objects/**',route=>route.fulfill({status:503,body:'Unavailable'}));
  async function selectResult(retry=false) {
    // A chosen result hides the list but retains the search field/query.
    // On retry, focus that field and change the query to reopen the results.
    await page.mouse.click(retry?350:165,retry?25:20);
    const search=page.getByRole('textbox',{name:'Patch select',exact:true});
    await page.keyboard.press('Control+a');await page.keyboard.press('Backspace');
    await expect(search).toHaveValue('');
    await page.keyboard.press('Control+v');
    await expect(search).toHaveValue('Init FM2');
    await expect(page.getByRole('list',{name:'Patch select',exact:true}).getByRole('listitem')).toHaveCount(1);
    await expect(page.getByRole('listitem',{name:'Init FM2 in Templates',exact:true})).toBeAttached();
    await page.mouse.click(250,50);
  }
  await selectResult();
  await expect(page.locator('#file-status')).toContainText('current patch retained');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.unroute('**/library/objects/**');
  await selectResult(true);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
});
