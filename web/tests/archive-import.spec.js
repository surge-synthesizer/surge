import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
import {zipBase64Files} from './helpers/archive.js';
const resource=path=>Array.from(readFileSync(new URL('../../resources/data/'+path,import.meta.url)));
const before={
  '/user/Patches/Mixed.fxp':resource('patches_factory/Templates/Init Saw.fxp'),
  '/user/Wavetables/Mixed.wt':resource('wavetables/Basic/Sine.wt'),
  '/user/Skins/Mixed.surge-skin/skin.xml':Array.from(Buffer.from('<surge-skin name="Original" version="2"><globals/><component-classes/><controls/></surge-skin>'))
};
const after={
  '/user/Patches/Mixed.fxp':resource('patches_factory/Templates/Init FM2.fxp'),
  '/user/Patches/New/Nested.fxp':resource('patches_factory/Templates/Init Wavetable.fxp'),
  '/user/Wavetables/Mixed.wt':resource('wavetables/Basic/Sine To Sawtooth.wt'),
  '/user/Skins/Mixed.surge-skin/skin.xml':Array.from(Buffer.from('<surge-skin name="Replacement" version="2"><globals/><component-classes/><controls/></surge-skin>'))
};
function archive(files=after){
  const entries=Object.fromEntries(Object.entries(files).map(([name,bytes])=>[name.replace(/^\/user\/[^/]+\//,''),Buffer.from(bytes).toString('base64')]));
  return zipBase64Files(entries);
}
async function read(page,paths){return page.evaluate(paths=>Object.fromEntries(paths.map(path=>[path,Array.from(Module.FS.readFile(path))])),paths);}
for(const mode of ['success','rollback','recovery'])test(`mixed archive ${mode}`,async({page})=>{
  const fail=mode!=='success';
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(({before,fail,mode})=>{
    const FS=Module.FS;
    for(const [path,bytes] of Object.entries(before)){FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));FS.writeFile(path,new Uint8Array(bytes));}
    const rename=FS.rename;globalThis.committedTargets=[];
    FS.rename=(from,to)=>{
      if(from.includes('/.surge-archive-')&&from.includes('/files/')){
        committedTargets.push(to);
        if(fail&&to==='/user/Wavetables/Mixed.wt')throw new FS.ErrnoError(29);
      }
      if(mode==='recovery'&&from.includes('/backups/')&&to==='/user/Patches/Mixed.fxp')throw new FS.ErrnoError(29);
      return rename.call(FS,from,to);
    };
  },{before,fail,mode});
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'mixed.zip'));
    const bounds=canvas.getBoundingClientRect();canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:bounds.left+10,clientY:bounds.top+10}));
  },Array.from(archive()));
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  if(fail){
    await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
    await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  }
  await expect.poll(()=>page.evaluate(()=>committedTargets.length)).toBe(4);
  const targets=await page.evaluate(()=>committedTargets);
  expect(targets).toContain('/user/Patches/Mixed.fxp');expect(targets).toContain('/user/Skins/Mixed.surge-skin/skin.xml');
  expect(targets.at(-1)).toBe('/user/Wavetables/Mixed.wt');
  if(mode==='recovery'){
    const recovery=await page.evaluate(()=>{
      const root='/user/'+Module.FS.readdir('/user').find(name=>name.startsWith('.surge-archive-'));
      return JSON.parse(Module.FS.readFile(root+'/recovery.json',{encoding:'utf8'}));
    });
    const entry=recovery.find(entry=>entry.target==='/user/Patches/Mixed.fxp');
    expect(entry).toBeTruthy();
    expect(await read(page,[entry.backup])).toEqual({[entry.backup]:before[entry.target]});
    await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
    await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
    expect(await read(page,[entry.backup])).toEqual({[entry.backup]:before[entry.target]});
    await page.getByRole('button',{name:'Recover archive files',exact:true}).click();
    await expect(page.getByRole('dialog',{name:'Archive file recovery',exact:true})).toBeVisible();
    const downloadButton=page.getByRole('button',{name:'Download Patches/Mixed.fxp',exact:true});
    await page.evaluate(()=>{globalThis.realRecoveryURL=URL.createObjectURL;URL.createObjectURL=()=>{throw Error('Download unavailable');};});
    await downloadButton.click();await expect(page.locator('#archive-recovery-dialog [role=alert]')).toContainText('Download unavailable');
    await page.evaluate(()=>{URL.createObjectURL=realRecoveryURL;});
    const pending=page.waitForEvent('download');await downloadButton.click();const download=await pending;
    expect(download.suggestedFilename()).toBe('Mixed.fxp');
    expect(Array.from(readFileSync(await download.path()))).toEqual(before[entry.target]);
    expect(await read(page,[entry.backup])).toEqual({[entry.backup]:before[entry.target]});
    await page.getByRole('button',{name:'Close recovery',exact:true}).click();
    await expect(page.getByRole('button',{name:'Recover archive files',exact:true})).toBeFocused();
    return;
  }
  const expected=fail?before:after;
  expect(await read(page,Object.keys(expected))).toEqual(expected);
  if(fail)expect(await page.evaluate(()=>Module.FS.analyzePath('/user/Patches/New').exists)).toBe(false);
  expect(await page.evaluate(()=>Module.FS.readdir('/user').filter(name=>name.startsWith('.surge-archive-')))).toEqual([]);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await read(page,Object.keys(expected))).toEqual(expected);
});

for(const invalid of ['<broken>', '<not-a-skin/>', '<surge-skin version="2"><globals/></surge-skin>'])
test(`mixed archive rejects invalid skin before committing any files: ${invalid}`,async({page})=>{
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(before=>{
    const FS=Module.FS;
    for(const [path,bytes] of Object.entries(before)){FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));FS.writeFile(path,new Uint8Array(bytes));}
    const rename=FS.rename;globalThis.committedTargets=[];
    FS.rename=(from,to)=>{if(from.includes('/.surge-archive-')&&from.includes('/files/'))committedTargets.push(to);return rename.call(FS,from,to);};
  },before);
  const files={...after,'/user/Skins/Mixed.surge-skin/skin.xml':Array.from(Buffer.from(invalid))};
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'invalid-skin.zip'));
    const bounds=canvas.getBoundingClientRect();canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:bounds.left+10,clientY:bounds.top+10}));
  },Array.from(archive(files)));
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await page.evaluate(()=>committedTargets)).toEqual([]);
  expect(await read(page,Object.keys(before))).toEqual(before);
  expect(await page.evaluate(()=>Module.FS.analyzePath('/user/Patches/New').exists)).toBe(false);
  expect(await page.evaluate(()=>Module.FS.readdir('/user').filter(name=>name.startsWith('.surge-archive-')))).toEqual([]);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await read(page,Object.keys(before))).toEqual(before);
});
