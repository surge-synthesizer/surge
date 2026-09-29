import {test,expect} from './fixtures.js';

test('patch search can be canceled across startup indexing and reload',async({page},testInfo)=>{
  test.setTimeout(90000);
  const events=[];
  await page.exposeFunction('recordSearchKey',event=>events.push(event));
  try{
    for(let reload=0;reload<4;reload++){
      await page.goto('/surge-xt-browser.html');
      await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
      await page.evaluate(()=>{
        const original=Module._surge_accessibility_key;
        Module._surge_accessibility_key=(id,code,flags)=>{
          const focus=[];
          const visit=n=>{if(n.focused)focus.push({id:n.id,label:n.label});n.children?.forEach(visit);};
          JSON.parse(Module.ccall('surge_accessibility_snapshot','string',[],[])).forEach(visit);
          const dom=document.activeElement?.dataset.juceAccessibleId;
          const result=original(id,code,flags);
          recordSearchKey({id,code,flags,dom,focus,result});return result;
        };
      });
      for(let repeat=0;repeat<8;repeat++){
        await page.locator('canvas').first().focus();await page.keyboard.press('Control+f');
        const search=page.getByRole('textbox',{name:'Patch select',exact:true});
        await expect(search).toBeAttached();
        events.push({reload,repeat,disabled:await search.isDisabled()});
        await search.press('Escape');await expect(search).toHaveCount(0);
      }
      await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_search_count','number',['string'],['Init FM2'])),{timeout:30000}).toBe(1);
      await expect(page.getByRole('textbox',{name:'Patch select',exact:true})).toHaveCount(0);
    }
    // Keep this a regression for the unavailable search field, rather than
    // silently passing if every attempt happens after catalog indexing.
    const disabledCount=events.filter(event=>event.disabled===true).length;
    expect(disabledCount).toBeGreaterThan(0);
    testInfo.annotations.push({type:'indexing-cancellations',description:String(disabledCount)});
  }finally{
    await testInfo.attach('search-focus-events',{body:JSON.stringify(events,null,2),contentType:'application/json'});
  }
});
