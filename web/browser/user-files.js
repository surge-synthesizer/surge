// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  globalThis.SurgeUserFiles = {
    attach(module) {
      const open=document.createElement('button');open.id='user-files';open.textContent='User files';
      open.style.cssText='position:fixed;right:120px;bottom:8px;z-index:1000';document.body.append(open);
      open.onclick=()=>{
        const FS=module.FS,root='/user';let directory=root;
        const dialog=document.createElement('dialog');dialog.setAttribute('aria-label','User files');
        dialog.style.cssText='width:min(700px,85vw);max-height:75vh;background:#202328;color:white;border:1px solid #888';
        const heading=document.createElement('h2');heading.textContent='User files';
        const hint=document.createElement('p');hint.textContent='Download saved patches and other user files. Files remain in browser storage.';
        const close=document.createElement('button');close.textContent='Close user files';
        const up=document.createElement('button');up.textContent='Parent folder';
        const refresh=document.createElement('button');refresh.textContent='Refresh files';
        const exportFolder=document.createElement('button');exportFolder.textContent='Download this folder';
        const location=document.createElement('p');location.setAttribute('aria-live','polite');
        const list=document.createElement('ul');const status=document.createElement('p');status.setAttribute('role','status');
        const error=document.createElement('p');error.setAttribute('role','alert');
        const render=()=>{
          error.textContent='';status.textContent='';list.replaceChildren();
          location.textContent=directory===root?'User files':directory.slice(root.length+1);up.disabled=directory===root;
          try {
            const entries=FS.readdir(directory).filter(name=>name!=='.'&&name!=='..').map(name=>{
              const path=directory+'/'+name,info=FS.lstat(path);
              return {name,path,folder:FS.isDir(info.mode),file:FS.isFile(info.mode)};
            }).filter(e=>e.folder||e.file).sort((a,b)=>Number(b.folder)-Number(a.folder)||a.name.localeCompare(b.name));
            for(const entry of entries){
              const row=document.createElement('li'),button=document.createElement('button');
              button.textContent=(entry.folder?'Open folder ':'Download ')+entry.name;row.append(button);list.append(row);
              button.onclick=async()=>{
                if(entry.folder){directory=entry.path;render();return;}
                button.disabled=true;error.textContent='';status.textContent='';let writable;
                try {
                  // Copy before awaiting a picker: subsequent saves cannot change
                  // the version the user selected for this export.
                  const bytes=FS.readFile(entry.path);
                  const handle=await window.showSaveFilePicker({suggestedName:entry.name});
                  writable=await handle.createWritable();await writable.write(bytes);await writable.close();writable=null;
                  if(dialog.isConnected)status.textContent='Downloaded '+entry.name;
                }catch(reason){
                  if(writable)try{await writable.abort();}catch{}
                  if(dialog.isConnected){
                    if(reason?.name==='AbortError')status.textContent='Download canceled. The file remains in browser storage.';
                    else error.textContent='Unable to download '+entry.name+': '+String(reason)+'. The file remains in browser storage.';
                  }
                }finally{button.disabled=false;}
              };
            }
            if(!entries.length)status.textContent='This folder is empty.';
          }catch(reason){error.textContent='Unable to read user files: '+String(reason);}
        };
        exportFolder.onclick=async()=>{
          const source=directory,name=source===root?'Surge User Files':source.slice(source.lastIndexOf('/')+1);
          exportFolder.disabled=true;error.textContent='';status.textContent='';let copying=false;
          try {
            const parent=await window.showDirectoryPicker({mode:'readwrite'});
            const occupied=new Set();for await(const name of parent.keys())occupied.add(name);
            // Export into a fresh folder so existing destination files are never
            // overwritten. Preserve the suffix used to recognize portable skins.
            const extension=name.endsWith('.surge-skin')?'.surge-skin':'';
            const stem=extension?name.slice(0,-extension.length):name;
            let destination=name,suffix=2;
            while(occupied.has(destination))destination=stem+' '+(suffix++)+extension;
            copying=true;
            if(dialog.isConnected)status.textContent='Downloading '+name+'…';
            await SurgeBrowser.exportDirectory(source,await parent.getDirectoryHandle(destination,{create:true}));
            if(dialog.isConnected)status.textContent='Downloaded folder '+destination;
          }catch(reason){
            if(dialog.isConnected){
              if(reason?.name==='AbortError'&&!copying)status.textContent='Folder download canceled. Files remain in browser storage.';
              else error.textContent='Unable to download folder: '+String(reason)+(copying?'. Some destination files may have been written.':'')+' Files remain in browser storage.';
            }
          }finally{exportFolder.disabled=false;}
        };
        up.onclick=()=>{directory=directory.slice(0,directory.lastIndexOf('/'))||root;render();};
        refresh.onclick=render;close.onclick=()=>dialog.close();
        dialog.addEventListener('close',()=>{dialog.remove();open.focus();});
        dialog.append(heading,hint,close,up,refresh,exportFolder,location,list,status,error);document.body.append(dialog);dialog.showModal();render();
      };
    }
  };
})();
