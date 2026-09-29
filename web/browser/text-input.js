// SPDX-License-Identifier: GPL-3.0-or-later
(() => {
  const peers=new Map();
  // JUCE indexes Unicode codepoints; EditContext indexes UTF-16 code units.
  function scalar(text,offset) {
    let units=0,points=0;
    for(const char of text) {if(units+char.length>offset)break;units+=char.length;++points;}
    return points;
  }
  const utf16=(text,offset)=>Array.from(text).slice(0,offset).join('').length;
  const rectangle=values=>new DOMRect(...values);
  function detach(id) {
    const state=peers.get(id);
    if(state){peers.delete(id);state.element.editContext=null;}
  }
  function create(id,generation,model) {
    const element=document.getElementById(`juce-${id}`);
    if(!element || typeof EditContext==='undefined')return;
    const context=new EditContext({text:model.text,selectionStart:utf16(model.text,model.start),selectionEnd:utf16(model.text,model.end)});
    const state={element,context,generation,model:model.text,composing:false};
    peers.set(id,state);element.editContext=context;
    const valid=()=>peers.get(id)===state;
    context.addEventListener('compositionstart',()=>{
      if(!valid())return;state.composing=true;
      Module._surge_text_composition(id,generation,1);
    });
    context.addEventListener('compositionend',()=>{
      if(!valid())return;state.composing=false;
      Module._surge_text_composition(id,generation,0);
    });
    context.addEventListener('textupdate',event=>{
      if(!valid())return;
      const before=state.model,after=context.text;
      const pointer=Module.stringToNewUTF8(event.text);
      // Native code takes ownership of this heap allocation.
      Module._surge_text_update(id,generation,scalar(before,event.updateRangeStart),scalar(before,event.updateRangeEnd),pointer,
        scalar(after,event.selectionStart),scalar(after,event.selectionEnd));
    });
    context.addEventListener('characterboundsupdate',event=>{
      if(!valid())return;
      const bounds=[];
      for(let index=event.rangeStart;index<Math.min(event.rangeEnd,context.text.length);++index)
        bounds.push(rectangle(JSON.parse(Module.ccall('surge_text_bounds','string',['number','number','number'],[id,generation,scalar(context.text,index)]))));
      context.updateCharacterBounds(event.rangeStart,bounds);
    });
    context.addEventListener('textformatupdate',event=>{
      if(!valid())return;
      Module._surge_text_format(id,generation,-1,0,0,0);
      for(const format of event.getTextFormats())
        Module._surge_text_format(id,generation,scalar(context.text,format.rangeStart),scalar(context.text,format.rangeEnd),
          ['none','solid','dotted','dashed','wavy'].indexOf(format.underlineStyle),['none','thin','thick'].indexOf(format.underlineThickness));
    });
    return state;
  }
  globalThis.SurgeTextInput={detach,isComposing:id=>peers.get(id)?.composing || false,sync(id,generation,model) {
    let state=peers.get(id);
    if(state && state.generation!==generation){detach(id);state=null;}
    state ||= create(id,generation,model);
    if(!state)return;
    const context=state.context;
    state.model=model.text;
    if(context.text!==model.text)context.updateText(0,context.text.length,model.text);
    const start=utf16(model.text,model.start),end=utf16(model.text,model.end);
    if(context.selectionStart!==start || context.selectionEnd!==end)context.updateSelection(start,end);
    context.updateControlBounds(rectangle(model.control));
    context.updateSelectionBounds(rectangle(model.caret));
  }};
})();
