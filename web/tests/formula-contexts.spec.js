import {test,expect} from '@playwright/test';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

test('concurrent audio and display formula contexts retain independent caches and shared tables',()=>{
  const harness=fileURLToPath(new URL('../../build-reference/src/surge-web/surge-engine-reference',import.meta.url));
  const resources=fileURLToPath(new URL('../../resources/data',import.meta.url));
  expect(execFileSync(harness,['--check-formula-contexts',resources],{encoding:'utf8'}))
    .toContain('Concurrent formula contexts preserved independent caches and shared tables');
});
