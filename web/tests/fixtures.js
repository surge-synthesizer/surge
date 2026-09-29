import {test as base,expect} from '@playwright/test';

export {expect};
export const test=base.extend({
  runtimeFailureEvidence:[async({page},use,testInfo)=>{
    await use();
    if(testInfo.status===testInfo.expectedStatus)return;
    let deadline;
    try{
      const snapshot=await Promise.race([
        page.evaluate(()=>({url:location.href,runtime:globalThis.SurgeRuntime?.snapshot()||null,
          engine:globalThis.Module?._surge_browser_control_state
            ?JSON.parse(Module.ccall('surge_browser_control_state','string',[],[])):null})),
        new Promise(resolve=>{deadline=setTimeout(()=>resolve({unavailable:'Browser main thread did not respond within 1 second'}),1000);})
      ]);
      await testInfo.attach('browser-runtime',{body:JSON.stringify(snapshot,null,2),contentType:'application/json'});
    }catch(error){
      await testInfo.attach('browser-runtime',{body:JSON.stringify({unavailable:String(error)}),contentType:'application/json'});
    }finally{clearTimeout(deadline);}
  },{auto:true}]
});
