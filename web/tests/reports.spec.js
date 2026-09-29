import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
async function diagnostic(page) {
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible({timeout:60000});
  await page.locator('canvas').click({position:{x:15,y:15}});
}
test('reports display in a sandbox and restore editor focus when closed',async({page})=>{
  await diagnostic(page);
  await page.evaluate(()=>SurgeReports.show('<h1>Patch details</h1><script>parent.reportExecuted=true</script><p>Scene A</p>'));
  await expect(page.getByRole('dialog',{name:'Surge report',exact:true})).toBeVisible();
  await expect(page.frameLocator('#surge-report iframe').getByRole('heading',{name:'Patch details'})).toBeVisible();
  expect(await page.locator('#surge-report iframe').getAttribute('sandbox')).toBe('');
  expect(await page.evaluate(()=>globalThis.reportExecuted)).toBeUndefined();
  await page.keyboard.press('Escape');
  await expect(page.locator('#surge-report')).toHaveCount(0);
  await expect(page.locator('canvas')).toBeFocused();
});
test('report download preserves generated HTML and a failed download keeps the report visible',async({page})=>{
  await diagnostic(page);
  const html='<!doctype html><title>Surge report</title><h1>Tuning ✓</h1><p>440 Hz</p>';
  await page.evaluate(html=>SurgeReports.show(html),html);
  const received=page.waitForEvent('download');
  await page.getByRole('button',{name:'Download report',exact:true}).click();
  const download=await received;
  expect(download.suggestedFilename()).toBe('surge-report.html');
  expect(readFileSync(await download.path(),'utf8')).toBe(html);
  await page.getByRole('button',{name:'Close report',exact:true}).click();
  await page.evaluate(()=>{URL.createObjectURL=()=>{throw new Error('Storage unavailable')};SurgeReports.show('<h1>Keep this report</h1>')});
  await page.getByRole('button',{name:'Download report',exact:true}).click();
  await expect(page.locator('#surge-report [role=alert]')).toContainText('Storage unavailable');
  await expect(page.frameLocator('#surge-report iframe').getByRole('heading',{name:'Keep this report'})).toBeVisible();
});
test('original JUCE patch report exports the current engine parameters',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  // Open the original canvas menu, then its Patch Settings submenu.
  await page.mouse.click(872,558);
  await expect(page.locator('canvas')).toHaveCount(2);
  await page.mouse.move(940,231);
  await expect(page.locator('canvas')).toHaveCount(3);
  // JUCE can reposition the submenu as it opens. Target its last row relative
  // to the actual canvas bounds; a fixed screen coordinate can land below it.
  const submenu=page.locator('canvas').last();
  const bounds=await submenu.boundingBox();
  const position={x:bounds.width/2,y:bounds.height-12};
  await submenu.hover({position});
  await page.waitForTimeout(350); // JUCE suppresses clicks during popup creation.
  await submenu.click({position});
  await expect(page.getByRole('dialog',{name:'Surge report',exact:true})).toBeVisible();
  const body=page.frameLocator('#surge-report iframe').locator('body');
  await expect(body).toContainText('Patch Name: Init Saw');
  await expect(body).toContainText('Oscillator 1: Classic');
  await expect(body).toContainText('Polyphony Limit: 16');
  const received=page.waitForEvent('download');
  await page.getByRole('button',{name:'Download report',exact:true}).click();
  const html=readFileSync(await (await received).path(),'utf8');
  expect(html).toContain('Init Saw');
  expect(html).toContain('Polyphony Limit');
  await page.getByRole('button',{name:'Close report',exact:true}).click();
  await expect(page.locator('#surge-report')).toHaveCount(0);
  await expect(page.locator('canvas')).toHaveCount(1);
  await expect(page.locator('canvas')).toBeFocused();
});
