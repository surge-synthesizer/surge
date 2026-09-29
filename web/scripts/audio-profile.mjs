// SPDX-License-Identifier: GPL-3.0-or-later
// Chrome tracing measures callback duration without instrumenting the DSP loop.
import {mkdir,writeFile} from 'node:fs/promises';
import {gzipSync} from 'node:zlib';
import {parseArgs} from 'node:util';
import {setTimeout as delay} from 'node:timers/promises';
import {fileURLToPath,pathToFileURL} from 'node:url';
import path from 'node:path';
import os from 'node:os';

export function distribution(values){
  if(!values.length || values.some(v=>!Number.isFinite(v)||v<0))throw Error('Invalid timing samples');
  const sorted=[...values].sort((a,b)=>a-b),at=p=>sorted[Math.ceil(p*sorted.length)-1];
  return {count:values.length,mean:values.reduce((a,b)=>a+b,0)/values.length,p50:at(.5),p95:at(.95),p99:at(.99),max:sorted.at(-1)};
}
export function summarizeTrace(trace,rate,frames){
  if(![44100,48000].includes(rate)||!Number.isInteger(frames)||frames<1)throw Error('Invalid audio format');
  const events=trace.traceEvents.filter(e=>e.name==='AudioWorkletProcessor::Process'&&e.ph==='X');
  if(!events.length)throw Error('Chrome trace contains no complete AudioWorklet callback events');
  if(new Set(events.map(e=>`${e.pid}:${e.tid}`)).size!==1)throw Error('Trace contains multiple worklet threads');
  const durations=events.map(e=>e.dur),budgetUs=frames/rate*1e6;
  const inconsistentCpuSamples=events.filter(e=>Number.isFinite(e.tdur)&&(e.tdur<0||e.tdur>e.dur)).length;
  const graph=trace.traceEvents.filter(e=>e.name==='RealtimeAudioDestinationHandler::Render'&&e.ph==='X'&&e.pid===events[0].pid&&e.tid===events[0].tid);
  return {budgetUs,wallUs:distribution(durations),
    threadCpuUs:!inconsistentCpuSamples&&events.every(e=>Number.isFinite(e.tdur))?distribution(events.map(e=>e.tdur)):null,
    inconsistentCpuSamples,
    wholeGraph:graph.length&&graph.every(e=>e.args?.frames===frames)
      ?{wallUs:distribution(graph.map(e=>e.dur)),observedOverBudget:graph.filter(e=>e.dur>budgetUs).length}:null,
    observedOverBudget:durations.filter(v=>v>budgetUs).length,
    capturedAudioSeconds:events.length*frames/rate};
}

