// SPDX-License-Identifier: GPL-3.0-or-later
// Emscripten --pre-js: evaluated in the browser and skipped in workers/worklets.
if (typeof window !== 'undefined') {
  const platform = globalThis.SurgeBrowser = {
    storageState: 'loading',
    storageError: null,
    restoring: true,
    generation: 0,
    savedGeneration: 0,
    pendingFlush: null,
    flushTimer: null,
    report(message) {
      const element = document.getElementById('storage-status');
      if (element) element.textContent = message;
      const retry = document.getElementById('retry-storage');
      if (retry) retry.hidden = platform.storageState !== 'error';
    },
    reportFile(message) {
      const element = document.getElementById('file-status');
      if (element) element.textContent = message;
    },
    changed() {
      if (platform.restoring) return;
      ++platform.generation;
      clearTimeout(platform.flushTimer);
      platform.flushTimer = setTimeout(() => platform.flush().catch(() => {}), 100);
    },
    async flush() {
      if (platform.storageState === 'unavailable') throw platform.storageError;
      if (platform.pendingFlush) {
        await platform.pendingFlush;
        if (platform.savedGeneration < platform.generation) return platform.flush();
        return;
      }
      const generation = platform.generation;
      platform.storageState = 'saving';
      platform.report('Saving user files…');
      platform.pendingFlush = new Promise((resolve, reject) => {
        FS.syncfs(false, error => error ? reject(error) : resolve());
      });
      try {
        await platform.pendingFlush;
        platform.savedGeneration = generation;
        platform.storageState = 'saved';
        platform.storageError = null;
        platform.report('');
      } catch (error) {
        platform.storageState = 'error';
        platform.storageError = error;
        platform.report('Unable to save browser storage. Changes remain in memory; export your work before closing. ' + String(error));
        throw error;
      } finally {
        platform.pendingFlush = null;
        globalThis.SurgeArchiveRecovery?.refresh(FS);
      }
    },
    devicePreference(key, fallback, controls) {
      if (!['midi', 'audio'].includes(key)) throw Error('Unknown device preference');
      const path = '/user/.surge-browser-' + key;
      const message = document.createElement('span');
      message.setAttribute('role', 'alert'); message.dataset.preferenceStatus = '';
      message.style.display = 'block';
      const retry = document.createElement('button');
      retry.textContent = 'Retry saving device'; retry.hidden = true;
      controls.append(message, retry);
      const state = {value: fallback, set(value) {
        state.value = value;
        const temporary = path + '.tmp';
        try {
          if (typeof value !== 'string' || value.length > 16384 || value.includes('\0'))
            throw Error('Invalid device identifier');
          FS.writeFile(temporary, value);
          FS.rename(temporary, path);
          message.textContent = ''; retry.hidden = true;
          return true;
        } catch (error) {
          try { FS.unlink(temporary); } catch {}
          message.textContent = 'Device choice remains active but could not be saved: ' + String(error);
          retry.hidden = false;
          return false;
        }
      }};
      retry.onclick = () => state.set(state.value);
      try {
        if (FS.stat(path).size > 65536) throw Error('Invalid device identifier');
        // Emscripten's string decoder stops at NUL; validate the entire file.
        const value = new TextDecoder('utf-8', {fatal: true}).decode(FS.readFile(path));
        if (value.length > 16384 || value.includes('\0')) throw Error('Invalid device identifier');
        state.value = value;
      } catch (error) {
        // ENOENT in the pinned Emscripten/WASI errno ABI means no choice saved yet.
        if (error.errno !== 44)
          message.textContent = 'Saved device choice could not be read. Select a device to replace this setting: ' + String(error);
      }
      return state;
    },
    safeName(name) {
      if (!name || name === '.' || name === '..' || /[\0/\\]/.test(name)) throw Error('Invalid file name');
      return name;
    },
    async importFiles(files) {
      const batch = '/user/imports/' + crypto.randomUUID();
      // Read everything before publishing a selection to JUCE; a failed read
      // never changes the current patch or returns a partial selection.
      const loaded = await Promise.all(Array.from(files, async file => ({
        name: platform.safeName(file.name), bytes: new Uint8Array(await file.arrayBuffer())
      })));
      FS.mkdirTree(batch);
      const paths = [];
      try {
        for (const file of loaded) {
          const name = batch + '/' + file.name;
          if (paths.includes(name)) throw Error('Duplicate file name in selection');
          paths.push(name);
          FS.writeFile(name, file.bytes);
          if (/\.fxp$/i.test(file.name) && Module['_surge_browser_validate_patch']) {
            const error = Module['ccall']('surge_browser_validate_patch', 'string', ['string'], [name]);
            if (error) throw Error('Invalid patch: ' + error);
          }
        }
        // Storage failures are surfaced, but retain imported data in memory.
        await platform.flush().catch(() => {});
        return paths;
      } catch (error) {
        for (const path of paths) { try { FS.unlink(path); } catch {} }
        try { FS.rmdir(batch); } catch {}
        throw error;
      }
    },
    attachFileDrop(canvas, peer) {
      let generation = 0;
      canvas.addEventListener('dragover', event => {
        if (Array.from(event.dataTransfer?.types || []).includes('Files')) {
          event.preventDefault(); event.dataTransfer.dropEffect = 'copy';
        }
      });
      canvas.addEventListener('drop', async event => {
        event.preventDefault();
        const files = Array.from(event.dataTransfer?.files || []);
        const token = ++generation;
        const bounds = canvas.getBoundingClientRect();
        const x = Math.round(event.clientX - bounds.left), y = Math.round(event.clientY - bounds.top);
        platform.reportFile('');
        try {
          if (!files.length) throw Error('Drop a file, not a folder');
          const paths = await platform.importFiles(files);
          if (!canvas.isConnected || token !== generation) return;
          const accepted = Module['ccall']('surge_browser_file_drop', 'number',
            ['number','string','number','number'], [peer, JSON.stringify(paths), x, y]);
          if (!accepted) throw Error('This target does not accept the dropped files');
        } catch (error) {
          if (canvas.isConnected && token === generation) platform.reportFile('File drop failed: ' + String(error));
        }
      });
    },
    async copyDirectory(handle, directory) {
      FS.mkdirTree(directory);
      for await (const entry of handle.values()) {
        const destination = directory + '/' + platform.safeName(entry.name);
        if (entry.kind === 'directory') await platform.copyDirectory(entry, destination);
        else FS.writeFile(destination, new Uint8Array(await (await entry.getFile()).arrayBuffer()));
      }
    },
    async exportDirectory(directory, handle) {
      // The virtual export directory is new for each picker. Check the actual
      // destination too so a repeated frame export cannot overwrite earlier work.
      const occupied = new Set();
      for await (const name of handle.keys()) occupied.add(name);
      for (const name of FS.readdir(directory)) {
        if (name === '.' || name === '..') continue;
        const source = directory + '/' + name;
        if (FS.isDir(FS.stat(source).mode)) {
          let destination = name, suffix = 2;
          while (occupied.has(destination)) destination = name + ' ' + suffix++;
          occupied.add(destination);
          await platform.exportDirectory(source, await handle.getDirectoryHandle(destination, {create:true}));
        } else {
          const file = await handle.getFileHandle(name, {create:true});
          const writer = await file.createWritable();
          try { await writer.write(FS.readFile(source)); await writer.close(); }
          catch (error) { await writer.abort().catch(() => {}); throw error; }
        }
      }
    },
    async pickFiles(token, title, filters, initial, save, directory, multiple) {
      platform.reportFile('');
      let completed = false;
      const complete = paths => {
        completed = true;
        return Module['ccall']('surge_file_dialog_complete', 'number', ['number','string'], [token, JSON.stringify(paths)]);
      };
      try {
        if (directory) {
          const handle = await window.showDirectoryPicker({mode:save?'readwrite':'read'});
          const local = '/user/directories/' + crypto.randomUUID() + '/' + platform.safeName(handle.name);
          if (save) FS.mkdirTree(local);
          else await platform.copyDirectory(handle, local);
          if (!complete([local])) return;
          // JUCE's export callbacks finish their file writes synchronously.
          if (save) await platform.exportDirectory(local, handle);
          await platform.flush();
        } else if (save) {
          const suggestedName = initial.split('/').pop() || 'Untitled';
          const handle = await window.showSaveFilePicker({suggestedName});
          const local = '/user/exports/' + crypto.randomUUID() + '/' + platform.safeName(handle.name);
          const exportDirectory = local.slice(0, local.lastIndexOf('/'));
          FS.mkdirTree(exportDirectory);
          if (!complete([local])) return;
          let output = local;
          if (!FS.analyzePath(output).exists) {
            // Native callbacks may normalize the filename extension. This
            // operation owns a fresh private directory, so its sole output
            // still belongs to the selected handle. Never guess among files.
            const files = FS.readdir(exportDirectory).filter(name => name !== '.' && name !== '..')
              .map(name => exportDirectory + '/' + name)
              .filter(path => FS.isFile(FS.stat(path).mode));
            if (!files.length) throw Error('The application did not produce an export file');
            if (files.length !== 1) throw Error('The application produced multiple export files; no file was written to the selected destination');
            output = files[0];
          }
          const writer = await handle.createWritable();
          try { await writer.write(FS.readFile(output)); await writer.close(); }
          catch (error) { await writer.abort().catch(() => {}); throw error; }
          await platform.flush();
        } else {
          const extensions = filters.split(/[;,]/).map(x => x.trim().replace(/^\*/, '')).filter(x => /^\.[\w.-]+$/.test(x));
          const options = {multiple:!!multiple};
          if (extensions.length) options.types = [{description:title, accept:{'application/octet-stream':extensions}}];
          const handles = await window.showOpenFilePicker(options);
          const files = await Promise.all(handles.map(handle => handle.getFile()));
          complete(await platform.importFiles(files));
        }
      } catch (error) {
        if (completed || error.name !== 'AbortError') platform.reportFile('File operation failed: ' + String(error));
        if (!completed) complete([]);
      }
    },
    ready: null
  };
  // Existing C++ filesystem operations keep their paths. IDBFS watches their
  // close/rename/unlink operations; our queue adds error reporting and retries.
  Module['preRun'] = Module['preRun'] || [];
  Module['preRun'].push(() => {
    addRunDependency('surge-user-storage');
    platform.ready = new Promise(resolve => {
      FS.mkdirTree('/user');
      IDBFS.queuePersist = () => platform.changed();
      FS.mount(IDBFS, {autoPersist:true}, '/user');
      FS.syncfs(true, error => {
        platform.restoring = false;
        if (error) {
          // Never write an empty in-memory tree back over an unreadable database.
          platform.storageState = 'unavailable'; platform.storageError = error;
          platform.report('Browser storage could not be opened. Existing stored files were not overwritten. ' + String(error));
        } else platform.storageState = 'saved';
        globalThis.SurgeArchiveRecovery?.refresh(FS);
        removeRunDependency('surge-user-storage');
        resolve();
      });
    });
  });
  window.addEventListener('beforeunload', event => {
    if (platform.generation > platform.savedGeneration || platform.storageState === 'error') {
      event.preventDefault(); event.returnValue = '';
    }
  });
  document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'hidden' && platform.generation > platform.savedGeneration)
      platform.flush().catch(() => {});
  });
}
