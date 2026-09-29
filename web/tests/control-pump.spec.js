import {test,expect} from './fixtures.js';

const patch=page=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]));
const select=(page,name)=>page.evaluate(name=>Module.ccall('surge_browser_request_patch','number',['string'],
  ['/factory/patches_factory/Templates/'+name+'.fxp']),name);

test('startup, patch changes and live audio progress while animation frames are paused',async({page})=>{
  await page.addInitScript(()=>{
    const request=window.requestAnimationFrame.bind(window),cancel=window.cancelAnimationFrame.bind(window);
    let held=true,serial=0;const pending=new Map();
    globalThis.pausedFrames={count:0,release(){held=false;for(const callback of pending.values())request(callback);pending.clear();}};
    window.requestAnimationFrame=callback=>{
      if(!held)return request(callback);
      ++pausedFrames.count;const id=--serial;pending.set(id,callback);return id;
    };
    window.cancelAnimationFrame=id=>{if(id<0)pending.delete(id);else cancel(id);};
  });
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>patch(page),{timeout:30000}).toBe('Init Saw');
  expect(await page.evaluate(()=>pausedFrames.count)).toBeGreaterThan(0);
  const first=await page.evaluate(()=>SurgeRuntime.snapshot().pumps);
  expect(await select(page,'Init FM2')).toBe(1);
  await expect.poll(()=>patch(page)).toBe('Init FM2');
  // JUCE messages and timers must run even though no canvas repaint occurs.
  const rate=page.getByRole('slider',{name:'Scene A LFO 1 Rate',exact:true});
  const previous=await rate.getAttribute('aria-valuetext');
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'LFO',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Delayed Vibrato',exact:true}).dispatchEvent('click');
  await expect(rate).toHaveAttribute('aria-valuetext','5.000 Hz');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(rate).toHaveAttribute('aria-valuetext',previous);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  expect(await select(page,'Init Sine')).toBe(1);
  await expect.poll(()=>patch(page)).toBe('Init Sine');
  const snapshot=await page.evaluate(()=>({runtime:SurgeRuntime.snapshot(),
    engine:JSON.parse(Module.ccall('surge_browser_control_state','string',[],[]))}));
  expect(snapshot.runtime.pumps).toBeGreaterThan(first);
  expect(snapshot.runtime.events.filter(event=>event.event==='patch-download-ready').length).toBeGreaterThanOrEqual(3);
  expect(snapshot.engine.halted).toBe(0);
  expect(snapshot.engine.audioActive).toBe(1);
  await page.evaluate(()=>pausedFrames.release());
  await expect.poll(()=>page.locator('canvas').first().evaluate(canvas=>
    canvas.getContext('2d').getImageData(10,10,1,1).data[3])).toBe(255);
});

test('bounded runtime diagnostics distinguish failed patch delivery from engine handoff',async({page})=>{
  await page.goto('/surge-xt-browser.html');await expect.poll(()=>patch(page)).toBe('Init Saw');
  await page.route('**/library/objects/**',route=>route.fulfill({status:503,body:'Unavailable'}));
  expect(await select(page,'Init FM2')).toBe(1);
  await expect(page.locator('#file-status')).toContainText('current patch retained');
  const failed=await page.evaluate(()=>SurgeRuntime.snapshot());
  const event=failed.events.find(event=>event.event==='patch-download-failed');
  expect(event).toBeDefined();expect(event.error).toContain('503');
  expect(await patch(page)).toBe('Init Saw');
  const state=await page.evaluate(()=>JSON.parse(Module.ccall('surge_browser_control_state','string',[],[])));
  expect(state.halted).toBe(0);expect(state.queued).toBe(-1);
  const bounded=await page.evaluate(()=>{
    for(let i=0;i<1000;i++)SurgeRuntime.record('diagnostic-capacity-check',{index:i});
    return SurgeRuntime.snapshot().events;
  });
  expect(bounded).toHaveLength(64);expect(bounded.at(-1).index).toBe(999);
  expect(bounded[0].sequence).toBe(bounded.at(-1).sequence-63);
});