export async function profile({rate,seconds,voices,patch,url,output,silentOutput}){
  if(![44100,48000].includes(rate)||!Number.isInteger(seconds)||seconds<1||seconds>120||
     !Number.isInteger(voices)||voices<1||voices>64)throw Error('Use rate 44100/48000, seconds 1..120 and voices 1..64');
  await mkdir(path.dirname(output),{recursive:true});await mkdir(output);
  const {chromium}=await import('@playwright/test');
  const browser=await chromium.launch({channel:'chrome',headless:true, args:silentOutput?['--disable-audio-output']:[]});
  const page=await browser.newPage({viewport:{width:1100,height:750}});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  try{
    const cdp=await browser.newBrowserCDPSession(),pageCdp=await page.context().newCDPSession(page);
    const {categories}=await cdp.send('Tracing.getCategories');
    if(!categories.includes('disabled-by-default-audio-worklet'))throw Error('Chrome has no AudioWorklet tracing category');
    await page.addInitScript(rate=>{
      const Original=AudioContext;
      globalThis.AudioContext=class extends Original{constructor(options={}){super({...options,sampleRate:rate});}};
    },rate);
    await page.goto(url);
    await page.getByRole('button',{name:'Main Menu',exact:true}).waitFor();
    await page.waitForFunction(()=>Module.ccall('surge_browser_patch_name','string',[],[])==='Init Saw');
    if(!await page.evaluate(patch=>Module.ccall('surge_browser_request_patch','number',['string'],[patch]),patch))throw Error('Patch request rejected');
    const expectedName=path.posix.basename(patch,'.fxp');
    await page.waitForFunction(name=>Module.ccall('surge_browser_patch_name','string',[],[])===name,expectedName);
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await page.waitForFunction(()=>Module._surge_browser_audio_blocks()>50);
    if(await page.evaluate(()=>Module._surge_browser_audio_rate())!==rate)throw Error('Unexpected AudioContext sample rate');
    await page.evaluate(voices=>{
      const {context,node}=SurgeAudioInput.input.graph;node.disconnect();
      const analyser=context.createAnalyser(),mute=context.createGain();mute.gain.value=0;
      node.connect(analyser);analyser.connect(mute);mute.connect(context.destination);
      const samples=new Float32Array(analyser.fftSize),lateness=[];
      let last=performance.now();
      const timer=setInterval(()=>{const now=performance.now();lateness.push(Math.max(0,now-last-50));last=now;},50);
      globalThis.audioProfile={analyser,mute,samples,lateness,timer};
      for(let i=0;i<voices;i++)if(!Module._surge_browser_midi(0x90,48+i,90,0))throw Error('MIDI queue rejected a note');
    },voices);
    await page.waitForFunction(()=>Module._surge_browser_active_voices()>0);
    await delay(2000);
    const snapshot=async()=>{
      const state=await page.evaluate(()=>{
        const p=audioProfile;p.analyser.getFloatTimeDomainData(p.samples);
        return {audioTime:Module._surge_browser_audio_time(),blocks:Module._surge_browser_audio_blocks(),
          patchName:Module.ccall('surge_browser_patch_name','string',[],[]),
          voices:Module._surge_browser_active_voices(),wasmMemoryBytes:Module._surge_browser_wasm_memory_bytes(),
          peak:Math.max(...p.samples.map(Math.abs)),diagnostics:JSON.parse(Module.ccall('surge_browser_audio_diagnostics','string',[],[]))};
      });
      if(state.patchName!==expectedName)throw Error('Patch changed during profiling: '+state.patchName);
      return {...state,jsHeap:await pageCdp.send('Runtime.getHeapUsage')};
    };
    const samples=[await snapshot()];
    await page.evaluate(()=>{audioProfile.lateness.length=0;});
    await cdp.send('Tracing.start',{transferMode:'ReturnAsStream',categories:'audio,disabled-by-default-audio-worklet,webaudio'});
    for(let second=0;second<seconds;second++){await delay(1000);samples.push(await snapshot());}
    const lateness=await page.evaluate(()=>{clearInterval(audioProfile.timer);return audioProfile.lateness;});
    const completed=new Promise(resolve=>cdp.once('Tracing.tracingComplete',resolve));
    await cdp.send('Tracing.end');
    let deadline,result;
    try{result=await Promise.race([completed,new Promise((_,reject)=>{deadline=setTimeout(()=>reject(Error('Chrome trace did not finish')),15000);})]);}
    finally{clearTimeout(deadline);}
    if(result.dataLossOccurred)throw Error('Chrome lost trace events');
    await page.evaluate(()=>Module._surge_browser_panic());
    let raw='';
    try{
      for(;;){const part=await cdp.send('IO.read',{handle:result.stream});raw+=part.base64Encoded?Buffer.from(part.data,'base64').toString():part.data;if(part.eof)break;}
    }finally{await cdp.send('IO.close',{handle:result.stream});}
    const frames=samples[0].diagnostics.frames;
    if(samples.some(s=>s.diagnostics.frames!==frames || s.diagnostics.outputChannels!==2))throw Error('Audio format changed while profiling');
    const timing=summarizeTrace(JSON.parse(raw),rate,frames);
    const report={schema:1,status:'measured',browser:browser.version(),host:{platform:os.platform(),arch:os.arch(),release:os.release()},
      patch,requestedNotes:voices,rate,frames,requestedSeconds:seconds,silentOutputDevice:silentOutput,
      timing,uiTimerLatenessMs:distribution(lateness),samples,errors,
      memory:{wasmGrowthBytes:samples.at(-1).wasmMemoryBytes-samples[0].wasmMemoryBytes,
        jsUsedHeapChangeBytes:samples.at(-1).jsHeap.usedSize-samples[0].jsHeap.usedSize},
      limitations:['Instrumented headless Chrome run; callback overruns are not physical-device underrun measurements.',
        'Wasm memory size and JavaScript heap usage do not measure live allocations within the Wasm heap.',
        'Thread CPU timing is withheld when trace clock samples exceed callback wall duration.',
        'UI timer lateness measures an idle editor during playback, not every interactive editing workflow.',
        'Requested notes are not a voice-count guarantee; actual engine voice counts are sampled.']};
    await writeFile(path.join(output,'trace.json.gz'),gzipSync(raw));
    await writeFile(path.join(output,'report.json'),JSON.stringify(report,null,2)+'\n');
    if(errors.length || samples.some(s=>!Number.isFinite(s.peak)) || samples.at(-1).blocks<=samples[0].blocks || !samples.some(s=>s.peak>1e-5))throw Error('Playback failed; inspect report.json');
    return report;
  }catch(error){
    await writeFile(path.join(output,'error.json'),JSON.stringify({error:String(error),pageErrors:errors},null,2));throw error;
  }finally{await browser.close();}
}

if(process.argv[1]&&import.meta.url===pathToFileURL(path.resolve(process.argv[1])).href){
  const {values}=parseArgs({options:{rate:{type:'string',default:'48000'},seconds:{type:'string',default:'60'},voices:{type:'string',default:'16'},
    patch:{type:'string',default:'/factory/patches_factory/Templates/Init FM2.fxp'},url:{type:'string',default:'http://127.0.0.1:8080/surge-xt-browser.html'},output:{type:'string'}}});
  const output=path.resolve(values.output||fileURLToPath(new URL(`../profiles/audio-profile-${Date.now()}-${values.rate}`,import.meta.url)));
  const result=await profile({...values,rate:Number(values.rate),seconds:Number(values.seconds),voices:Number(values.voices),output,silentOutput:process.env.SURGE_TEST_SILENT_OUTPUT==='1'});
  console.log(JSON.stringify({output,timing:result.timing,voiceCounts:[...new Set(result.samples.map(s=>s.voices))],uiTimerLatenessMs:result.uiTimerLatenessMs},null,2));
}
