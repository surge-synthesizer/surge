import {test,expect} from './fixtures.js';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {airwindowsInventory} from '../scripts/airwindows-inventory.mjs';

test('native Airwindows selector defaults and prepared suspension cover every registered processor',()=>{
  const root=new URL('../../',import.meta.url);
  const output=execFileSync(fileURLToPath(new URL('build-reference/src/surge-web/surge-airwindows-lifecycle-check',root)),
    [fileURLToPath(new URL('resources/data',root))],{encoding:'utf8',timeout:30000});
  expect(output).toContain(`${airwindowsInventory().length*2} direct selections preserve native defaults`);
  expect(output).toContain('stable processing has no observed C++ heap operations');
  expect(output).toContain('0 allocating suspensions');
});

test('prepared Airwindows selection preserves native metadata and smoothing without callback heap work',()=>{
  const root=new URL('../../',import.meta.url);
  const output=execFileSync(fileURLToPath(new URL('build-reference/src/surge-web/surge-airwindows-selection-check',root)),
    [fileURLToPath(new URL('resources/data',root))],{encoding:'utf8',timeout:30000});
  expect(output).toContain(`${airwindowsInventory().length*2} cases match native defaults and smoothing`);
  expect(output).toContain('retain formatter ownership, and capture/adopt/process without C++ heap operations');
  expect(output).toContain('Mailbox: 2000 threaded requests, injected failures, bounded ownership and control-only reclamation passed');
});
