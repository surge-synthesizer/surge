import {readFileSync} from 'node:fs';
import {test,expect} from './fixtures.js';

// Derive note order from the authoritative native layout definitions. Numeric
// entries are the OEM key codes used by those definitions, driven here through
// Chrome's corresponding US test-key names (not a physical layout assertion).
const source=readFileSync(new URL('../../src/surge-xt/SurgeSynthEditor.h',import.meta.url),'utf8');
const definitions=source.split('vkbLayouts =')[1].split('// clang-format on')[0];
const oem={186:';',187:'=',189:'-',191:'/',192:'`',219:'[',220:'\\',221:']',222:"'"};
const layouts=[...definitions.matchAll(/\{"([^"]+)",\s*\{([^}]+)\}\}/g)].map(([,name,keys])=>({name,
  keys:[...keys.matchAll(/'(.)'|(\d+)/g)].map(([,char,number])=>char||oem[Number(number)])}));
expect(layouts).toHaveLength(8);
for(const layout of layouts)expect(layout.keys.every(Boolean)).toBe(true);

for(const rate of [44100,48000])for(const layout of layouts)
test(`${layout.name} preserves every note position at ${rate} Hz`,async({page},testInfo)=>{
  test.setTimeout(90000);
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  const canvas=page.locator('canvas').first();await canvas.focus();await page.keyboard.press('Alt+b');
  const overlay=page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first();
  if(layout.name==='QWERTY (2 Octave)'){
    await expect(overlay).toBeAttached();await page.mouse.move(450,300);await page.mouse.wheel(0,1500);
    for(const action of ['Octave Down','Octave Up','Velocity Down 10%','Velocity Up 10%'])
      await page.getByRole('checkbox',{name:'Toggle Virtual Keyboard: '+action,exact:true}).dispatchEvent('click');
  }
  await page.getByRole('button',{name:'Select virtual keyboard layout',exact:true}).dispatchEvent('click');
  await page.getByRole(layout.name==='QWERTY'?'menuitemcheckbox':'menuitem',
    {name:layout.name+(layout.name==='QWERTY'?' (Checked)':''),exact:true}).dispatchEvent('click');
  await overlay.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');await expect(overlay).toHaveCount(0);
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Sine.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const analyser=context.createAnalyser();analyser.fftSize=4096;
    const silent=context.createGain();silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.pitchProbe={analyser,context};
  });
  await canvas.focus();await page.keyboard.press('Alt+k');
  const measurements=[];
  for(const [index,key] of layout.keys.entries()){
    const expected=440*2**((60+index-69)/12);
    await page.keyboard.down(key);
    let actual=0;
    await expect.poll(async()=>{
      actual=await page.evaluate(()=>{
        const {analyser,context}=pitchProbe,samples=new Float32Array(analyser.fftSize);
        analyser.getFloatTimeDomainData(samples);
        if(Math.max(...samples.map(Math.abs))<0.001)return 0;
        const crossings=[];
        for(let i=1;i<samples.length;i++)if(samples[i-1]<=0&&samples[i]>0)
          crossings.push(i-1-samples[i-1]/(samples[i]-samples[i-1]));
        return crossings.length>4?context.sampleRate*(crossings.length-1)/(crossings.at(-1)-crossings[0]):0;
      });
      return Math.abs(actual/expected-1);
    },{message:`${layout.name} key ${key} must play MIDI ${60+index} (${expected} Hz)`}).toBeLessThan(0.002);
    measurements.push({key,midi:60+index,expected,actual});
    await page.keyboard.up(key);
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_active_voices())).toBe(0);
  }
  await testInfo.attach('keyboard-note-pitches',{body:JSON.stringify(measurements,null,2),contentType:'application/json'});
});
