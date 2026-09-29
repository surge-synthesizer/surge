// SPDX-License-Identifier: GPL-3.0-or-later
// JUCE owns metadata and actions. DOM nodes retain identity between snapshots.
(() => {
  const roles=['button','checkbox','radio','combobox','img','slider','paragraph','paragraph',
    'textbox','menuitem','menubar','menu','table','row','columnheader','row','cell',
    'link','list','listitem','tree','treeitem','progressbar','group','dialog','group',
    'scrollbar','tooltip','dialog','none','group'];
  const scalar=(text,offset)=>{
    let units=0,points=0;
    for(const character of text){if(units+character.length>offset)break;units+=character.length;++points;}
    return points;
  };
  const utf16=(text,offset)=>Array.from(text).slice(0,offset).join('').length;
  const announcements=new Map();
  globalThis.SurgeAccessibility={
    announce(text,priority){
      if(!text)return;
      const urgency=priority===2?'assertive':'polite';
      let state=announcements.get(urgency);
      if(!state){
        const region=document.createElement('div');
        region.id=`juce-announcement-${urgency}`;
        region.setAttribute('role',urgency==='assertive'?'alert':'status');
        region.setAttribute('aria-live',urgency);
        region.setAttribute('aria-atomic','true');
        region.style.cssText='position:absolute;width:1px;height:1px;overflow:hidden;clip-path:inset(50%);white-space:nowrap';
        document.body.append(region);
        state={region,timer:null};announcements.set(urgency,state);
      }
      clearTimeout(state.timer);
      state.region.textContent='';
      // Separate clearing and insertion so repeated announcements create a
      // fresh live-region update. Rapid pending updates coalesce to the latest.
      state.timer=setTimeout(()=>{state.region.textContent=text;state.timer=null;},50);
    },
    attach(module) {
      if(!module._surge_accessibility_snapshot) return;
      const root=document.createElement('div');root.id='juce-accessibility';
      root.style.cssText='position:absolute;left:0;top:0;pointer-events:none';
      const ring=document.createElement('div');ring.setAttribute('aria-hidden','true');
      ring.style.cssText='position:fixed;pointer-events:none;outline:2px solid #ffb347;z-index:3000;display:none';
      document.body.append(root,ring);
      const nodes=new Map();
      const drawRing=data=>{
        const [x,y,width,height]=data.bounds;
        Object.assign(ring.style,{display:'block',left:x+'px',top:y+'px',width:width+'px',height:height+'px'});
      };
      const update=()=>{
        const tree=JSON.parse(module.ccall('surge_accessibility_snapshot','string',[],[]));
        const seen=new Set();
        const visit=(data,parent)=>{
          if(data.ignored){for(const child of data.children)visit(child,parent);return;}
          seen.add(data.id);
          let node=nodes.get(data.id);
          if(node && (node instanceof HTMLTextAreaElement)!==(typeof data.text==='string')){
            node.remove();nodes.delete(data.id);node=null;
          }
          if(!node){
            node=document.createElement(typeof data.text==='string'?'textarea':'div');node.dataset.juceAccessibleId=data.id;
            node.style.cssText='position:fixed;opacity:0;pointer-events:none';
            nodes.set(data.id,node);
            const act=(action,value=0)=>{
              const result=module._surge_accessibility_action(data.id,action,value);
              queueMicrotask(update);return result;
            };
            node.addEventListener('focus',()=>{act(2);drawRing(node.juceData);});
            node.addEventListener('blur',event=>{
              if(node instanceof HTMLTextAreaElement)module._surge_accessibility_composition(data.id,0);
              const destination=event.relatedTarget;
              if(destination?.dataset.jucePeer!==node.dataset.jucePeer&&destination?.id!==node.dataset.jucePeer)act(5);
              ring.style.display='none';
            });
            let commitText=()=>{};
            if(node instanceof HTMLTextAreaElement){
              node.spellcheck=false;
              const commit=commitText=()=>{
                if(document.activeElement!==node)return;
                // A native commit callback can release JUCE focus while this
                // DOM editor remains focused. A new user edit owns focus again.
                act(2);
                const before=node.juceData.text;
                module._surge_accessibility_text(data.id,module.stringToNewUTF8(before),module.stringToNewUTF8(node.value),
                  scalar(node.value,node.selectionStart),scalar(node.value,node.selectionEnd));
                update();
              };
              node.addEventListener('input',commit);
              node.addEventListener('select',()=>{
                const d=node.juceData;
                if(scalar(node.value,node.selectionStart)!==d.selectionStart||scalar(node.value,node.selectionEnd)!==d.selectionEnd)commit();
              });
              node.addEventListener('compositionstart',()=>{if(nodes.get(data.id)===node&&document.activeElement===node)module._surge_accessibility_composition(data.id,1);});
              node.addEventListener('compositionend',()=>{if(nodes.get(data.id)===node)module._surge_accessibility_composition(data.id,0);});
              node.addEventListener('beforeinput',event=>{
                // A typed delimiter uses the original Lua pairing rules.
                // Paste, replacement input and IME commits remain literal.
                if(node.juceData.codeEditorKeys && !event.isComposing && event.inputType==='insertText' &&
                    event.data?.length===1 && '()[]{}"\''.includes(event.data)){
                  commit();
                  if(module._surge_accessibility_character(data.id,event.data.codePointAt(0))){
                    event.preventDefault();update();return;
                  }
                }
                if(event.inputType==='historyUndo'||event.inputType==='historyRedo'){
                  if(document.activeElement===node)act(2);
                  event.preventDefault();module._surge_accessibility_key(data.id,90,event.inputType==='historyRedo'?9:8);update();
                }
              });
            }
            node.addEventListener('click',event=>{
              if(event.target!==node)return;
              const d=node.juceData;
              if(d.press)act(0);else if(d.toggle)act(1);else if(d.menu)act(3);
            });
            node.addEventListener('keydown',event=>{
              if(event.target!==node)return;
              const d=node.juceData;
              if(node instanceof HTMLTextAreaElement && event.isComposing)return;
              if(node instanceof HTMLTextAreaElement && document.activeElement===node)act(2);
              if(node instanceof HTMLTextAreaElement && d.codeEditorKeys)commitText();
              // Learning a shortcut owns the next chord even when focus stays
              // on the accessible Learn button. Modifier transitions are not
              // chords; ordinary buttons keep their browser activation model.
              if(d.captureKeys){
                if(event.isComposing||['Shift','Control','Alt','Meta','AltGraph'].includes(event.key))return;
                const flags=(event.shiftKey?1:0)|(event.ctrlKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0);
                if(module._surge_accessibility_key(data.id,event.keyCode,flags)){
                  event.preventDefault();event.stopPropagation();update();
                }
                return;
              }
              if(d.nativeKeys && !event.isComposing &&
                 ['ArrowUp','ArrowDown','PageUp','PageDown','Home','End','Enter','Escape'].includes(event.key)){
                const flags=(event.shiftKey?1:0)|(event.ctrlKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0);
                if(module._surge_accessibility_key(data.id,event.keyCode,flags)){
                  event.preventDefault();event.stopPropagation();update();
                  // Type-ahead navigation moves JUCE focus from its editor to
                  // the result list. Follow only this explicit native action;
                  // ordinary browser text selection keeps its existing model.
                  const focused=[...nodes.values()].find(n=>n.juceData.focused&&n.dataset.jucePeer===node.dataset.jucePeer);
                  focused?.focus({preventScroll:true});
                }
                return;
              }
              // Custom editors (MSEG, for example) own their keyboard model.
              // Only forward from the focused group itself, never an event
              // bubbling from one of its interactive children. Unhandled Tab
              // retains normal browser focus navigation.
              if(event.target===node && node.getAttribute('role')==='group' && d.focusable){
                const flags=(event.shiftKey?1:0)|(event.ctrlKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0);
                if(module._surge_accessibility_key(data.id,event.keyCode,flags)){
                  event.preventDefault();event.stopPropagation();update();
                }
                return;
              }
              if(node instanceof HTMLTextAreaElement){
                // The original Lua editor owns indentation and its command
                // shortcuts. Keep ordinary text input and IME in the browser.
                if(d.codeEditorKeys && (['Tab','Enter','Backspace'].includes(event.key) ||
                    ((event.metaKey||event.ctrlKey)&&['Enter','d','f','g','h'].includes(event.key.length===1?event.key.toLowerCase():event.key)))){
                  const flags=(event.shiftKey?1:0)|(event.ctrlKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0);
                  if(module._surge_accessibility_key(data.id,event.keyCode,flags)){
                    event.preventDefault();event.stopPropagation();update();
                    const focused=[...nodes.values()].find(n=>n.juceData.focused&&n.dataset.jucePeer===node.dataset.jucePeer);
                    focused?.focus({preventScroll:true});
                  }
                  return;
                }
                if(((event.metaKey||event.ctrlKey)&&['z','y'].includes(event.key.toLowerCase()))||
                    (event.key==='Enter'&&!d.multiline)||event.key==='Escape'){
                  const flags=(event.shiftKey?1:0)|(event.ctrlKey?2:0)|(event.altKey?4:0)|(event.metaKey?8:0);
                  event.preventDefault();module._surge_accessibility_key(data.id,event.keyCode,flags);update();
                }
                return;
              }
              if(event.key==='Enter'||event.key===' '){event.preventDefault();node.click();return;}
              if(event.key==='F10'&&event.shiftKey&&d.menu){event.preventDefault();act(3);return;}
              if(d.readonly || !Number.isFinite(d.value))return;
              const step=d.step>0?d.step:(d.max-d.min)/100;
              let value=d.value;
              if(event.key==='ArrowUp'||event.key==='ArrowRight')value+=step;
              else if(event.key==='ArrowDown'||event.key==='ArrowLeft')value-=step;
              else if(event.key==='Home')value=d.min;
              else if(event.key==='End')value=d.max;
              else return;
              event.preventDefault();act(4,Math.max(d.min,Math.min(d.max,value)));
            });
          }
          const previous=node.juceData;
          node.juceData=data;
          node.dataset.jucePeer=data.peer||'';
          if(node instanceof HTMLTextAreaElement){
            node.readOnly=data.textReadonly;
            node.disabled=data.disabled;
            const valueChanged=node.value!==data.text;
            if(valueChanged)node.value=data.text;
            const start=utf16(data.text,data.selectionStart),end=utf16(data.text,data.selectionEnd);
            // Do not erase a native selection whose select event has not yet
            // reached JUCE (for example select-all immediately after focus).
            const selectionChanged=!previous||previous.selectionStart!==data.selectionStart||previous.selectionEnd!==data.selectionEnd;
            if(document.activeElement!==node||valueChanged||selectionChanged)
              if(node.selectionStart!==start||node.selectionEnd!==end)node.setSelectionRange(start,end);
          }
          if(node.parentNode!==parent)parent.append(node);
          const role=node instanceof HTMLTextAreaElement?'textbox':data.role===9&&data.checked!==undefined?'menuitemcheckbox':roles[data.role]||'group';
          node.setAttribute('role',role);
          node.setAttribute('aria-label',data.label||data.description||'');
          if(role==='paragraph'){
            node.removeAttribute('aria-label');
            if(!data.children.length)node.textContent=data.label||data.valueText||data.description||'';
          }
          node.setAttribute('aria-description',data.help||data.description||'');
          node.setAttribute('aria-disabled',String(data.disabled));
          for(const [key,attribute]of Object.entries({checked:'aria-checked',selected:'aria-selected',expanded:'aria-expanded',value:'aria-valuenow',min:'aria-valuemin',max:'aria-valuemax',valueText:'aria-valuetext'})){
            if(data[key]!==undefined)node.setAttribute(attribute,String(data[key]));else node.removeAttribute(attribute);
          }
          if(['slider','textbox','combobox','checkbox','radio'].includes(role)&&data.readonly!==undefined)
            node.setAttribute('aria-readonly',String(data.readonly));
          else node.removeAttribute('aria-readonly');
          node.tabIndex=!data.disabled&&(node instanceof HTMLTextAreaElement||data.nativeKeys||data.press||data.toggle||data.menu||Number.isFinite(data.value))?0:-1;
          const [x,y,width,height]=data.bounds;
          Object.assign(node.style,{left:x+'px',top:y+'px',width:width+'px',height:height+'px'});
          if(document.activeElement===node)drawRing(data);
          for(const child of data.children)visit(child,node);
        };
        for(const data of tree)visit(data,root);
        for(const [id,node]of nodes)if(!seen.has(id)){
          if(document.activeElement===node)ring.style.display='none';
          node.remove();nodes.delete(id);
        }
      };
      this.update=update;
      const menuKey=event=>{
        const code={Enter:13,' ':32,ArrowLeft:37,ArrowUp:38,ArrowRight:39,ArrowDown:40}[event.key];
        if(event.key!=='Escape' && (!code || event.ctrlKey || event.metaKey || event.altKey || event.target instanceof HTMLTextAreaElement))return;
        // Opening a submenu may remove the old focused accessible node. Route
        // navigation from the document through a current JUCE popup, even when the
        // browser focus has consequently fallen back to body.
        for(const node of [...nodes.values()].reverse()){
          if(node.getAttribute('role')==='menu' && module._surge_accessibility_action(node.juceData.id,event.key==='Escape'?6:7,code||0)){
            event.preventDefault();event.stopPropagation();queueMicrotask(update);return;
          }
        }
      };
      document.addEventListener('keydown',menuKey,true);
      this.timer=setInterval(update,250);
      addEventListener('pagehide',()=>{clearInterval(this.timer);document.removeEventListener('keydown',menuKey,true);});
      addEventListener('pageshow',event=>{if(event.persisted){update();this.timer=setInterval(update,250);document.addEventListener('keydown',menuKey,true);}});
    }
  };
})();
