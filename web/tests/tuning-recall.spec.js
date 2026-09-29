import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const scale=(steps,name)=>`! ${name}.scl\n${name}\n${steps}\n!\n`+Array.from({length:steps},(_,i)=>i===steps-1?'2/1':((i+1)*1200/steps).toFixed(8)).join('\n')+'\n';
const mapping=frequency=>`! reference.kbm\n0\n0\n127\n60\n69\n${frequency}.0\n0\n`;
const retainedScale=scale(5,'Retained Five'),embeddedScale=scale(7,'Embedded Seven');
function fixture(){
  const template=readFileSync(new URL('../../resources/data/patches_factory/Templates/Init Sine.fxp',import.meta.url));
  const size=template.readUInt32LE(64);
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  if(xml.includes('<patchTuning'))throw Error('Review tuning fixture template');
  xml=xml.replace('</patch>',`<patchTuning v="${Buffer.from(embeddedScale).toString('base64')}" m="${Buffer.from(mapping(444)).toString('base64')}" mname="Embedded Reference"/></patch>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92)),tail=template.subarray(92+size);
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
async function menu(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Tuning on Patch Load',exact:true}).dispatchEvent('click');
}
async function importTuning(page,extension,text){
  await page.evaluate(({extension,text})=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([text],'current.'+extension)}];},{extension,text});
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Tuning',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:extension==='scl'?'Load .scl Tuning...':'Load .kbm Keyboard Mapping...',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menu')).toHaveCount(0);
}
async function checkTuning(page,scaleName,reference){
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
  await expect(page.getByRole('textbox',{name:'Scala Scale',exact:true})).toHaveValue(new RegExp(scaleName));
  await expect(page.getByRole('textbox',{name:'Keyboard Mapping',exact:true})).toHaveValue(new RegExp(reference+'\\.0'));
  await page.getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Scala Scale',exact:true})).toHaveCount(0);
}
for(const rate of [44100,48000])for(const overrideScale of [false,true])for(const overrideMapping of [false,true])
test(`patch tuning recall scale=${overrideScale} mapping=${overrideMapping} at ${rate} Hz`,async({page})=>{
  test.setTimeout(60000);
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  for(const [kind,override] of [['Tuning',overrideScale],['Mapping',overrideMapping]]){
    await menu(page);await page.getByRole('menuitem',{name:`Override With Embedded ${kind} if Available`,exact:true}).dispatchEvent('click');
    if(!override){
      await menu(page);await page.getByRole('menuitem',{name:`Keep Current ${kind}`,exact:true}).dispatchEvent('click');
    }
  }
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await menu(page);
  for(const [kind,override] of [['Tuning',overrideScale],['Mapping',overrideMapping]]){
    const name=(override?`Override With Embedded ${kind} if Available`:`Keep Current ${kind}`)+' (Checked)';
    await expect(page.getByRole('menuitemcheckbox',{name,exact:true})).toHaveAttribute('aria-checked','true');
  }
  await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
  await importTuning(page,'scl',retainedScale);await importTuning(page,'kbm',mapping(432));
  await checkTuning(page,'Retained Five',432);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
  await page.evaluate(bytes=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'Embedded Tuning.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },[...fixture()]);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Embedded Tuning');
  const before=await page.evaluate(()=>Module._surge_browser_audio_blocks());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before+64);
  await checkTuning(page,overrideScale?'Embedded Seven':'Retained Five',overrideMapping?444:432);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const analyser=context.createAnalyser();analyser.fftSize=4096;
    const silent=context.createGain();silent.gain.value=0;node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.tuningProbe={analyser,context};Module._surge_browser_midi(0x90,70,100,0);
  });
  const expected=(overrideMapping?444:432)*2**(1/(overrideScale?7:5));
  const checkPitch=()=>expect.poll(()=>page.evaluate(expected=>{
    const {analyser,context}=tuningProbe,samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);
    const crossings=[];for(let i=1;i<samples.length;i++)if(samples[i-1]<=0&&samples[i]>0)crossings.push(i-1-samples[i-1]/(samples[i]-samples[i-1]));
    return crossings.length>4?Math.abs(context.sampleRate*(crossings.length-1)/(crossings.at(-1)-crossings[0])/expected-1):1;
  },expected)).toBeLessThan(0.002);
  await checkPitch();
  await page.evaluate(()=>Module._surge_browser_midi(0x80,70,0,0));
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_active_voices())).toBe(0);
  // A patch without embedded tuning must retain the active scale and mapping
  // even when either override preference is enabled.
  expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Sine.fxp']))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  const after=await page.evaluate(()=>Module._surge_browser_audio_blocks());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(after+64);
  await checkTuning(page,overrideScale?'Embedded Seven':'Retained Five',overrideMapping?444:432);
  await page.evaluate(()=>Module._surge_browser_midi(0x90,70,100,0));await checkPitch();
  await page.evaluate(()=>Module._surge_browser_midi(0x80,70,0,0));
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_active_voices())).toBe(0);
});
