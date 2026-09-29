import {test,expect} from './fixtures.js';
import {readFileSync,readdirSync} from 'node:fs';
import {zipBase64Files} from './helpers/archive.js';
const directory=new URL('../../resources/data/skins/Tutorials/02 Changing Images and Colors.surge-skin/',import.meta.url);
const xml=readFileSync(new URL('skin.xml',directory),'utf8').replace('name="02 Changing Images and Colors" category="Tutorials"','name="Browser Test Skin" category=""');
const images=Object.fromEntries(readdirSync(new URL('SVG/',directory)).map(name=>[name,Array.from(readFileSync(new URL('SVG/'+name,directory)))]));
async function skins(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Skins',exact:true}).dispatchEvent('click');
}
async function picker(page,name='Browser.surge-skin',fail=false,replacement=false){
  await page.evaluate(({xml,name,fail,images})=>{
    window.showDirectoryPicker=async()=>({name,kind:'directory',async *values(){
      yield {name:'skin.xml',kind:'file',getFile:async()=>{if(fail)throw Error('Skin file unreadable');return new File([xml],'skin.xml');}};
      yield {name:'SVG',kind:'directory',async *values(){for(const [name,bytes] of Object.entries(images))yield {name,kind:'file',getFile:async()=>new File([new Uint8Array(bytes)],name)};}};
    }});
  },{xml:replacement?xml.replace('Browser Test Skin','Replacement Skin'):xml,name,fail,images:replacement?Object.fromEntries(Object.entries(images).map(([name,bytes])=>[name,[...bytes,...Buffer.from('<!-- replacement -->')]])):images});
}
async function start(page){
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
}
test('original skin installation imports a browser folder and persists the selected skin',async({page})=>{
  await start(page);await picker(page);await skins(page);
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Yes',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Browser.surge-skin/skin.xml').exists)).toBe(true);
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await skins(page);
  const entry=page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/});
  await expect(entry).toBeChecked();await entry.dispatchEvent('click');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();await skins(page);
  await expect(page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/})).toBeChecked();
  expect(await page.evaluate(()=>Module.FS.readFile('/user/Skins/Browser.surge-skin/skin.xml',{encoding:'utf8'}))).toBe(xml);
  for(const [name,bytes] of Object.entries(images))
    expect(await page.evaluate(name=>Array.from(Module.FS.readFile('/user/Skins/Browser.surge-skin/SVG/'+name)),name)).toEqual(bytes);
  await page.keyboard.press('Escape');
  await expect.poll(()=>page.locator('canvas').first().evaluate(canvas=>{
    const pixels=canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data;let green=0;
    for(let i=0;i<pixels.length;i+=4)if(pixels[i]===0&&pixels[i+1]===255&&pixels[i+2]===0)green++;
    return green;
  })).toBeGreaterThan(20);
});
test('skin folder read failure leaves the current editor available and reports the error',async({page})=>{
  await start(page);await picker(page,'Browser.surge-skin',true);await skins(page);
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await expect(page.locator('#file-status')).toContainText('Skin file unreadable');
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/})).toHaveCount(0);
});

