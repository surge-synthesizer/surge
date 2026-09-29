import {test,expect} from './fixtures.js';

async function ready(page){
  await expect.poll(()=>page.evaluate(()=>globalThis.Module?._surge_browser_scene?.())).toBe(0);
}
async function openEditor(page){
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+b');
  await expect(page.getByRole('button',{name:'Select virtual keyboard layout',exact:true})).toBeAttached();
}
async function menu(page){
  await page.getByRole('button',{name:'Select virtual keyboard layout',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menu')).toBeAttached();
}
async function selected(page,name){
  await menu(page);
  await expect(page.getByRole('menuitemcheckbox',{name:name+' (Checked)',exact:true})).toHaveAttribute('aria-checked','true');
  await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
}

test('original virtual-keyboard layouts persist and conflicting layout selection retains the previous choice',async({page})=>{
  test.setTimeout(90000);
  await page.goto('/surge-xt-browser.html');await ready(page);await openEditor(page);
  await selected(page,'QWERTY');
  await menu(page);await page.getByRole('menuitem',{name:'QWERTY (2 Octave)',exact:true}).dispatchEvent('click');
  const conflict=page.getByRole('group',{name:'Virtual Keyboard Layout Conflict',exact:true});
  await expect(conflict).toContainText('Octave Down');
  await expect(conflict).toContainText('Velocity Up');
  await conflict.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await selected(page,'QWERTY');
  for(const layout of ['QWERTZ (German)','QWERTZ (Slavic)','AZERTY','Dvorak','Colemak','Workman','QWERTY']){
    await menu(page);await page.getByRole('menuitem',{name:layout,exact:true}).dispatchEvent('click');
    await selected(page,layout);
    await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready(page);await openEditor(page);
    await selected(page,layout);
  }
});

test('two-octave layout becomes available after resolving shortcut conflicts',async({page})=>{
  await page.goto('/surge-xt-browser.html');await ready(page);await openEditor(page);
  await page.mouse.move(450,300);await page.mouse.wheel(0,1500);
  for(const action of ['Octave Down','Octave Up','Velocity Down 10%','Velocity Up 10%']){
    const checkbox=page.getByRole('checkbox',{name:'Toggle Virtual Keyboard: '+action,exact:true});
    await expect(checkbox).toHaveAttribute('aria-checked','true');
    await checkbox.dispatchEvent('click');await expect(checkbox).toHaveAttribute('aria-checked','false');
  }
  await menu(page);await page.getByRole('menuitem',{name:'QWERTY (2 Octave)',exact:true}).dispatchEvent('click');
  await selected(page,'QWERTY (2 Octave)');
  await page.getByRole('group',{name:'Keyboard Shortcut Editor',exact:true}).first().getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready(page);await openEditor(page);
  await selected(page,'QWERTY (2 Octave)');
});

test('Dvorak punctuation conflicts use browser key codes and do not alias Delete',async({page})=>{
  await page.goto('/surge-xt-browser.html');await ready(page);await openEditor(page);
  const row=page.getByRole('listitem',{name:'Find Patch',exact:true});
  const learn=row.getByRole('button',{name:'Learn Find Patch',exact:true});
  await learn.focus();await page.keyboard.press('Enter');await page.keyboard.press('.');
  await menu(page);await page.getByRole('menuitem',{name:'Dvorak',exact:true}).dispatchEvent('click');
  const conflict=page.getByRole('group',{name:'Virtual Keyboard Layout Conflict',exact:true});
  await expect(conflict).toContainText('Find Patch');
  await conflict.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  await selected(page,'QWERTY');
  await row.getByRole('button',{name:'Reset Find Patch',exact:true}).dispatchEvent('click');
  await learn.focus();await page.keyboard.press('Enter');await page.keyboard.press('Delete');
  await menu(page);await page.getByRole('menuitem',{name:'Dvorak',exact:true}).dispatchEvent('click');
  await selected(page,'Dvorak');await expect(conflict).toHaveCount(0);
});

for(const [layout,playing,silentKeys] of [['AZERTY',['q'],['a','F2']],['Dvorak',[',','.'],['Delete','F4']]])
for(const sampleRate of [44100,48000])
test(`${layout} playing reaches the actual worklet after reload at ${sampleRate} Hz`,async({page})=>{
  await page.addInitScript(rate=>{const Original=AudioContext;globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};},sampleRate);
  await page.goto('/surge-xt-browser.html');await ready(page);await openEditor(page);
  await menu(page);await page.getByRole('menuitem',{name:layout,exact:true}).dispatchEvent('click');
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();await ready(page);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
  expect(await page.evaluate(()=>Module._surge_browser_audio_rate())).toBe(sampleRate);
  await page.evaluate(()=>{
    const {context,node}=SurgeAudioInput.input.graph;
    node.disconnect();const analyser=context.createAnalyser();analyser.fftSize=2048;
    const silent=context.createGain();silent.gain.value=0;
    node.connect(analyser);analyser.connect(silent);silent.connect(context.destination);
    globalThis.keyboardProbe=analyser;
  });
  await page.locator('canvas').first().focus();await page.keyboard.press('Alt+k');
  const voices=()=>page.evaluate(()=>Module._surge_browser_active_voices());
  const blocks=()=>page.evaluate(()=>Module._surge_browser_audio_blocks());
  await expect.poll(voices).toBe(0);
  for(const key of playing){
    await page.keyboard.down(key);await expect.poll(voices).toBeGreaterThan(0);
    await expect.poll(()=>page.evaluate(()=>{
      const samples=new Float32Array(keyboardProbe.fftSize);keyboardProbe.getFloatTimeDomainData(samples);
      return Math.max(...samples.map(Math.abs));
    })).toBeGreaterThan(0.001);
    await page.keyboard.up(key);await expect.poll(voices).toBe(0);
  }
  for(const key of silentKeys){
    const before=await blocks();await page.keyboard.down(key);
    await expect.poll(blocks).toBeGreaterThan(before+32);
    expect(await voices()).toBe(0);await page.keyboard.up(key);
  }
});
