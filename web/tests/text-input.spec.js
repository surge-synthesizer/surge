import {test,expect} from './fixtures.js';
const errors=new WeakMap();
test.beforeEach(({page})=>{const found=[];errors.set(page,found);page.on('pageerror',e=>found.push(e.message))});
test.afterEach(({page})=>expect(errors.get(page)).toEqual([]));
const text=(page,code)=>page.evaluate(code=>Module.ccall('surge_check_editor_text','string',['number'],[code]),code);
async function open(page,code) {
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_timer_callbacks())).toBeGreaterThan(2);
  await page.mouse.click(code?450:120,280);
  await expect.poll(()=>page.evaluate(()=>!!document.querySelector('canvas').editContext)).toBe(true);
  await page.keyboard.press('Control+a');
  return page.context().newCDPSession(page);
}
for(const code of [0,1]) {
  test(`Chrome text insertion reaches the JUCE ${code?'code':'text'} editor without duplicate typing`,async({page})=>{
    const cdp=await open(page,code);
    await cdp.send('Input.insertText',{text:'日本語 😀'});
    await expect.poll(()=>text(page,code)).toBe('日本語 😀');
    await page.keyboard.press('Control+a');await page.keyboard.type('plain text');
    await expect.poll(()=>text(page,code)).toBe('plain text');
    expect(await page.evaluate(()=>document.querySelector('canvas').editContext.text)).toBe('plain text');
  });
  test(`JUCE ${code?'code':'text'} composition remains one undo transaction across a pause`,async({page})=>{
    const cdp=await open(page,code);const before=await text(page,code);
    await cdp.send('Input.imeSetComposition',{text:'にほん',selectionStart:3,selectionEnd:3});
    await expect.poll(()=>text(page,code)).toBe('にほん');
    await page.waitForTimeout(1100); // Cross both editors' normal undo-transaction timers.
    await cdp.send('Input.imeSetComposition',{text:'日本',selectionStart:2,selectionEnd:2});
    await expect.poll(()=>text(page,code)).toBe('日本');
    await cdp.send('Input.insertText',{text:'日本語'});
    await expect.poll(()=>text(page,code)).toBe('日本語');
    await page.keyboard.press('Control+z');
    await expect.poll(()=>text(page,code)).toBe(before);
    await page.keyboard.press('Control+Shift+z');
    await expect.poll(()=>text(page,code)).toBe('日本語');
  });
  test(`JUCE ${code?'code':'text'} selection maps emoji codepoints to Chrome UTF-16 offsets`,async({page})=>{
    const cdp=await open(page,code);
    await cdp.send('Input.insertText',{text:'A😀B'});
    await page.keyboard.press('Home');await page.keyboard.press('ArrowRight');await page.keyboard.press('Shift+ArrowRight');
    expect(await page.evaluate(()=>{const e=document.querySelector('canvas').editContext;return [e.selectionStart,e.selectionEnd]})).toEqual([1,3]);
    await cdp.send('Input.insertText',{text:'🦄'});
    await expect.poll(()=>text(page,code)).toBe('A🦄B');
  });
  test(`JUCE ${code?'code':'text'} read-only state detaches the browser input context`,async({page})=>{
    await open(page,code);const before=await text(page,code);
    await page.evaluate(code=>Module._surge_check_editor_readonly(code,1),code);
    await expect.poll(()=>page.evaluate(()=>document.querySelector('canvas').editContext===null)).toBe(true);
    await page.keyboard.type('must not edit');
    expect(await text(page,code)).toBe(before);
  });
}
for(const code of [0,1]) {
  test(`JUCE ${code?'code':'text'} composition supplies bounds for every UTF-16 code unit`,async({page})=>{
    const cdp=await open(page,code);
    await cdp.send('Input.imeSetComposition',{text:'😀日',selectionStart:3,selectionEnd:3});
    const bounds=await page.evaluate(()=>document.querySelector('canvas').editContext.characterBounds().map(r=>({x:r.x,y:r.y,width:r.width,height:r.height})));
    expect(bounds).toHaveLength(3);
    expect(bounds[0]).toEqual(bounds[1]); // Both surrogate units describe one glyph.
    expect(bounds[0].x).toBeGreaterThanOrEqual(code?360:30);
    expect(bounds[0].y).toBeGreaterThanOrEqual(268);
    expect(bounds[0].width).toBeGreaterThan(0);
    expect(bounds[2].x).toBeGreaterThan(bounds[1].x);
  });
  test(`canceling JUCE ${code?'code':'text'} composition preserves surrounding content`,async({page})=>{
    const cdp=await open(page,code);const before=await text(page,code);
    await page.keyboard.press('End');
    await cdp.send('Input.imeSetComposition',{text:'かな',selectionStart:2,selectionEnd:2});
    await expect.poll(()=>text(page,code)).toBe(before+'かな');
    await cdp.send('Input.imeSetComposition',{text:'',selectionStart:0,selectionEnd:0});
    await expect.poll(()=>text(page,code)).toBe(before);
    await page.keyboard.type('!');
    await expect.poll(()=>text(page,code)).toBe(before+'!');
  });
}
test('switching JUCE text targets replaces the input context and ignores late events from the old target',async({page})=>{
  await open(page,0);
  await page.evaluate(()=>globalThis.oldInputContext=document.querySelector('canvas').editContext);
  await page.mouse.click(450,280);
  await expect.poll(()=>page.evaluate(()=>document.querySelector('canvas').editContext!==oldInputContext)).toBe(true);
  await page.evaluate(()=>oldInputContext.dispatchEvent(new TextUpdateEvent('textupdate',{updateRangeStart:0,updateRangeEnd:12,text:'stale',selectionStart:5,selectionEnd:5})));
  expect(await text(page,0)).toBe('initial text');
  expect(await text(page,1)).toBe('return 1');
});
test('moving focus to browser controls detaches JUCE input and ignores old context events',async({page})=>{
  await open(page,0);
  await page.evaluate(()=>globalThis.oldInputContext=document.querySelector('canvas').editContext);
  await page.getByRole('button',{name:'Enable audio',exact:true}).focus();
  await expect.poll(()=>page.evaluate(()=>document.querySelector('canvas').editContext===null)).toBe(true);
  await page.evaluate(()=>oldInputContext.dispatchEvent(new TextUpdateEvent('textupdate',{updateRangeStart:0,updateRangeEnd:12,text:'stale',selectionStart:5,selectionEnd:5})));
  expect(await text(page,0)).toBe('initial text');
});
for(const code of [0,1]) {
  test(`JUCE ${code?'code':'text'} renders composition formatting and clears it on commit`,async({page})=>{
    const cdp=await open(page,code);
    await cdp.send('Input.imeSetComposition',{text:'abcdefg',selectionStart:7,selectionEnd:7});
    await page.waitForTimeout(50);
    const line=await page.evaluate(()=>{
      const e=document.querySelector('canvas').editContext,r=e.characterBounds();
      return {x:r[0].x,y:r[0].bottom-1,width:r.at(-1).right-r[0].x};
    });
    const pixels=()=>page.evaluate(({x,y,width})=>Array.from(document.querySelector('canvas').getContext('2d').getImageData(x,y,width,1).data),line);
    const before=await pixels();
    // CDP composition input supplies no native IME formatting. Deliver Chrome's
    // real formatting event separately to verify the renderer's response.
    await page.evaluate(()=>document.querySelector('canvas').editContext.dispatchEvent(new TextFormatUpdateEvent('textformatupdate',{
      textFormats:[new TextFormat({rangeStart:0,rangeEnd:7,underlineStyle:'solid',underlineThickness:'thick'})]
    })));
    await expect.poll(pixels).not.toEqual(before);
    await cdp.send('Input.insertText',{text:'abcdefg'});
    await expect.poll(pixels).toEqual(before);
  });
}
