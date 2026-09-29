import {test,expect} from './fixtures.js';

async function menu(page,scene,slot){
  await page.getByRole('radio',{name:new RegExp(`^${scene} Insert FX ${slot}:`)}).focus();
  await page.keyboard.press('Shift+F10');
}
async function select(page,scene,slot){
  await page.getByRole('radio',{name:new RegExp(`^${scene} Insert FX ${slot}:`)}).dispatchEvent('click');
}
const damping=(page,scene,slot)=>page.getByRole('slider',{name:`FX ${scene}${slot} HF Damping - EQ`,exact:true});

test('chain preset saves, overwrites, persists and reloads same-type settings',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await menu(page,'A',1);
  await page.getByRole('menuitem',{name:'Reverb 2',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Back Room',exact:true}).dispatchEvent('click');
  await expect(damping(page,'A',1)).toHaveAttribute('aria-valuetext','90.00 %');
  const save=async()=>{
    await menu(page,'A',1);
    await page.getByRole('menuitem',{name:'Save FX Chain Preset As...',exact:true}).dispatchEvent('click');
    await page.getByRole('textbox',{name:'Value',exact:true}).fill('Browser chain');
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  };
  const path='/user/FX Chains/Browser chain.srgfxchain';
  await save();
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const original=await page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),path);
  await damping(page,'A',1).focus();await page.keyboard.press('End');
  await expect(damping(page,'A',1)).toHaveAttribute('aria-valuetext','100.00 %');
  await save();
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),path)).not.toBe(original);
  const saved=await page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),path);
  await damping(page,'A',1).focus();await page.keyboard.press('Home');
  await menu(page,'A',1);
  await page.getByRole('menuitem',{name:'FX Chains',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Browser chain',exact:true}).dispatchEvent('click');
  await expect(damping(page,'A',1)).toHaveAttribute('aria-valuetext','100.00 %');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),path)).toBe(saved);
  await menu(page,'B',1);
  await page.getByRole('menuitem',{name:'FX Chains',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Browser chain',exact:true}).dispatchEvent('click');
  await expect(damping(page,'B',1)).toHaveAttribute('aria-valuetext','100.00 %');
});

for(const sampleRate of [0,44100,48000])test(`chain clipboard reloads all four slots, repeats and undoes at ${sampleRate || 'inactive audio'}`,async({page})=>{
  if(sampleRate)await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },sampleRate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  if(sampleRate){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
    expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(sampleRate);
    await page.evaluate(()=>{
      const {context,node}=SurgeAudioInput.input.graph;
      node.disconnect();const analyser=context.createAnalyser(),silent=context.createGain();
      silent.gain.value=0;node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
      globalThis.chainProbe=analyser;Module._surge_browser_midi(0x90,60,100,0);
    });
  }
  const values=[];
  for(let slot=1;slot<=4;slot++){
    await menu(page,'A',slot);
    await page.getByRole('menuitem',{name:'Reverb 2',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Back Room',exact:true}).dispatchEvent('click');
    const slider=damping(page,'A',slot);
    await expect(slider).toHaveAttribute('aria-valuetext','90.00 %');
    await slider.focus();await page.keyboard.press('Home');
    for(let step=0;step<slot;step++)await page.keyboard.press('ArrowRight');
    values.push(await slider.getAttribute('aria-valuetext'));
  }
  expect(new Set(values).size).toBe(4);
  await menu(page,'A',1);
  await page.getByRole('menuitem',{name:'Copy FX Chain',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem',{name:'Copy FX Chain',exact:true})).toHaveCount(0);
  await menu(page,'B',1);
  await page.getByRole('menuitem',{name:'Paste FX Chain',exact:true}).dispatchEvent('click');
  for(let slot=1;slot<=4;slot++){
    await select(page,'B',slot);
    await expect(damping(page,'B',slot)).toHaveAttribute('aria-valuetext',values[slot-1]);
  }
  await select(page,'B',2);await damping(page,'B',2).focus();await page.keyboard.press('End');
  await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  await menu(page,'B',1);
  await page.getByRole('menuitem',{name:'Paste FX Chain',exact:true}).dispatchEvent('click');
  await select(page,'B',2);
  await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext',values[1]);
  // Desktop chain operations record one undo entry per slot.
  for(let i=0;i<4;i++)await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await select(page,'B',2);
  await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  if(sampleRate){
    await expect.poll(()=>page.evaluate(()=>{
      const samples=new Float32Array(chainProbe.fftSize);
      chainProbe.getFloatTimeDomainData(samples);
      return Math.max(...samples.map(Math.abs));
    })).toBeGreaterThan(.00001);
    await page.evaluate(()=>Module._surge_browser_panic());
  }
});
