import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';

function patchWithTempo(bpm,revision=23){
  const template=readFileSync(new URL('../../resources/data/patches_factory/Templates/Init Sine.fxp',import.meta.url));
  const size=template.readUInt32LE(64);
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  if(!xml.includes('revision="22"')||xml.includes('<tempoOnSave'))throw Error('Review tempo fixture template');
  xml=xml.replace('revision="22"',`revision="${revision}"`);
  if(bpm!==undefined)xml=xml.replace('</patch>',`<tempoOnSave v="${bpm}"/></patch>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92)),tail=template.subarray(92+size);
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);
  header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
const ready=page=>expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
const bpm=page=>page.evaluate(()=>Module._surge_browser_transport_bpm());
async function menu(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Tempo on Patch Load',exact:true}).dispatchEvent('click');
}
async function selected(page,override){
  await menu(page);
  const name=override?/^Override With Embedded Tempo if Available \(Checked\)$/i:/^Keep Current Tempo \(Checked\)$/i;
  await expect(page.getByRole('menuitemcheckbox',{name})).toHaveAttribute('aria-checked','true');
  await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
}
async function drop(page,bytes=patchWithTempo(192.5),name='Tempo 192.5'){
  await page.evaluate(({bytes,name})=>{
    const dataTransfer=new DataTransfer();
    dataTransfer.items.add(new File([new Uint8Array(bytes)],name+'.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{dataTransfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },{bytes:[...bytes],name});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe(name);
  const before=await page.evaluate(()=>Module._surge_browser_audio_blocks());
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(before+64);
}
for(const rate of [44100,48000])test(`patch tempo preference persists and controls live recall at ${rate} Hz`,async({page})=>{
  test.setTimeout(60000);
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');await ready(page);await selected(page,true);
  for(const override of [false,true]){
    await menu(page);
    await page.getByRole('menuitem',{name:override?/^Override With Embedded Tempo if Available$/i:/^Keep Current Tempo$/i}).dispatchEvent('click');
    await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready(page);await selected(page,override);
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(rate);
    const tempo=page.getByLabel('Transport tempo',{exact:true});await tempo.fill('147.5');await tempo.press('Tab');
    await expect.poll(()=>bpm(page)).toBe(147.5);
    await drop(page);
    await expect.poll(()=>bpm(page)).toBe(override?192.5:147.5);
    await expect(tempo).toHaveValue(override?'192.5':'147.5');
    await drop(page,patchWithTempo(undefined,22),'Legacy Tempo');
    expect(await bpm(page)).toBe(override?192.5:147.5);
    await drop(page,patchWithTempo(undefined),'Missing Tempo');
    await expect.poll(()=>bpm(page)).toBe(override?120:147.5);
    await expect(tempo).toHaveValue(override?'120':'147.5');
  }
});
