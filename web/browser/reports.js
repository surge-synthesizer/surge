// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  globalThis.SurgeReports={show(html) {
    document.querySelector('#surge-report')?.close();
    const previous=document.activeElement;
    const dialog=document.createElement('dialog');
    dialog.id='surge-report';dialog.setAttribute('aria-label','Surge report');
    const toolbar=document.createElement('div');
    const download=document.createElement('button');download.textContent='Download report';
    const close=document.createElement('button');close.textContent='Close report';
    const message=document.createElement('span');message.setAttribute('role','alert');
    const content=document.createElement('iframe');content.title='Surge report content';
    // Imported patch/skin text may appear in a report. Display it without
    // granting script, same-origin, top-navigation, form, or popup privileges.
    content.setAttribute('sandbox','');content.srcdoc=html;
    toolbar.append(download,close,message);dialog.append(toolbar,content);
    let url;
    download.onclick=()=>{
      try {
        url ||= URL.createObjectURL(new Blob([html],{type:'text/html;charset=utf-8'}));
        const link=document.createElement('a');link.href=url;link.download='surge-report.html';
        dialog.append(link);link.click();link.remove();message.textContent='';
      } catch(error) { message.textContent=`Unable to download report: ${error.message || error}`; }
    };
    close.onclick=()=>dialog.close();
    dialog.addEventListener('close',()=>{
      if(url) URL.revokeObjectURL(url);
      dialog.remove();
      if(previous?.isConnected) previous.focus();
    },{once:true});
    document.body.append(dialog);dialog.showModal();close.focus();
  }};
})();
