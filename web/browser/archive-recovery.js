// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  let files=[], problems=[], filesystem;
  globalThis.SurgeArchiveRecovery={refresh(FS){
    filesystem=FS;files=[];problems=[];
    const visit=path=>{
      for(const name of FS.readdir(path)){
        if(name==='.'||name==='..')continue;
        const child=path+'/'+name,stat=FS.lstat(child);
        if(!FS.isDir(stat.mode))continue;
        if(!name.startsWith('.surge-archive-')){visit(child);continue;}
        const manifest=child+'/recovery.json';
        if(!FS.analyzePath(manifest).exists)continue;
        try{
          const entries=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(FS.readFile(manifest)));
          if(!Array.isArray(entries))throw Error('Invalid recovery list');
          for(const entry of entries){
            if(typeof entry.target!=='string'||!entry.target.startsWith('/user/')||
               typeof entry.backup!=='string'||!entry.backup.startsWith(child+'/backups/')||
               !/^\d+$/.test(entry.backup.slice((child+'/backups/').length)))throw Error('Invalid recovery path');
            if(FS.analyzePath(entry.backup).exists&&FS.isFile(FS.lstat(entry.backup).mode))files.push(entry);
          }
        }catch(error){problems.push('Unable to read an archive recovery record: '+String(error));}
      }
    };
    try{visit('/user');}catch(error){problems.push('Unable to inspect archive backups: '+String(error));}
    let button=document.getElementById('archive-recovery');
    if(!button){
      button=document.createElement('button');button.id='archive-recovery';button.textContent='Recover archive files';
      button.style.cssText='position:fixed;right:8px;bottom:190px;z-index:1000';
      button.onclick=()=>SurgeArchiveRecovery.show();document.body.append(button);
    }
    button.hidden=files.length===0&&problems.length===0;
  },show(){
    document.getElementById('archive-recovery-dialog')?.close();
    const previous=document.activeElement,dialog=document.createElement('dialog');
    dialog.id='archive-recovery-dialog';dialog.setAttribute('aria-label','Archive file recovery');
    dialog.style.cssText='width:min(800px,90vw);max-height:80vh;background:#202328;color:white;padding:16px';
    const heading=document.createElement('h2');heading.textContent='Retained archive originals';
    const info=document.createElement('p');info.textContent='Download original files retained after an incomplete archive rollback.';
    const error=document.createElement('p');error.setAttribute('role','alert');error.textContent=problems.join('\n');
    const list=document.createElement('ul');list.style.cssText='max-height:50vh;overflow:auto';
    for(const entry of files){
      const item=document.createElement('li'),button=document.createElement('button');
      button.textContent='Download '+entry.target.slice('/user/'.length);
      button.onclick=()=>{
        let url;
        try{
          const bytes=filesystem.readFile(entry.backup);
          url=URL.createObjectURL(new Blob([bytes],{type:'application/octet-stream'}));
          const link=document.createElement('a');link.href=url;link.download=entry.target.split('/').pop();
          dialog.append(link);link.click();link.remove();error.textContent='';
        }catch(reason){error.textContent='Unable to download original file: '+String(reason);}
        finally{if(url)setTimeout(()=>URL.revokeObjectURL(url),1000);}
      };
      item.append(button);list.append(item);
    }
    const close=document.createElement('button');close.textContent='Close recovery';close.onclick=()=>dialog.close();
    dialog.append(heading,info,error,list,close);
    dialog.addEventListener('close',()=>{dialog.remove();if(previous?.isConnected)previous.focus();},{once:true});
    document.body.append(dialog);dialog.showModal();close.focus();
  }};
})();
