import {test,expect} from './fixtures.js';
test('JUCE paints and handles a browser mouse click',async({page})=>{
  const errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible({timeout:60000});
  expect(await page.evaluate(()=>crossOriginIsolated)).toBe(true);
  const canvas=page.locator('canvas');
  await page.waitForFunction(()=>document.querySelector('canvas')?.getContext('2d').getImageData(10,10,1,1).data[3]===255);
  expect(await page.evaluate(()=>Module._surge_check_clicks())).toBe(0);
  const before=await canvas.screenshot();
  await canvas.click({position:{x:150,y:180}});
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_clicks())).toBe(1);
  const after=await canvas.screenshot({path:'test-results/juce-callback.png'});
  expect(after.equals(before)).toBe(false);
  expect(errors).toEqual([]);
});
test('original Surge JUCE editor starts and switches scenes',async({page})=>{
  const errors=[];
  page.on('pageerror',error=>errors.push(error.message));
  await page.goto('/surge-xt-browser.html');
  const canvas=page.locator('canvas').first();
  await expect(canvas).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(0);
  await canvas.click({position:{x:25,y:43}});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(1);
  await canvas.click({position:{x:25,y:22}});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(0);
  await canvas.screenshot({path:'test-results/surge-editor.png'});
  expect(errors).toEqual([]);
});

test('JUCE paints at device resolution and keeps logical mouse coordinates',async({browser})=>{
  const context=await browser.newContext({deviceScaleFactor:2,viewport:{width:1100,height:750}});
  try {
    const page=await context.newPage();
    await page.goto('/surge-juce-browser-check.html');
    const canvas=page.locator('canvas');
    await expect(canvas).toBeVisible();
    await expect.poll(()=>canvas.evaluate(c=>({width:c.width,height:c.height,cssWidth:c.clientWidth,cssHeight:c.clientHeight})))
      .toEqual({width:1400,height:600,cssWidth:700,cssHeight:300});
    await canvas.click({position:{x:150,y:180}});
    await expect.poll(()=>page.evaluate(()=>Module._surge_check_clicks())).toBe(1);
    await canvas.screenshot({path:'test-results/juce-hidpi.png'});
  } finally { await context.close(); }
});

test('slider drags continue outside the JUCE canvas and release cleanly',async({page})=>{
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await page.mouse.move(350,100);await page.mouse.down();
  await page.mouse.move(950,100,{steps:10});
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_slider())).toBe(100);
  await page.mouse.up();
  await page.mouse.move(40,100,{steps:10});
  expect(await page.evaluate(()=>Module._surge_check_slider())).toBe(100);
});

test('JUCE timers run from the browser event loop',async({page})=>{
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module._surge_check_timer_callbacks())).toBeGreaterThan(3);
});

test('the original editor redraws oscillator controls after an asynchronous patch load',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  const region={x:1,y:90,width:145,height:87};
  const before=await page.screenshot({clip:region});
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await expect.poll(async()=>!(await page.screenshot({clip:region})).equals(before)).toBe(true);
  await page.locator('canvas').first().screenshot({path:'test-results/wavetable-editor.png'});
});
