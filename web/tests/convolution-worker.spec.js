import {test, expect} from '@playwright/test';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {airwindowsInventory} from '../scripts/airwindows-inventory.mjs';

test('native convolution worker preserves audio and retires ownership off the producer', () => {
  const executable = fileURLToPath(new URL(
    '../../build-reference/src/surge-web/surge-convolution-worker-check', import.meta.url));
  const output = execFileSync(executable, [], {encoding:'utf8', timeout:20000});
  expect(output).toContain('exact audio, bounded reuse, stale/failure retention and worker retirement passed');
  expect(output).toContain('Pending IR edits: newer values, flags, MIDI mappings and selection ownership passed');
});

test('native effect retirement preserves bounded ownership and destroys resources on its worker', () => {
  const executable = fileURLToPath(new URL(
    '../../build-reference/src/surge-web/surge-effect-retirement-check', import.meta.url));
  const output = execFileSync(executable, [], {encoding:'utf8', timeout:20000});
  expect(output).toContain('bounded ownership, reuse, worker destruction and shutdown passed');
});

test('all native effect families attach prepared parameter storage at both sample rates', () => {
  const executable = fileURLToPath(new URL(
    '../../build-reference/src/surge-web/surge-effect-binding-check', import.meta.url));
  const data = fileURLToPath(new URL('../../resources/data', import.meta.url));
  const output = execFileSync(executable, [data], {encoding:'utf8', timeout:20000});
  const count=airwindowsInventory().length;
  expect(output).toContain(`${(30+count)*2} preparations attach live parameter storage and render finite audio`);
  expect(output).toContain(`${count} Airwindows variants retain their prepared sub-effect at both rates`);
  expect(output).toContain('Prepared metadata transfers: no C++ allocations or deallocations, including Off');
});
