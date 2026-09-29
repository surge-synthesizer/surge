// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  class AudioInput {
    constructor(module, element) {
      this.module=module; this.element=element;
      this.graph=null; this.stream=null; this.source=null;
      this.generation=0; this.desired=false;
      this.selector=element.querySelector('select');
      this.button=element.querySelector('[data-enable]');
      this.status=element.querySelector('[role=status]');
      this.button.onclick=()=>this.enable();
      element.querySelector('[data-stop]').onclick=()=>this.stop();
      this.preference=SurgeBrowser.devicePreference('audio','',element);
      if(this.preference.value) this.selector.add(new Option('Saved input (enable input to connect)',this.preference.value));
      this.selector.value=this.preference.value;
      this.selector.onchange=()=>{this.preference.set(this.selector.value);if(this.desired)this.enable()};
      navigator.mediaDevices?.addEventListener('devicechange',()=>this.listDevices());
      addEventListener('pagehide',()=>this.stop());
    }
    report(text) { this.status.textContent=text; }
    setGraph(context,node) {
      if(this.graph && this.graph.context!==context) this.stop();
      this.graph={context,node};
      if(this.desired) this.enable();
    }
    audioFailed(message) { this.stop(); this.graph=null; this.report(message); }
    async listDevices() {
      if(!navigator.mediaDevices?.enumerateDevices) return;
      try {
        const devices=await navigator.mediaDevices.enumerateDevices();
        const selected=this.selector.value;
        this.selector.replaceChildren(new Option('Default input',''));
        for(const device of devices.filter(d=>d.kind==='audioinput' && d.deviceId && d.deviceId!=='default'))
          this.selector.add(new Option(device.label || 'Audio input',device.deviceId));
        if(selected && ![...this.selector.options].some(o=>o.value===selected))
          this.selector.add(new Option('Selected input unavailable',selected));
        this.selector.value=selected;
      } catch(error) { this.report(`Unable to list audio inputs: ${error.message || error}`); }
    }
    release(stream,source) {
      source?.disconnect();
      for(const track of stream?.getTracks() || []) { track.onended=null; track.stop(); }
    }
    stop() {
      ++this.generation; this.desired=false;
      this.release(this.stream,this.source); this.stream=null; this.source=null;
      this.button.disabled=false; this.report('Audio input stopped');
    }
    async enable() {
      if(!navigator.mediaDevices?.getUserMedia) { this.report('Audio input is unavailable.'); return; }
      this.desired=true;
      if(!this.graph) {
        this.module._surge_enable_audio();
        if(this.desired) this.report('Starting audio before requesting input permission…');
        return;
      }
      const generation=++this.generation;
      const graph=this.graph;
      const deviceId=this.selector.value;
      this.button.disabled=true;
      this.report('Waiting for audio input permission…');
      let stream=null,source=null;
      try {
        const audio={echoCancellation:false,noiseSuppression:false,autoGainControl:false,channelCount:{ideal:2}};
        if(deviceId) audio.deviceId={exact:deviceId};
        stream=await navigator.mediaDevices.getUserMedia({audio,video:false});
        if(generation!==this.generation || this.graph!==graph || !this.desired) { this.release(stream,null); return; }
        if(!stream.getAudioTracks().length) throw new Error('The selected device supplied no audio track');
        source=graph.context.createMediaStreamSource(stream);
        source.connect(graph.node,0,0);
        const previousStream=this.stream,previousSource=this.source;
        this.stream=stream;this.source=source;
        for(const track of stream.getAudioTracks()) track.onended=()=>{
          if(this.stream===stream) {this.stop();this.report('Audio input disconnected. Select Enable input to retry.');}
        };
        this.release(previousStream,previousSource);
        this.report('Audio input enabled');
        await this.listDevices();
      } catch(error) {
        this.release(stream,source);
        if(generation===this.generation) {
          this.desired=!!this.stream;
          this.report(`Audio input failed: ${error.message || error}.${this.stream?' Current input retained.':''} Select Enable input to retry.`);
        }
      } finally { if(generation===this.generation) this.button.disabled=false; }
    }
  }
  globalThis.SurgeAudioInput={attach(module) {
    if(!module._surge_browser_midi) return;
    const element=document.createElement('div');element.id='audio-input-controls';
    element.innerHTML='<label>Audio input <select aria-label="Audio input device"><option value="">Default input</option></select></label> <button data-enable>Enable input</button> <button data-stop>Stop input</button><span role="status" aria-live="polite"></span>';
    document.body.append(element);this.input=new AudioInput(module,element);
  }};
})();
