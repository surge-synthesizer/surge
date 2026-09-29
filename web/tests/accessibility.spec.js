import {test,expect} from './fixtures.js';
const pageErrors=new WeakMap();
test.beforeEach(({page})=>{const errors=[];pageErrors.set(page,errors);page.on('pageerror',error=>errors.push(error.message));});
test.afterEach(({page})=>expect(pageErrors.get(page)).toEqual([]));
async function start(page,path='/surge-juce-browser-check.html') {
  await page.goto(path);
  await expect(page.locator('#juce-accessibility [data-juce-accessible-id]').first()).toBeAttached();
}
test('JUCE labels and slider values reach the Chrome accessibility tree and keyboard edits reach JUCE',async({page})=>{
  await start(page);
  const slider=page.getByRole('slider',{name:'Verification value',exact:true});
  await expect(slider).toHaveAttribute('aria-valuenow','50');
  const cdp=await page.context().newCDPSession(page);
  const tree=await cdp.send('Accessibility.getFullAXTree');
  const accessible=tree.nodes.find(node=>node.role?.value==='slider'&&node.name?.value==='Verification value');
  expect(accessible).toBeTruthy();expect(accessible.value.value).toBe(50);
  await slider.focus();await page.keyboard.press('ArrowRight');
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_slider())).toBe(51);
  await expect(slider).toHaveAttribute('aria-valuenow','51');
  await page.keyboard.press('End');
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_slider())).toBe(100);
  await page.keyboard.press('Home');
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_slider())).toBe(0);
});
test('accessible button actions honor disabled and hidden state and discard retired identifiers',async({page})=>{
  await start(page);
  const button=page.getByRole('button',{name:'Test JUCE callback',exact:true});
  await button.focus();await page.keyboard.press('Enter');
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_clicks())).toBe(1);
  const id=Number(await button.getAttribute('data-juce-accessible-id'));
  await page.evaluate(()=>Module._surge_check_button_state(0,1));
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,0,0),id)).toBe(0);
  await expect(button).toHaveAttribute('aria-disabled','true');
  await page.evaluate(()=>Module._surge_check_button_state(1,0));
  await expect(button).toHaveCount(0);
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,0,0),id)).toBe(0);
  await page.evaluate(()=>Module._surge_check_button_state(1,1));
  await expect(button).toHaveCount(1);
  expect(Number(await button.getAttribute('data-juce-accessible-id'))).not.toBe(id);
  await button.dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_clicks())).toBe(2);
});
test('the original Surge editor exposes named controls through JUCE metadata',async({page})=>{
  await start(page,'/surge-xt-browser.html');
  const cdp=await page.context().newCDPSession(page);
  await expect.poll(async()=>{
    const tree=await cdp.send('Accessibility.getFullAXTree');
    return tree.nodes.filter(node=>node.role?.value==='slider'&&node.name?.value).length;
  }).toBeGreaterThan(20);
});

for(const code of [0,1]){
  const name=code?'Verification code':'Verification text';
  const nativeText=(page)=>page.evaluate(code=>Module.ccall('surge_check_editor_text','string',['number'],[code]),code);
  test(`accessible ${name} supports Unicode replacement, JUCE undo, and canvas handoff`,async({page})=>{
    await start(page);
    const editor=page.getByRole('textbox',{name,exact:true});
    await expect(editor).toBeAttached();
    const before=await nativeText(page);
    await editor.focus();
    await editor.fill('A😀B');
    await expect.poll(()=>nativeText(page)).toBe('A😀B');
    await page.keyboard.press('Control+z');
    await expect.poll(()=>nativeText(page)).toBe(before);
    await page.keyboard.press('Control+Shift+z');
    await expect(editor).toHaveValue('A😀B');
    await editor.evaluate(node=>node.setSelectionRange(1,3));
    await page.keyboard.insertText('🦄');
    await expect.poll(()=>nativeText(page)).toBe('A🦄B');
    await page.mouse.click(code?450:120,280);
    await expect.poll(()=>page.evaluate(()=>!!document.querySelector('canvas').editContext)).toBe(true);
    await page.keyboard.press('Control+a');await page.keyboard.insertText('canvas edit');
    await expect(editor).toHaveValue('canvas edit');
    await editor.focus();await page.keyboard.insertText('!');
    await expect.poll(()=>nativeText(page)).toBe('canvas edit!');
  });
  test(`accessible ${name} preserves one composition undo transaction and rejects stale or read-only edits`,async({page})=>{
    await start(page);
    const editor=page.getByRole('textbox',{name,exact:true});
    await editor.focus();
    const before=await nativeText(page);
    await page.keyboard.press(process.platform==='darwin'?'Meta+a':'Control+a');
    const cdp=await page.context().newCDPSession(page);
    await cdp.send('Input.imeSetComposition',{text:'にほん',selectionStart:3,selectionEnd:3});
    await expect.poll(()=>nativeText(page)).toBe('にほん');
    await page.waitForTimeout(1100);
    await cdp.send('Input.imeSetComposition',{text:'日本',selectionStart:2,selectionEnd:2});
    await cdp.send('Input.insertText',{text:'日本語'});
    await expect.poll(()=>nativeText(page)).toBe('日本語');
    await page.keyboard.press('Control+z');
    await expect.poll(()=>nativeText(page)).toBe(before);
    const id=Number(await editor.getAttribute('data-juce-accessible-id'));
    expect(await page.evaluate(id=>Module._surge_accessibility_text(id,Module.stringToNewUTF8('stale'),Module.stringToNewUTF8('bad'),3,3),id)).toBe(0);
    await page.evaluate(code=>Module._surge_check_editor_readonly(code,1),code);
    expect(await page.evaluate(({id,before})=>Module._surge_accessibility_text(id,Module.stringToNewUTF8(before),Module.stringToNewUTF8('bad'),3,3),{id,before})).toBe(0);
    await expect(editor).toHaveJSProperty('readOnly',true);
    expect(await nativeText(page)).toBe(before);
  });
}

