import {test,expect} from './fixtures.js';
async function start(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null),{timeout:30000}).toBe('Init Saw');
}
async function select(page,name){
  expect(await page.evaluate(name=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/'+name+'.fxp']),name)).toBe(1);
}
test('original engine indexes the full factory patch catalog and lazily selects a patch',async({page})=>{
  const objects=[];page.on('request',r=>{if(r.url().includes('/library/objects/'))objects.push(r.url())});
  await start(page);
  expect(await page.evaluate(()=>Module._surge_browser_patch_count())).toBe(3561);
  expect(objects).toHaveLength(3); // Search metadata, initial wavetable and patch.
  await select(page,'Init FM2');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  expect(objects).toHaveLength(4);
});
test('failed factory patch download retains the original patch and can be retried',async({page})=>{
  await start(page);
  await page.route('**/library/objects/**',route=>route.fulfill({status:503,body:'Unavailable'}));
  await select(page,'Init FM2');
  await expect(page.locator('#file-status')).toContainText('current patch retained');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.unroute('**/library/objects/**');
  await select(page,'Init FM2');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
});
test('a slow older selection cannot replace a newer completed patch',async({page})=>{
  await start(page);
  const first=await page.evaluate(()=>SurgeFactory.library.entries.get('patches_factory/Templates/Init FM2.fxp').url);
  let release;
  const held=new Promise(resolve=>{release=resolve});
  let requested;
  const seen=new Promise(resolve=>{requested=resolve});
  await page.route('**/library/'+first,async route=>{requested();await held;await route.continue()});
  await select(page,'Init FM2');await seen;
  await select(page,'Init Sine');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  release();
  await expect.poll(()=>page.evaluate(()=>SurgeFactory.library.pending.size)).toBe(0);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
});

test('native patch search indexes metadata without fetching all patch files',async({page})=>{
  const downloads=[];page.on('request',r=>{if(r.url().includes('/library/objects/'))downloads.push(r.url())});
  await start(page);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
  expect(downloads).toHaveLength(3);
});

test('Find Patch and Favorite Patch shortcuts reach JUCE and suppress browser defaults',async({page})=>{
  await page.addInitScript(()=>{
    globalThis.patchShortcutEvents=[];
    document.addEventListener('keydown',event=>{
      if(event.keyCode===70)patchShortcutEvents.push({ctrl:event.ctrlKey,meta:event.metaKey,alt:event.altKey,prevented:event.defaultPrevented});
    });
  });
  await start(page);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
  await select(page,'Init FM2');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  const canvas=page.locator('canvas').first();
  for(const modifier of ['Control','Meta']){
    await canvas.focus();await page.keyboard.press(modifier+'+f');
    const search=page.getByRole('textbox',{name:'Patch select',exact:true});
    await expect(search).toBeAttached();await search.focus();await page.keyboard.press('Escape');
    await expect(search).toHaveCount(0);
  }
  for(const count of [1,0]){
    await canvas.focus();await page.keyboard.press('Alt+f');
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['FAV=1'])),{timeout:30000}).toBe(count);
    await expect(page.getByRole('button',{name:count?'Remove from Favorites':'Add to Favorites',exact:true})).toBeAttached();
  }
  expect(await page.evaluate(()=>patchShortcutEvents)).toEqual([
    {ctrl:true,meta:false,alt:false,prevented:true},
    {ctrl:false,meta:true,alt:false,prevented:true},
    {ctrl:false,meta:false,alt:true,prevented:true},
    {ctrl:false,meta:false,alt:true,prevented:true}
  ]);
});

test('accessible patch search loads results and favorite removal survives reload',async({page})=>{
  test.setTimeout(90000);
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  page.on('console',message=>{if(message.text().startsWith('Surge Error'))errors.push(message.text());});
  await start(page);
  const count=query=>page.evaluate(query=>Module.ccall('surge_browser_search_count','number',['string'],[query]),query);
  await expect.poll(()=>count('Init FM2'),{timeout:30000}).toBe(1);
  await page.getByRole('button',{name:'Open patch search',exact:true}).dispatchEvent('click');
  const search=page.getByRole('textbox',{name:'Patch select',exact:true});
  await expect(search).toBeAttached();await search.fill('__no_such_patch_73915__');
  await search.press('Escape');await expect(search).toHaveCount(0);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.getByRole('button',{name:'Open patch search',exact:true}).dispatchEvent('click');
  await search.fill('Init FM2');
  // IME confirmation belongs to the browser until composition finishes; it
  // must not dismiss the result list or select a different patch.
  await search.dispatchEvent('keydown',{key:'Enter',code:'Enter',keyCode:13,isComposing:true});
  await expect(search).toHaveValue('Init FM2');
  await search.press('ArrowDown');
  await expect(page.getByRole('list',{name:'Patch select',exact:true})).toBeFocused();
  await page.keyboard.press('Enter');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  await page.getByRole('button',{name:'Add to Favorites',exact:true}).dispatchEvent('click');
  await expect.poll(()=>count('FAV=1'),{timeout:30000}).toBe(1);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await expect.poll(()=>count('FAV=1'),{timeout:30000}).toBe(1);
  await page.getByRole('button',{name:'Add to Favorites',exact:true}).focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Init FM2',exact:true}).dispatchEvent('click');
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
  await page.getByRole('button',{name:'Remove from Favorites',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('button',{name:'Add to Favorites',exact:true})).toBeAttached();
  await expect.poll(()=>count('FAV=1'),{timeout:30000}).toBe(0);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect.poll(()=>page.evaluate(()=>typeof globalThis.Module?._surge_browser_patch_name==='function' ? Module.ccall('surge_browser_patch_name','string',[],[]) : null)).toBe('Init Saw');
  await expect.poll(()=>count('FAV=1'),{timeout:30000}).toBe(0);
  expect(errors).toEqual([]);
});

test('original JUCE category menu selects a downloadable factory patch',async({page})=>{
  await start(page);
  await page.locator('canvas').first().click({position:{x:350,y:26}});
  await expect(page.locator('canvas')).toHaveCount(2);
  await page.mouse.move(260,365); // Templates in the original category menu.
  await expect(page.locator('canvas')).toHaveCount(3);
  await page.screenshot({path:'test-results/factory-patch-menu.png'});
  // JUCE ignores releases during the first 250 ms of a popup's lifetime.
  await page.waitForTimeout(300);
  await page.mouse.click(395,550); // Init FM2 in the native Templates submenu.
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
});

test('native popup entries remain selectable above browser transport controls',async({page})=>{
  await start(page);
  await page.locator('canvas').first().click({position:{x:350,y:26}});
  await expect(page.locator('canvas')).toHaveCount(2);
  await page.mouse.move(260,365);
  await expect(page.locator('canvas')).toHaveCount(3);
  await page.waitForTimeout(300); // JUCE popup release guard.
  expect(await page.evaluate(()=>document.elementFromPoint(395,665)?.tagName)).toBe('CANVAS');
  await page.mouse.click(395,665); // Init Sine, overlapping the browser transport strip.
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
});
