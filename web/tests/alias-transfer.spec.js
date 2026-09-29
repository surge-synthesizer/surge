import {test,expect} from './fixtures.js';
const harmonics=page=>page.getByRole('slider',{name:/^Harmonic \d+$/}).evaluateAll(nodes=>nodes.map(n=>Number(n.getAttribute('aria-valuenow'))));
async function expectHarmonics(page,expected){
  await expect.poll(async()=>{
    const values=await harmonics(page);
    if(!values.length){
      // Paste and the native refresh queue can rebuild the panel separately.
      // Reopen a retired view; do not alter coefficients to satisfy the check.
      await page.getByRole('button',{name:'Open Custom Editor',exact:true}).evaluateAll(nodes=>nodes[0]?.click());
    }
    return values;
  }).toEqual(expected);
}
async function additive(page,osc){
  await page.getByRole('button',{name:'Oscillator Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Alias',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:`Scene A Osc ${osc} Wrap`,exact:true})).toBeAttached();
  await page.getByRole('slider',{name:`Scene A Osc ${osc} Shape`,exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Harmonic 1',exact:true})).toBeAttached();
}
for(const active of [false,true])test(`Alias copy and type reset publish complete coefficients with audio ${active?'running':'inactive'}`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  if(active){
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  }
  await additive(page,1);
  await page.getByRole('slider',{name:'Harmonic 3',exact:true}).press('End');
  const source=await harmonics(page);expect(source[2]).toBe(-1);
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  const select=page.getByRole('group',{name:'Oscillator Select',exact:true});
  await select.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Copy from Osc 1',exact:true}).dispatchEvent('click');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+2');
  await expect(select).toHaveAttribute('aria-valuenow','1');
  await additive(page,2);
  await page.getByRole('slider',{name:'Harmonic 4',exact:true}).press('End');
  const target=await harmonics(page);expect(target).not.toEqual(source);
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.waitForTimeout(225); // Separate setup from the native undo coalescing window.
  const oldEditorButton=await page.getByRole('button',{name:'Open Custom Editor',exact:true}).getAttribute('data-juce-accessible-id');
  await select.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Paste to Osc 2',exact:true}).dispatchEvent('click');
  // Both oscillators already use Alias, so the existing Wrap control alone
  // cannot signal completion of paste's queued panel rebuild.
  await expect(page.getByRole('button',{name:'Open Custom Editor',exact:true})).not.toHaveAttribute('data-juce-accessible-id',oldEditorButton);
  await expect(page.getByRole('slider',{name:'Scene A Osc 2 Wrap',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await expectHarmonics(page,source);
  for(const [action,expected] of [['Undo',target],['Redo',source]]){
    await page.getByRole('button',{name:action,exact:true}).dispatchEvent('click');
    await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
    await expectHarmonics(page,expected);
  }
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Oscillator Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Classic',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Sawtooth',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
  await expect(page.getByRole('slider',{name:'Scene A Osc 2 Wrap',exact:true})).toHaveCount(0);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Scene A Osc 2 Wrap',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await expectHarmonics(page,source);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Scene A Osc 2 Wrap',exact:true})).toHaveCount(0);
  await additive(page,2);
  await expect.poll(()=>harmonics(page).then(a=>a.length)).toBe(16);
  const reset=await harmonics(page);
  for(let i=0;i<16;i++)expect(reset[i]).toBeCloseTo(1/(i+1),6);
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+1');
  await expect(select).toHaveAttribute('aria-valuenow','0');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await expectHarmonics(page,source);
});
