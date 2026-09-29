// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  const contexts=new WeakMap();
  globalThis.SurgeAudioLifecycle = {
    guard(context) {
      if(contexts.has(context)) return;
      const state={retired:false};contexts.set(context,state);
      const worklet=context.audioWorklet;
      if(!worklet) return;
      const addModule=worklet.addModule.bind(worklet);
      worklet.addModule=(...args)=>addModule(...args).then(result=>{
        // Emscripten constructs its bootstrap node after this promise resolves.
        // A retired context must never construct a late bootstrap on a reused
        // Wasm stack, even if its import completed after close().
        if(state.retired || context.state==='closed') throw new Error('Audio context retired during worklet import');
        return result;
      });
    },
    watchProcessor(context, node, onError) {
      const state=contexts.get(context);
      state.processorCleanup?.();
      node.addEventListener('processorerror',onError);
      state.processorCleanup=()=>node.removeEventListener('processorerror',onError);
    },
    async close(context, node) {
      const state=contexts.get(context);if(state) state.retired=true;
      // Keep the context and its Wasm stack alive until Chrome confirms that
      // processing has stopped. suspend() alone does not release the context.
      node?.disconnect();
      if(context.state!=='closed') await context.close();
      context.onstatechange=null;
      state?.processorCleanup?.();
      const bootstrap=context.audioWorklet?.bootstrapMessage;
      if(bootstrap) {
        bootstrap.port.onmessage=null;
        bootstrap.port.close();
        bootstrap.disconnect();
      }
    }
  };
})();
