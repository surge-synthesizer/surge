// SPDX-License-Identifier: GPL-3.0-or-later
if (typeof window !== 'undefined') {
  globalThis.SurgeFactory = {
    library: null,
    ready: null,
    skinSerial: 0,
    skinRequests: new Map(),
    impulseSerial: 0,
    impulseRequests: new Map(),
    requestImpulse(path) {
      const id=++this.impulseSerial;this.impulseRequests.set(id,0);
      SurgeBrowser.reportFile('Loading selected impulse response...');
      (async()=>{
        try {
          await this.ready;
          if(!this.library)throw Error('Factory index unavailable');
          await this.library.install(path.replace(/^\/factory\//,''),FS);
          if(this.impulseRequests.has(id)){this.impulseRequests.set(id,1);SurgeBrowser.reportFile('');}
        }catch(error){
          if(this.impulseRequests.has(id)){this.impulseRequests.set(id,-1);SurgeBrowser.reportFile('Impulse download failed; current response retained. '+error);}
        }
      })();
      return id;
    },
    requestSkin(path) {
      const id=++this.skinSerial;this.skinRequests.set(id,0);
      SurgeBrowser.reportFile('Loading selected skin...');
      (async()=>{
        try {
          await this.ready;
          if(!this.library)throw Error('Factory index unavailable');
          await this.library.installSkin(path.replace(/^\/factory\//,''),FS);
          if(this.skinRequests.has(id)){this.skinRequests.set(id,1);SurgeBrowser.reportFile('');}
        }catch(error){
          if(this.skinRequests.has(id)){this.skinRequests.set(id,-1);SurgeBrowser.reportFile('Skin download failed; current skin retained. '+error);}
        }
      })();
      return id;
    },
    async prepareWavetable(slot, token, path) {
      try {
        if (path.startsWith('/factory/')) {
          await SurgeFactory.ready;
          if (!SurgeFactory.library) throw Error('Factory index unavailable');
          await SurgeFactory.library.install(path.slice('/factory/'.length), FS);
        }
        Module.ccall('surge_browser_wt_downloaded', null, ['number','number','number','string'], [slot,token,1,'']);
      } catch (error) {
        Module.ccall('surge_browser_wt_downloaded', null, ['number','number','number','string'], [slot,token,0,String(error)]);
      }
    },
    async prepare(id, token, path) {
      SurgeRuntime.record('patch-download-start',{id,token});
      try {
        if (path.startsWith('/factory/')) {
          await SurgeFactory.ready;
          if (!SurgeFactory.library) throw Error('Factory index is unavailable; reload to retry');
          await SurgeFactory.library.install(path.slice('/factory/'.length), FS);
        }
        SurgeRuntime.record('patch-download-ready',{id,token});
        Module.ccall('surge_browser_patch_prepared', null, ['number','number','number','string'], [id,token,1,'']);
      } catch (error) {
        SurgeRuntime.record('patch-download-failed',{id,token,error:String(error)});
        Module.ccall('surge_browser_patch_prepared', null, ['number','number','number','string'], [id,token,0,String(error)]);
      }
    }
  };
  Module.preRun = Module.preRun || [];
  Module.preRun.push(() => {
    SurgeRuntime.record('factory-startup-start');
    addRunDependency('surge-factory-index');
    SurgeFactory.ready = (async () => {
      const {FactoryLibrary} = await import('./library.js');
      const library = await FactoryLibrary.open('./library/manifest.json', message => SurgeBrowser.reportFile(message));
      await library.installPatchIndex(FS);
      library.installWavetableCatalog(FS);
      library.installImpulseCatalog(FS);
      SurgeFactory.library = library;
      const initial = [...library.entries.values()].find(entry =>
        entry.path.startsWith('wavetables/') && ['.wt','.wav'].includes(entry.extension));
      if (initial) await library.install(initial.path, FS).catch(error =>
        SurgeBrowser.reportFile('Initial wavetable unavailable; using the embedded default. ' + error));
    })().catch(error => {
      SurgeBrowser.reportFile('Factory index could not be loaded: ' + error);
    }).finally(() => {SurgeRuntime.record('factory-startup-finished');removeRunDependency('surge-factory-index');});
  });
}
