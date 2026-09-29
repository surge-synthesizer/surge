import {test} from '@playwright/test';
import assert from 'node:assert/strict';
import {summarizeTrace,distribution} from '../scripts/audio-profile.mjs';
const event=(dur,extra={})=>({name:'AudioWorkletProcessor::Process',ph:'X',pid:1,tid:2,dur,tdur:dur/2,...extra});

test('callback budget uses the actual quantum and excludes nested author events',()=>{
  const trace={traceEvents:[event(100),event(3000),event(700),event(2000,{name:'AudioWorkletProcessor::Process (author script execution)'})]};
  const result=summarizeTrace(trace,48000,128);
  assert.equal(result.wallUs.count,3);assert.equal(result.wallUs.max,3000);
  assert.equal(result.wallUs.p50,700);assert.equal(result.threadCpuUs.max,1500);
  assert.equal(result.observedOverBudget,1);assert.equal(result.capturedAudioSeconds,.008);
  assert.equal(summarizeTrace(trace,44100,256).observedOverBudget,0);
});
test('absent, ambiguous and malformed callback timing is rejected',()=>{
  for(const traceEvents of [[],[event(1,{ph:'B'})],[event(10),event(10,{tid:3})],[event(-1)],[event(NaN)]])
    assert.throws(()=>summarizeTrace({traceEvents},48000,128));
  assert.throws(()=>summarizeTrace({traceEvents:[event(1)]},0,128));
  assert.throws(()=>summarizeTrace({traceEvents:[event(1)]},48000,0));
  assert.throws(()=>distribution([]));
});
test('missing thread CPU timing remains explicitly unavailable',()=>{
  const result=summarizeTrace({traceEvents:[event(12,{tdur:undefined})]},48000,128);
  assert.equal(result.threadCpuUs,null);assert.equal(result.wallUs.mean,12);
});
test('inconsistent CPU clock samples are reported and not summarized as valid timing',()=>{
  const result=summarizeTrace({traceEvents:[event(12,{tdur:100})]},48000,128);
  assert.equal(result.threadCpuUs,null);assert.equal(result.inconsistentCpuSamples,1);
  assert.equal(result.wallUs.max,12);
});
test('whole-graph timing is separate and requires matching audio frames',()=>{
  const callback=event(200),graph=event(3000,{name:'RealtimeAudioDestinationHandler::Render',args:{frames:128}});
  const result=summarizeTrace({traceEvents:[callback,graph]},48000,128);
  assert.equal(result.observedOverBudget,0);assert.equal(result.wholeGraph.observedOverBudget,1);
  assert.equal(result.wholeGraph.wallUs.max,3000);
  graph.args.frames=256;
  assert.equal(summarizeTrace({traceEvents:[callback,graph]},48000,128).wholeGraph,null);
});
