import {test,expect} from './fixtures.js';
import {oscillatorFixture} from '../scripts/oscillator-fixtures.mjs';

for(const rate of [44100,48000])test(`Alias harmonic edits and undo reach a held worklet note at ${rate} Hz`,async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(bytes=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'Alias Live.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{
      dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },[...oscillatorFixture()]);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Alias Live');
  await page.getByRole('button',{name:'Oscillator Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Alias',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Scene A Osc 1 Wrap',exact:true})).toBeAttached();
  await page.getByRole('slider',{name:'Scene A Osc 1 Shape',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await page.getByRole('slider',{name:'Harmonic 1',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Sine',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;
    node.disconnect();const analyser=context.createAnalyser(),silent=context.createGain();
    analyser.fftSize=8192;analyser.smoothingTimeConstant=0;silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.aliasProbe={context,analyser,silent};
  });
  const spectrum=()=>page.evaluate(()=>{
    const {context,analyser}=aliasProbe,data=new Float32Array(analyser.frequencyBinCount);
    analyser.getFloatFrequencyData(data);
    const band=frequency=>{
      const center=Math.round(frequency*analyser.fftSize/context.sampleRate);
      let power=0;for(let i=center-2;i<=center+2;i++)power+=10**(data[i]/10);
      return power;
    };
    const fundamental=band(440*2**(-9/12)),second=band(2*440*2**(-9/12));
    return {fundamental,ratio:second/fundamental};
  });
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(1);
  await expect.poll(()=>spectrum().then(s=>s.fundamental)).toBeGreaterThan(1e-5);
  await expect.poll(()=>spectrum().then(s=>s.ratio)).toBeLessThan(0.05);
  const harmonic=page.getByRole('slider',{name:'Harmonic 2',exact:true});
  await harmonic.press('Home');await expect(harmonic).toHaveAttribute('aria-valuenow','1');
  await expect.poll(()=>spectrum().then(s=>s.ratio)).toBeGreaterThan(0.2);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(harmonic).toHaveAttribute('aria-valuenow','0');
  await expect.poll(()=>spectrum().then(s=>s.ratio)).toBeLessThan(0.05);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(harmonic).toHaveAttribute('aria-valuenow','1');
  await expect.poll(()=>spectrum().then(s=>s.ratio)).toBeGreaterThan(0.2);
  await page.evaluate(()=>aliasProbe.context.suspend());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(4);
  await harmonic.press('Delete');await expect(harmonic).toHaveAttribute('aria-valuenow','0');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(1);
  await expect.poll(()=>spectrum().then(s=>s.fundamental)).toBeGreaterThan(1e-5);
  await expect.poll(()=>spectrum().then(s=>s.ratio)).toBeLessThan(0.05);
  await page.evaluate(()=>Module._surge_browser_panic());
  expect(errors).toEqual([]);
});
