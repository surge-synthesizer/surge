import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
import {sourceIds} from '../scripts/mseg-fixtures.mjs';

function wheelFixture(){
  const template=readFileSync(new URL('../../resources/data/patches_factory/Templates/Init Sine.fxp',import.meta.url));
  const size=template.readUInt32LE(64),source=sourceIds.find(s=>s.name==='ms_modwheel').id;
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  const parameter=/<a_osc1_pitch\s+([^>]*?)\/>/;
  if(!parameter.test(xml))throw Error('Review wheel fixture pitch routing');
  xml=xml.replace('name="Init Sine"','name="Wheel Test"').replace(parameter,(_,attrs)=>
    `<a_osc1_pitch ${attrs}><modrouting source="${source}" depth="12" muted="0" source_index="0"/></a_osc1_pitch>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92)),tail=template.subarray(92+size);
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);
  header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}

async function start(page,rate){
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Sine.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const analyser=context.createAnalyser();analyser.fftSize=4096;
    const silent=context.createGain();silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.performanceProbe={analyser,context};
  });
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+k');
}
const voices=page=>page.evaluate(()=>Module._surge_browser_active_voices());
const pitch=page=>page.evaluate(()=>{
  const {analyser,context}=performanceProbe,samples=new Float32Array(analyser.fftSize);
  analyser.getFloatTimeDomainData(samples);
  if(Math.max(...samples.map(Math.abs))<0.001)return 0;
  const crosses=[];
  for(let i=1;i<samples.length;i++)if(samples[i-1]<=0&&samples[i]>0)crosses.push(i-1-samples[i-1]/(samples[i]-samples[i-1]));
  return crosses.length>4?context.sampleRate*(crosses.length-1)/(crosses.at(-1)-crosses[0]):0;
});

for(const rate of [44100,48000]){
  test(`modulation wheel sets and retains its routed depth at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    await page.evaluate(bytes=>{
      const dataTransfer=new DataTransfer();
      dataTransfer.items.add(new File([new Uint8Array(bytes)],'Wheel Test.fxp'));
      document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{dataTransfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
    },[...wheelFixture()]);
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Wheel Test');
    const bounds=await page.locator('canvas').first().boundingBox(),scale=bounds.width/913;
    const x=bounds.x+76*scale,top=bounds.y+bounds.height-50*scale;
    await page.keyboard.down('a');
    const check=async(note)=>{
      const frequency=440*2**((note-69)/12);
      await expect.poll(async()=>Math.abs((await pitch(page))/frequency-1)).toBeLessThan(0.002);
      await expect.poll(()=>voices(page)).toBe(1);
    };
    await check(60);
    await page.mouse.move(x,top+1*scale);await page.mouse.down();await check(72);
    await page.mouse.move(x,top-20*scale);await check(72);
    await page.mouse.up();await check(72);
    await page.mouse.move(x,top+25*scale);await page.mouse.down();await check(60+12*63/127);
    await page.mouse.up();await check(60+12*63/127);
    await page.mouse.move(x,top+49*scale);await page.mouse.down();await check(60);
    await page.mouse.move(x,top+70*scale);await check(60);
    await page.mouse.up();await check(60);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
  });
  test(`pitch wheel bends, clamps and returns to center at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    const bounds=await page.locator('canvas').first().boundingBox(),scale=bounds.width/913;
    const x=bounds.x+58*scale,top=bounds.y+bounds.height-50*scale;
    await page.keyboard.down('a');
    const check=async(note)=>{
      const frequency=440*2**((note-69)/12);
      await expect.poll(async()=>Math.abs((await pitch(page))/frequency-1)).toBeLessThan(0.002);
      await expect.poll(()=>voices(page)).toBe(1);
    };
    await check(60);
    await page.mouse.move(x,top+1*scale);await page.mouse.down();await check(62);
    await page.mouse.move(x,top-20*scale);await check(62);
    await page.mouse.move(x,top+49*scale);await check(58);
    await page.mouse.move(x,top+70*scale);await check(58);
    await page.mouse.up();await check(60);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
  });
  test(`sustain pedal holds released keys and releases its voices at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    const bounds=await page.locator('canvas').first().boundingBox(),scale=bounds.width/913;
    const x=bounds.x+25*scale,y=bounds.y+bounds.height-8*scale;
    await page.mouse.click(x,y);
    await page.keyboard.down('a');await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.up('a');
    const before=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before+128);
    await expect.poll(()=>voices(page)).toBe(1);
    await expect.poll(async()=>Math.abs((await pitch(page))/261.6255653-1)).toBeLessThan(0.002);
    await page.mouse.click(x,y);await expect.poll(()=>voices(page)).toBe(0);
    await page.keyboard.down('a');await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
  });
  test(`pointer playing, dragging and note latching at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    const canvas=page.locator('canvas').first();
    const bounds=await canvas.boundingBox();
    // Original standalone layout: keyboard starts at x=88, white keys are
    // 16 px wide, first visible note is C1 (24). C4 is 21 white keys later.
    const scale=bounds.width/913;
    const point={x:bounds.x+432*scale,y:bounds.y+bounds.height-8*scale};
    const checkPitch=async(note)=>{
      const frequency=440*2**((note-69)/12);
      await expect.poll(async()=>Math.abs((await pitch(page))/frequency-1)).toBeLessThan(0.002);
      await expect.poll(()=>voices(page)).toBe(1);
    };
    await page.mouse.move(point.x,point.y);await page.mouse.down();await checkPitch(60);
    await page.mouse.move(point.x+16*scale,point.y);await checkPitch(62);
    // JUCE retains the dragged note outside the keyboard until mouse-up.
    await page.mouse.move(point.x,bounds.y+bounds.height+20);await checkPitch(62);
    await page.mouse.up();await expect.poll(()=>voices(page)).toBe(0);
    await page.mouse.click(point.x,point.y,{button:'right'});await checkPitch(60);
    // A latch survives pointer release and moving outside the keyboard.
    await page.mouse.move(point.x,bounds.y+bounds.height+20);await checkPitch(60);
    await page.mouse.click(point.x,point.y,{button:'right'});await expect.poll(()=>voices(page)).toBe(0);
    await page.mouse.click(point.x,point.y,{button:'right'});await checkPitch(60);
    await page.mouse.click(point.x,point.y);await expect.poll(()=>voices(page)).toBe(0);
    await page.mouse.move(point.x,point.y);await page.mouse.down();await checkPitch(60);
    await page.mouse.up();await expect.poll(()=>voices(page)).toBe(0);
  });
  test(`virtual keyboard velocity shortcuts attenuate, clamp and recover at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    const sensitivity=page.getByRole('slider',{name:'Scene A Velocity > VCA Gain',exact:true});
    await sensitivity.focus();await page.keyboard.press('Home');
    await expect(sensitivity).toHaveAttribute('aria-valuenow','0');
    await page.locator('canvas').first().focus();
    const blocks=()=>page.evaluate(()=>Module._surge_browser_audio_blocks());
    const peak=()=>page.evaluate(()=>{
      const {analyser}=performanceProbe,samples=new Float32Array(analyser.fftSize);
      analyser.getFloatTimeDomainData(samples);return Math.max(...samples.map(Math.abs));
    });
    const play=async()=>{
      const before=await blocks();await page.keyboard.down('a');
      await expect.poll(()=>voices(page)).toBe(1);await expect.poll(blocks).toBeGreaterThan(before+64);
      const level=await peak();await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
      return level;
    };
    const full=await play();expect(full).toBeGreaterThan(0.001);
    for(let i=0;i<5;i++)await page.keyboard.press('9');
    const reduced=await play();expect(reduced).toBeLessThan(full*0.8);expect(reduced).toBeGreaterThan(full*0.001);
    for(let i=0;i<20;i++)await page.keyboard.press('9');
    // GUI note-on velocity zero is an actual note in the native processor,
    // unlike wire MIDI velocity-zero note-on. At -48 dB sensitivity its level
    // is the documented minimum gain, rather than silence.
    const minimum=await play();
    expect(Math.abs(minimum/full/10**(-48/20)-1)).toBeLessThan(0.01);
    for(let i=0;i<5;i++)await page.keyboard.press('9');
    expect(Math.abs((await play())/minimum-1)).toBeLessThan(0.01);
    await page.keyboard.press('0');const recovered=await play();
    expect(recovered).toBeGreaterThan(minimum);expect(recovered).toBeLessThan(reduced);
    for(let i=0;i<20;i++)await page.keyboard.press('0');
    expect(Math.abs((await play())/full-1)).toBeLessThan(0.01);
  });
  test(`virtual keyboard octave shortcuts change sounding pitch at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    for(const [key,note] of [[null,60],['x',48],['c',60],['c',72],['x',60]]){
      if(key)await page.keyboard.press(key);
      await page.keyboard.down('a');const frequency=440*2**((note-69)/12);
      await expect.poll(async()=>Math.abs((await pitch(page))/frequency-1)).toBeLessThan(0.002);
      await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
    }
  });
  test(`octave changes release the old held note at ${rate} Hz`,async({page})=>{
    await start(page,rate);await page.keyboard.down('a');
    await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.press('x');
    await expect.poll(async()=>Math.abs((await pitch(page))/130.81278265-1)).toBeLessThan(0.002);
    await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
    await page.keyboard.down('a');await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.press('c');
    await expect.poll(async()=>Math.abs((await pitch(page))/261.6255653-1)).toBeLessThan(0.002);
    await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
  });
  for(const focusTarget of ['dom','canvas'])
  test(`held notes release when focus leaves playing or enters ${focusTarget} search at ${rate} Hz`,async({page})=>{
    await start(page,rate);
    await page.keyboard.down('a');await expect.poll(()=>voices(page)).toBeGreaterThan(0);
    await page.getByRole('button',{name:'Enable MIDI',exact:true}).focus();
    await expect.poll(()=>voices(page)).toBe(0);await page.keyboard.up('a');
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init Sine'])),{timeout:30000}).toBe(1);
    // A separately held MIDI note must survive virtual-keyboard cleanup.
    expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,72,100,0))).toBe(1);
    await expect.poll(()=>voices(page)).toBe(1);
    await page.locator('canvas').first().focus();await page.keyboard.down('a');
    await expect.poll(()=>voices(page)).toBe(2);
    await page.getByRole('button',{name:'Open patch search',exact:true}).dispatchEvent('click');
    const search=page.getByRole('textbox',{name:'Patch select',exact:true});
    await expect(search).toBeEnabled();if(focusTarget==='dom')await search.focus();
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(1);
    await page.keyboard.press('a');
    const before=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before+32);
    expect(await voices(page)).toBe(1);
    expect(await page.evaluate(()=>Module._surge_browser_midi(0x80,72,0,0))).toBe(1);
    await expect.poll(()=>voices(page)).toBe(0);
    await search.fill('Init Sine');await search.press('Escape');await expect(search).toHaveCount(0);
    await page.locator('canvas').first().focus();await page.keyboard.down('a');
    await expect.poll(()=>voices(page)).toBeGreaterThan(0);
    await page.keyboard.up('a');await expect.poll(()=>voices(page)).toBe(0);
  });
  test(`hiding the virtual keyboard releases held notes and showing it restores playing at ${rate} Hz`,async({page})=>{
    await start(page,rate);await page.keyboard.down('a');
    await expect.poll(()=>voices(page)).toBe(1);
    const canvas=page.locator('canvas').first();const shownHeight=await canvas.evaluate(node=>node.clientHeight);
    await page.keyboard.press('Alt+k');await page.keyboard.up('a');
    await expect.poll(()=>voices(page)).toBe(0);
    await expect.poll(()=>canvas.evaluate(node=>node.clientHeight)).toBeLessThan(shownHeight);
    const before=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await page.keyboard.down('a');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before+32);
    expect(await voices(page)).toBe(0);await page.keyboard.up('a');
    await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
    await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
    await expect.poll(()=>canvas.evaluate(node=>node.clientHeight)).toBeLessThan(shownHeight);
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    await canvas.focus();
    await page.keyboard.press('Alt+k');await page.keyboard.down('a');
    await expect.poll(()=>canvas.evaluate(node=>node.clientHeight)).toBe(shownHeight);
    await expect.poll(()=>voices(page)).toBe(1);await page.keyboard.up('a');
    await expect.poll(()=>voices(page)).toBe(0);
    await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
    await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
    await expect.poll(()=>canvas.evaluate(node=>node.clientHeight)).toBe(shownHeight);
  });
}
