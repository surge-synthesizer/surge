// SPDX-License-Identifier: GPL-3.0-or-later
// Web MIDI forwards channel messages unchanged to Surge's existing MIDI/MPE and
// MIDI-learn implementation. Only the audio thread consumes the bounded queue.
(() => {
  class MidiInput {
    constructor(module, controls) {
      this.module = module;
      this.controls = controls;
      this.access = null;
      this.ports = new Map();
      this.preference = SurgeBrowser.devicePreference('midi', '*', controls);
      this.selected = this.preference.value;
      this.pending = false;
      this.message = controls.querySelector('[role=status]');
      this.selector = controls.querySelector('select');
      this.button = controls.querySelector('button');
      if(this.selected!=='*') this.selector.add(new Option('Saved MIDI device (enable MIDI to connect)',this.selected));
      this.selector.value=this.selected;
      this.button.onclick = () => this.enable();
      this.selector.onchange = () => {
        this.module._surge_browser_panic();
        this.selected = this.selector.value;
        this.preference.set(this.selected);
      };
      controls.querySelector('[data-panic]').onclick = () => this.module._surge_browser_panic();
      controls.querySelector('[data-disable]').onclick = () => this.disable();
    }
    report(text) { this.message.textContent = text; }
    async enable() {
      if (this.pending || this.access) return;
      if (!navigator.requestMIDIAccess) { this.report('Web MIDI is unavailable.'); return; }
      this.pending = true;
      this.button.disabled = true;
      this.report('Waiting for MIDI permission…');
      try {
        const access = await navigator.requestMIDIAccess({sysex:false});
        this.access = access;
        access.onstatechange = () => this.refresh();
        this.refresh();
      } catch (error) {
        this.report(`MIDI permission failed: ${error.message || error}. Select Enable MIDI to retry.`);
      } finally {
        this.pending = false;
        this.button.disabled = !!this.access;
      }
    }
    refresh() {
      if (!this.access) return;
      const connected = new Map([...this.access.inputs.values()]
        .filter(port => port.state === 'connected').map(port => [port.id, port]));
      for (const [id, port] of this.ports) {
        if (!connected.has(id) || connected.get(id) !== port) {
          port.onmidimessage = null;
          this.module._surge_browser_panic();
        }
      }
      const previous = this.ports;
      this.ports = connected;
      this.selector.replaceChildren(new Option('All MIDI inputs', '*'));
      for (const [id, port] of connected) {
        this.selector.add(new Option(port.name || 'MIDI input', id));
        if (previous.get(id) !== port && port.open) {
          const access = this.access;
          port.open().catch(error => {
            if (this.access === access && this.ports.get(id) === port)
              this.report(`Unable to open ${port.name || 'MIDI input'}: ${error.message || error}. Disable MIDI and enable it to retry.`);
          });
        }
        port.onmidimessage = event => {
          if (this.selected === '*' || this.selected === id) this.receive(event);
        };
      }
      if (this.selected !== '*' && !connected.has(this.selected)) {
        // Do not silently switch a disconnected selected device to all inputs.
        this.selector.add(new Option('Selected device disconnected', this.selected));
      }
      this.selector.value = this.selected;
      this.report(connected.size ? `${connected.size} MIDI input(s) connected` : 'No MIDI inputs connected');
    }
    receive(event) {
      const bytes = event.data;
      const status = bytes?.[0];
      if (!(status >= 0x80 && status < 0xf0)) return;
      const length = (status & 0xf0) === 0xc0 || (status & 0xf0) === 0xd0 ? 2 : 3;
      if (bytes.length !== length || [...bytes].slice(1).some(b => b > 127)) return;
      if (this.module._surge_browser_audio_status() !== 2) {
        this.report('Enable audio before playing MIDI.');
        return;
      }
      const time = this.module._surge_browser_audio_time();
      const rate = this.module._surge_browser_audio_rate();
      // Web MIDI timestamps share performance.now()'s clock. Clamp delayed
      // delivery to now and reject unreasonable future timing from a device.
      const delay = Number.isFinite(event.timeStamp) ? Math.max(0, Math.min(1, (event.timeStamp-performance.now())/1000)) : 0;
      const frame = Math.floor((time + delay) * rate);
      if (!this.module._surge_browser_midi(status, bytes[1], bytes[2] || 0, frame))
        this.report('MIDI queue full; notes stopped to prevent stuck notes.');
    }
    disable() {
      if (this.pending) return;
      this.module._surge_browser_panic();
      if (this.access) this.access.onstatechange = null;
      for (const port of this.ports.values()) {
        port.onmidimessage = null;
        port.close?.().catch(error => this.report(`Unable to close MIDI input: ${error.message || error}`));
      }
      this.ports.clear();
      this.access = null;
      this.button.disabled = false;
      this.selector.replaceChildren(new Option('All MIDI inputs', '*'));
      if(this.selected!=='*') this.selector.add(new Option('Saved MIDI device (enable MIDI to connect)',this.selected));
      this.selector.value=this.selected;
      this.report('MIDI disabled');
    }
  }
  globalThis.SurgeMidi = {
    attach(module) {
      if (!module._surge_browser_midi) return;
      const controls = document.createElement('div');
      controls.id = 'midi-controls';
      controls.innerHTML = '<button>Enable MIDI</button> <label>MIDI input <select aria-label="MIDI input"><option value="*">All MIDI inputs</option></select></label> <button data-panic>All notes off</button> <button data-disable>Disable MIDI</button><span role="status" aria-live="polite"></span>';
      document.body.append(controls);
      this.input = new MidiInput(module, controls);
    }
  };
})();
