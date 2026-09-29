import {test,expect} from './fixtures.js';

for(const [chord,description] of [['Alt+Shift+f',/Shift/],['Alt+Shift+ArrowLeft',/Shift/],['F4',/F4/],['Enter',/return/i]])
test(`accessible Learn button captures ${chord} without moving focus to the canvas`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+b');
  const overlay=page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first();
  const row=overlay.getByRole('listitem',{name:'Find Patch',exact:true});
  const learn=row.getByRole('button',{name:'Learn Find Patch',exact:true});
  await learn.focus();await page.keyboard.press('Enter');
  await expect(learn).toBeFocused();
  await learn.dispatchEvent('keydown',{key:'f',code:'KeyF',keyCode:70,isComposing:true});
  await expect(row).toContainText('Ctrl + F');
  await page.keyboard.press(chord);
  await expect(row).toContainText(description);await expect(learn).toBeFocused();
  await overlay.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(overlay).toHaveCount(0);
  if(chord==='Enter'){
    // JUCE gives the focused button normal Enter activation before global
    // shortcuts. Verify that learning Enter does not replace that behavior.
    await page.getByRole('button',{name:'Open Modulation Overview',exact:true}).focus();
    await page.locator('canvas').first().focus();await page.keyboard.press('Enter');
    await expect(page.getByRole('group',{name:'Modulation List',exact:true}).first()).toBeAttached();
    return;
  }
  await page.locator('canvas').first().focus();await page.keyboard.press(chord);
  await expect(page.getByRole('textbox',{name:'Patch select',exact:true})).toBeAttached();
});

test('original shortcut editor learns, cancels, persists and resets a binding',async({page})=>{
  const errors=[];page.on('pageerror',error=>errors.push(error.message));
  const ready=()=>expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await page.goto('/surge-xt-browser.html');await ready();
  const canvas=page.locator('canvas').first();
  const overlay=page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first();
  const open=async()=>{await canvas.focus();await page.keyboard.press('Alt+b');await expect(overlay).toBeAttached();};
  const close=async name=>{await overlay.getByRole('button',{name,exact:true}).dispatchEvent('click');await expect(overlay).toHaveCount(0);};
  const row=()=>overlay.getByRole('listitem',{name:'Find Patch',exact:true});
  const learn=async key=>{
    await row().getByRole('button',{name:'Learn Find Patch',exact:true}).dispatchEvent('click');
    await canvas.focus();
    for(const modifier of ['Alt','Shift','Control','Meta']){
      await page.keyboard.down(modifier);await page.keyboard.up(modifier);
    }
    await expect(row()).toContainText('Ctrl + F');
    await page.keyboard.press(key);
  };
  const find=async key=>{
    await canvas.focus();await page.keyboard.press(key);
    const search=page.getByRole('textbox',{name:'Patch select',exact:true});
    await expect(search).toBeAttached();await search.press('Escape');await expect(search).toHaveCount(0);
  };
  await open();await canvas.focus();await page.keyboard.press('Alt+b');await expect(overlay).toHaveCount(0);
  await open();await learn('Alt+Shift+f');await expect(row()).toContainText('Shift');
  await close('Cancel');await find('Control+f');
  await open();await expect(row()).toContainText('Ctrl + F');
  await learn('Alt+Shift+f');await expect(row()).toContainText('Shift');await close('OK');
  await find('Alt+Shift+f');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready();
  await find('Alt+Shift+f');
  await open();await row().getByRole('button',{name:'Reset Find Patch',exact:true}).dispatchEvent('click');
  await expect(row()).toContainText('Ctrl + F');await close('OK');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready();
  await find('Control+f');await open();await expect(row()).toContainText('Ctrl + F');await close('Cancel');
  expect(errors).toEqual([]);
});

test('scrolled shortcut rows keep accessible actions matched to their current binding',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  const canvas=page.locator('canvas').first();await canvas.focus();await page.keyboard.press('Alt+b');
  const overlay=page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first();
  await expect(overlay).toBeAttached();
  const checkRows=async()=>{
    const rows=overlay.getByRole('listitem');
    for(const row of await rows.all()){
      const name=await row.getAttribute('aria-label');
      for(const action of ['Learn','Reset'])
        await expect(row.getByRole('button',{name:`${action} ${name}`,exact:true})).toBeAttached();
      await expect(row.getByRole('checkbox',{name:`Toggle ${name}`,exact:true})).toBeAttached();
    }
  };
  await checkRows();await page.mouse.move(450,300);await page.mouse.wheel(0,1500);
  const manual=overlay.getByRole('listitem',{name:'Open Manual',exact:true});
  await expect(manual).toBeAttached();await checkRows();
  await manual.getByRole('checkbox',{name:'Toggle Open Manual',exact:true}).dispatchEvent('click');
  await expect(manual.getByRole('checkbox')).toHaveAttribute('aria-checked','false');
  await manual.getByRole('button',{name:'Reset Open Manual',exact:true}).dispatchEvent('click');
  await expect(manual.getByRole('checkbox')).toHaveAttribute('aria-checked','true');
  await page.mouse.wheel(0,-1500);
  await expect(overlay.getByRole('listitem',{name:'Undo',exact:true})).toBeAttached();await checkRows();
  await overlay.getByRole('button',{name:'Cancel',exact:true}).dispatchEvent('click');
  await expect(overlay).toHaveCount(0);
});

test('shortcut conflicts preserve bindings and allow retry, disable and reset all',async({page})=>{
  await page.goto('/surge-xt-browser.html');
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  const canvas=page.locator('canvas').first();await canvas.focus();await page.keyboard.press('Alt+b');
  const overlay=page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first();
  const row=overlay.getByRole('listitem',{name:'Find Patch',exact:true});
  await row.getByRole('button',{name:'Learn Find Patch',exact:true}).dispatchEvent('click');
  await canvas.focus();await page.keyboard.press('Alt+f');
  const conflict=page.getByRole('group',{name:'Keyboard Shortcut Conflict',exact:true});
  await expect(conflict).toContainText('Mark Patch as Favorite (Alt + F)');
  await expect(row).toContainText('Ctrl + F');
  await conflict.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(conflict).toHaveCount(0);
  await canvas.focus();await page.keyboard.press('Alt+Shift+f');
  await expect(row).toContainText('Shift');
  const enabled=row.getByRole('checkbox',{name:'Toggle Find Patch',exact:true});
  await enabled.dispatchEvent('click');await expect(enabled).toHaveAttribute('aria-checked','false');
  await overlay.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(overlay).toHaveCount(0);
  await canvas.focus();await page.keyboard.press('Alt+Shift+f');
  await page.waitForTimeout(300); // Allow the 250 ms accessibility snapshot to expose any accidental search.
  await expect(page.getByRole('textbox',{name:'Patch select',exact:true})).toHaveCount(0);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
  await canvas.focus();await page.keyboard.press('Alt+b');
  await expect(enabled).toHaveAttribute('aria-checked','false');
  // Native shortcut descriptions are intentionally blank while disabled.
  await enabled.dispatchEvent('click');await expect(row).toContainText('Shift');
  await enabled.dispatchEvent('click');await expect(enabled).toHaveAttribute('aria-checked','false');
  await overlay.getByRole('button',{name:'Reset all shortcuts to default',exact:true}).dispatchEvent('click');
  await expect(enabled).toHaveAttribute('aria-checked','true');await expect(row).toContainText('Ctrl + F');
  await overlay.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await expect(overlay).toHaveCount(0);
  await canvas.focus();await page.keyboard.press('Control+f');
  await expect(page.getByRole('textbox',{name:'Patch select',exact:true})).toBeAttached();
});
