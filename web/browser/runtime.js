// SPDX-License-Identifier: GPL-3.0-or-later
// Local, bounded diagnostics. No telemetry or network transmission.
(() => {
  const events=[], peaks={};
  let stage='startup',started=performance.now(),lastPump=0,pumps=0,sequence=0;
  const record=(event,detail={})=>{
    if(events.length===64)events.shift();
    events.push({sequence:++sequence,event,at:performance.now(),wall:Date.now(),...detail});
  };
  globalThis.SurgeRuntime={
    record,
    phase(next){
      const now=performance.now(),duration=Math.max(0,now-started);
      if(stage!=='idle'){
        peaks[stage]=Math.max(peaks[stage]||0,duration);
        if(duration>250)record('slow-control-phase',{stage,milliseconds:duration});
      }
      if(next==='messages'){
        if(lastPump && now-lastPump>1000)record('control-gap',{milliseconds:now-lastPump});
        lastPump=now;
      }
      if(next==='idle')++pumps;
      stage=next;started=now;
    },
    snapshot(){
      return {stage,stageMilliseconds:Math.max(0,performance.now()-started),pumps,
        lastPump,peaks:{...peaks},events:events.map(event=>({...event}))};
    }
  };
  record('runtime-script');
})();
