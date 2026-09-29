import {test,expect} from './fixtures.js';
for(const file of ['surge-juce-browser-check.js','text-input.js']) {
  test(`failed bootstrap download ${file} reports the failure and supports reload`,async({page})=>{
    await page.route('**/'+file,route=>route.abort('connectionfailed'));
    await page.goto('/surge-juce-browser-check.html');
    await expect(page.locator('#status')).toContainText('Unable to download application file '+file);
    await expect(page.locator('canvas')).toHaveCount(0);
    await page.unroute('**/'+file);
    await page.reload();
    await expect(page.locator('canvas')).toBeVisible();
    await expect(page.locator('#status')).toHaveText('');
  });
}
