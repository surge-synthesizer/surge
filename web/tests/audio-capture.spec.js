import {test,expect,chromium} from '@playwright/test';
import {mkdtempSync,writeFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';

// Deterministic stereo capture: Chrome's file-backed device loops this WAV.
// No getUserMedia, MediaStream or AudioNode implementations are replaced.
function captureFile(directory) {
  const rate=48000,frames=rate,channels=2;
  const wav=Buffer.alloc(44+frames*channels*2);
  wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);
  wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(channels,22);
  wav.writeUInt32LE(rate,24);wav.writeUInt32LE(rate*channels*2,28);
  wav.writeUInt16LE(channels*2,32);wav.writeUInt16LE(16,34);
  wav.write('data',36);wav.writeUInt32LE(wav.length-44,40);
  for(let frame=0;frame<frames;frame++)
    for(let channel=0;channel<channels;channel++)
      wav.writeInt16LE(Math.round(3276*Math.sin(2*Math.PI*[220,330][channel]*frame/rate)),44+(frame*2+channel)*2);
  const filename=path.join(directory,'stereo.wav');writeFileSync(filename,wav);return filename;
}

for(const sampleRate of [44100,48000]) test(`Chrome capture reaches the real Surge worklet at ${sampleRate} Hz`,async({baseURL})=>{
  test.setTimeout(60000);
  const directory=mkdtempSync(path.join(tmpdir(),'surge-capture-'));
  let browser;
  try {
    browser=await chromium.launch({channel:'chrome',headless:true,args:[
    ...(process.env.SURGE_TEST_SILENT_OUTPUT==='1'?['--disable-audio-output']:[]),
    '--use-fake-device-for-media-stream',`--use-file-for-fake-audio-capture=${captureFile(directory)}`
    ]});
    const context=await browser.newContext({baseURL,viewport:{width:1100,height:750}});
    await context.grantPermissions([]); // Chrome denies permissions not in the list.
    const page=await context.newPage();
    const errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.addInitScript(rate=>{
      const Original=AudioContext;
      globalThis.AudioContext=class extends Original {
        constructor(options={}) {super({...options,sampleRate:rate});}
      };
    },sampleRate);
    await page.goto('/surge-xt-browser.html');
    await expect(page.locator('canvas').first()).toBeVisible({timeout:30000});
    expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],
      ['/factory/patches_factory/Templates/Audio In Stereo Osc 1.fxp']))).toBe(1);
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Audio In Stereo Osc 1');
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    await page.evaluate(()=>{
      const {context,node}=SurgeAudioInput.input.graph;
      node.disconnect();
      const splitter=context.createChannelSplitter(2);
      const analysers=[context.createAnalyser(),context.createAnalyser()];
      const silence=context.createGain();silence.gain.value=0;
      node.connect(splitter);
      analysers.forEach((analyser,index)=>{analyser.fftSize=4096;splitter.connect(analyser,index);analyser.connect(silence)});
      silence.connect(context.destination);
      globalThis.captureProbe={analysers,splitter,silence};
    });
    const levels=()=>page.evaluate(()=>captureProbe.analysers.map(analyser=>{
      const samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);
      return Math.max(...samples.map(Math.abs));
    }));
    expect(await levels()).toEqual([0,0]);
    await page.getByRole('button',{name:'Enable input',exact:true}).click();
    await expect(page.locator('#audio-input-controls [role=status]')).toContainText('Audio input failed');
    expect(await page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Audio In Stereo Osc 1');
    await context.grantPermissions(['microphone']);
    await page.getByRole('button',{name:'Enable input',exact:true}).click();
    await expect(page.locator('#audio-input-controls [role=status]')).toHaveText('Audio input enabled');
    await expect.poll(async()=>Math.min(...await levels())).toBeGreaterThan(0.01);
    const settings=await page.evaluate(()=>{
      const input=SurgeAudioInput.input;
      globalThis.firstCaptureTrack=input.stream.getAudioTracks()[0];
      return {stream:input.stream instanceof MediaStream,source:input.source instanceof MediaStreamAudioSourceNode,
        state:firstCaptureTrack.readyState,...firstCaptureTrack.getSettings()};
    });
    expect(settings.stream).toBe(true);expect(settings.source).toBe(true);expect(settings.state).toBe('live');
    expect(settings.channelCount).toBe(2);expect(settings.echoCancellation).toBe(false);
    expect(settings.noiseSuppression).toBe(false);expect(settings.autoGainControl).toBe(false);
    const tones=()=>page.evaluate(rate=>captureProbe.analysers.map(analyser=>{
      const samples=new Float32Array(analyser.fftSize);analyser.getFloatTimeDomainData(samples);
      return [220,330].map(frequency=>{
        let real=0,imaginary=0;
        for(let i=0;i<samples.length;i++) {
          const phase=2*Math.PI*frequency*i/rate;
          real+=samples[i]*Math.cos(phase);imaginary+=samples[i]*Math.sin(phase);
        }
        return Math.hypot(real,imaginary)/samples.length;
      });
    }),sampleRate);
    // Capture starts asynchronously; a first nonzero sample does not mean the
    // analyser's whole window is populated. Require the same separation once
    // both channels have a complete, stable capture window.
    await expect.poll(async()=>{
      const values=await tones();
      return Math.min(values[0][0]/Math.max(values[0][1],1e-12),
        values[1][1]/Math.max(values[1][0],1e-12));
    }).toBeGreaterThan(10);
    const selector=page.getByLabel('Audio input device',{exact:true});
    const device=await selector.locator('option').evaluateAll(options=>options.find(option=>option.value)?.value);
    expect(device).toBeTruthy();await selector.selectOption(device);
    await expect.poll(()=>page.evaluate(()=>firstCaptureTrack.readyState)).toBe('ended');
    await expect.poll(async()=>Math.min(...await levels())).toBeGreaterThan(0.01);
    await page.evaluate(()=>globalThis.lastCaptureTrack=SurgeAudioInput.input.stream.getAudioTracks()[0]);
    await page.getByRole('button',{name:'Stop input',exact:true}).click();
    expect(await page.evaluate(()=>lastCaptureTrack.readyState)).toBe('ended');
    await expect.poll(async()=>Math.max(...await levels())).toBeLessThan(0.00001);
    expect(await page.evaluate(()=>Module._surge_browser_audio_status())).toBe(2);
    expect(errors).toEqual([]);
  } finally {
    try {await browser?.close();}
    finally {rmSync(directory,{recursive:true,force:true});}
  }
});
