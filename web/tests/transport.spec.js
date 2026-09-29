import {test, expect} from './fixtures.js';
import {execFileSync} from 'node:child_process';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';

test('transport advances by samples, freezes when paused, rewinds, and accepts atomic meter changes',()=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-transport-'));
  try {
    const executable=path.join(directory,'transport-test');
    execFileSync('c++',['-std=c++17','-pthread',path.resolve('tests/transport.cpp'),'-o',executable]);
    execFileSync(executable,[],{timeout:10000});
  } finally { rmSync(directory,{recursive:true,force:true}); }
});
async function start(page) {
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
}
const state=page=>page.evaluate(()=>JSON.parse(Module.ccall('surge_browser_offline_state','string',[],[])));
for (const rate of [44100,48000]) {
  test(`original tempo editor and browser transport stay synchronized at ${rate} Hz`,async({page})=>{
    await start(page);
    expect(await page.evaluate(rate=>Module._surge_browser_offline_begin(rate),rate)).toBe(1);
    await page.locator('canvas').first().focus();await page.keyboard.press('Alt+k');
    const original=page.getByRole('textbox',{name:'Tempo',exact:true});
    const browser=page.getByLabel('Transport tempo',{exact:true});
    await expect(original).toBeVisible();
    await original.fill('147');await original.press('Enter');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_transport_bpm())).toBe(147);
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    expect((await state(page)).tempo).toBe(147);
    await expect(browser).toHaveValue('147');
    await browser.fill('135.5');await browser.press('Tab');
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    expect((await state(page)).tempo).toBe(135.5);
    // The original three-digit JUCE editor displays rounded integer BPM.
    await expect(original).toHaveValue('136');
    await original.fill('0');await original.press('Enter');
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    expect((await state(page)).tempo).toBe(135.5);
    await original.fill('999');await original.press('Enter');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_transport_bpm())).toBe(999);
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    expect((await state(page)).tempo).toBe(999);
    await expect(browser).toHaveValue('999');
    await original.fill('1');await original.press('Enter');
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_transport_bpm())).toBe(1);
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    expect((await state(page)).tempo).toBe(1);
    await expect(browser).toHaveValue('1');
    await page.evaluate(()=>Module._surge_browser_offline_end());
  });
  test(`browser transport drives the original JUCE processor at ${rate} Hz`,async({page})=>{
    await start(page);
    expect(await page.evaluate(rate=>Module._surge_browser_offline_begin(rate),rate)).toBe(1);
    await page.evaluate(()=>{globalThis.audioRequests=0;Module._surge_enable_audio=()=>++audioRequests});
    await page.getByLabel('Transport tempo',{exact:true}).fill('135.5');
    await page.getByLabel('Beats per bar',{exact:true}).fill('7');
    await page.getByLabel('Beat unit',{exact:true}).selectOption('8');
    await page.getByRole('button',{name:'Play transport',exact:true}).click();
    await expect(page.getByRole('button',{name:'Pause transport',exact:true})).toHaveAttribute('aria-pressed','true');
    const frames=Math.ceil(rate/128)*128;
    expect(await page.evaluate(frames=>Module._surge_browser_offline_render(frames),frames)).toBeGreaterThanOrEqual(0);
    let current=await state(page);
    expect(current.tempo).toBe(135.5);
    expect(current.numerator).toBe(7);expect(current.denominator).toBe(8);
    expect(current.playing).toBe(true);
    const expected=frames/rate*135.5/60;
    expect(current.ppq).toBeCloseTo(expected,9);
    expect(current.enginePpq).toBeCloseTo(expected,9);
    await page.getByRole('button',{name:'Pause transport',exact:true}).click();
    await page.evaluate(()=>Module._surge_browser_offline_render(1024));
    current=await state(page);expect(current.playing).toBe(false);
    expect(current.ppq).toBeCloseTo(expected,9);expect(current.enginePpq).toBeCloseTo(expected,9);
    await page.getByRole('button',{name:'Rewind',exact:true}).click();
    await page.evaluate(()=>Module._surge_browser_offline_render(128));
    current=await state(page);expect(current.ppq).toBe(0);expect(current.enginePpq).toBe(0);
    expect(await page.evaluate(()=>audioRequests)).toBe(1);
    await page.evaluate(()=>Module._surge_browser_offline_end());
    expect(await page.evaluate(()=>Module._surge_browser_offline_render(128))).toBe(-1);
  });
}

test('timestamped MIDI reaches JUCE synthesis and panic silences held notes in offline rendering',async({page})=>{
  await start(page);
  expect(await page.evaluate(()=>Module._surge_browser_offline_begin(48000))).toBe(1);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,1024))).toBe(1);
  expect(await page.evaluate(()=>Module._surge_browser_offline_render(512))).toBe(0);
  const sounding=await page.evaluate(()=>Module._surge_browser_offline_render(8192));
  expect(sounding).toBeGreaterThan(0.001);
  await page.evaluate(()=>{Module._surge_browser_panic();Module._surge_browser_offline_render(1024)});
  expect(await page.evaluate(()=>Module._surge_browser_offline_render(8192))).toBe(0);
  await page.evaluate(()=>Module._surge_browser_offline_end());
});

test('invalid transport settings are rejected without changing tempo or starting playback',async({page})=>{
  await start(page);
  const before=await page.evaluate(()=>Module._surge_browser_transport_bpm());
  expect(await page.evaluate(()=>[
    Module._surge_browser_transport_tempo(NaN),Module._surge_browser_transport_tempo(0),
    Module._surge_browser_transport_tempo(1000),Module._surge_browser_transport(1,0,4),
    Module._surge_browser_transport(1,4,3)
  ])).toEqual([0,0,0,0,0]);
  expect(await page.evaluate(()=>Module._surge_browser_transport_bpm())).toBe(before);
  expect(await page.evaluate(()=>Module._surge_browser_transport_playing())).toBe(0);
});
