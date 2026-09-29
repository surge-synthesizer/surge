import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
import {sourceIds} from '../scripts/mseg-fixtures.mjs';

const script='-- Formula Ω 日本語\nfunction init(state)\n    state.counter = 0\n    return state\nend\n\nfunction process(state)\n    state.counter = state.counter + 1\n    state.output = 0.25\n    return state\nend';
const code=page=>page.getByRole('textbox',{name:'Formula Modulator Code',exact:true});
const overlay=page=>page.getByRole('group',{name:'Voice Formula 1 Editor',exact:true});
async function open(page){
  await page.getByRole('button',{name:'Show Formula Editor',exact:true}).dispatchEvent('click');
  await expect(code(page)).toBeAttached();
  return code(page);
}
async function close(page){
  await overlay(page).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(code(page)).toHaveCount(0);
}
async function start(page,fixture){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  if(fixture){
    await page.evaluate(bytes=>{
      const dataTransfer=new DataTransfer();
      dataTransfer.items.add(new File([new Uint8Array(bytes)],'Formula Live.fxp'));
      document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{
        dataTransfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
    },[...fixture]);
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Formula Live');
  }
  await page.getByRole('radio',{name:'Formula',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'Formula',exact:true})).toBeChecked();
  return open(page);
}
async function apply(page,text){
  await code(page).fill(text);
  await page.getByRole('button',{name:'Apply',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Apply',exact:true})).toBeDisabled();
}

test('formula Apply preserves the script through patch undo, redo and saved patch reload',async({page})=>{
  const editor=await start(page),original=await editor.inputValue();
  await apply(page,script);
  await close(page);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(await open(page)).toHaveValue(original);
  await close(page);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(await open(page)).toHaveValue(script);
  await close(page);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Formula browser edit');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Patches/Browser Tests/Formula browser edit.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  const saved=await page.evaluate(path=>{
    const bytes=Module.FS.readFile(path),xml=new TextDecoder().decode(bytes);
    const start=xml.indexOf('<?xml'),end=xml.indexOf('</patch>')+8;
    const doc=new DOMParser().parseFromString(xml.slice(start,end),'text/xml');
    return [...doc.querySelectorAll('formulae formula')].map(n=>new TextDecoder().decode(Uint8Array.from(atob(n.getAttribute('formula')),c=>c.charCodeAt(0))));
  },path);
  expect(saved).toContain(script);
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),path);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Formula browser edit');
  await expect(await open(page)).toHaveValue(script);
});

