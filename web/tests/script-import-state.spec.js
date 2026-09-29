import {test,expect} from '@playwright/test';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
test('native script-import validation preserves existing snapshot inputs and metadata',()=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-script-import-'));
  try{
    const harness=fileURLToPath(new URL('../../build-reference/src/surge-web/surge-engine-reference',import.meta.url));
    expect(execFileSync(harness,['--check-script-import',directory],{encoding:'utf8'})).toContain('Invalid script import retained metadata and snapshots');
  }finally{rmSync(directory,{recursive:true,force:true});}
});
