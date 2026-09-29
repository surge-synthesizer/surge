import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const patch=name=>Array.from(readFileSync(new URL(`../../resources/data/patches_factory/Templates/${name}.fxp`,import.meta.url)));
const sine=readFileSync(new URL('../../resources/data/wavetables/Basic/Sine.wt',import.meta.url));
const patchName=page=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]));
async function start(page){
  await page.goto('/surge-xt-browser.html');await expect.poll(()=>patchName(page)).toBe('Init Saw');
  await page.evaluate(()=>{
    const original=SurgeBrowser.importFiles.bind(SurgeBrowser);globalThis.dropReads=0;
    SurgeBrowser.importFiles=async files=>{try{return await original(files);}finally{dropReads++;}};
  });
}
async function drop(page,name,bytes,hold=false){
  await page.locator('canvas').first().evaluate((canvas,{name,bytes,hold})=>{
    const file=new File([new Uint8Array(bytes)],name),transfer=new DataTransfer();
    if(hold)file.arrayBuffer=()=>new Promise(resolve=>{globalThis.releaseDrop=()=>resolve(new Uint8Array(bytes).buffer);});
    transfer.items.add(file);const rect=canvas.getBoundingClientRect();
    canvas.dispatchEvent(new DragEvent('drop',{cancelable:true,bubbles:true,dataTransfer:transfer,clientX:rect.left+10,clientY:rect.top+10}));
  },{name,bytes,hold});
}
test('patch drops load through the original editor and reject corrupt replacements',async({page})=>{
  await start(page);await drop(page,'selected.fxp',patch('Init FM2'));
  await expect.poll(()=>patchName(page)).toBe('selected');
  await drop(page,'corrupt.fxp',[1,2,3]);
  await expect(page.locator('#file-status')).toContainText('Invalid patch');
  expect(await patchName(page)).toBe('selected');
});
for(const format of ['wt','wav'])test(`${format} wavetable drops preserve exact samples and retain them after an invalid drop`,async({page})=>{
  await start(page);await drop(page,'table-patch.fxp',patch('Init Wavetable'));
  await expect.poll(()=>patchName(page)).toBe('table-patch');
  let bytes=sine;
  let expected=Array.from({length:1024},(_,i)=>sine.readInt16LE(12+i*2)/16384);
  if(format==='wav'){
    // Four distinct frames, so lost frames or wrong frame ordering cannot pass.
    expected=Array.from({length:256},(_,i)=>Math.fround((1+Math.floor(i/64))*0.1*Math.sin(2*Math.PI*(i%64)/64)));
    bytes=Buffer.alloc(60+expected.length*4);bytes.write('RIFF');bytes.writeUInt32LE(bytes.length-8,4);bytes.write('WAVEfmt ',8);
    bytes.writeUInt32LE(16,16);bytes.writeUInt16LE(3,20);bytes.writeUInt16LE(1,22);bytes.writeUInt32LE(44100,24);
    bytes.writeUInt32LE(176400,28);bytes.writeUInt16LE(4,32);bytes.writeUInt16LE(32,34);
    bytes.write('srge',36);bytes.writeUInt32LE(8,40);bytes.writeUInt32LE(1,44);bytes.writeUInt32LE(64,48);
    bytes.write('data',52);bytes.writeUInt32LE(expected.length*4,56);expected.forEach((value,i)=>bytes.writeFloatLE(value,60+i*4));
  }
  await drop(page,'Dropped Sine.'+format,Array.from(bytes));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('Dropped Sine');
  const samples=()=>page.evaluate(()=>{
    const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
    return Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/size),i%size));
  });
  expect(await samples()).toEqual(expected);
  await drop(page,'invalid.'+format,[1,2,3]);
  await expect(page.locator('#file-status')).toContainText('could not be decoded');
  expect(await samples()).toEqual(expected);
});
test('a slow earlier file drop cannot replace a newer patch drop',async({page})=>{
  await start(page);await drop(page,'old.fxp',patch('Init Wavetable'),true);
  await expect.poll(()=>page.evaluate(()=>typeof releaseDrop)).toBe('function');
  await drop(page,'new.fxp',patch('Init FM2'));
  await expect.poll(()=>patchName(page)).toBe('new');
  await page.evaluate(()=>releaseDrop());
  await expect.poll(()=>page.evaluate(()=>dropReads)).toBe(2);
  // Let the JUCE message queue consume any incorrectly delivered late drop.
  await page.waitForTimeout(300);
  expect(await patchName(page)).toBe('new');
});

test('scripted wavetable drops generate samples and reject malformed metadata without replacing the script',async({page})=>{
  const script='function init(wt) wt.name="Imported Script" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=0.125 end return r end';
  const xml=`<wtscript><script lua="${Buffer.from(script).toString('base64')}" frames="2" samples="2"/></wtscript>`;
  await start(page);await drop(page,'generated.wtscript',Array.from(Buffer.from(xml)));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('Imported Script');
  expect(await page.evaluate(()=>[Module._surge_browser_wt_size(0),Module._surge_browser_wt_frames(0)])).toEqual([64,2]);
  const samples=()=>page.evaluate(()=>Array.from({length:128},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/64),i%64)));
  expect(await samples()).toEqual(Array(128).fill(0.125));
  const corruptSnapshots=Buffer.alloc(12+Buffer.byteLength(xml)+4);corruptSnapshots.write('wts1');
  corruptSnapshots.writeUInt32LE(Buffer.byteLength(xml),4);corruptSnapshots.writeUInt32LE(4,8);
  corruptSnapshots.write(xml,12);corruptSnapshots.write('oops',12+Buffer.byteLength(xml));
  for(const invalid of [Buffer.from('<wtscript><broken/>'),corruptSnapshots]){
    await drop(page,'invalid.wtscript',Array.from(invalid));
    await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
    expect(await samples()).toEqual(Array(128).fill(0.125));
  }
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(script);
});
