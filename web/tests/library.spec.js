import {test,expect} from './fixtures.js';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {gunzipSync} from 'node:zlib';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../../',import.meta.url));

test('published manifest covers every factory resource with exact bytes',async()=>{
  const source=path.join(root,'resources/data');
  const published=path.join(root,'build-web/web/library');
  const manifest=JSON.parse(fs.readFileSync(path.join(published,'manifest.json'),'utf8'));
  const files=fs.readdirSync(source,{recursive:true,withFileTypes:true})
    .filter(entry=>entry.isFile()&&entry.name!=='.DS_Store')
    .map(entry=>path.relative(source,path.join(entry.parentPath,entry.name))).sort();
  expect(manifest.entries.map(entry=>entry.path).sort()).toEqual(files);
  let total=0;
  for(const entry of manifest.entries){
    const original=fs.readFileSync(path.join(source,entry.path));
    expect(entry.size).toBe(original.length);
    expect(entry.sha256).toBe(crypto.createHash('sha256').update(original).digest('hex'));
    expect(fs.readFileSync(path.join(published,entry.url)).equals(original)).toBe(true);
    total+=original.length;
  }
  expect(manifest.totalBytes).toBe(total);
  const packed=fs.readFileSync(path.join(published,manifest.patchIndex.url));
  expect(crypto.createHash('sha256').update(packed).digest('hex')).toBe(manifest.patchIndex.sha256);
  const metadata=JSON.parse(gunzipSync(packed));
  const patches=manifest.entries.filter(entry=>entry.extension==='.fxp');
  expect(Object.keys(metadata).sort()).toEqual(manifest.entries.filter(entry=>['.fxp','.wtscript','.srgfx','.modpreset'].includes(entry.extension)||entry.path.endsWith('.surge-skin/skin.xml')).map(entry=>entry.path).sort());
  for(const entry of manifest.entries.filter(entry=>['.srgfx','.modpreset'].includes(entry.extension)))
    expect(metadata[entry.path]).toBe(fs.readFileSync(path.join(source,entry.path),'utf8'));
  for(const entry of manifest.entries.filter(entry=>entry.path.endsWith('.surge-skin/skin.xml')))
    expect(metadata[entry.path]).toBe(fs.readFileSync(path.join(source,entry.path),'utf8'));
  for(const entry of manifest.entries.filter(entry=>entry.extension==='.wtscript'))
    expect(metadata[entry.path]).toBe(fs.readFileSync(path.join(source,entry.path),'utf8'));
  for(const entry of patches){
    const original=fs.readFileSync(path.join(source,entry.path));
    const xmlSize=original.readUInt32LE(64);
    expect(metadata[entry.path]).toBe(original.subarray(92,92+xmlSize).toString('utf8').replace(/\0+$/,''));
  }
});
async function open(page){
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await page.evaluate(async()=>{
    const {FactoryLibrary}=await import('/library.js');
    window.library=await FactoryLibrary.open('/library/manifest.json',message=>{window.cacheWarning=message});
  });
}
test('factory search fetches only selected assets and caches them across reloads',async({page})=>{
  const downloads=[];page.on('request',request=>{if(request.url().includes('/objects/'))downloads.push(request.url())});
  await open(page);
  expect(downloads).toEqual([]);
  const fixture=await page.evaluate(()=>library.search('templates init fm2','.fxp')[0].path);
  expect(fixture).toBe('patches_factory/Templates/Init FM2.fxp');
  const size=await page.evaluate(async file=>{
    const [a,b]=await Promise.all([library.bytes(file),library.bytes(file)]);
    return a.length===b.length?a.length:0;
  },fixture);
  expect(size).toBeGreaterThan(100);
  expect(downloads).toHaveLength(1);
  await page.reload();await open(page);
  await page.route('**/library/objects/**',route=>route.abort());
  expect(await page.evaluate(async file=>(await library.bytes(file)).length,fixture)).toBe(size);
  expect(downloads).toHaveLength(1);
});
test('corrupt download leaves existing filesystem content intact and allows retry',async({page})=>{
  await open(page);
  const file='patches_factory/Templates/Init FM2.fxp';
  await page.evaluate(file=>{Module.FS.mkdirTree('/factory/patches_factory/Templates');Module.FS.writeFile('/factory/'+file,'retain current file')},file);
  await page.route('**/library/objects/**',route=>route.fulfill({status:200,body:'invalid download'}));
  expect(await page.evaluate(async file=>{try{await library.install(file,Module.FS);return ''}catch(error){return error.message}},file)).toContain('Incomplete factory asset');
  expect(await page.evaluate(file=>Module.FS.readFile('/factory/'+file,{encoding:'utf8'}),file)).toBe('retain current file');
  await page.unroute('**/library/objects/**');
  expect(await page.evaluate(async file=>{const installed=await library.install(file,Module.FS);return Module.FS.stat(installed).size},file)).toBeGreaterThan(100);
});
test('corrupt cached bytes are replaced by a verified download',async({page})=>{
  await open(page);
  await page.evaluate(async()=>{
    const entry=library.search('templates init fm2','.fxp')[0];
    const cache=await caches.open('surge-factory-v1');
    await cache.put(new URL(entry.url,library.location),new Response('corrupt'));
  });
  expect(await page.evaluate(async()=>(await library.bytes(library.search('templates init fm2','.fxp')[0].path)).length)).toBeGreaterThan(100);
});

test('cache quota failure still returns verified asset bytes with a warning',async({page})=>{
  await open(page);
  const result=await page.evaluate(async()=>{
    Cache.prototype.put=async()=>{throw new DOMException('Cache full','QuotaExceededError')};
    const bytes=await library.bytes(library.search('templates init fm2','.fxp')[0].path);
    return {size:bytes.length,warning:window.cacheWarning};
  });
  expect(result.size).toBeGreaterThan(100);
  expect(result.warning).toContain('Factory cache write failed');
});

test('missing download preserves the existing file and reports failure',async({page})=>{
  await open(page);
  await page.route('**/library/objects/**',route=>route.fulfill({status:503,body:'Unavailable'}));
  const result=await page.evaluate(async()=>{
    const file=library.search('templates init fm2','.fxp')[0].path;
    Module.FS.mkdirTree('/factory/patches_factory/Templates');
    Module.FS.writeFile('/factory/'+file,'keep me');
    let error='';try{await library.install(file,Module.FS)}catch(e){error=e.message}
    return {error,contents:Module.FS.readFile('/factory/'+file,{encoding:'utf8'})};
  });
  expect(result.error).toContain('503');
  expect(result.contents).toBe('keep me');
});
