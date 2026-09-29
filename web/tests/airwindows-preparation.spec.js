import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';

for(const rate of [44100,48000])
for(const preset of ['Lo-Fi/DeRez','Filter/Cabs'])
test(`prepared Airwindows ${preset} preserves streamed parameters at ${rate} Hz`,async({page})=>{
  const original=readFileSync(new URL('../../resources/data/fx_presets/Airwindows/'+preset+'.srgfx',import.meta.url),'utf8');
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  const before=await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const mute=context.createGain();mute.gain.value=0;node.connect(mute);mute.connect(context.destination);
    globalThis.preparationMute=mute;Module._surge_browser_midi(0x90,60,100,0);
    return Module._surge_browser_effects_constructed();
  });
  await page.getByRole('button',{name:'FX Type',exact:true}).dispatchEvent('click');
  for(const name of ['Airwindows',...preset.split('/')])
    await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_effects_constructed())).toBeGreaterThan(before);
  const blocks=await page.evaluate(()=>Module._surge_browser_audio_blocks());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(blocks+128);
  await page.getByRole('button',{name:'FX Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Save FX Preset As...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Prepared Airwindows');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const saved='/user/FX Presets/Airwindows/Prepared Airwindows.srgfx';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,saved)).toBe(true);
  const pairs=await page.evaluate(({original,saved})=>{
    const parse=text=>new DOMParser().parseFromString(text,'text/xml').querySelector('snapshot');
    const source=parse(original),target=parse(Module.FS.readFile(saved,{encoding:'utf8'}));
    return [...source.attributes].filter(a=>a.name==='type'||/^p\d+$/.test(a.name))
      .map(a=>({name:a.name,expected:Number(a.value),actual:target.hasAttribute(a.name)?Number(target.getAttribute(a.name)):null}));
  },{original,saved});
  for(const {name,expected,actual} of pairs){
    expect(actual,name).not.toBeNull();expect(actual,name).toBeCloseTo(expected,5);
  }
  await page.evaluate(()=>Module._surge_browser_panic());
});
