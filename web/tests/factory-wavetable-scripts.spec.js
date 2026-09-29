import {test,expect} from './fixtures.js';
import {readdirSync,readFileSync} from 'node:fs';
const folder=new URL('../../resources/data/wavetables/Scripted/Additive/',import.meta.url);
const scripts=readdirSync(folder).filter(name=>name.endsWith('.wtscript')).sort().map(file=>{
  const xml=readFileSync(new URL(file,folder),'utf8');
  return {name:file.slice(0,-9),source:Buffer.from(xml.match(/lua="([^"]+)"/)[1],'base64').toString(),
    frames:Number(xml.match(/frames="(\d+)"/)[1]),size:32*2**(Number(xml.match(/samples="(\d+)"/)[1])-1)};
});
for(const item of scripts)test(`factory script menu loads ${item.name}`,async({page})=>{
  test.setTimeout(120000);
  page.setDefaultTimeout(10000);
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:item.name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(item.source.replace(/\r\n?/g,'\n'));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0])),{timeout:90000}).toBe(item.name);
  await expect.poll(()=>page.evaluate(()=>[Module._surge_browser_wt_size(0),Module._surge_browser_wt_frames(0)]),{timeout:90000}).toEqual([item.size,item.frames]);
  const result=await page.evaluate(()=>{
    const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
    let finite=true,energy=0;
    for(let frame=0;frame<frames;frame++)for(let i=0;i<size;i++){
      const v=Module._surge_browser_wt_sample(0,frame,i);finite=finite&&Number.isFinite(v);energy+=v*v;
    }
    return {size,frames,finite,energy};
  });
  expect(result.size).toBe(item.size);expect(result.frames).toBe(item.frames);
  expect(result.finite).toBe(true);expect(result.energy).toBeGreaterThan(0);
});

async function openEditor(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init Wavetable.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Wavetable');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toBeAttached();
}
const menuEntry=(page,name)=>page.getByRole('menuitem',{name,exact:true}).or(page.getByRole('menuitemcheckbox',{name:name+' (Checked)',exact:true}));
async function menuCategory(page,category){
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await menuEntry(page,category).dispatchEvent('click');
}
test('factory script menu supports keyboard activation and checked-item reselection',async({page})=>{
  page.setDefaultTimeout(10000);await openEditor(page);
  const item=scripts.find(s=>s.name==='Sine to Triangle');
  await menuCategory(page,'Additive');
  await page.getByRole('menuitem',{name:item.name,exact:true}).press('Enter');
  const code=page.getByRole('textbox',{name:'Wavetable Code',exact:true});
  await expect(code).toHaveValue(item.source.replace(/\r\n?/g,'\n'));
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(item.frames);
  await code.fill('-- unapplied replacement');
  await menuCategory(page,'Additive');
  await page.getByRole('menuitemcheckbox',{name:item.name+' (Checked)',exact:true}).press('Enter');
  await expect(code).toHaveValue(item.source.replace(/\r\n?/g,'\n'));
  await expect(page.getByRole('menuitem')).toHaveCount(0);
});
for(const origin of ['script editor','oscillator'])test(`saved user script categories reload through the ${origin} menu`,async({page})=>{
  page.setDefaultTimeout(10000);await openEditor(page);
  const source='function init(wt) wt.name="User Menu" return wt end\nfunction generate(wt) local r={} for i=1,wt.sample_count do r[i]=0.125 end return r end';
  await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).fill(source);
  await page.getByRole('button',{name:'Generate',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('User Menu');
  await page.getByRole('button',{name:'Wavetable Script Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Save as .wtscript...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill('Browser Tests/Nested/User Menu');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.FS.analyzePath('/user/Wavetables/Scripted/Browser Tests/Nested/User Menu.wtscript').exists)).toBe(true);
  await page.evaluate(()=>SurgeBrowser.flush());await openEditor(page);
  if(origin==='script editor')await menuCategory(page,'Browser Tests');
  else{
    await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
    await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
    await expect(menuEntry(page,'Scripted')).toHaveCount(2);
    await menuEntry(page,'Scripted').last().dispatchEvent('click');
    await menuEntry(page,'Browser Tests').dispatchEvent('click');
  }
  await menuEntry(page,'Nested').dispatchEvent('click');
  await page.getByRole('menuitem',{name:'User Menu',exact:true}).press('Enter');
  if(origin==='oscillator'){await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');}
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(source);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('User Menu');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_sample(0,0,0))).toBe(0.125);
  const values=await page.evaluate(()=>{
    const r=[];for(let f=0;f<Module._surge_browser_wt_frames(0);f++)for(let i=0;i<Module._surge_browser_wt_size(0);i++)r.push(Module._surge_browser_wt_sample(0,f,i));return r;
  });
  expect(values.length).toBeGreaterThan(0);expect(values.every(v=>v===0.125)).toBe(true);
});

for(const input of ['accessible','keyboard','pointer'])test(`oscillator wavetable menu loads scripted entries through ${input} input`,async({page})=>{
  page.setDefaultTimeout(10000);await openEditor(page);
  await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
  await menuEntry(page,'Scripted').dispatchEvent('click');
  await menuEntry(page,'Additive').dispatchEvent('click');
  const entry=menuEntry(page,'Sine to Triangle');
  if(input==='keyboard')await entry.press('Enter');
  else if(input==='pointer'){
    const bounds=await entry.boundingBox();expect(bounds).not.toBeNull();
    // JUCE standard menus suppress release during their first 250 ms.
    await page.mouse.click(bounds.x+bounds.width/2,bounds.y+bounds.height/2,{delay:300});
  }else await entry.dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_wt_name','string',['number'],[0]))).toBe('Sine to Triangle');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(50);
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(scripts.find(s=>s.name==='Sine to Triangle').source.replace(/\r\n?/g,'\n'));
});

async function tableState(page){return page.evaluate(()=>{
  const size=Module._surge_browser_wt_size(0),frames=Module._surge_browser_wt_frames(0);
  return {name:Module.ccall('surge_browser_wt_name','string',['number'],[0]),size,frames,
    samples:Array.from({length:size*frames},(_,i)=>Module._surge_browser_wt_sample(0,Math.floor(i/size),i%size))};
});}
for(const origin of ['script editor','oscillator'])test(`${origin} script selection restores exact tables through undo and redo`,async({page})=>{
  page.setDefaultTimeout(10000);await openEditor(page);
  const before=await tableState(page),originalSource=await page.getByRole('textbox',{name:'Wavetable Code',exact:true}).inputValue();
  if(origin==='script editor')await menuCategory(page,'Additive');
  else{
    await page.getByRole('group',{name:'Osc 1 Wavetable Script Editor',exact:true}).getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
    await page.getByRole('button',{name:/^Wavetable: /}).dispatchEvent('click');
    await menuEntry(page,'Scripted').dispatchEvent('click');await menuEntry(page,'Additive').dispatchEvent('click');
  }
  await menuEntry(page,'Sine to Triangle').dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(50);
  const selected=await tableState(page);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(before.frames);
  expect(await tableState(page)).toEqual(before);
  if(origin==='script editor')await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(originalSource);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_wt_frames(0))).toBe(selected.frames);
  expect(await tableState(page)).toEqual(selected);
  if(origin==='oscillator'){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+w');
  }
  await expect(page.getByRole('textbox',{name:'Wavetable Code',exact:true})).toHaveValue(scripts.find(s=>s.name==='Sine to Triangle').source.replace(/\r\n?/g,'\n'));
});