test('the accessible original patch-search field drives factory results and patch loading',async({page})=>{
  await start(page,'/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
  await page.mouse.click(165,20);
  const search=page.getByRole('textbox',{name:'Patch select',exact:true});
  await search.fill('Init FM2');
  await expect.poll(()=>page.locator('#juce-accessibility [aria-description*="Init FM2"]').count()).toBeGreaterThan(0);
  await page.mouse.click(250,50);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
});

test('JUCE announcements reach Chrome live regions with priority, repeat, and literal text intact',async({page})=>{
  await start(page);
  const announce=(text,priority)=>page.evaluate(({text,priority})=>Module.ccall('surge_check_announce',null,['string','number'],[text,priority]),{text,priority});
  await announce('Patch ready 日本語',0);
  const polite=page.locator('#juce-announcement-polite');
  await expect(polite).toHaveText('Patch ready 日本語');
  await expect(polite).toHaveAttribute('aria-live','polite');
  await page.evaluate(()=>{
    globalThis.liveUpdates=[];
    new MutationObserver(()=>liveUpdates.push(document.getElementById('juce-announcement-polite').textContent))
      .observe(document.getElementById('juce-announcement-polite'),{childList:true,subtree:true});
  });
  await announce('Patch ready 日本語',1);
  await expect.poll(()=>page.evaluate(()=>liveUpdates)).toEqual(['','Patch ready 日本語']);
  await announce('<img src=x onerror=alert(1)>',2);
  const urgent=page.locator('#juce-announcement-assertive');
  await expect(urgent).toHaveText('<img src=x onerror=alert(1)>');
  expect(await urgent.locator('img').count()).toBe(0);
  const cdp=await page.context().newCDPSession(page);
  const tree=await cdp.send('Accessibility.getFullAXTree');
  expect(tree.nodes.some(node=>node.role?.value==='alert'&&node.properties?.some(property=>property.name==='live'&&property.value.value==='assertive'))).toBe(true);
  await page.evaluate(()=>{
    Module.ccall('surge_check_announce',null,['string','number'],['superseded',0]);
    Module.ccall('surge_check_announce',null,['string','number'],['latest',0]);
  });
  await expect(polite).toHaveText('latest');
});

for(const destination of [0,1]) test(`a delayed DOM composition end cannot end a newer canvas composition in editor ${destination}`,async({page})=>{
  await start(page);
  const editor=page.getByRole('textbox',{name:'Verification text',exact:true});
  await editor.focus();
  const id=Number(await editor.getAttribute('data-juce-accessible-id'));
  await page.evaluate(id=>Module._surge_accessibility_composition(id,1),id);
  expect(await page.evaluate(()=>Module._surge_check_composing(0))).toBe(1);
  await page.evaluate(ownerId=>{
    globalThis.compositionEnds=[];
    const original=Module._surge_accessibility_composition;
    globalThis.finishOldComposition=()=>original(ownerId,0);
    Module._surge_accessibility_composition=(id,begin)=>{
      if(!begin&&id===ownerId){compositionEnds.push(id);return;}
      return original(id,begin);
    };
  },id);
  // Delay completion at the bridge boundary until the replacement owns input.
  await editor.evaluate(node=>node.remove());
  await page.mouse.click(destination?450:120,280);
  await expect.poll(()=>page.evaluate(()=>!!document.querySelector('canvas').editContext)).toBe(true);
  await page.keyboard.press('Control+a');
  const cdp=await page.context().newCDPSession(page);
  await cdp.send('Input.imeSetComposition',{text:'にほん',selectionStart:3,selectionEnd:3});
  expect(await page.evaluate(code=>Module._surge_check_composing(code),destination)).toBe(1);
  expect(await page.evaluate(()=>compositionEnds)).toContain(id);
  await page.evaluate(()=>finishOldComposition());
  expect(await page.evaluate(code=>Module._surge_check_composing(code),destination)).toBe(1);
  await page.waitForTimeout(1100);
  await cdp.send('Input.imeSetComposition',{text:'日本',selectionStart:2,selectionEnd:2});
  await cdp.send('Input.insertText',{text:'日本語'});
  await page.keyboard.press('Control+z');
  await expect.poll(()=>page.evaluate(code=>Module.ccall('surge_check_editor_text','string',['number'],[code]),destination)).toBe(destination?'return 1':'initial text');
});