function skinZip(replacement=false){
  const files={'Zip.surge-skin/skin.xml':Buffer.from(xml.replace('Browser Test Skin',replacement?'ZIP Replacement Skin':'ZIP Test Skin')).toString('base64')};
  for(const [name,bytes] of Object.entries(images))files['Zip.surge-skin/SVG/'+name]=Buffer.concat([Buffer.from(bytes),Buffer.from(replacement?'<!-- replacement -->':'')]).toString('base64');
  return zipBase64Files(files);
}
async function dropZip(page,bytes){
  await page.locator('canvas').first().evaluate((canvas,bytes)=>{
    const transfer=new DataTransfer();transfer.items.add(new File([new Uint8Array(bytes)],'skins.zip'));
    const rect=canvas.getBoundingClientRect();canvas.dispatchEvent(new DragEvent('drop',{bubbles:true,cancelable:true,dataTransfer:transfer,clientX:rect.left+10,clientY:rect.top+10}));
  },Array.from(bytes));
}
test('ZIP skin drops install nested assets and persist the selected skin',async({page})=>{
  await start(page);await dropZip(page,skinZip());
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Zip.surge-skin/skin.xml').exists)).toBe(true);
  await skins(page);await page.getByRole('menuitem',{name:'ZIP Test Skin',exact:true}).dispatchEvent('click');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();await skins(page);
  await expect(page.getByRole('menuitemcheckbox',{name:/ZIP Test Skin/})).toBeChecked();
  for(const [name,bytes] of Object.entries(images))
    expect(await page.evaluate(name=>Array.from(Module.FS.readFile('/user/Skins/Zip.surge-skin/SVG/'+name)),name)).toEqual(bytes);
});
test('canceling ZIP skin installation leaves the user skin directory untouched',async({page})=>{
  await start(page);await dropZip(page,skinZip());
  await page.getByRole('button',{name:'No',exact:true}).dispatchEvent('click');
  expect(await page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Zip.surge-skin').exists)).toBe(false);
  await skins(page);await expect(page.getByRole('menuitem',{name:'ZIP Test Skin',exact:true})).toHaveCount(0);
});
test('a corrupt ZIP drop reports an error and keeps the current editor available',async({page})=>{
  await start(page);await dropZip(page,Buffer.from('not a ZIP archive'));
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
});

test('failed archive commit restores replaced skin files and survives reload',async({page})=>{
  await start(page);await dropZip(page,skinZip());
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Zip.surge-skin/skin.xml').exists)).toBe(true);
  await skins(page);await page.getByRole('menuitem',{name:'ZIP Test Skin',exact:true}).dispatchEvent('click');
  const snapshot=()=>page.evaluate(()=>{
    const FS=Module.FS,root='/user/Skins/Zip.surge-skin',files={};
    function visit(path){for(const name of FS.readdir(path)){if(name==='.'||name==='..')continue;const child=path+'/'+name;
      if(FS.isDir(FS.stat(child).mode))visit(child);else files[child]=Array.from(FS.readFile(child));}}
    visit(root);return files;
  });
  const before=await snapshot();
  await page.evaluate(()=>{
    const FS=Module.FS,rename=FS.rename;globalThis.archiveWrites=0;
    FS.rename=(from,to)=>{
      if(from.includes('/.surge-archive-')&&from.includes('/files/')&&to.startsWith('/user/Skins/Zip.surge-skin/')){
        if(++archiveWrites===2)throw new FS.ErrnoError(29);
      }
      return rename.call(FS,from,to);
    };
  });
  await dropZip(page,skinZip(true));
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await page.evaluate(()=>archiveWrites)).toBe(2);
  expect(await snapshot()).toEqual(before);
  expect(await page.evaluate(()=>Module.FS.readdir('/user').filter(name=>name.startsWith('.surge-archive-')))).toEqual([]);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await snapshot()).toEqual(before);
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/ZIP Test Skin/})).toBeChecked();
});

test('failed folder skin replacement restores every installed file across reload',async({page})=>{
  await start(page);await picker(page);await skins(page);
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Browser.surge-skin/skin.xml').exists)).toBe(true);
  await skins(page);await page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/}).dispatchEvent('click');
  const snapshot=()=>page.evaluate(()=>{
    const FS=Module.FS,files={};
    function visit(path){for(const name of FS.readdir(path)){if(name==='.'||name==='..')continue;const child=path+'/'+name;
      if(FS.isDir(FS.stat(child).mode))visit(child);else files[child]=Array.from(FS.readFile(child));}}
    visit('/user/Skins/Browser.surge-skin');return files;
  });
  const before=await snapshot();
  await page.evaluate(()=>{
    const FS=Module.FS,rename=FS.rename;globalThis.folderWrites=0;
    FS.rename=(from,to)=>{
      if(from.includes('/.surge-archive-')&&from.includes('/files/')&&to.startsWith('/user/Skins/Browser.surge-skin/')){
        if(++folderWrites===2)throw new FS.ErrnoError(29);
      }
      return rename.call(FS,from,to);
    };
  });
  await picker(page,'Browser.surge-skin',false,true);await skins(page);
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  expect(await page.evaluate(()=>folderWrites)).toBe(2);
  expect(await snapshot()).toEqual(before);
  expect(await page.evaluate(()=>Module.FS.readdir('/user').filter(name=>name.startsWith('.surge-archive-')))).toEqual([]);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await snapshot()).toEqual(before);
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/})).toBeChecked();
});

for(const invalid of ['<broken>', '<not-a-skin/>', '<surge-skin version="2"><globals/></surge-skin>'])
test(`invalid folder skin preserves the installed skin: ${invalid}`,async({page})=>{
  await start(page);await picker(page);await skins(page);
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Browser.surge-skin/skin.xml').exists)).toBe(true);
  await skins(page);await page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/}).dispatchEvent('click');
  await page.evaluate(invalid=>{
    window.showDirectoryPicker=async()=>({name:'Browser.surge-skin',kind:'directory',async *values(){
      yield {name:'skin.xml',kind:'file',getFile:async()=>new File([invalid],'skin.xml')};
    }});
  },invalid);
  await skins(page);await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'OK',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const installed=()=>page.evaluate(()=>Module.FS.readFile('/user/Skins/Browser.surge-skin/skin.xml',{encoding:'utf8'}));
  expect(await installed()).toBe(xml);
  expect(await page.evaluate(()=>Module.FS.readdir('/user').filter(name=>name.startsWith('.surge-archive-')))).toEqual([]);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await installed()).toBe(xml);
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/Browser Test Skin/})).toBeChecked();
});
