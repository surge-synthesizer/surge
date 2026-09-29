import {test,expect} from './fixtures.js';
const fixture='A browser file read by JUCE — ✓';
async function start(page) {
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible();
  await page.evaluate(()=>SurgeBrowser.ready);
}
test('picker selection reaches JUCE and survives reload in IndexedDB',async({page})=>{
  await start(page);
  await page.evaluate(text=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([text],'notes.txt')}];},fixture);
  await page.locator('canvas').click({position:{x:130,y:240}});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_check_selected_text','string',[],[]))).toBe(fixture);
  const selected=await page.evaluate(()=>Module.ccall('surge_check_selected_file','string',[],[]));
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.locator('canvas')).toBeVisible();
  expect(await page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),selected)).toBe(fixture);
});
test('JUCE export writes through the selected browser file handle',async({page})=>{
  await start(page);
  await page.evaluate(()=>{window.exported=null;window.showSaveFilePicker=async()=>({name:'test.txt',createWritable:async()=>({write:async bytes=>{window.exported=new TextDecoder().decode(bytes)},close:async()=>{},abort:async()=>{}})});});
  await page.locator('canvas').click({position:{x:420,y:240}});
  await expect.poll(()=>page.evaluate(()=>window.exported)).toBe('JUCE export ✓');
});
test('denied file permission reports an error without returning a selection',async({page})=>{
  await start(page);
  await page.evaluate(()=>{window.showOpenFilePicker=async()=>{throw new DOMException('Permission denied','NotAllowedError')};});
  await page.locator('canvas').click({position:{x:130,y:240}});
  await expect(page.locator('#file-status')).toContainText('Permission denied');
  expect(await page.evaluate(()=>Module.ccall('surge_check_selected_file','string',[],[]))).toBe('');
});
test('storage failure preserves in-memory data and a retry persists it',async({page})=>{
  await start(page);
  await page.evaluate(()=>{
    window.realSync=Module.FS.syncfs;
    Module.FS.syncfs=(_populate,callback)=>callback(new DOMException('Test quota failure','QuotaExceededError'));
    Module.FS.writeFile('/user/retained.txt','Retain this data');
  });
  await expect(page.locator('#storage-status')).toContainText('Changes remain in memory');
  expect(await page.evaluate(()=>Module.FS.readFile('/user/retained.txt',{encoding:'utf8'}))).toBe('Retain this data');
  await page.evaluate(()=>{Module.FS.syncfs=window.realSync;});
  await page.getByRole('button',{name:'Retry saving'}).click();
  await expect(page.locator('#storage-status')).toBeEmpty();
  await page.reload();await expect(page.locator('canvas')).toBeVisible();
  expect(await page.evaluate(()=>Module.FS.readFile('/user/retained.txt',{encoding:'utf8'}))).toBe('Retain this data');
});

test('canceling a picker keeps the previous JUCE selection',async({page})=>{
  await start(page);
  await page.evaluate(text=>{window.showOpenFilePicker=async()=>[{getFile:async()=>new File([text],'kept.txt')}];},fixture);
  await page.locator('canvas').click({position:{x:130,y:240}});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_check_selected_text','string',[],[]))).toBe(fixture);
  await page.evaluate(()=>{window.showOpenFilePicker=async()=>{throw new DOMException('Canceled','AbortError')};});
  await page.locator('canvas').click({position:{x:130,y:240}});
  expect(await page.evaluate(()=>Module.ccall('surge_check_selected_text','string',[],[]))).toBe(fixture);
  await expect(page.locator('#storage-status')).toBeEmpty();
});

test('failed export reports the error and retains the generated file',async({page})=>{
  await start(page);
  await page.evaluate(()=>{window.aborted=false;window.showSaveFilePicker=async()=>({name:'failed.txt',createWritable:async()=>({write:async()=>{throw Error('Disk write failed')},close:async()=>{},abort:async()=>{window.aborted=true}})});});
  await page.locator('canvas').click({position:{x:420,y:240}});
  await expect(page.locator('#file-status')).toContainText('Disk write failed');
  expect(await page.evaluate(()=>window.aborted)).toBe(true);
  await page.evaluate(()=>SurgeBrowser.flush());
  await expect(page.locator('#file-status')).toContainText('Disk write failed');
  expect(await page.evaluate(()=>{
    const root='/user/exports';const folder=Module.FS.readdir(root).find(x=>x!=='.'&&x!=='..');
    return Module.FS.readFile(root+'/'+folder+'/failed.txt',{encoding:'utf8'});
  })).toBe('JUCE export ✓');
});

for(const mode of ['missing','ambiguous','renamed-write-failure'])test(`export handoff handles ${mode} output without losing generated files`,async({page})=>{
  await start(page);
  await page.evaluate(mode=>{
    globalThis.exportWriterOpened=false;globalThis.exportAborted=false;globalThis.generatedPaths=[];
    window.showSaveFilePicker=async()=>({name:'chosen',createWritable:async()=>{
      exportWriterOpened=true;return {write:async()=>{throw Error('Normalized output disk failure');},close:async()=>{},abort:async()=>{exportAborted=true;}};
    }});
    const call=Module.ccall;
    Module.ccall=function(name,returnType,types,args){
      const result=call.call(this,name,returnType,types,args);
      if(name==='surge_file_dialog_complete' && result){
        const [path]=JSON.parse(args[1]);
        if(mode==='missing')Module.FS.unlink(path);
        else{
          const normalized=path+'.txt';Module.FS.rename(path,normalized);generatedPaths.push(normalized);
          if(mode==='ambiguous'){Module.FS.writeFile(path+'.other','Other export');generatedPaths.push(path+'.other');}
        }
      }
      return result;
    };
  },mode);
  await page.locator('canvas').click({position:{x:420,y:240}});
  const message=mode==='missing'?'did not produce an export file':mode==='ambiguous'?'produced multiple export files':'Normalized output disk failure';
  await expect(page.locator('#file-status')).toContainText(message);
  expect(await page.evaluate(()=>exportWriterOpened)).toBe(mode==='renamed-write-failure');
  expect(await page.evaluate(()=>exportAborted)).toBe(mode==='renamed-write-failure');
  const retained=await page.evaluate(()=>generatedPaths.map(path=>({path,text:Module.FS.readFile(path,{encoding:'utf8'})})));
  if(mode!=='missing')expect(retained[0].text).toBe('JUCE export ✓');
  if(mode==='ambiguous')expect(retained[1].text).toBe('Other export');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await expect(page.locator('canvas')).toBeVisible();
  for(const file of retained)expect(await page.evaluate(path=>Module.FS.readFile(path,{encoding:'utf8'}),file.path)).toBe(file.text);
});
