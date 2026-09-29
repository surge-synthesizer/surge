import {test, expect} from './fixtures.js';

async function muteAndObserve(page) {
  await page.evaluate(() => {
    const {context, node} = SurgeAudioInput.input.graph;
    node.disconnect();
    const analyser = context.createAnalyser();
    analyser.fftSize = 2048;
    const silent = context.createGain();
    silent.gain.value = 0;
    node.connect(analyser); analyser.connect(silent); silent.connect(context.destination);
    globalThis.workletProbe = {context, node, analyser, silent};
  });
}
const blocks = page => page.evaluate(() => Module._surge_browser_audio_blocks());
const status = page => page.evaluate(() => Module._surge_browser_audio_status());
const peak = page => page.evaluate(() => {
  const samples = new Float32Array(workletProbe.analyser.fftSize);
  workletProbe.analyser.getFloatTimeDomainData(samples);
  return Math.max(...samples.map(Math.abs));
});

for (const sampleRate of [44100, 48000]) {
  test(`real AudioWorklet plays notes, panics, resumes and recovers at ${sampleRate} Hz`, async ({page}) => {
    test.setTimeout(90000);
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    await page.addInitScript(rate => {
      const Original = AudioContext;
      globalThis.AudioContext = class extends Original {
        constructor(options = {}) { super({...options, sampleRate: rate}); }
      };
    }, sampleRate);
    await page.goto('/surge-xt-browser.html');
    await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
    await page.getByRole('button', {name:'Enable audio', exact:true}).click();
    await expect.poll(() => status(page), {timeout:15000}).toBe(2);
    await expect.poll(() => blocks(page)).toBeGreaterThan(10);
    expect(await page.evaluate(() => Module._surge_browser_audio_rate())).toBe(sampleRate);
    await muteAndObserve(page);
    expect(await peak(page)).toBe(0);
    const beforeNote = await blocks(page);
    expect(await page.evaluate(() => Module._surge_browser_midi(0x90, 60, 100, 0))).toBe(1);
    await expect.poll(() => peak(page)).toBeGreaterThan(0.001);
    await expect.poll(() => blocks(page)).toBeGreaterThan(beforeNote + 32);
    // Download failure must leave the sounding patch and callback intact.
    await page.route('**/library/objects/**', route => route.fulfill({status:503, body:'Unavailable'}));
    expect(await page.evaluate(() => Module.ccall('surge_browser_request_patch','number',['string'],
      ['/factory/patches_factory/Templates/Init FM2.fxp']))).toBe(1);
    await expect(page.locator('#file-status')).toContainText('current patch retained');
    expect(await page.evaluate(() => Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
    expect(await peak(page)).toBeGreaterThan(0.001);
    await page.unroute('**/library/objects/**');
    expect(await page.evaluate(() => Module.ccall('surge_browser_request_patch','number',['string'],
      ['/factory/patches_factory/Templates/Init FM2.fxp']))).toBe(1);
    await expect.poll(() => page.evaluate(() => Module.ccall('surge_browser_patch_name','string',[],[])),
      {timeout:15000}).toBe('Init FM2');
    expect(await page.evaluate(() => Module._surge_browser_midi(0x90, 64, 100, 0))).toBe(1);
    await expect.poll(() => peak(page)).toBeGreaterThan(0.001);
    await page.evaluate(() => Module._surge_browser_panic());
    await expect.poll(() => peak(page)).toBe(0);

    await page.evaluate(() => workletProbe.context.suspend());
    await expect.poll(() => status(page)).toBe(4);
    const suspended = await blocks(page);
    await page.getByRole('button', {name:'Enable audio', exact:true}).click();
    await expect.poll(() => status(page)).toBe(2);
    await expect.poll(() => blocks(page)).toBeGreaterThan(suspended + 10);
    expect(await page.evaluate(() => SurgeAudioInput.input.graph.context === workletProbe.context)).toBe(true);

    // Exercise the actual node's error handler, without manufacturing a DSP crash.
    await page.evaluate(() => workletProbe.node.dispatchEvent(new Event('processorerror')));
    await expect.poll(() => status(page)).toBe(-1);
    await expect(page.locator('#status')).toContainText('audio processor stopped');
    expect(await page.evaluate(() => Module._surge_browser_midi(0x90, 60, 100, 0))).toBe(0);
    await page.getByRole('button', {name:'Enable audio', exact:true}).click();
    await expect.poll(() => status(page), {timeout:15000}).toBe(2);
    expect(await page.evaluate(() => workletProbe.context.state)).toBe('closed');
    expect(await page.evaluate(() => SurgeAudioInput.input.graph.context !== workletProbe.context)).toBe(true);
    await muteAndObserve(page);
    // Loading another patch also starts a pthread after worklet retirement,
    // exercising the SDK thread list rather than just the reused audio stack.
    expect(await page.evaluate(() => Module.ccall('surge_browser_request_patch','number',['string'],
      ['/factory/patches_factory/Templates/Init Sine.fxp']))).toBe(1);
    await expect.poll(() => page.evaluate(() => Module.ccall('surge_browser_patch_name','string',[],[])),
      {timeout:15000}).toBe('Init Sine');
    const restarted = await blocks(page);
    expect(await page.evaluate(() => Module._surge_browser_midi(0x90, 67, 90, 0))).toBe(1);
    await expect.poll(() => peak(page)).toBeGreaterThan(0.001);
    await expect.poll(() => blocks(page)).toBeGreaterThan(restarted + 10);
    await page.evaluate(() => Module._surge_browser_panic());
    expect(errors).toEqual([]);
  });
}

test('audio restart waits for the patch loader before changing the engine sample rate', async ({page}) => {
  test.setTimeout(60000);
  await page.addInitScript(() => {
    globalThis.restartContexts=[];
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original {
      constructor(options={}) {
        super({...options,sampleRate:restartContexts.length ? 48000 : 44100});
        restartContexts.push(this);
      }
    };
    globalThis.holdPatchWorker=false;
    globalThis.heldPatchWorkers=[];
    const post=Worker.prototype.postMessage;
    Worker.prototype.postMessage=function(...args) {
      // Emscripten 6.0.10 CMD_RUN=2. Hold the real loader before it starts,
      // after the audio callback has transferred engine ownership to it.
      if(holdPatchWorker && args[0]?.cmd===2 && args[0]?.start_routine) {
        heldPatchWorkers.push(()=>post.apply(this,args));
        return;
      }
      return post.apply(this,args);
    };
  });
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:30000});
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>status(page)).toBe(2);
  await muteAndObserve(page);
  await page.evaluate(()=>{
    holdPatchWorker=true;
    Module.ccall('surge_browser_request_patch','number',['string'],
      ['/factory/patches_factory/Templates/Init Sine.fxp']);
  });
  await expect.poll(()=>page.evaluate(()=>heldPatchWorkers.length)).toBe(1);
  await page.evaluate(()=>workletProbe.node.dispatchEvent(new Event('processorerror')));
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>restartContexts.length)).toBe(2);
  await expect(page.locator('#status')).toContainText('Waiting for patch preparation');
  expect(await status(page)).toBe(1);
  expect(await page.evaluate(()=>restartContexts[0].state)).toBe('closed');
  expect(await page.evaluate(()=>restartContexts[1].state)).toBe('running');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>{
    holdPatchWorker=false;
    for(const release of heldPatchWorkers.splice(0)) release();
  });
  await expect.poll(()=>status(page),{timeout:15000}).toBe(2);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(48000);
  await muteAndObserve(page);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,69,100,0))).toBe(1);
  await expect.poll(()=>peak(page)).toBeGreaterThan(0.001);
  const frequency=await page.evaluate(()=>{
    const samples=new Float32Array(workletProbe.analyser.fftSize);
    workletProbe.analyser.getFloatTimeDomainData(samples);
    const crossings=[];
    for(let i=1;i<samples.length;i++)
      if(samples[i-1]<0 && samples[i]>=0) crossings.push(i);
    return 48000*(crossings.length-1)/(crossings.at(-1)-crossings[0]);
  });
  expect(Math.abs(frequency-440)).toBeLessThan(2);
  await page.evaluate(()=>Module._surge_browser_panic());
  expect(errors).toEqual([]);
});
