import {test,expect} from '@playwright/test';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

test('compiled Lua chunks defer side effects and preserve function ordering and error stacks',()=>{
  const harness=fileURLToPath(new URL('../../build-reference/src/surge-web/surge-engine-reference',import.meta.url));
  const resources=fileURLToPath(new URL('../../resources/data',import.meta.url));
  expect(execFileSync(harness,['--check-lua-compilation',resources],{encoding:'utf8'}))
    .toContain('Lua compilation preserved deferred execution, ownership and error stack contracts');
});
