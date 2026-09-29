import {test,expect} from './fixtures.js';

const slotControl=(page,scene,slot)=>page.getByRole('radio',{name:new RegExp(`^${scene} Insert FX ${slot}:`)});
const damping=(page,scene,slot)=>page.getByRole('slider',{name:`FX ${scene}${slot} HF Damping - EQ`,exact:true});
async function select(page,scene,slot){await slotControl(page,scene,slot).dispatchEvent('click');}
async function preset(page,slot){
  await slotControl(page,'A',slot).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Reverb 2',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Back Room',exact:true}).dispatchEvent('click');
  await expect(damping(page,'A',slot)).toHaveAttribute('aria-valuetext','90.00 %');
}
async function drag(page,source,target,modifier){
  const center=control=>control.evaluate(node=>{
    const [x,y,w,h]=node.juceData.bounds;return {x:x+w/2,y:y+h/2};
  });
  const a=await center(source),b=await center(target);
  if(modifier)await page.keyboard.down(modifier);
  try{
    await page.mouse.move(a.x,a.y);await page.mouse.down();
    await page.mouse.move(b.x,b.y,{steps:10});await page.mouse.up();
  }finally{if(modifier)await page.keyboard.up(modifier);}
}
async function sceneMenu(page,scene){
  const canvas=page.locator('canvas').first(),y=scene==='A'?22:43;
  await canvas.click({position:{x:25,y}});
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(scene==='A'?0:1);
  await canvas.click({position:{x:25,y},button:'right'});
}

for(const rate of [44100,48000])test(`live FX drag swap/copy/move and scene paste at ${rate} Hz`,async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
  const retiredBefore=await page.evaluate(()=>Module._surge_browser_effects_retired());
  const constructedBefore=await page.evaluate(()=>Module._surge_browser_effects_constructed());
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const analyser=context.createAnalyser(),silent=context.createGain();silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.reorderProbe=analyser;Module._surge_browser_midi(0x90,60,100,0);
  });
  await preset(page,1);await damping(page,'A',1).focus();await page.keyboard.press('End');
  await expect(damping(page,'A',1)).toHaveAttribute('aria-valuetext','100.00 %');
  await preset(page,2);
  await drag(page,slotControl(page,'A',1),slotControl(page,'A',2));
  await select(page,'A',1);await expect(damping(page,'A',1)).toHaveAttribute('aria-valuetext','90.00 %');
  await select(page,'A',2);await expect(damping(page,'A',2)).toHaveAttribute('aria-valuetext','100.00 %');
  await drag(page,slotControl(page,'A',2),slotControl(page,'B',1),'Meta');
  await select(page,'B',1);await expect(damping(page,'B',1)).toHaveAttribute('aria-valuetext','100.00 %');
  await expect(slotControl(page,'A',2)).toHaveAttribute('aria-label','A Insert FX 2: Reverb 2');
  await drag(page,slotControl(page,'B',1),slotControl(page,'B',2),'Shift');
  await expect(slotControl(page,'B',1)).toHaveAttribute('aria-label','B Insert FX 1: Off');
  await select(page,'B',2);await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(slotControl(page,'B',1)).toHaveAttribute('aria-label','B Insert FX 1: Reverb 2');
  await expect(slotControl(page,'B',2)).toHaveAttribute('aria-label','B Insert FX 2: Off');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(slotControl(page,'B',1)).toHaveAttribute('aria-label','B Insert FX 1: Off');
  await select(page,'B',2);await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  await sceneMenu(page,'A');
  await page.getByRole('menuitem',{name:'Copy Scene',exact:true}).dispatchEvent('click');
  await sceneMenu(page,'B');
  await page.getByRole('menuitem',{name:'Paste Scene',exact:true}).dispatchEvent('click');
  await select(page,'B',1);await expect(damping(page,'B',1)).toHaveAttribute('aria-valuetext','90.00 %');
  await select(page,'B',2);await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(slotControl(page,'B',1)).toHaveAttribute('aria-label','B Insert FX 1: Off');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await select(page,'B',1);await expect(damping(page,'B',1)).toHaveAttribute('aria-valuetext','90.00 %');
  await select(page,'B',2);await expect(damping(page,'B',2)).toHaveAttribute('aria-valuetext','100.00 %');
  // Whole-patch undo restores state through the patch loader, which releases
  // held notes. Verify that a fresh note plays after that restoration.
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>{
    const samples=new Float32Array(reorderProbe.fftSize);reorderProbe.getFloatTimeDomainData(samples);
    return Math.max(...samples.map(Math.abs));
  })).toBeGreaterThan(.00001);
  await page.evaluate(()=>Module._surge_browser_panic());expect(errors).toEqual([]);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_effects_retired())).toBeGreaterThan(retiredBefore);
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_effects_constructed())).toBeGreaterThan(constructedBefore);
});
