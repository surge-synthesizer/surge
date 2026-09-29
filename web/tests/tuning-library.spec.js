import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
async function open(page,editor=false){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  if(editor){
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+t');
    await page.getByRole('button',{name:'Tuning Library',exact:true}).dispatchEvent('click');
  }else{
    await page.getByRole('button',{name:'Main Menu',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Tuning',exact:true}).dispatchEvent('click');
    await page.getByRole('menuitem',{name:'Factory Tuning Library...',exact:true}).dispatchEvent('click');
  }
  await expect(page.getByRole('dialog',{name:'Factory tuning library',exact:true})).toBeVisible();
  await expect(page.locator('#surge-tuning-library [role=status]')).toHaveText(/\d+ files/);
}
test('factory tuning menu searches the complete catalog and downloads exact files on demand',async({page})=>{
  const downloads=[];page.on('request',request=>{if(request.url().includes('/library/objects/'))downloads.push(request.url());});
  await open(page);
  const entries=await page.evaluate(()=>[...SurgeFactory.library.entries.values()].filter(e=>e.path.startsWith('tuning_library/')));
  expect(await page.locator('#surge-tuning-library li').count()).toBe(entries.length);
  expect(downloads.some(url=>entries.some(e=>url.endsWith(e.sha256)))).toBe(false);
  for(const extension of ['.scl','.kbm','.txt']){
    const entry=entries.find(e=>e.path.toLowerCase().endsWith(extension));expect(entry).toBeTruthy();
    const relative=entry.path.slice('tuning_library/'.length);
    await page.getByRole('searchbox',{name:'Search factory tunings'}).fill(relative);
    const pending=page.waitForEvent('download');
    await page.getByRole('button',{name:'Download '+relative,exact:true}).click();
    const download=await pending;
    expect(download.suggestedFilename()).toBe(entry.path.split('/').pop());
    expect(readFileSync(await download.path())).toEqual(readFileSync('../resources/data/'+entry.path));
  }
  await page.getByRole('button',{name:'Close tuning library',exact:true}).click();
  await expect(page.locator('#surge-tuning-library')).toHaveCount(0);
});
test('tuning editor library reports a failed download and allows retry without changing tuning',async({page})=>{
  await open(page,true);
  const entry=await page.evaluate(()=>[...SurgeFactory.library.entries.values()].find(e=>e.path.startsWith('tuning_library/')&&e.extension==='.scl'));
  const url='**/library/objects/'+entry.sha256;
  await page.route(url,route=>route.fulfill({status:503,body:'Unavailable'}));
  await page.getByRole('searchbox',{name:'Search factory tunings'}).fill(entry.path.slice('tuning_library/'.length));
  const button=page.getByRole('button',{name:'Download '+entry.path.slice('tuning_library/'.length),exact:true});
  await button.click();await expect(page.locator('#surge-tuning-library [role=alert]')).toContainText('503');
  await expect(button).toBeEnabled();await page.unroute(url);
  const pending=page.waitForEvent('download');await button.click();await pending;
  await expect(page.locator('#surge-tuning-library [role=alert]')).toBeEmpty();
  await page.keyboard.press('Escape');await expect(page.locator('#surge-tuning-library')).toHaveCount(0);
  await expect(page.getByRole('textbox',{name:'Scala Scale',exact:true})).toHaveValue(/12 Tone Equal Temperament/);
});
