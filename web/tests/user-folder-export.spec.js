import {test,expect} from './fixtures.js';
import {readFileSync,readdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
const source='/user/Export Tests/Portable.surge-skin';
function skinFiles(){
  const files={},root=new URL('../../resources/data/skins/Tutorials/06 Using PNG.surge-skin/',import.meta.url);
  const visit=(url,prefix='')=>{for(const entry of readdirSync(url,{withFileTypes:true})){
    if(entry.isDirectory())visit(new URL(entry.name+'/',url),prefix+entry.name+'/');
    else{let bytes=readFileSync(new URL(entry.name,url));if(prefix+entry.name==='skin.xml')bytes=Buffer.from(bytes.toString().replace(/<surge-skin name="[^"]+" category="[^"]*"/,'<surge-skin name="Exported Portable Skin" category=""'));files[prefix+entry.name]=bytes.toString('base64');}
  }};visit(root);return files;
}
async function setup(page){
  await page.goto('/surge-xt-browser.html');await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  const files=skinFiles();
  await page.evaluate(({source,files})=>{
    for(const [name,bytes] of Object.entries(files)){const path=source+'/'+name;Module.FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));Module.FS.writeFile(path,Uint8Array.from(atob(bytes),c=>c.charCodeAt(0)));}
    globalThis.folderFiles={'Portable.surge-skin/keep.txt':[1,2,3]};globalThis.folderWrites=0;globalThis.folderFailAfter=-1;globalThis.folderAborted=false;
    const directories=new Set(['','Portable.surge-skin']);
    const directory=prefix=>({kind:'directory',async *keys(){
      const entries=new Set();for(const path of [...directories,...Object.keys(folderFiles)])if(path.startsWith(prefix)){const rest=path.slice(prefix.length);if(rest)entries.add(rest.split('/')[0]);}yield* entries;
    },async getDirectoryHandle(name){directories.add(prefix+name);return directory(prefix+name+'/');},async getFileHandle(name){return {createWritable:async()=>({
      write:async bytes=>{if(folderFailAfter>=0&&folderWrites>=folderFailAfter)throw Error('Interrupted folder write');folderFiles[prefix+name]=new Uint8Array(bytes);folderWrites++;},
      close:async()=>{},abort:async()=>{folderAborted=true;}
    })};}});
    window.showDirectoryPicker=async options=>{globalThis.folderMode=options.mode;return directory('');};
  },{source,files});
  await page.getByRole('button',{name:'User files',exact:true}).click();
  const dialog=page.getByRole('dialog',{name:'User files',exact:true});
  await dialog.getByRole('button',{name:'Open folder Export Tests',exact:true}).click();
  await dialog.getByRole('button',{name:'Open folder Portable.surge-skin',exact:true}).click();
  return {dialog,files:Object.fromEntries(Object.entries(files).map(([name,data])=>[name,createHash('sha256').update(Buffer.from(data,'base64')).digest('hex')]))};
}
const exportedHashes=page=>page.evaluate(async()=>Object.fromEntries(await Promise.all(Object.entries(folderFiles).map(async([path,bytes])=>[
  path,Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new Uint8Array(bytes))),b=>b.toString(16).padStart(2,'0')).join('')
]))));
const sourceHashes=(page,root,names)=>page.evaluate(async({root,names})=>Object.fromEntries(await Promise.all(names.map(async name=>[
  name,Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile(root+'/'+name))),b=>b.toString(16).padStart(2,'0')).join('')
]))),{root,names});
test('folder export preserves a portable skin bundle and existing destinations, then reinstalls it',async({page})=>{
  const {dialog,files}=await setup(page);
  await dialog.getByRole('button',{name:'Download this folder',exact:true}).click();
  await expect(dialog.getByRole('status')).toHaveText('Downloaded folder Portable 2.surge-skin');
  expect(await page.evaluate(()=>folderMode)).toBe('readwrite');
  const exported=await exportedHashes(page);
  expect(exported['Portable.surge-skin/keep.txt']).toBe(createHash('sha256').update(Buffer.from([1,2,3])).digest('hex'));
  expect(Object.fromEntries(Object.entries(exported).filter(([p])=>p.startsWith('Portable 2.surge-skin/')).map(([p,b])=>[p.slice('Portable 2.surge-skin/'.length),b]))).toEqual(files);
  await dialog.getByRole('button',{name:'Close user files',exact:true}).click();
  await page.evaluate(()=>{
    const directory=(prefix,name)=>({name,kind:'directory',async *values(){
      const entries=new Set(Object.keys(folderFiles).filter(p=>p.startsWith(prefix)).map(p=>p.slice(prefix.length).split('/')[0]));
      for(const name of entries){const path=prefix+name;if(Object.hasOwn(folderFiles,path))yield {name,kind:'file',getFile:async()=>new File([new Uint8Array(folderFiles[path])],name)};else yield directory(path+'/',name);}
    }});window.showDirectoryPicker=async()=>directory('Portable 2.surge-skin/','Portable 2.surge-skin');
  });
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Skins',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Portable 2.surge-skin/skin.xml').exists)).toBe(true);
  const actual=await sourceHashes(page,'/user/Skins/Portable 2.surge-skin',Object.keys(files));
  expect(actual).toEqual(files);
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Skins',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitemcheckbox',{name:/Exported Portable Skin/})).toBeChecked();
});
test('partial folder failure retains sources, reports partial output and retries into a fresh folder',async({page})=>{
  const {dialog,files}=await setup(page);
  await page.evaluate(()=>{folderFailAfter=1;});await dialog.getByRole('button',{name:'Download this folder',exact:true}).click();
  await expect(dialog.getByRole('alert')).toContainText('Some destination files may have been written');
  expect(await page.evaluate(()=>folderAborted)).toBe(true);
  expect(await sourceHashes(page,source,Object.keys(files))).toEqual(files);
  const partial=await exportedHashes(page);
  await page.evaluate(()=>{folderFailAfter=-1;});await dialog.getByRole('button',{name:'Download this folder',exact:true}).click();
  await expect(dialog.getByRole('status')).toHaveText('Downloaded folder Portable 3.surge-skin');
  const exported=await exportedHashes(page);for(const [path,bytes] of Object.entries(partial))expect(exported[path]).toEqual(bytes);
  expect(Object.fromEntries(Object.entries(exported).filter(([p])=>p.startsWith('Portable 3.surge-skin/')).map(([p,b])=>[p.slice('Portable 3.surge-skin/'.length),b]))).toEqual(files);
  await page.evaluate(()=>{window.showDirectoryPicker=async()=>{throw new DOMException('Canceled','AbortError');};});
  await dialog.getByRole('button',{name:'Download this folder',exact:true}).click();await expect(dialog.getByRole('status')).toContainText('Folder download canceled');
  expect(await exportedHashes(page)).toEqual(exported);
});
