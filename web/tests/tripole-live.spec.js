import {test,expect} from './fixtures.js';
import {filterFixture} from '../scripts/filter-fixtures.mjs';
import {filterTypes} from '../scripts/filter-inventory.mjs';
const tripole=filterTypes.find(type=>type.symbol==='fut_tripole');
if(!tripole || tripole.subtypeCount!==12)throw Error('Review the Tri-pole live fixture modes');

// This checks real worklet playback and patch handoffs. Numerical comparisons
// with deterministic input are recorded separately by filter-survey.mjs.
for(const rate of [44100,48000])test(`all Tri-pole modes play through the JUCE worklet at ${rate} Hz`,async({page})=>{
  test.setTimeout(180000);
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;
    node.disconnect();const analyser=context.createAnalyser(),silent=context.createGain();
    analyser.fftSize=2048;silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.tripoleProbe={context,analyser,silent};
  });
  const level=()=>page.evaluate(()=>{
    const samples=new Float32Array(tripoleProbe.analyser.fftSize);
    tripoleProbe.analyser.getFloatTimeDomainData(samples);
    if(!samples.every(Number.isFinite))throw Error('Non-finite Tri-pole worklet output');
    return Math.max(...samples.map(Math.abs));
  });
  for(let subtype=0;subtype<tripole.subtypeCount;subtype++){
    await page.evaluate(()=>Module._surge_browser_panic());
    await expect.poll(level).toBeLessThan(1e-7);
    const name=`TriPole ${subtype}`;
    await page.evaluate(({bytes,name})=>{
      const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],name+'.fxp'));
      document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{
        dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
    },{bytes:[...filterFixture({type:tripole.id,subtype})],name});
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe(name);
    expect(await page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(1);
    await expect.poll(level).toBeGreaterThan(1e-6);
  }
  await page.evaluate(()=>tripoleProbe.context.suspend());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(1);
  await expect.poll(level).toBeGreaterThan(1e-6);
  await page.evaluate(()=>Module._surge_browser_panic());
});