test('formula debugger initializes, steps and recovers from a script error',async({page})=>{
  await start(page);await apply(page,script);
  await page.getByRole('button',{name:'Show Debugger',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Step Debugger',exact:true})).toBeAttached();
  const row=page.getByRole('row').filter({has:page.getByText('counter',{exact:true})});
  await expect(row).toBeAttached();
  await expect(row.getByText('1.000',{exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Step Debugger',exact:true}).dispatchEvent('click');
  await expect(row.getByText('2.000',{exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Init Debugger',exact:true}).dispatchEvent('click');
  await expect(row.getByText('1.000',{exact:true})).toBeAttached();
  await page.getByRole('textbox',{name:'Filter',exact:true}).fill('counter');
  // The native debugger refresh evaluates the current formula once.
  await expect(row.getByText('2.000',{exact:true})).toBeAttached();
  for(const [invalid,message] of [
    ['function process(state) error("Browser formula failure") end',/Browser formula failure/],
    ['function process(state) this is broken end',/Unable to determine process/],
    ['function init(state) return 1 end\nfunction process(state) return state end',/init\(\) function must return a table/]
  ]){
    await apply(page,invalid);
    await expect(page.getByText(message).first()).toBeAttached();
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    await expect(code(page)).toHaveValue(invalid);
    await apply(page,script);
    await expect(row.getByText('1.000',{exact:true})).toBeAttached();
    await page.getByRole('button',{name:'Step Debugger',exact:true}).dispatchEvent('click');
    await expect(row.getByText('2.000',{exact:true})).toBeAttached();
  }
  await page.getByRole('button',{name:'Hide Debugger',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Step Debugger',exact:true})).toHaveCount(0);
  await expect(code(page)).toHaveValue(script);
});

test('formula close confirmation retains unapplied edits on No and discards them on Yes',async({page})=>{
  await start(page);await apply(page,script);
  const pending=script+'\n-- unapplied edit';
  await code(page).fill(pending);
  await overlay(page).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(page.getByText(/changes that were not applied will be lost/)).toBeAttached();
  await page.getByRole('button',{name:'No',exact:true}).dispatchEvent('click');
  await expect(code(page)).toHaveValue(pending);
  await expect(page.getByRole('button',{name:'Apply',exact:true})).toBeEnabled();
  await overlay(page).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(code(page)).toHaveCount(0);
  await expect(await open(page)).toHaveValue(script);
  await expect(page.getByRole('button',{name:'Apply',exact:true})).toBeDisabled();
});

test('formula prelude is readable and searchable without accepting edits',async({page})=>{
  await start(page);await apply(page,script);
  await page.getByRole('radio',{name:'Prelude',exact:true}).dispatchEvent('click');
  const prelude=page.getByRole('textbox',{name:'Formula Modulator Prelude Code',exact:true});
  await expect(prelude).toHaveJSProperty('readOnly',true);
  const original=await prelude.inputValue();
  expect(original.length).toBeGreaterThan(100);
  await prelude.focus();
  await prelude.press('Meta+a');
  await prelude.press('Backspace');
  await prelude.press('(');
  await expect(prelude).toHaveValue(original);
  const id=Number(await prelude.getAttribute('data-juce-accessible-id'));
  expect(await page.evaluate(id=>Module._surge_accessibility_character(id,40),id)).toBe(0);
  await prelude.press('Meta+f');
  await page.getByRole('textbox',{name:'Find',exact:true}).fill('function');
  await expect.poll(()=>prelude.evaluate(node=>node.juceData.selectionEnd-node.juceData.selectionStart)).toBe(8);
  await page.getByRole('textbox',{name:'Find',exact:true}).press('Escape');
  await page.getByRole('radio',{name:'Editor',exact:true}).dispatchEvent('click');
  await expect(code(page)).toHaveValue(script);
});

function pitchFixture(){
  const template=readFileSync(new URL('../../resources/data/patches_factory/Templates/Init Sine.fxp',import.meta.url));
  const size=template.readUInt32LE(64),source=sourceIds.find(s=>s.name==='ms_lfo1').id;
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  const pitch=/<a_osc1_pitch\s+([^>]*?)\/>/;
  if(!pitch.test(xml))throw Error('Review formula fixture pitch routing');
  xml=xml.replace('name="Init Sine"','name="Formula Live"').replace(pitch,(_,attrs)=>
    `<a_osc1_pitch ${attrs}><modrouting source="${source}" depth="12" muted="0" source_index="0"/></a_osc1_pitch>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92)),tail=template.subarray(92+size);
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);
  header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
const constant=value=>`function process(state) state.use_envelope = false state.use_amplitude = false state.output = ${value} return state end`;
for(const rate of [44100,48000])test(`formula edits preserve held voices and reach new notes at ${rate} Hz`,async({page},testInfo)=>{
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await start(page,pitchFixture());await apply(page,constant(0));
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
    const analyser=context.createAnalyser();analyser.fftSize=4096;
    const silence=context.createGain();silence.gain.value=0;
    node.connect(analyser);analyser.connect(silence);silence.connect(context.destination);
    globalThis.formulaProbe={context,analyser};Module._surge_browser_midi(0x90,60,100,0);
  });
  const checkPitch=note=>expect.poll(()=>page.evaluate(note=>{
    const {context,analyser}=formulaProbe,samples=new Float32Array(analyser.fftSize);
    analyser.getFloatTimeDomainData(samples);const crossings=[];
    for(let i=1;i<samples.length;i++)if(samples[i-1]<=0&&samples[i]>0)crossings.push(i-1-samples[i-1]/(samples[i]-samples[i-1]));
    const expected=440*2**((note-69)/12);
    return crossings.length>4?Math.abs(context.sampleRate*(crossings.length-1)/(crossings.at(-1)-crossings[0])/expected-1):1;
  },note)).toBeLessThan(0.002);
  await checkPitch(60);
  // Diagnostic only: sound continuity is asserted below, but live source
  // preparation is a separate, still-open real-time migration gate.
  const compilations=()=>page.evaluate(()=>Module._surge_browser_formula_compilation_count());
  const compilationEvidence=[];
  let previous=0;
  for(const value of [0.5,-0.25]){
    const beforeApply=await compilations();
    await apply(page,constant(value));
    const blocks=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(blocks+64);
    // Native formula evaluators retain their function for an existing voice;
    // a note attack prepares the newly applied source.
    await checkPitch(60+12*previous);
    await page.evaluate(()=>Module._surge_browser_midi(0x80,60,0,0));
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_active_voices())).toBe(0);
    const beforeAttack=await compilations();
    await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0));
    await checkPitch(60+12*value);previous=value;
    compilationEvidence.push({value,beforeApply,beforeAttack,afterAttack:await compilations()});
  }
  await page.evaluate(()=>Module._surge_browser_midi(0x80,60,0,0));
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_active_voices())).toBe(0);
  expect(await page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  await testInfo.attach('live-formula-compilation',{
    body:JSON.stringify({rate,edits:compilationEvidence},null,2),contentType:'application/json'});
});

test('formula prelude context menu selects and copies text without exposing edit operations',async({page})=>{
  await page.addInitScript(()=>Object.defineProperty(navigator,'clipboard',{value:{writeText:async text=>{globalThis.formulaCopiedText=text;}}}));
  await start(page);
  await page.getByRole('radio',{name:'Prelude',exact:true}).dispatchEvent('click');
  const prelude=page.getByRole('textbox',{name:'Formula Modulator Prelude Code',exact:true});
  const original=await prelude.inputValue();
  const menu=async()=>{await expect(page.getByRole('menuitem')).toHaveCount(0);const bounds=await prelude.boundingBox();await page.mouse.click(bounds.x+50,bounds.y+12,{button:'right'});await expect(page.getByRole('menuitem',{name:'Find...',exact:true})).toBeAttached();};
  await menu();
  for(const name of ['Cut','Paste','Delete','Replace...'])await expect(page.getByRole('menuitem',{name,exact:true})).toHaveCount(0);
  await page.getByRole('menuitem',{name:'Select All',exact:true}).dispatchEvent('click');
  await expect.poll(()=>prelude.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual([0,[...original].length]);
  await menu();
  await page.getByRole('menuitem',{name:'Copy',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>globalThis.formulaCopiedText)).toBe(original);
  await menu();
  await page.getByRole('menuitem',{name:'Find...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Find',exact:true}).fill('function');
  await expect.poll(()=>prelude.evaluate(node=>node.juceData.selectionEnd-node.juceData.selectionStart)).toBe(8);
  await page.getByRole('textbox',{name:'Find',exact:true}).press('Escape');
  await menu();
  await page.getByRole('menuitem',{name:'Go to Line...',exact:true}).dispatchEvent('click');
  const line=page.getByRole('textbox',{name:'Go to line',exact:true});
  await line.fill('2');await line.press('Enter');
  const endOfSecondLine=[...original.split('\n').slice(0,2).join('\n')].length;
  await expect.poll(()=>prelude.evaluate(node=>[node.juceData.selectionStart,node.juceData.selectionEnd])).toEqual([endOfSecondLine,endOfSecondLine]);
  await expect(prelude).toHaveValue(original);
});
