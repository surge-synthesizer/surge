// Shared native/Wasm comparison fixture. Executed inside the browser page.
export async function renderEngineFixture({rate,bytes,convolution,effectType,effectParameter,effectParameters=[],audioInput,effectOnly,verifyMseg,verifyFormulaPreparation,oscillatorType,storageSeed,msegSeed}){
  const {default:create}=await import('/surge-web.js');
  const m=await create();
  m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');
  m.FS.writeFile('/patch.fxp',new Uint8Array(bytes));
  const engine=m.ccall('surge_create','number',['number','string'],[rate,'/factory']);
  if(!engine)throw Error(m.ccall('surge_error','string',[],[]));
  const out=m._malloc(2048);
  try {
    if(!m.ccall('surge_load_patch','number',['number','string'],[engine,'/patch.fxp']))
      throw Error(m.ccall('surge_error','string',[],[]));
    if(oscillatorType!==undefined){
      if(!m._surge_set_oscillator_type(engine,0,0,oscillatorType))throw Error('Oscillator selection failed');
      for(const [scene,slot,type] of [[-1,0,oscillatorType],[2,0,oscillatorType],[0,3,oscillatorType],[0,0,999]])
        if(m._surge_set_oscillator_type(engine,scene,slot,type))throw Error('Invalid oscillator selection accepted');
      if(!m.ccall('surge_save_patch','number',['number','string'],[engine,'/oscillator.fxp']))throw Error('Oscillator verification save failed');
      const b=m.FS.readFile('/oscillator.fxp'),length=new DataView(b.buffer,b.byteOffset,b.byteLength).getUint32(64,true);
      const xml=new DOMParser().parseFromString(new TextDecoder().decode(b.slice(92,92+length)).replace(/\0+$/,''),'text/xml');
      if(Number(xml.querySelector('a_osc1_type')?.getAttribute('value'))!==oscillatorType ||
         Number(xml.querySelector('a_osc1_retrigger')?.getAttribute('value'))!==1)
        throw Error('Loaded oscillator type or retrigger differs from the intended fixture');
    }
    if(verifyMseg){
      if(!m.ccall('surge_save_patch','number',['number','string'],[engine,'/verify.fxp']))throw Error('Unable to verify loaded MSEG');
      const b=m.FS.readFile('/verify.fxp'),length=new DataView(b.buffer,b.byteOffset,b.byteLength).getUint32(64,true);
      const xml=new DOMParser().parseFromString(new TextDecoder().decode(b.slice(92,92+length)).replace(/\0+$/,''),'text/xml');
      const model=xml.querySelector('mseg[scene="0"][i="0"]'),segments=model?.querySelectorAll('segment');
      const routing=xml.querySelector('a_osc1_pitch modrouting');
      const segmentTypes=verifyMseg.segmentTypes||[verifyMseg.type,verifyMseg.type,verifyMseg.type];
      if(Number(xml.querySelector('a_lfo0_shape')?.getAttribute('value'))!==verifyMseg.shape ||
         segments?.length!==segmentTypes.length || [...segments].some((s,i)=>Number(s.getAttribute('type'))!==segmentTypes[i]) ||
         Number(routing?.getAttribute('source'))!==verifyMseg.source || Number(routing?.getAttribute('depth'))!==6)
        throw Error('Loaded patch does not contain the intended MSEG and modulation route');
    }
    if(effectType!==undefined && !m._surge_set_effect_type(engine,0,effectType))
      throw Error(m.ccall('surge_error','string',[],[]));
    if(effectType!==undefined){
      if(m._surge_set_effect_type(engine,0,999999) || m._surge_set_effect_type(engine,99,effectType))
        throw Error('Accepted an invalid effect edit');
    }
    for(const edit of [...(effectParameter?[effectParameter]:[]),...effectParameters]){
      if(!m._surge_set_effect_parameter(engine,0,edit.index,edit.value))
        throw Error(m.ccall('surge_error','string',[],[]));
      if(m._surge_set_effect_parameter(engine,0,edit.index,NaN) ||
        m._surge_set_effect_parameter(engine,0,999,0.5))throw Error('Accepted an invalid effect parameter');
    }
    if(convolution){
      const left=new Float32Array(4096),right=new Float32Array(4096);
      left[0]=.8;left[63]=.2;left[511]=-.1;left[2048]=.05;
      right[0]=.4;right[127]=-.25;right[1500]=.1;right[3072]=.01;
      if(!m.ccall('surge_set_impulse','number',['number','number','number','array','array','number'],
        [engine,0,32000,new Uint8Array(left.buffer),new Uint8Array(right.buffer),4096]))
        throw Error(m.ccall('surge_error','string',[],[]));
      // Rejected input must not alter the already prepared impulse. The
      // following audio comparison checks that the previous state remains.
      const invalid=new Uint8Array(new Float32Array([NaN]).buffer);
      if(m.ccall('surge_set_impulse','number',['number','number','number','array','number','number'],
        [engine,0,32000,invalid,0,1]))throw Error('Accepted a non-finite impulse');
    }
    if(effectOnly){
      for(let i=0;i<32;++i){m.setValue(out+1024+i*4,0,'float');m.setValue(out+1536+i*4,0,'float');}
      m.setValue(out,7.25,'float');m.setValue(out+512,-3.5,'float');
      if(m._surge_render_effect_block(engine,99,out+1024,out+1536,out,out+512) ||
        m._surge_render_effect_block(engine,0,0,out+1536,out,out+512))throw Error('Accepted invalid isolated render arguments');
      m.setValue(out+1024,NaN,'float');
      if(m._surge_render_effect_block(engine,0,out+1024,out+1536,out,out+512))throw Error('Accepted non-finite isolated input');
      if(m.getValue(out,'float')!==7.25 || m.getValue(out+512,'float')!==-3.5)throw Error('Rejected render changed output');
      const result=[];
      for(let frame=0;frame<rate*3;frame+=32){
        for(let i=0;i<32;++i){
          const active=frame+i<rate;
          m.setValue(out+1024+i*4,active?(((frame+i)*17)%257-128)/256:0,'float');
          m.setValue(out+1536+i*4,active?(((frame+i)*29)%193-96)/256:0,'float');
        }
        if(!m._surge_render_effect_block(engine,0,out+1024,out+1536,out,out+512))
          throw Error(m.ccall('surge_error','string',[],[]));
        for(let i=0;i<32 && frame+i<rate*3;++i)result.push(m.getValue(out+i*4,'float'),m.getValue(out+512+i*4,'float'));
      }
      return result;
    }
    if(storageSeed!==undefined){
      if(!Number.isInteger(storageSeed)||storageSeed<0||storageSeed>0xffffffff||!m._surge_seed_storage_rng(engine,storageSeed))
        throw Error('Invalid storage RNG seed');
    }
    if(msegSeed!==undefined && m._surge_seed_voice_mseg(engine,0,0,msegSeed))
      throw Error('MSEG seed accepted an absent voice');
    const formulaCompilations=verifyFormulaPreparation?m._surge_formula_compilation_count(engine):0;
    if(verifyFormulaPreparation && formulaCompilations<=0)throw Error('Patch loading did not prepare formulas');
    m._surge_midi(engine,0x90,60,100);
    if(msegSeed!==undefined){
      if(!Number.isInteger(msegSeed)||msegSeed<0||msegSeed>0xffffffff||!m._surge_seed_voice_mseg(engine,0,0,msegSeed))
        throw Error('Invalid or absent voice MSEG seed target');
    }
    const result=[],sizes=[17,128,63,32];
    const release=Math.floor(rate/32)*32;
    let index=0;
    for(let frame=0;frame<rate*3;) {
      if(frame===release)m._surge_midi(engine,0x80,60,0);
      const boundary=frame<release?release:rate*3;
      const n=Math.min(sizes[index++%sizes.length],boundary-frame);
      if(audioInput)for(let i=0;i<n;++i){
        m.setValue(out+1024+i*4,((frame+i)%257-128)/256,'float');
        m.setValue(out+1536+i*4,((frame+i)%193-96)/256,'float');
      }
      if(!m._surge_render(engine,audioInput?out+1024:0,audioInput?out+1536:0,out,out+512,n))throw Error('Render failed');
      for(let i=0;i<n;++i)result.push(m.getValue(out+i*4,'float'),m.getValue(out+512+i*4,'float'));
      frame+=n;
    }
    if(verifyFormulaPreparation && m._surge_formula_compilation_count(engine)!==formulaCompilations)
      throw Error('Formula compiled during note attack or rendering');
    return result;
  } finally {m._free(out);m._surge_destroy(engine);}
}
