import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';

const configuration=readFileSync(new URL('../../resources/surge-shared/configuration.xml',import.meta.url),'utf8');
const convolution=Number(configuration.match(/<type i="(\d+)" name="Convolution"/)[1]);
const response=Array.from(readFileSync(new URL('../../resources/data/impulses_3rdparty/Airwindows/Plate Small.flac',import.meta.url)));
async function menu(page){
  await page.getByRole('radio',{name:/^A Insert FX 1:/}).focus();
  await page.keyboard.press('Shift+F10');
}
async function preset(page,family,name){
  await menu(page);
  await page.getByRole('menuitem',{name:family,exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
}
for(const rate of [0,44100,48000])test(`file-backed FX preset failure preserves state and retries at ${rate || 'inactive audio'}`,async({page})=>{
  if(rate)await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(type=>{
    Module.FS.mkdirTree('/user/FX Presets');
    Module.FS.writeFile('/user/FX Presets/Retry.srgfx',`<single-fx><snapshot name="Retry response" type="${type}" filename="/user/Retry.flac" p0="0" p1="0" p11="1" /></single-fx>`);
    Module.FS.writeFile('/user/FX Presets/Invalid.srgfx','<single-fx><snapshot name="Invalid type" type="999999" /></single-fx>');
  },convolution);
  await menu(page);
  await page.getByRole('menuitem',{name:'Refresh FX Preset List',exact:true}).dispatchEvent('click');
  await preset(page,'Reverb 2','Back Room');
  const damping=page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true});
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  if(rate){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
    await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0));
  }
  const before=await page.evaluate(()=>Module._surge_browser_effects_constructed());
  for(const corrupt of [false,true]){
    if(corrupt)await page.evaluate(()=>Module.FS.writeFile('/user/Retry.flac','invalid response'));
    await preset(page,'Convolution','Retry response');
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
    await expect(page.getByText('Back Room',{exact:true})).toBeAttached();
    await expect(page.getByRole('radio',{name:'A Insert FX 1: Reverb 2',exact:true})).toBeAttached();
    expect(await page.evaluate(()=>Module._surge_browser_effects_constructed())).toBe(before);
  }
  // Neither failed selection may consume an undo entry.
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Off',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.evaluate(bytes=>Module.FS.writeFile('/user/Retry.flac',new Uint8Array(bytes)),response);
  await preset(page,'Convolution','Retry response');
  await expect(page.getByRole('button',{name:'Retry',exact:true})).toBeAttached();
  if(rate){
    const blocks=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(blocks+32);
  }
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.evaluate(()=>Module._surge_browser_panic());
});

for(const rate of [0,44100,48000])test(`last-slot file failure retains the complete FX chain at ${rate || 'inactive audio'}`,async({page})=>{
  if(rate)await page.addInitScript(rate=>{
    const Original=AudioContext;
    globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
  },rate);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(async type=>{
    Module.FS.mkdirTree('/user/FX Chains');
    Module.FS.writeFile('/user/FX Chains/Retry.srgfxchain',`<chain-fx><snapshot name="Retry chain">
      <fx slot="0" type="0" preset_name="Off" />
      <fx slot="1" type="0" preset_name="Off" />
      <fx slot="2" type="0" preset_name="Off" />
      <fx slot="3" type="${type}" preset_name="Retry response" filename="/user/Chain Retry.flac" p1="1" p11="1" />
    </snapshot></chain-fx>`);
    Module.FS.writeFile('/user/FX Chains/Invalid.srgfxchain','<chain-fx><snapshot name="Invalid chain"><fx slot="0" type="999999" /></snapshot></chain-fx>');
    await SurgeBrowser.flush();
  },convolution);
  // Reopen with the on-disk chain catalog present before its initial scan.
  await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await preset(page,'Reverb 2','Back Room');
  const damping=page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true});
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  if(rate){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(10);
    await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0));
  }
  const chooseChain=async()=>{
    await menu(page);
    await page.getByRole('menuitem',{name:'FX Chains',exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitem',{name:'Invalid chain',exact:true})).toHaveCount(0);
    await page.getByRole('menuitem',{name:'Retry chain',exact:true}).dispatchEvent('click');
  };
  const before=await page.evaluate(()=>Module._surge_browser_effects_constructed());
  for(const corrupt of [false,true]){
    if(corrupt)await page.evaluate(()=>Module.FS.writeFile('/user/Chain Retry.flac','invalid response'));
    await chooseChain();
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
    await expect(page.getByText('Back Room',{exact:true})).toBeAttached();
    for(let slot=2;slot<=4;slot++)
      await expect(page.getByRole('radio',{name:`A Insert FX ${slot}: Off`,exact:true})).toBeAttached();
    expect(await page.evaluate(()=>Module._surge_browser_effects_constructed())).toBe(before);
  }
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Off',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.evaluate(bytes=>Module.FS.writeFile('/user/Chain Retry.flac',new Uint8Array(bytes)),response);
  await chooseChain();
  await expect(page.getByRole('radio',{name:'A Insert FX 1: Off',exact:true})).toBeAttached();
  await page.getByRole('radio',{name:'A Insert FX 4: Convolution',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Chain Retry',exact:true})).toBeAttached();
  for(let i=0;i<4;i++)await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await page.getByRole('radio',{name:'A Insert FX 1: Reverb 2',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  if(rate){
    const blocks=await page.evaluate(()=>Module._surge_browser_audio_blocks());
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(blocks+32);
  }
  await page.evaluate(()=>Module._surge_browser_panic());
});
