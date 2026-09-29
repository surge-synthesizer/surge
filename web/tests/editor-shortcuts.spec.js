import {test,expect} from './fixtures.js';

test('scene and oscillator shortcuts select the original controls and retain independent edits',async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.addInitScript(()=>{
    globalThis.navigationShortcutEvents=[];
    document.addEventListener('keydown',event=>{
      if(event.altKey && ['Digit1','Digit2','Digit3','KeyS'].includes(event.code))
        navigationShortcutEvents.push({code:event.code,prevented:event.defaultPrevented});
    });
  });
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  const canvas=page.locator('canvas').first();
  const shortcut=async key=>{await canvas.focus();await page.keyboard.press('Alt+'+key);};
  const select=async(scene,osc)=>{
    await shortcut(String(osc));
    await expect(page.getByRole('group',{name:'Oscillator Select',exact:true}))
      .toHaveAttribute('aria-valuenow',String(osc-1));
    const pitch=page.getByRole('slider',{name:`Scene ${scene} Osc ${osc} Pitch`,exact:true});
    await expect(pitch).toBeAttached();return pitch;
  };
  const values={};
  for(const scene of ['A','B']){
    for(const osc of [2,3,1]){
      const pitch=await select(scene,osc);
      await expect(pitch).toHaveAttribute('aria-valuenow','0.5');
      await pitch.focus();
      await page.keyboard.press(scene==='A'?'Home':'End');
      const expected=scene==='A'?'0':'1';
      await expect(pitch).toHaveAttribute('aria-valuenow',expected);
      values[`${scene}${osc}`]=expected;
    }
    await shortcut('s');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(scene==='A'?1:0);
  }
  // Returning to every panel must expose its own edited parameter, not the
  // last oscillator or the other scene's value.
  for(const scene of ['A','B']){
    for(const osc of [1,2,3])
      await expect(await select(scene,osc)).toHaveAttribute('aria-valuenow',values[`${scene}${osc}`]);
    await shortcut('s');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_scene())).toBe(scene==='A'?1:0);
  }
  const events=await page.evaluate(()=>navigationShortcutEvents);
  expect(events).toHaveLength(16);
  expect(events.every(event=>event.prevented)).toBe(true);
  expect(errors).toEqual([]);
});
