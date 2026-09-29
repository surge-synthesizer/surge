import {test,expect} from './fixtures.js';
import {fileURLToPath} from 'node:url';

test('popup key routing detaches on pagehide and restores once on persisted pageshow',async({page})=>{
  await page.goto('about:blank');
  await page.addScriptTag({path:fileURLToPath(new URL('../browser/accessibility.js',import.meta.url))});
  await page.evaluate(()=>{
    globalThis.calls=[];
    globalThis.menuVisible=true;
    SurgeAccessibility.attach({
      _surge_accessibility_snapshot:()=>{},
      ccall:()=>JSON.stringify(menuVisible?[{id:1,role:11,label:'Popup',bounds:[0,0,100,100],children:[]}]:[]),
      _surge_accessibility_action:(...args)=>{calls.push(args);return 1;},
    });
    SurgeAccessibility.update();
  });
  await page.keyboard.press('ArrowDown');
  expect(await page.evaluate(()=>calls)).toEqual([[1,7,40]]);
  await page.evaluate(()=>dispatchEvent(new PageTransitionEvent('pagehide',{persisted:true})));
  await page.keyboard.press('ArrowDown');
  expect(await page.evaluate(()=>calls.length)).toBe(1);
  await page.evaluate(()=>dispatchEvent(new PageTransitionEvent('pageshow',{persisted:true})));
  await page.keyboard.press('Enter');
  expect(await page.evaluate(()=>calls)).toEqual([[1,7,40],[1,7,13]]);
  await page.evaluate(()=>{
    dispatchEvent(new PageTransitionEvent('pagehide',{persisted:true}));
    dispatchEvent(new PageTransitionEvent('pageshow',{persisted:true}));
  });
  await page.keyboard.press('Escape');
  expect(await page.evaluate(()=>calls)).toEqual([[1,7,40],[1,7,13],[1,6,0]]);
  await page.evaluate(()=>{menuVisible=false;SurgeAccessibility.update();});
  await page.keyboard.press('ArrowDown');await page.keyboard.press('Escape');
  expect(await page.evaluate(()=>calls.length)).toBe(3);
});
