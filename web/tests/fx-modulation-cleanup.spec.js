import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {sourceIds} from '../scripts/mseg-fixtures.mjs';
const root=new URL('../../',import.meta.url);
const configuration=readFileSync(new URL('resources/surge-shared/configuration.xml',root),'utf8');
const reverb=Number(configuration.match(/<type i="(\d+)" name="Reverb 2"/)[1]);
const ctrl1=sourceIds.find(s=>s.name==='ms_ctrl1').id,ctrl2=sourceIds.find(s=>s.name==='ms_ctrl2').id;
// Scene LFOs have distinct routes in A and B; macros normalize to scene A on import.
const sceneLfo=sourceIds.find(s=>s.name==='ms_slfo1').id;
const original=readFileSync(new URL('resources/data/patches_factory/Templates/Init FM2.fxp',root));
const size=original.readUInt32LE(64);let xml=original.subarray(92,92+size).toString().replace(/\0+$/,'');
const route=(source,scene,depth,muted=0)=>`<modrouting source="${source}" source_scene="${scene}" source_index="0" depth="${depth}" muted="${muted}"/>`;
xml=xml.replace(/<volume\s+([^>]*?)\/>/,(_,a)=>`<volume ${a}>${route(ctrl2,0,1)}</volume>`);
xml=xml.replace('</parameters>',`<fx1_type type="0" value="${reverb}"/><fx1_p0 type="2" value="-3">${route(ctrl1,0,.1)}</fx1_p0><fx1_p9 type="2" value=".5">${route(sceneLfo,0,.1)}${route(sceneLfo,1,.2,1)}</fx1_p9><fx2_type type="0" value="${reverb}"/><fx2_p9 type="2" value=".5">${route(ctrl2,1,.3)}</fx2_p9></parameters>`);
const payload=Buffer.from(xml),header=Buffer.from(original.subarray(0,92)),tail=original.subarray(92+size);
header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);header.writeUInt32BE(84+payload.length+tail.length,4);
const bytes=[...Buffer.concat([header,payload,tail])];
async function saveRoutes(page,name){
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill(name);
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Patches/Browser Tests/'+name+'.fxp';
  await expect.poll(()=>page.evaluate(p=>Module.FS.analyzePath(p).exists,path)).toBe(true);
  return page.evaluate(path=>{
    const b=Module.FS.readFile(path),length=new DataView(b.buffer,b.byteOffset,b.byteLength).getUint32(64,true);
    const x=new DOMParser().parseFromString(new TextDecoder().decode(b.slice(92,92+length)).replace(/\0+$/,''),'text/xml');
    return [...x.querySelectorAll('modrouting')].map(r=>({parameter:r.parentElement.tagName,attributes:[...r.attributes].map(a=>[a.name,a.value]).sort()}));
  },path);
}
test('FX routing cleanup retains unrelated routes without temporary allocations',()=>{
  const out=execFileSync(fileURLToPath(new URL('build-reference/src/surge-web/surge-fx-modulation-check',root)),[fileURLToPath(new URL('resources/data',root))],{encoding:'utf8',timeout:20000});
  expect(out).toContain('inactive destinations, both scenes, indices, notifications and unrelated routes passed without C++ heap operations');
});
for(const rate of [0,44100,48000])test(`effect replacement clears newly inactive routes and undo restores them at ${rate||'offline'}`,async({page})=>{
  test.setTimeout(60000);
  if(rate)await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(o={}){super({...o,sampleRate:rate});}};},rate);
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'FX routes.fxp'));
    const r=canvas.getBoundingClientRect();canvas.dispatchEvent(new DragEvent('drop',{cancelable:true,bubbles:true,dataTransfer:transfer,clientX:r.left+10,clientY:r.top+10}));
  },bytes);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('FX routes');
  const before=await saveRoutes(page,'Routes before');
  expect(before.filter(r=>r.parameter.startsWith('fx1_'))).toHaveLength(3);
  expect(before.filter(r=>r.parameter==='fx2_p9'||r.parameter==='volume')).toHaveLength(2);
  if(rate){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
    await page.evaluate(()=>{const {context,node}=SurgeAudioInput.input.graph;node.disconnect();const mute=context.createGain();mute.gain.value=0;node.connect(mute);mute.connect(context.destination);globalThis.routingMute=mute;Module._surge_browser_midi(0x90,60,100,0);});
  }
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Conditioner',exact:true}).dispatchEvent('click');
  const preset=configuration.match(/<type i="\d+" name="Conditioner">[\s\S]*?<snapshot name="([^"]+)"/)[1];
  await page.getByRole('menuitem',{name:preset,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Conditioner',exact:true})).toBeAttached();
  const after=await saveRoutes(page,'Routes after');
  expect(after).toEqual(before.filter(r=>!r.parameter.startsWith('fx1_')));
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Reverb 2',exact:true})).toBeAttached();
  expect(await saveRoutes(page,'Routes restored')).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Conditioner',exact:true})).toBeAttached();
  expect(await saveRoutes(page,'Routes redone')).toEqual(after);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Reverb 2',exact:true})).toBeAttached();
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Clear Scene A, Insert FX Slot 1',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Off',exact:true})).toBeAttached();
  expect(await saveRoutes(page,'Routes cleared')).toEqual(after);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Reverb 2',exact:true})).toBeAttached();
  expect(await saveRoutes(page,'Routes off restored')).toEqual(before);
  for(const name of ['Routes before','Routes after','Routes restored','Routes redone','Routes cleared','Routes off restored'])
    await expect.poll(()=>page.evaluate(q=>Module.ccall('surge_browser_search_count','number',['string'],['"'+q+'"']),name),{timeout:30000}).toBe(1);
  await expect(page.getByRole('group',{name:'Database Error',exact:true})).toHaveCount(0);
  if(rate)await page.evaluate(()=>Module._surge_browser_panic());
});
