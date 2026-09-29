// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  function report(message) {
    let status=document.getElementById('clipboard-status');
    if(!status) {
      status=document.createElement('div');status.id='clipboard-status';status.setAttribute('role','alert');
      Object.assign(status.style,{position:'fixed',left:'8px',bottom:'110px',zIndex:2100,background:'#202328',padding:'8px',maxWidth:'600px'});
      document.body.append(status);
    }
    status.textContent=message;
  }
  globalThis.SurgeClipboard={report,async run(id,write,text,complete) {
    let timer;
    try {
      const operation=write?navigator.clipboard.writeText(text):navigator.clipboard.readText();
      const result=await Promise.race([operation,new Promise((_,reject)=>{
        timer=setTimeout(()=>reject(new Error('Clipboard request timed out. Try again.')),30000);
      })]);
      report('');
      complete(1,write?'':result);
    } catch(error) {
      report(`Unable to ${write?'copy':'paste'}: ${error.message || error}`);
      complete(0,'');
    } finally {clearTimeout(timer);}
  }};
})();
