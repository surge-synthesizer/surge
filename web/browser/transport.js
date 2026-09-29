// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  globalThis.SurgeTransport = {
    attach(module) {
      if (!module._surge_browser_transport) return;
      const controls = document.createElement('div');
      controls.id = 'transport-controls';
      controls.innerHTML = '<button data-play aria-pressed="false">Play transport</button> <button data-rewind>Rewind</button> <label>Tempo <input aria-label="Transport tempo" type="number" min="1" max="999" step="0.01" value="120"></label> <label>Meter <input aria-label="Beats per bar" type="number" min="1" max="32" step="1" value="4"> / <select aria-label="Beat unit"><option>1</option><option>2</option><option selected>4</option><option>8</option><option>16</option><option>32</option></select></label><span role="status" aria-live="polite"></span>';
      document.body.append(controls);
      const play = controls.querySelector('[data-play]');
      const tempo = controls.querySelector('[aria-label="Transport tempo"]');
      const numerator = controls.querySelector('[aria-label="Beats per bar"]');
      const denominator = controls.querySelector('select');
      const status = controls.querySelector('[role=status]');
      const update = playing => {
        if (!numerator.validity.valid || !Number.isInteger(numerator.valueAsNumber) ||
            !module._surge_browser_transport(playing ? 1 : 0, numerator.valueAsNumber, Number(denominator.value))) {
          status.textContent = 'Choose 1–32 beats per bar and a valid beat unit.';
          return false;
        }
        status.textContent = '';
        play.textContent = playing ? 'Pause transport' : 'Play transport';
        play.setAttribute('aria-pressed', String(playing));
        return true;
      };
      play.onclick = () => {
        const playing = !module._surge_browser_transport_playing();
        if (update(playing) && playing) module._surge_enable_audio();
      };
      controls.querySelector('[data-rewind]').onclick = () => module._surge_browser_rewind();
      tempo.onchange = () => {
        if (!tempo.validity.valid || !module._surge_browser_transport_tempo(tempo.valueAsNumber))
          status.textContent = 'Choose a tempo from 1 to 999 BPM.';
        else status.textContent = '';
      };
      numerator.onchange = denominator.onchange = () => update(module._surge_browser_transport_playing());
      // Reflect changes made in the original JUCE tempo editor or by patch
      // recall. Do not overwrite an in-progress edit in the browser field.
      setInterval(() => {
        if (document.activeElement !== tempo) tempo.value = String(module._surge_browser_transport_bpm());
      }, 250);
    }
  };
})();
