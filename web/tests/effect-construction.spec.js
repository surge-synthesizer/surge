import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';

const configuration=readFileSync(new URL('../../resources/surge-shared/configuration.xml',import.meta.url),'utf8');
const fx=configuration.match(/<fx>([\s\S]*?)<\/fx>/)[1];
const families=[...fx.matchAll(/<type i="(\d+)" name="([^"]+)">([\s\S]*?)<\/type>/g)]
  .map(([,id,name,body])=>({id:Number(id),name,preset:body.match(/<snapshot name="([^"]+)"/)[1]}));

for(const rate of [44100,48000])test(`every effect family adopts a control-thread construction during playback at ${rate} Hz`,async({page})=>{
  test.setTimeout(120000);expect(families).toHaveLength(31);
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const silent=context.createGain();silent.gain.value=0;node.connect(silent);silent.connect(context.destination);
    globalThis.constructionMute=silent;Module._surge_browser_midi(0x90,60,100,0);
  });
  for(const family of families){
    const before=await page.evaluate(()=>({effects:Module._surge_browser_effects_constructed(),blocks:Module._surge_browser_audio_blocks()}));
    await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem',{name:family.name,exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:family.preset,exact:true}).dispatchEvent('click');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_effects_constructed()),{message:family.name}).toBeGreaterThan(before.effects);
    await expect(page.getByRole('radio',{name:`A Insert FX 1: ${family.name}`,exact:true})).toBeAttached();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before.blocks+32);
  }
  const beforeOff=await page.evaluate(()=>({effects:Module._surge_browser_effects_constructed(),blocks:Module._surge_browser_audio_blocks()}));
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Clear Scene A, Insert FX Slot 1',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_effects_constructed())).toBeGreaterThan(beforeOff.effects);
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Off',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(beforeOff.blocks+32);
  await page.evaluate(()=>Module._surge_browser_panic());expect(errors).toEqual([]);
});
