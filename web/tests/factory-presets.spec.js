import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const manifest=JSON.parse(readFileSync(new URL('../../build-web/web/library/manifest.json',import.meta.url)));
const presets=manifest.entries.filter(entry=>['.srgfx','.modpreset'].includes(entry.extension));
async function start(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
}
test('compressed preset catalog installs every exact factory effect and modulator definition',async({page})=>{
  const downloads=[];page.on('request',request=>downloads.push(request.url()));await start(page);
  expect(presets.filter(entry=>entry.extension==='.srgfx')).toHaveLength(295);
  expect(presets.filter(entry=>entry.extension==='.modpreset')).toHaveLength(130);
  const hashes=await page.evaluate(async entries=>{
    const result={};for(const entry of entries)
      result[entry.path]=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/factory/'+entry.path))),b=>b.toString(16).padStart(2,'0')).join('');
    return result;
  },presets);
  for(const entry of presets){
    expect(hashes[entry.path]).toBe(entry.sha256);
    expect(downloads.some(url=>url.endsWith(entry.url))).toBe(false);
  }
});
test('original LFO preset menu applies a factory preset and preserves undo and redo',async({page})=>{
  await start(page);const rate=page.getByRole('slider',{name:'Scene A LFO 1 Rate',exact:true});
  const before=await rate.getAttribute('aria-valuetext');
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  for(const name of ['Envelope','Formula','LFO','MSEG','Step Seq'])await expect(page.getByRole('menuitem',{name,exact:true})).toBeAttached();
  await page.getByRole('menuitem',{name:'LFO',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Delayed Vibrato',exact:true}).dispatchEvent('click');
  await expect(rate).toHaveAttribute('aria-valuetext','5.000 Hz');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(rate).toHaveAttribute('aria-valuetext',before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(rate).toHaveAttribute('aria-valuetext','5.000 Hz');
});
test('original FX preset menu applies factory parameter values with undo and redo',async({page})=>{
  await start(page);await page.getByRole('button',{name:'FX Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Reverb 2',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Back Room',exact:true}).dispatchEvent('click');
  const damping=page.getByRole('slider',{name:'FX A1 HF Damping - EQ',exact:true});
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveCount(0);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(damping).toHaveAttribute('aria-valuetext','90.00 %');
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Patch Settings',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Export Patch as Text (All Parameters)',exact:true}).dispatchEvent('click');
  const report=page.frameLocator('#surge-report iframe').locator('body');
  await expect(report).toContainText('A Insert FX 1: Reverb 2');
  await expect(report).toContainText('Decay Time: 550.0 ms');
  await expect(report).toContainText('HF Damping: 90.00 %');
});
for(const [path,shape,tag] of [
  ['Envelope/1 Bar Fade In','Envelope','params'],
  ['MSEG/1 Chords/1 Major','MSEG','mseg'],
  ['Step Seq/Melodic/Major Arpeggio','Step Sequencer','sequence'],
  ['Formula/Generator/DAHDSR Envelope','Formula','formula']
])test(`factory ${shape} preset retains its data through undo, save and reload`,async({page})=>{
  const original=readFileSync(new URL('../../resources/data/modulator_presets/'+path+'.modpreset',import.meta.url),'utf8');
  await start(page);await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  for(const name of path.split('/'))await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:shape,exact:true})).toBeChecked();
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'Sine',exact:true})).toBeChecked();
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:shape,exact:true})).toBeChecked();
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  const family=path.split('/')[0];
  await page.getByRole('menuitem',{name:`Save ${family} Preset As...`,exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Browser roundtrip');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const saved='/user/Modulator Presets/'+family+'/Browser roundtrip.modpreset';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,saved)).toBe(true);
  const pairs=await page.evaluate(({original,saved,tag})=>{
    const parse=xml=>new DOMParser().parseFromString(xml,'text/xml');
    const source=parse(original),target=parse(Module.FS.readFile(saved,{encoding:'utf8'})),pairs=[];
    const walk=(a,b)=>{
      if(!b){pairs.push(['missing element',a.tagName,null]);return;}
      for(const attribute of a.attributes){
        // Disabled flags for parameters that cannot deactivate are normalized by
        // the native loader. Compare envelope values and timing flags here.
        if(tag==='params'&&attribute.name==='deactivated')continue;
        // Factory MSEGs use the legacy segment-marker names. The desktop
        // serializer writes endpoint markers and its explicit unset sentinel.
        if(a.tagName==='mseg'&&['loopStart','loopEnd'].includes(attribute.name)){
          pairs.push([attribute.name+'Point',Number(attribute.value)<0?'-13214':attribute.value,b.getAttribute(attribute.name+'Point')]);
          continue;
        }
        pairs.push([a.tagName+'.'+attribute.name,attribute.value,b.getAttribute(attribute.name)]);
      }
      Array.from(a.children).forEach((child,index)=>walk(child,b.children[index]));
    };
    pairs.push(['shape',source.documentElement.getAttribute('shape'),target.documentElement.getAttribute('shape')]);
    walk(source.querySelector(tag),target.querySelector(tag));return pairs;
  },{original,saved,tag});
  for(const [name,expected,actual] of pairs){
    expect(actual,name).not.toBeNull();
    if(/^-?\d+(\.\d+)?$/.test(expected))expect(Number(actual),name).toBeCloseTo(Number(expected),5);
    else expect(actual,name).toBe(expected);
  }
  const bytes=await page.evaluate(path=>Array.from(Module.FS.readFile(path)),saved);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'LFO Menu',exact:true})).toBeAttached();
  expect(await page.evaluate(path=>Array.from(Module.FS.readFile(path)),saved)).toEqual(bytes);
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:family,exact:true}).last().dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Browser roundtrip',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:shape,exact:true})).toBeChecked();
});
