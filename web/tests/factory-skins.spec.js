import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const manifest=JSON.parse(readFileSync(new URL('../../build-web/web/library/manifest.json',import.meta.url)));
const root='skins/Tutorials/10 Adding Fonts.surge-skin/';
const files=manifest.entries.filter(entry=>entry.path.startsWith(root));
async function skins(page){
  await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Skins',exact:true}).dispatchEvent('click');
}
async function choose(page){
  const serial=await page.evaluate(()=>SurgeFactory.skinSerial);
  await skins(page);await page.getByRole('menuitem',{name:'Tutorials',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'10 Adding Fonts',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>SurgeFactory.skinSerial)).toBeGreaterThan(serial);
}
async function selected(page){
  await expect.poll(()=>page.evaluate(()=>SurgeFactory.skinRequests.size)).toBe(0);
  await skins(page);await page.getByRole('menuitem',{name:'Tutorials',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitemcheckbox',{name:/^10 Adding Fonts/})).toBeChecked();
  await page.keyboard.press('Escape');
}
test('factory skin menu loads only the selected complete bundle and restores it from cache',async({page})=>{
  const downloads=[];page.on('request',request=>downloads.push(request.url()));
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  expect(await page.evaluate(()=>[...SurgeFactory.library.entries.values()].filter(entry=>entry.path.endsWith('.surge-skin/skin.xml')).length)).toBe(11);
  for(const entry of files)expect(downloads.some(url=>url.endsWith(entry.url))).toBe(false);
  await choose(page);await selected(page);
  for(const entry of files){
    expect(downloads.some(url=>url.endsWith(entry.url))).toBe(true);
    const hash=await page.evaluate(async path=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/factory/'+path))),b=>b.toString(16).padStart(2,'0')).join(''),entry.path);
    expect(hash).toBe(entry.sha256);
  }
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.route('**/library/objects/**',route=>route.abort());
  await page.reload();await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await selected(page);
});
test('failed factory skin download keeps the current skin and can be retried',async({page})=>{
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  const font=files.find(entry=>entry.extension==='.ttf');
  await page.route('**/'+font.url,route=>route.fulfill({status:503,body:'unavailable'}));
  await choose(page);await expect(page.locator('#file-status')).toContainText('Skin download failed');
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/^Surge Classic/})).toBeChecked();await page.keyboard.press('Escape');
  expect(await page.evaluate(path=>Module.FS.analyzePath('/factory/'+path).exists,font.path)).toBe(false);
  await page.unroute('**/'+font.url);await choose(page);await selected(page);
});
test('a slow factory skin cannot override a later local selection',async({page})=>{
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  const font=files.find(entry=>entry.extension==='.ttf');let release,arrive;
  const held=new Promise(resolve=>release=resolve),requested=new Promise(resolve=>arrive=resolve);
  await page.route('**/'+font.url,async route=>{arrive();await held;await route.continue();});
  await choose(page);await requested;
  await skins(page);await page.getByRole('menuitemcheckbox',{name:/^Surge Classic/}).dispatchEvent('click');
  await expect(page.locator('#file-status')).toHaveText('');
  release();await expect.poll(()=>page.evaluate(()=>SurgeFactory.library.pending.size)).toBe(0);
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/^Surge Classic/})).toBeChecked();
  await page.keyboard.press('Escape');await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/^Surge Classic/})).toBeChecked();
});
for(const skin of manifest.entries.filter(entry=>entry.path.endsWith('.surge-skin/skin.xml'))){
  const xml=readFileSync(new URL('../../resources/data/'+skin.path,import.meta.url),'utf8');
  const tag=xml.match(/<surge-skin\s[^>]+>/)[0],name=tag.match(/\bname="([^"]+)"/)[1],category=tag.match(/\bcategory="([^"]*)"/)?.[1];
  test(`factory skin selection: ${name}`,async({page})=>{
    await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
    await skins(page);if(category)await page.getByRole('menuitem',{name:category,exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
    await expect.poll(()=>page.evaluate(()=>SurgeFactory.skinSerial)).toBeGreaterThan(0);
    await expect.poll(()=>page.evaluate(()=>SurgeFactory.skinRequests.size)).toBe(0);
    await expect(page.locator('#file-status')).toHaveText('');
    const root=skin.path.slice(0,-'skin.xml'.length),expected=manifest.entries.filter(entry=>entry.path.startsWith(root));
    const hashes=await page.evaluate(async entries=>{
      const result={};for(const entry of entries){
        result[entry.path]=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/factory/'+entry.path))),b=>b.toString(16).padStart(2,'0')).join('');
      }return result;
    },expected);
    for(const entry of expected)expect(hashes[entry.path]).toBe(entry.sha256);
    await skins(page);if(category)await page.getByRole('menuitem',{name:category,exact:true}).dispatchEvent('click');
    await expect(page.getByRole('menuitemcheckbox',{name:name+' (Checked)',exact:true})).toBeChecked();
  });
}
for(const mode of ['stage','commit'])test(`factory skin ${mode} write failure preserves the catalog and permits retry`,async({page})=>{
  await page.goto('/surge-xt-browser.html');await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  const before=await page.evaluate(root=>Array.from(Module.FS.readFile('/factory/'+root+'skin.xml')),root);
  await page.evaluate(({root,mode})=>{
    const FS=Module.FS,write=FS.writeFile,rename=FS.rename;globalThis.failedSkinWrite=false;
    FS.writeFile=function(path,...args){
      if(mode==='stage'&&!failedSkinWrite&&path.includes('.download-')&&path.endsWith('.ttf')){failedSkinWrite=true;throw new FS.ErrnoError(29);}
      return write.call(FS,path,...args);
    };
    FS.rename=function(from,to){
      if(mode==='commit'&&!failedSkinWrite&&from.includes('.download-')&&to==='/factory/'+root.slice(0,-1)){failedSkinWrite=true;throw new FS.ErrnoError(29);}
      return rename.call(FS,from,to);
    };
  },{root,mode});
  await choose(page);await expect(page.locator('#file-status')).toContainText('Skin download failed');
  expect(await page.evaluate(()=>failedSkinWrite)).toBe(true);
  expect(await page.evaluate(root=>Array.from(Module.FS.readFile('/factory/'+root+'skin.xml')),root)).toEqual(before);
  expect(await page.evaluate(()=>Module.FS.readdir('/factory/skins/Tutorials').filter(name=>name.includes('.download-')))).toEqual([]);
  await skins(page);await expect(page.getByRole('menuitemcheckbox',{name:/^Surge Classic/})).toBeChecked();await page.keyboard.press('Escape');
  await choose(page);await selected(page);
});
