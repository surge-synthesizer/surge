import {test,expect} from './fixtures.js';
async function start(page) {
  await page.addInitScript(()=>{
    const media=new EventTarget();
    globalThis.capture={requests:[],streams:[],connections:[],deny:false,defer:false,pending:[]};
    media.enumerateDevices=async()=>[{kind:'audioinput',deviceId:'device-a',label:'Input A'},{kind:'audioinput',deviceId:'device-b',label:'Input B'}];
    media.getUserMedia=async options=>{
      capture.requests.push(options);
      if(capture.deny) throw new DOMException('Permission denied','NotAllowedError');
      const track={stopped:false,onended:null,readyState:'live',stop(){this.stopped=true;this.readyState='ended'}};
      const stream={track,getTracks:()=>[track],getAudioTracks:()=>[track]};capture.streams.push(stream);
      if(capture.defer) await new Promise(resolve=>capture.pending.push(resolve));
      return stream;
    };
    Object.defineProperty(navigator,'mediaDevices',{value:media});
  });
  await page.goto('/surge-xt-browser.html');
  await expect(page.locator('canvas').first()).toBeVisible({timeout:60000});
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>{
    globalThis.inputNode={};
    globalThis.inputContext={createMediaStreamSource(stream){
      const source={stream,connected:false,connect(node,output,input){this.connected=true;capture.connections.push({source:this,node,output,input})},disconnect(){this.connected=false}};
      return source;
    }};
    SurgeAudioInput.input.setGraph(inputContext,inputNode);
  });
}
const status=page=>page.locator('#audio-input-controls [role=status]');
test('audio input permission denial retains the patch and permits retry',async({page})=>{
  await start(page);await page.evaluate(()=>capture.deny=true);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toContainText('Permission denied');
  expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.evaluate(()=>capture.deny=false);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  expect(await page.evaluate(()=>capture.requests[1])).toEqual({audio:{echoCancellation:false,noiseSuppression:false,autoGainControl:false,channelCount:{ideal:2}},video:false});
  expect(await page.evaluate(()=>({node:capture.connections[0].node===inputNode,input:capture.connections[0].input}))).toEqual({node:true,input:0});
  await page.getByRole('button',{name:'Stop input',exact:true}).click();
  expect(await page.evaluate(()=>({stopped:capture.streams[0].track.stopped,connected:capture.connections[0].source.connected}))).toEqual({stopped:true,connected:false});
});
test('device switching connects the replacement before releasing the old input and retains it on failure',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  await page.evaluate(()=>{capture.defer=true});
  await page.getByLabel('Audio input device',{exact:true}).selectOption('device-b');
  await expect.poll(()=>page.evaluate(()=>capture.pending.length)).toBe(1);
  expect(await page.evaluate(()=>capture.streams[0].track.stopped)).toBe(false);
  await page.evaluate(()=>{capture.defer=false;capture.pending.shift()()});
  await expect(status(page)).toHaveText('Audio input enabled');
  expect(await page.evaluate(()=>capture.streams.map(s=>s.track.stopped))).toEqual([true,false]);
  expect(await page.evaluate(()=>capture.requests[1].audio.deviceId)).toEqual({exact:'device-b'});
  await page.evaluate(()=>capture.deny=true);
  await page.getByLabel('Audio input device',{exact:true}).selectOption('device-a');
  await expect(status(page)).toContainText('Current input retained');
  expect(await page.evaluate(()=>capture.streams[1].track.stopped)).toBe(false);
});
test('stopping during permission acquisition discards and stops a late stream',async({page})=>{
  await start(page);await page.evaluate(()=>capture.defer=true);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect.poll(()=>page.evaluate(()=>capture.pending.length)).toBe(1);
  await page.getByRole('button',{name:'Stop input',exact:true}).click();
  await page.evaluate(()=>capture.pending.shift()());
  await expect.poll(()=>page.evaluate(()=>capture.streams[0].track.stopped)).toBe(true);
  expect(await page.evaluate(()=>capture.connections.length)).toBe(0);
  await expect(status(page)).toHaveText('Audio input stopped');
});
test('device disconnect and page exit release captured tracks',async({page})=>{
  await start(page);await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  await page.evaluate(()=>capture.streams[0].track.onended());
  await expect(status(page)).toContainText('disconnected');
  expect(await page.evaluate(()=>capture.streams[0].track.stopped)).toBe(true);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  await page.evaluate(()=>dispatchEvent(new Event('pagehide')));
  expect(await page.evaluate(()=>capture.streams.every(s=>s.track.stopped))).toBe(true);
});

test('input waits for the audio graph and stopping cancels that pending activation',async({page})=>{
  await start(page);
  await page.evaluate(()=>{SurgeAudioInput.input.graph=null;globalThis.audioStarts=0;Module._surge_enable_audio=()=>++audioStarts});
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  expect(await page.evaluate(()=>capture.requests.length)).toBe(0);
  await page.getByRole('button',{name:'Stop input',exact:true}).click();
  await page.evaluate(()=>SurgeAudioInput.input.setGraph(inputContext,inputNode));
  expect(await page.evaluate(()=>capture.requests.length)).toBe(0);
  await page.evaluate(()=>SurgeAudioInput.input.graph=null);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await page.evaluate(()=>SurgeAudioInput.input.setGraph(inputContext,inputNode));
  await expect(status(page)).toHaveText('Audio input enabled');
  expect(await page.evaluate(()=>audioStarts)).toBe(2);
});

for(const rate of [44100,48000]) test(`factory stereo audio-input patch processes supplied samples at ${rate} Hz`,async({page})=>{
  await start(page);
  await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Audio In Stereo Osc 1.fxp']));
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Audio In Stereo Osc 1');
  expect(await page.evaluate(rate=>Module._surge_browser_offline_begin(rate),rate)).toBe(1);
  expect(await page.evaluate(()=>Module._surge_browser_offline_render(4096))).toBe(0);
  const energy=await page.evaluate(rate=>{
    const left=new Float32Array(4096),right=new Float32Array(4096);
    for(let i=0;i<left.length;++i){left[i]=0.1*Math.sin(2*Math.PI*220*i/rate);right[i]=0.1*Math.sin(2*Math.PI*330*i/rate)}
    return Module.ccall('surge_browser_offline_render_input','number',['number','array','array'],[left.length,new Uint8Array(left.buffer),new Uint8Array(right.buffer)]);
  },rate);
  expect(energy).toBeGreaterThan(0.001);
  expect(Number.isFinite(energy)).toBe(true);
  await page.evaluate(()=>Module._surge_browser_offline_end());
});

test('saved audio input is restored after reload without starting capture',async({page})=>{
  await start(page);
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  await page.getByLabel('Audio input device',{exact:true}).selectOption('device-b');
  await expect.poll(()=>page.evaluate(()=>capture.requests.length)).toBe(2);
  await page.getByRole('button',{name:'Stop input',exact:true}).click();
  await page.evaluate(()=>SurgeBrowser.flush());
  await page.reload();
  await expect(page.getByLabel('Audio input device',{exact:true})).toHaveValue('device-b');
  expect(await page.evaluate(()=>capture.requests.length)).toBe(0);
  await page.evaluate(()=>SurgeAudioInput.input.setGraph({createMediaStreamSource:()=>({connect(){},disconnect(){}})},{}));
  await page.getByRole('button',{name:'Enable input',exact:true}).click();
  await expect(status(page)).toHaveText('Audio input enabled');
  expect(await page.evaluate(()=>capture.requests[0].audio.deviceId)).toEqual({exact:'device-b'});
});
