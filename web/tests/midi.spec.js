import {test, expect} from './fixtures.js';
import {execFileSync} from 'node:child_process';
import {mkdtempSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';

test('bounded MIDI queue preserves timing and ordering across threads and recovers from overflow', () => {
  const directory=mkdtempSync(path.join(tmpdir(),'surge-midi-'));
  try {
    const executable=path.join(directory,'queue-test');
    execFileSync('c++',['-std=c++17','-pthread',path.resolve('tests/midi-queue.cpp'),'-o',executable]);
    execFileSync(executable,[],{timeout:10000});
  } finally { rmSync(directory,{recursive:true,force:true}); }
});
async function start(page) {
  await page.addInitScript(()=>{
    const port=id=>({id,name:`Controller ${id}`,state:'connected',onmidimessage:null,close:async()=>{mockMidi.closed.push(id)}});
    globalThis.mockMidi={requests:0,deny:false,closed:[],inputs:new Map(),onstatechange:null};
    mockMidi.inputs.set('a',port('a'));mockMidi.inputs.set('b',port('b'));
    Object.defineProperty(navigator,'requestMIDIAccess',{value:async options=>{
      ++mockMidi.requests;mockMidi.options=options;
      if(mockMidi.deny) throw new DOMException('Permission denied','NotAllowedError');
      return mockMidi;
    }});
  });
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Enable MIDI',exact:true})).toBeVisible();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
}
async function capture(page) {
  await page.evaluate(()=>{
    globalThis.delivered=[];globalThis.panics=0;globalThis.acceptMidi=true;
    Module._surge_browser_audio_status=()=>2;
    Module._surge_browser_audio_time=()=>10;
    Module._surge_browser_audio_rate=()=>48000;
    Module._surge_browser_midi=(...event)=>{delivered.push(event);return acceptMidi?1:0};
    Module._surge_browser_panic=()=>++panics;
  });
}

test('Web MIDI permission denial preserves the patch and permits an explicit retry',async({page})=>{
  await start(page);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(0);
  await page.evaluate(()=>mockMidi.deny=true);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await expect(page.locator('#midi-controls [role=status]')).toContainText('Permission denied');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>mockMidi.deny=false);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await expect(page.locator('#midi-controls [role=status]')).toContainText('2 MIDI input(s)');
  expect(await page.evaluate(()=>({requests:mockMidi.requests,options:mockMidi.options}))).toEqual({requests:2,options:{sysex:false}});
});

test('Web MIDI forwards MPE channel messages and reports queue overflow',async({page})=>{
  await start(page);await capture(page);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  const messages=[[0x91,60,100],[0xe1,0,96],[0xd1,92],[0xb1,74,80],[0xa1,60,77],[0xc1,2],[0x81,60,0]];
  await page.evaluate(messages=>{
    for(const bytes of messages) mockMidi.inputs.get('a').onmidimessage({data:new Uint8Array(bytes),timeStamp:0});
    mockMidi.inputs.get('a').onmidimessage({data:new Uint8Array([0x90,60]),timeStamp:0});
    mockMidi.inputs.get('a').onmidimessage({data:new Uint8Array([0xf8]),timeStamp:0});
  },messages);
  expect(await page.evaluate(()=>delivered)).toEqual(messages.map(bytes=>[bytes[0],bytes[1],bytes[2]||0,480000]));
  await page.evaluate(()=>{acceptMidi=false;mockMidi.inputs.get('a').onmidimessage({data:new Uint8Array([0x80,60,0]),timeStamp:0})});
  await expect(page.locator('#midi-controls [role=status]')).toContainText('queue full');
});

test('MIDI input selection, disconnect, reconnect, and disable release notes without selecting another device',async({page})=>{
  await start(page);await capture(page);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await page.getByLabel('MIDI input',{exact:true}).selectOption('b');
  await page.evaluate(()=>{
    const event={data:new Uint8Array([0x90,60,100]),timeStamp:0};
    mockMidi.inputs.get('a').onmidimessage(event);mockMidi.inputs.get('b').onmidimessage(event);
    globalThis.detachedPort=mockMidi.inputs.get('b');detachedPort.state='disconnected';
    mockMidi.inputs.delete('b');mockMidi.onstatechange();
    mockMidi.inputs.get('a').onmidimessage(event);
  });
  expect(await page.evaluate(()=>({count:delivered.length,panics,handler:detachedPort.onmidimessage}))).toEqual({count:1,panics:2,handler:null});
  await expect(page.getByLabel('MIDI input',{exact:true})).toHaveValue('b');
  await page.evaluate(()=>{detachedPort.state='connected';mockMidi.inputs.set('b',detachedPort);mockMidi.onstatechange();detachedPort.onmidimessage({data:new Uint8Array([0x80,60,0]),timeStamp:0})});
  expect(await page.evaluate(()=>delivered.length)).toBe(2);
  await page.getByRole('button',{name:'All notes off',exact:true}).click();
  await page.getByRole('button',{name:'Disable MIDI',exact:true}).click();
  expect(await page.evaluate(()=>({panics,closed:mockMidi.closed.sort(),handler:mockMidi.onstatechange}))).toEqual({panics:4,closed:['a','b'],handler:null});
  await expect(page.getByRole('button',{name:'Enable MIDI',exact:true})).toBeEnabled();
});

