import {test,expect} from './fixtures.js';
async function start(page) {
  await page.addInitScript(()=>{
    const originalNow=performance.now.bind(performance);
    globalThis.audioClockOffset=0;
    Object.defineProperty(performance,'now',{value:()=>originalNow()+audioClockOffset});
    globalThis.audioSimulation={contexts:[],nodes:[],deferClose:true};
    class Context {
      constructor(){
        this.sampleRate=48000;this.currentTime=0;this.state='suspended';this.destination={};this.closeCalls=0;
        this.audioWorklet={addModule:()=>{++this.imports;return new Promise((resolve,reject)=>{this.finishImport=resolve;this.failImport=reject})}};
        this.imports=0;audioSimulation.contexts.push(this);
      }
      resume(){this.state='running';queueMicrotask(()=>this.onstatechange?.());return Promise.resolve()}
      suspend(){this.state='suspended';return Promise.resolve()}
      close(){
        ++this.closeCalls;
        return new Promise((resolve,reject)=>{
          this.finishClose=()=>{this.state='closed';resolve()};this.failClose=reject;
          if(!audioSimulation.deferClose)this.finishClose();
        });
      }
    }
    class WorkletNode extends EventTarget {
      constructor(context,name,options){
        super();
        this.context=context;this.name=name;this.options=options;this.connected=false;
        this.port={onmessage:null,closed:false,close(){this.closed=true},postMessage(message){
          queueMicrotask(()=>this.onmessage?.({data:{_wsc:message.callback,args:[message.contextHandle,1,message.userData]}}));
        }};
        audioSimulation.nodes.push(this);
      }
      connect(){this.connected=true}
      disconnect(){this.connected=false}
    }
    globalThis.AudioContext=Context;globalThis.AudioWorkletNode=WorkletNode;
  });
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
}
const audioStatus=page=>page.evaluate(()=>Module._surge_browser_audio_status());
test('startup timeout retries keep one context and one pending worklet import',async({page})=>{
  await start(page);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(1);
  await page.evaluate(()=>audioClockOffset+=16000);
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  expect(await audioStatus(page)).toBe(1);
  expect(await page.evaluate(()=>audioSimulation.contexts.map(c=>c.imports))).toEqual([1]);
  await page.evaluate(()=>audioSimulation.contexts[0].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  expect(await page.evaluate(()=>audioSimulation.nodes.filter(n=>n.name==='surge-xt').length)).toBe(1);
});
test('a late successful import is handled after a startup timeout without allocating another worklet',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioClockOffset+=16000);
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.evaluate(()=>audioSimulation.contexts[0].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  expect(await page.evaluate(()=>audioSimulation.contexts.length)).toBe(1);
});
test('failed initialization closes its context before a fresh retry and repeated clicks cannot overlap teardown',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].failImport(new Error('Download failed')));
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(3);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  expect(await page.evaluate(()=>({contexts:audioSimulation.contexts.length,closes:audioSimulation.contexts[0].closeCalls}))).toEqual({contexts:1,closes:1});
  await page.evaluate(()=>audioSimulation.contexts[0].finishClose());
  await expect.poll(()=>audioStatus(page)).toBe(1);
  expect(await page.evaluate(()=>audioSimulation.contexts.map(c=>c.state))).toEqual(['closed','running']);
  await page.evaluate(()=>audioSimulation.contexts[1].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
});
test('failed context closure prevents replacement and can be retried',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].failImport(new Error('Download failed')));
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].failClose(new Error('Close failed')));
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await expect(page.locator('#status')).toContainText('Unable to close');
  expect(await page.evaluate(()=>audioSimulation.contexts.length)).toBe(1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].finishClose());
  await expect.poll(()=>audioStatus(page)).toBe(1);
  expect(await page.evaluate(()=>audioSimulation.contexts.length)).toBe(2);
});

test('suspension rejects new MIDI and resumption keeps the existing worklet',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  await page.evaluate(()=>{const c=audioSimulation.contexts[0];c.state='suspended';c.onstatechange()});
  await expect.poll(()=>audioStatus(page)).toBe(4);
  expect(await page.evaluate(()=>Module._surge_browser_midi(0x90,60,100,0))).toBe(0);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(2);
  expect(await page.evaluate(()=>({contexts:audioSimulation.contexts.length,nodes:audioSimulation.nodes.length,imports:audioSimulation.contexts[0].imports}))).toEqual({contexts:1,nodes:2,imports:1});
});
test('unexpected context closure cleans up the bootstrap port before restarting',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>audioSimulation.contexts[0].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  await page.evaluate(()=>{const c=audioSimulation.contexts[0];c.state='closed';c.onstatechange()});
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(1);
  expect(await page.evaluate(()=>({contexts:audioSimulation.contexts.length,closed:audioSimulation.nodes[0].port.closed,handler:audioSimulation.contexts[0].onstatechange}))).toEqual({contexts:2,closed:true,handler:null});
});

test('an import finishing after its context retired cannot construct a late bootstrap on the replacement stack',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await page.evaluate(()=>{const c=audioSimulation.contexts[0];c.state='closed';c.onstatechange()});
  await expect.poll(()=>audioStatus(page)).toBe(-1);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(1);
  await page.evaluate(async()=>{audioSimulation.contexts[0].finishImport();await new Promise(resolve=>setTimeout(resolve,0))});
  expect(await page.evaluate(()=>audioSimulation.nodes.length)).toBe(0);
  expect(await audioStatus(page)).toBe(1);
  await page.evaluate(()=>audioSimulation.contexts[1].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  expect(await page.evaluate(()=>audioSimulation.nodes.length)).toBe(2);
});

test('stable Chrome closes a guarded real audio context',async({page})=>{
  await page.goto('/surge-juce-browser-check.html');
  await expect(page.locator('canvas')).toBeVisible({timeout:60000});
  expect(await page.evaluate(async()=>{
    const context=new AudioContext();
    SurgeAudioLifecycle.guard(context);
    await SurgeAudioLifecycle.close(context,null);
    return context.state;
  })).toBe('closed');
});

test('patch browsing remains available while the worklet module is still loading',async({page})=>{
  await start(page);
  await page.getByRole('button',{name:'Enable audio',exact:true}).click();
  await expect.poll(()=>audioStatus(page)).toBe(1);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],
    ['/factory/patches_factory/Templates/Init Sine.fxp']))).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
  expect(await audioStatus(page)).toBe(1);
  await page.evaluate(()=>audioSimulation.contexts[0].finishImport());
  await expect.poll(()=>audioStatus(page)).toBe(2);
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Sine');
});
