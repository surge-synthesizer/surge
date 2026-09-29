import {test,expect} from '@playwright/test';
import {spawnSync} from 'node:child_process';
import path from 'node:path';
import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';

test('desktop UI inventory matches source and reviews reference current source',()=>{
  const result=spawnSync('python3',[path.resolve('scripts/feature-inventory.py')],{encoding:'utf8'});
  expect(result.status,result.stdout+result.stderr).toBe(0);
});

test('parity completion gate rejects a review ledger with missing browser equivalents',()=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-parity-'));
  try {
    const reviews=path.join(directory,'reviews.json');
    writeFileSync(reviews,'{}');
    const result=spawnSync('python3',[path.resolve('scripts/feature-inventory.py'),'--require-complete','--reviews',reviews],{encoding:'utf8'});
    expect(result.status).toBe(1);
    expect(result.stderr).toContain('entry points have no browser parity review');
  } finally { rmSync(directory,{recursive:true,force:true}); }
});

test('generated menu labels require expansion even when they contain a literal suffix',()=>{
  const inventory=JSON.parse(readFileSync(path.resolve('parity/source-inventory.json')));
  const generated=inventory.entries.filter(e=>e.source.endsWith('/MSEGEditor.cpp')&&['" Step Sequencer"','" Lines Sine"','" Sawtooth Plucks"'].includes(e.labelHint));
  expect(generated).toHaveLength(3);
  for(const entry of generated)expect(entry.requiresRuntimeExpansion).toBe(true);
  const fixed=inventory.entries.find(e=>e.source.endsWith('/MSEGEditor.cpp')&&e.labelHint==='"Minimal MSEG"');
  expect(fixed.requiresRuntimeExpansion).toBe(false);
});

test('verified dynamic menus must list their runtime expansion',()=>{
  const directory=mkdtempSync(path.join(tmpdir(),'surge-parity-dynamic-'));
  try{
    const reviews=JSON.parse(readFileSync(path.resolve('parity/reviews.json')));
    const key='src/surge-xt/gui/overlays/MSEGEditor.cpp:menu-call:7dab997d8043e11d';
    delete reviews[key].runtimeExpansion;
    const file=path.join(directory,'reviews.json');writeFileSync(file,JSON.stringify(reviews));
    const result=spawnSync('python3',[path.resolve('scripts/feature-inventory.py'),'--reviews',file],{encoding:'utf8'});
    expect(result.status).toBe(1);expect(result.stderr).toContain('Missing runtime menu expansion: '+key);
  }finally{rmSync(directory,{recursive:true,force:true});}
});