test('a MIDI device open failure is visible and can be retried without replacing the patch',async({page})=>{
  await start(page);
  await page.evaluate(()=>mockMidi.inputs.get('a').open=async()=>{throw new Error('Device busy')});
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await expect(page.locator('#midi-controls [role=status]')).toContainText('Device busy');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.getByRole('button',{name:'Disable MIDI',exact:true}).click();
  await page.evaluate(()=>mockMidi.inputs.get('a').open=async()=>mockMidi.inputs.get('a'));
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await expect(page.locator('#midi-controls [role=status]')).toContainText('2 MIDI input(s)');
});

test('MIDI device choice survives disable and reload without automatically enabling or switching inputs',async({page})=>{
  await start(page);await capture(page);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await page.getByLabel('MIDI input',{exact:true}).selectOption('b');
  await page.getByRole('button',{name:'Disable MIDI',exact:true}).click();
  await expect(page.getByLabel('MIDI input',{exact:true})).toHaveValue('b');
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.getByLabel('MIDI input',{exact:true})).toHaveValue('b');
  expect(await page.evaluate(()=>mockMidi.requests)).toBe(0);
  await capture(page);
  await page.evaluate(()=>mockMidi.inputs.delete('b'));
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await expect(page.getByLabel('MIDI input',{exact:true})).toHaveValue('b');
  await expect(page.getByLabel('MIDI input',{exact:true}).locator('option:checked')).toHaveText('Selected device disconnected');
  await page.evaluate(()=>mockMidi.inputs.get('a').onmidimessage({data:new Uint8Array([0x90,60,100]),timeStamp:0}));
  expect(await page.evaluate(()=>delivered)).toEqual([]);
});

test('failed device preference writes retain the saved choice and active selection, with retry',async({page})=>{
  await start(page);await capture(page);
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  const selector=page.getByLabel('MIDI input',{exact:true});
  await selector.selectOption('a');
  await page.evaluate(()=>{
    globalThis.preferenceRename=Module.FS.rename;
    Module.FS.rename=(source,destination)=>{
      if(destination==='/user/.surge-browser-midi') throw Error('Device preference disk full');
      return preferenceRename(source,destination);
    };
  });
  await selector.selectOption('b');
  await expect(page.locator('#midi-controls [data-preference-status]')).toContainText('could not be saved');
  expect(await page.evaluate(()=>Module.FS.readFile('/user/.surge-browser-midi',{encoding:'utf8'}))).toBe('a');
  expect(await page.evaluate(()=>Module.FS.analyzePath('/user/.surge-browser-midi.tmp').exists)).toBe(false);
  await page.evaluate(()=>mockMidi.inputs.get('b').onmidimessage({data:new Uint8Array([0x90,60,100]),timeStamp:0}));
  expect(await page.evaluate(()=>delivered.length)).toBe(1);
  await page.evaluate(()=>Module.FS.rename=preferenceRename);
  await page.getByRole('button',{name:'Retry saving device',exact:true}).click();
  await expect(page.locator('#midi-controls [data-preference-status]')).toHaveText('');
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.getByLabel('MIDI input',{exact:true})).toHaveValue('b');
});

test('invalid saved device choices remain untouched until an explicit replacement',async({page})=>{
  await start(page);
  await page.evaluate(async()=>{Module.FS.writeFile('/user/.surge-browser-midi','bad\0choice');await SurgeBrowser.flush()});
  await page.reload();
  await expect(page.locator('#midi-controls [data-preference-status]')).toContainText('could not be read');
  expect(await page.evaluate(()=>new TextDecoder().decode(Module.FS.readFile('/user/.surge-browser-midi')))).toBe('bad\0choice');
  await page.getByRole('button',{name:'Enable MIDI',exact:true}).click();
  await page.getByLabel('MIDI input',{exact:true}).selectOption('a');
  await expect(page.locator('#midi-controls [data-preference-status]')).toHaveText('');
  expect(await page.evaluate(()=>Module.FS.readFile('/user/.surge-browser-midi',{encoding:'utf8'}))).toBe('a');
});
