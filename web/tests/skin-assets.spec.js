import {test,expect} from './fixtures.js';
import {readFileSync,readdirSync} from 'node:fs';
import {createHash} from 'node:crypto';
function bundle(folder,label){
  const root=new URL('../../resources/data/skins/Tutorials/'+folder+'/',import.meta.url),expected={};
  function directory(url,prefix=''){
    return readdirSync(url,{withFileTypes:true}).map(entry=>{
      const relative=prefix+entry.name;
      if(entry.isDirectory())return {name:entry.name,kind:'directory',children:directory(new URL(entry.name+'/',url),relative+'/')};
      let bytes=readFileSync(new URL(entry.name,url));
      if(relative==='skin.xml')bytes=Buffer.from(bytes.toString().replace(/<surge-skin name="[^"]+" category="[^"]*"/,'<surge-skin name="'+label+'" category=""'));
      expected[relative]=createHash('sha256').update(bytes).digest('hex');
      return {name:entry.name,kind:'file',data:bytes.toString('base64')};
    });
  }
  return {tree:directory(root),expected};
}
async function skins(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Skins',exact:true}).dispatchEvent('click');
}
const pixels=page=>page.locator('canvas').first().evaluate(async canvas=>{
  const bytes=canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data;
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))).join(',');
});
for(const [folder,label] of [['06 Using PNG.surge-skin','Portable PNG'],['10 Adding Fonts.surge-skin','Portable Fonts']])
test(`${label} imports the complete tutorial bundle and renders after reload`,async({page})=>{
  const {tree,expected}=bundle(folder,label);
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  const original=await pixels(page);
  await page.evaluate(tree=>{
    const directory=(name,children)=>({name,kind:'directory',async *values(){
      for(const child of children){
        if(child.kind==='directory')yield directory(child.name,child.children);
        else yield {name:child.name,kind:'file',getFile:async()=>new File([Uint8Array.from(atob(child.data),c=>c.charCodeAt(0))],child.name)};
      }
    }});
    window.showDirectoryPicker=async()=>directory('Portable.surge-skin',tree);
  },tree);
  await skins(page);await page.getByRole('menuitem',{name:'Install a New Skin...',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Yes',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Skins/Portable.surge-skin/skin.xml').exists)).toBe(true);
  await skins(page);const entry=page.getByRole('menuitemcheckbox',{name:new RegExp(label)});
  await expect(entry).toBeChecked();await entry.dispatchEvent('click');
  await expect.poll(()=>pixels(page)).not.toBe(original);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:new RegExp(label)})).toBeChecked();
  const actual=await page.evaluate(async paths=>{
    const result={};for(const path of paths){
      const hash=new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/user/Skins/Portable.surge-skin/'+path)));
      result[path]=Array.from(hash,b=>b.toString(16).padStart(2,'0')).join('');
    }return result;
  },Object.keys(expected));
  expect(actual).toEqual(expected);
  await page.keyboard.press('Escape');await expect.poll(()=>pixels(page)).not.toBe(original);
  if(label==='Portable Fonts'){
    // Compare only the static patch title, away from meters, popups and focus rings.
    // A fresh engine instance is necessary: reloading a skin in place can retain
    // its previous font manager override when the requested font is missing.
    const title=()=>page.locator('canvas').first().evaluate(canvas=>{
      const scale=canvas.width/canvas.getBoundingClientRect().width;
      return Array.from(canvas.getContext('2d').getImageData(220*scale,18*scale,240*scale,20*scale).data);
    });
    const withFont=await title();
    await page.evaluate(async()=>{
      const root='/user/Skins/Portable.surge-skin/fonts';
      for(const name of Module.FS.readdir(root))if(name.endsWith('.ttf'))Module.FS.unlink(root+'/'+name);
      await SurgeBrowser.flush();
    });
    await page.reload();await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
    await expect.poll(title).not.toEqual(withFont);
    await page.evaluate(async fonts=>{
      for(const font of fonts)if(font.name.endsWith('.ttf'))
        Module.FS.writeFile('/user/Skins/Portable.surge-skin/fonts/'+font.name,Uint8Array.from(atob(font.data),c=>c.charCodeAt(0)));
      await SurgeBrowser.flush();
    },tree.find(entry=>entry.name==='fonts').children);
    await page.reload();await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
    await expect.poll(title).toEqual(withFont);
  }
});
