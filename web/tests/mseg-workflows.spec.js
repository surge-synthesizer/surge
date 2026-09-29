import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';

async function closeEditor(page){
  const editor=page.getByRole('group',{name:'Voice MSEG 1 Editor',exact:true});
  if(await editor.count())await editor.getByRole('button',{name:'close',exact:true}).dispatchEvent('click');
  await expect(editor).toHaveCount(0);
}
async function snapshot(page,name){
  await closeEditor(page);
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Save MSEG Preset As...',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'Value',exact:true}).fill(name);
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const path='/user/Modulator Presets/MSEG/'+name+'.modpreset';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,path)).toBe(true);
  return page.evaluate(path=>{
    const xml=new DOMParser().parseFromString(Module.FS.readFile(path,{encoding:'utf8'}),'text/xml');
    return new XMLSerializer().serializeToString(xml.querySelector('mseg'));
  },path);
}
async function openEditor(page){
  await page.getByRole('button',{name:'Show MSEG Editor',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('group',{name:'MSEG Settings',exact:true})).toBeAttached();
  return page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
}

async function startMSEG(page){
  await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  await page.getByRole('button',{name:'LFO Menu',exact:true}).dispatchEvent('click');
  for(const name of ['MSEG','1 Chords','1 Major'])await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('radio',{name:'MSEG',exact:true})).toBeChecked();
  // Opening initializes the factory preset's unset viewport to the fitted axes.
  // Establish that view before capturing the state the edit must undo to.
  await openEditor(page);
}

for(const [key,value] of [['ArrowUp',-0.95],['Shift+ArrowUp',-0.99]])
test(`MSEG ${key} node edit survives undo, redo and a saved patch reload`,async({page})=>{
  await startMSEG(page);
  const before=await snapshot(page,'MSEG before');
  const display=await openEditor(page);
  await display.focus();
  await page.keyboard.press('Tab');
  await expect(page.getByRole('alert').filter({hasText:'Node 2 of 4'})).toBeAttached();
  await expect(display).toBeFocused();
  await page.keyboard.press('Shift+Tab');
  await expect(page.getByRole('alert').filter({hasText:'Node 1 of 4'})).toBeAttached();
  await page.keyboard.press(key);
  await expect(page.getByRole('alert').filter({hasText:`value ${value.toFixed(2)}`})).toBeAttached();
  const edited=await snapshot(page,'MSEG edited');
  expect(edited).not.toBe(before);
  const values=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');
    return [...m.querySelectorAll('segment')].map(s=>Number(s.getAttribute('v0')));
  },edited);
  expect(values[0]).toBeCloseTo(value,5);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'MSEG undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'MSEG redo')).toBe(edited);
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('MSEG browser edit');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const saved='/user/Patches/Browser Tests/MSEG browser edit.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,saved)).toBe(true);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  expect(await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),saved)).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('MSEG browser edit');
  expect(await snapshot(page,'MSEG reloaded')).toBe(edited);
  await openEditor(page);
  await page.screenshot({path:`test-results/mseg-${key}-reloaded.png`});
});


test('MSEG node insertion and deletion preserve undo and redo state',async({page})=>{
  await startMSEG(page);
  const before=await snapshot(page,'Structure before');
  let display=await openEditor(page);await display.focus();
  await page.keyboard.press('Alt+n');
  await expect(page.getByRole('alert').filter({hasText:'Node 2 of 5'})).toBeAttached();
  const inserted=await snapshot(page,'Structure inserted');
  expect(inserted).toContain('activeSegments="4"');
  expect(inserted).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Structure insert undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Structure insert redo')).toBe(inserted);
  display=await openEditor(page);await display.focus();
  await page.keyboard.press('Home');await page.keyboard.press('Tab');
  await page.keyboard.press('Backspace');
  const deleted=await snapshot(page,'Structure deleted');
  expect(deleted).toContain('activeSegments="3"');
  // Native unsplitSegment resets the merged control-point duration to the
  // combined segment duration; deleting a node is not the inverse of splitting.
  const merged=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');
    const first=m.querySelector('segment');first.setAttribute('cpduration',first.getAttribute('duration'));
    return new XMLSerializer().serializeToString(m.documentElement);
  },before);
  expect(deleted).toBe(merged);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Structure delete undo')).toBe(inserted);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Structure delete redo')).toBe(deleted);
});


const storageHeader=readFileSync(new URL('../../src/common/SurgeStorage.h',import.meta.url),'utf8');
const typeBody=storageHeader.match(/enum Type\s*\{(\s*LINEAR = 1,[\s\S]*?)\}/)?.[1];
if(!typeBody)throw Error('Review MSEG segment type declarations');
const members=typeBody.replace(/\/\/[^\n]*/g,'').split(',').map(x=>x.trim()).filter(Boolean);
if(members.some((x,i)=>!/^\w+(?: = 1)?$/.test(x)||(i&&x.includes('='))))throw Error('Review MSEG type IDs');
const typeIds=new Map(members.map((x,i)=>[x.split(' ')[0],i+1]));
const editorSource=readFileSync(new URL('../../src/surge-xt/gui/overlays/MSEGEditor.cpp',import.meta.url),'utf8');
const layout=editorSource.split('static const std::vector<SegmentTypeMenuEntry> &segmentTypeMenuLayout()')[1]?.split('return layout;')[0];
if(!layout || !layout.includes('i < 8'))throw Error('Review MSEG segment menu');
const curveTypes=[...layout.matchAll(/v\.push_back\(\{segType::(\w+), (?:toOSCase\()?"([^"]*)"/g)]
  .filter(([,type])=>type!=='NONE').map(([,type,label])=>({type,label}));
for(let i=1;i<=8;i++)curveTypes.push({type:`RATCHET_${i}`,label:`Ratchet ${i}`});
if(curveTypes.length!==typeIds.size-1 || curveTypes.some(x=>!typeIds.has(x.type)))throw Error('Incomplete MSEG segment menu coverage');
for(const {type,label} of curveTypes)test(`MSEG ${label} context-menu edit serializes and supports undo/redo`,async({page})=>{
  await startMSEG(page);
  // Start Hold from Linear so every test applies a real type change.
  if(type==='HOLD'){
    await page.getByRole('group',{name:'MSEG Display/Editor',exact:true}).focus();
    await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem',{name:'Linear',exact:true}).dispatchEvent('click');
  }
  const before=await snapshot(page,`${type} before`);
  const display=await openEditor(page);await display.focus();
  await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
  const edited=await snapshot(page,`${type} edited`);
  const ids=await page.evaluate(xml=>[...new DOMParser().parseFromString(xml,'text/xml').querySelectorAll('segment')].map(s=>Number(s.getAttribute('type'))),edited);
  expect(ids).toEqual([typeIds.get(type),typeIds.get('HOLD'),typeIds.get('HOLD')]);
  expect(edited).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,`${type} undo`)).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,`${type} redo`)).toBe(edited);
});

test('MSEG exact-value entry rejects zero denominators, cancels, and supports undo/redo',async({page})=>{
  await startMSEG(page);const before=await snapshot(page,'Typein before');
  let display=await openEditor(page);await display.focus();await page.keyboard.press('Enter');
  const input=page.getByRole('textbox',{name:'New Value',exact:true});
  await expect(input).toBeAttached();await input.fill('1/0');await page.keyboard.press('Enter');
  await expect(input).toBeAttached();await page.keyboard.press('Escape');
  expect(await snapshot(page,'Typein cancelled')).toBe(before);
  display=await openEditor(page);await display.focus();await page.keyboard.press('Enter');
  await input.fill('1/4');await page.keyboard.press('Enter');await expect(input).toHaveCount(0);
  const edited=await snapshot(page,'Typein edited');
  expect(await page.evaluate(xml=>Number(new DOMParser().parseFromString(xml,'text/xml').querySelector('segment').getAttribute('v0')),edited)).toBe(0.25);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Typein undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Typein redo')).toBe(edited);
  display=await openEditor(page);await display.focus();await page.keyboard.press('Enter');
  await input.fill('-1/2');await page.keyboard.press('Enter');await expect(input).toHaveCount(0);
  const second=await snapshot(page,'Typein second');
  expect(second).not.toBe(edited);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Typein second undo')).toBe(edited);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Typein second redo')).toBe(second);
});

const controlPointCases=curveTypes.filter(({type})=>type!=='HOLD').flatMap(({type,label})=>{
  const twoD=type==='QUAD_BEZIER'||type==='BROWNIAN'||type.startsWith('RATCHET_');
  return [{label,twoD,axis:twoD?' Y':'',attribute:'cpv',input:'1/4',value:'0.250000'},
    ...(twoD?[{label,twoD,axis:' X',attribute:'cpduration',input:'3/4',value:'0.750000'}]:[])];
});
test('MSEG Hold omits control-point editors and preserves state after the keyboard guidance',async({page})=>{
  await startMSEG(page);const before=await snapshot(page,'Hold CP before');
  const display=await openEditor(page);await display.focus();await page.keyboard.press('Alt+Enter');
  await expect(page.getByRole('alert').filter({hasText:'No control point for this segment type!'})).toBeAttached();
  await expect(page.getByRole('textbox',{name:'New Value',exact:true})).toHaveCount(0);
  await page.keyboard.press('Shift+F10');
  await expect(page.getByRole('menuitem',{name:/^Duration:/})).toBeAttached();
  await expect(page.getByRole('menuitem',{name:/^Control Point/})).toHaveCount(0);
  await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
  expect(await snapshot(page,'Hold CP after')).toBe(before);
});
for(const route of ['menu','keyboard'])
for(const {label,twoD,axis,attribute,input,value} of controlPointCases)
test(`MSEG ${label} control point${axis} ${route} type-in edits only its property with undo/redo`,async({page})=>{
  await startMSEG(page);
  let display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  await display.focus();await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
  const before=await snapshot(page,'CP before');
  display=await openEditor(page);await display.focus();await page.waitForTimeout(250);
  if(route==='menu'){
    await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem',{name:new RegExp('^Control Point'+axis+':')}).dispatchEvent('click');
  }else{
    await page.keyboard.press('Alt+Enter');
    if(twoD)await page.getByRole('menuitem',{name:'Control Point'+axis,exact:true}).dispatchEvent('click');
  }
  await page.getByRole('textbox',{name:'New Value',exact:true}).fill(input);await page.keyboard.press('Enter');
  const edited=await snapshot(page,'CP edited');
  const expected=await page.evaluate(({xml,attribute,value})=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');m.querySelector('segment').setAttribute(attribute,value);
    return new XMLSerializer().serializeToString(m.documentElement);
  },{xml:before,attribute,value});
  expect(edited).toBe(expected);expect(edited).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'CP undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'CP redo')).toBe(edited);
});

test('MSEG keyboard context menus set exact loop endpoint markers with undo/redo',async({page})=>{
  await startMSEG(page);
  const settings=page.getByRole('group',{name:'MSEG Settings',exact:true});
  await settings.getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name:'Envelope',exact:true}).dispatchEvent('click');
  await expect(settings.getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name:'Envelope',exact:true})).toBeChecked();
  await expect(page.getByRole('menu')).toHaveCount(0);
  await settings.getByRole('group',{name:'Loop Mode',exact:true}).getByRole('radio',{name:'Gated Loop',exact:true}).dispatchEvent('click');
  await expect(settings.getByRole('group',{name:'Loop Mode',exact:true}).getByRole('radio',{name:'Gated Loop',exact:true})).toBeChecked();
  await expect(page.getByRole('menu')).toHaveCount(0);
  const before=await snapshot(page,'Loop before');
  let display=await openEditor(page);await display.focus();await page.keyboard.press('Home');await page.keyboard.press('Tab');
  await page.keyboard.press('Shift+F10');await page.getByRole('menuitem',{name:'Set Loop Start',exact:true}).dispatchEvent('click');
  const start=await snapshot(page,'Loop start');expect(start).toContain('loopStartPoint="1"');
  display=await openEditor(page);await display.focus();await page.keyboard.press('End');
  await page.keyboard.press('Shift+F10');await page.getByRole('menuitem',{name:'Set Loop End',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('alert').filter({hasText:'Loop ends at node 4'})).toBeAttached();
  // The native serializer writes loop_end (the preceding segment index),
  // even though the attribute is named loopEndPoint.
  const end=await snapshot(page,'Loop end');expect(end).toContain('loopStartPoint="1"');expect(end).toContain('loopEndPoint="2"');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Loop end undo')).toBe(start);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Loop start undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Loop redo')).toBe(end);
});

async function chooseMode(page,group,name,route){
  // UndoManager::pushUndo coalesces edits to the same MSEG within 200 ms,
  // including edits just after redo. These are independent user gestures;
  // preserve the native coalescing behavior and wait outside that window.
  await page.waitForTimeout(250);
  const control=page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('group',{name:group,exact:true});
  const radio=control.getByRole('radio',{name,exact:true});
  if(route==='radio')await radio.dispatchEvent('click');
  else if(route==='menu'){
    await control.focus();await page.keyboard.press('Shift+F10');
    await page.getByRole('menuitem',{name:name==='Gated Loop'?'Gate (Loop Until Release)':name,exact:true}).dispatchEvent('click');
  }else{
    await page.getByRole('group',{name:'MSEG Display/Editor',exact:true}).focus();
    // Each rotation has its own native undo transaction. The tests below use
    // adjacent loop choices so a single undo must restore the previous mode.
    await page.keyboard.press(group==='Edit Mode'?'Alt+t':'Alt+l');
  }
  await expect(radio).toBeChecked();
}

async function segmentMenuCommand(page,submenu,label){
  await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:submenu,exact:true}).dispatchEvent('click');
  const name=new RegExp('^'+label+'(?: \\(Checked\\))?$');
  await page.getByRole('menuitem',{name}).or(page.getByRole('menuitemcheckbox',{name})).dispatchEvent('click');
}

const stepCounts=editorSource.match(/int stepCounts\[\] = \{([^}]+)\}/)?.[1].split(',').map(Number);
if(!stepCounts || stepCounts.some(n=>!Number.isInteger(n)||n<2))throw Error('Review MSEG creation counts');
const sawLimit=Number(editorSource.match(/if \(i <= (\d+)\)[\s\S]*?Sawtooth Plucks/)?.[1]);
if(!sawLimit)throw Error('Review MSEG saw creation limit');
const creations=[
  {label:'Minimal MSEG',kind:'minimal',count:1},
  {label:'Default Voice MSEG',kind:'voice',count:4},
  {label:'Default Scene MSEG',kind:'scene',count:4},
  ...stepCounts.map(count=>({label:`${count} Step Sequencer`,kind:'step',count})),
  ...stepCounts.map(count=>({label:`${count} Lines Sine`,kind:'sine',count})),
  ...stepCounts.filter(count=>count<=sawLimit).map(count=>({label:`${count} Sawtooth Plucks`,kind:'saw',count})),
];

for(const mode of ['LFO','Envelope'])
for(const label of ['Double Duration','Half Duration','Flip Vertically','Flip Horizontally','Quantize Nodes to Snap Divisions','Quantize Nodes to Whole Units','Distribute Nodes Evenly'])
test(`MSEG ${label} in ${mode} preserves transform semantics and undo/redo`,async({page})=>{
  await startMSEG(page);
  if(mode==='Envelope')await chooseMode(page,'Edit Mode',mode,'radio');
  let display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  await display.focus();await page.keyboard.press('Home');await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:/^Duration:/}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'New Value',exact:true}).fill(mode==='Envelope'?'2':'0.5');
  await page.keyboard.press('Enter');
  const before=await snapshot(page,'Transform before');
  display=await openEditor(page);await display.focus();await page.waitForTimeout(250);
  await page.keyboard.press('Shift+F10');await page.getByRole('menuitem',{name:'Actions',exact:true}).dispatchEvent('click');
  const command=page.getByRole('menuitem',{name:label,exact:true});
  if(mode==='LFO' && !['Flip Vertically','Flip Horizontally','Distribute Nodes Evenly'].includes(label)){
    // JUCE gives unavailable menu entries the ignored accessibility role.
    await expect(page.getByRole('menuitem',{name:'Flip Vertically',exact:true})).toBeAttached();
    await expect(command).toHaveCount(0);await page.keyboard.press('Escape');
    await expect(page.getByRole('menu')).toHaveCount(0);
    await expect(page.getByRole('group',{name:'MSEG Display/Editor',exact:true})).toBeAttached();
    expect(await snapshot(page,'Transform disabled')).toBe(before);return;
  }
  await command.dispatchEvent('click');
  const edited=await snapshot(page,'Transform edited');expect(edited).not.toBe(before);
  const models=await page.evaluate(xmls=>xmls.map(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml').documentElement;
    const numeric=e=>Object.fromEntries([...e.attributes].map(a=>[a.name,Number(a.value)]));
    return {...numeric(m),segments:[...m.querySelectorAll('segment')].map(numeric)};
  }),[before,edited]);
  const [old,changed]=models,total=old.segments.reduce((sum,s)=>sum+s.duration,0);
  expect(changed.activeSegments).toBe(old.activeSegments);
  expect(changed.editMode).toBe(old.editMode);expect(changed.loopMode).toBe(old.loopMode);
  for(const [i,s] of changed.segments.entries()){
    const original=old.segments[label==='Flip Horizontally'?old.segments.length-1-i:i];
    const duration=label==='Double Duration'?original.duration*2:label==='Half Duration'?original.duration/2:label==='Quantize Nodes to Snap Divisions'?old.hSnapDefault:label==='Quantize Nodes to Whole Units'?1:label==='Distribute Nodes Evenly'?(mode==='LFO'?1:total)/old.segments.length:original.duration;
    expect(s.duration).toBeCloseTo(duration,5);
    expect(s.v0).toBeCloseTo(label==='Flip Vertically'?-original.v0:label==='Flip Horizontally'?original.nv1:original.v0,5);
    for(const property of ['type','useDeform','invertDeform','retriggerFEG','retriggerAEG'])expect(s[property]).toBe(original[property]);
  }
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Transform undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Transform redo')).toBe(edited);
});

test('MSEG Escape dismisses an accessible popup and rejects non-menu or retired dismissal targets',async({page})=>{
  await startMSEG(page);
  const display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  const displayId=Number(await display.getAttribute('data-juce-accessible-id'));
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,6,0),displayId)).toBe(0);
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,7,13),displayId)).toBe(0);
  await display.focus();await page.keyboard.press('Shift+F10');
  await expect(page.getByRole('menu')).toHaveCount(1);
  const menuId=Number(await page.getByRole('menu').getAttribute('data-juce-accessible-id'));
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,7,65),menuId)).toBe(0);
  await page.keyboard.press('Escape');await expect(page.getByRole('menu')).toHaveCount(0);
  await expect(display).toBeAttached();
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,6,0),menuId)).toBe(0);
  expect(await page.evaluate(id=>Module._surge_accessibility_action(id,7,13),menuId)).toBe(0);
});

for(const dismissal of ['Escape','Enter'])
test(`MSEG ${dismissal} popup dismissal returns keyboard editing to the canvas`,async({page})=>{
  await startMSEG(page);
  const before=await snapshot(page,'Return focus before');
  const display=await openEditor(page);await display.focus();await page.keyboard.press('Home');
  await page.keyboard.press('Shift+F10');
  if(dismissal==='Enter'){
    // Re-select the current segment type so activating the menu does not
    // change the curve before the subsequent keyboard edit.
    const hold=page.getByRole('menuitemcheckbox',{name:'Hold (Checked)',exact:true});
    await hold.focus();
  }
  await page.keyboard.press(dismissal);await expect(page.getByRole('menu')).toHaveCount(0);
  await page.waitForTimeout(250);
  await page.keyboard.press('ArrowUp');
  await expect(page.getByRole('alert').filter({hasText:'value -0.95'})).toBeAttached();
  const edited=await snapshot(page,'Return focus edited');
  const expected=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');
    m.querySelector('segment').setAttribute('v0','-0.950000');
    return new XMLSerializer().serializeToString(m.documentElement);
  },before);
  expect(edited).toBe(expected);
});

for(const open of ['action','keyboard'])
for(const activate of ['Enter','Space'])
test(`MSEG submenu opened by ${open} navigates with arrows and ${activate} executes the highlighted command`,async({page})=>{
  await startMSEG(page);const before=await snapshot(page,'Menu keys before');
  const display=await openEditor(page);await display.focus();await page.keyboard.press('Shift+F10');
  const actions=page.getByRole('menuitem',{name:'Actions',exact:true});
  if(open==='action')await actions.dispatchEvent('click');
  else{
    await actions.focus();await expect(actions).toHaveAttribute('aria-selected','true');
    await page.keyboard.press('ArrowRight');
  }
  const selected=name=>page.getByRole('menuitem',{name,exact:true});
  await expect(selected('Split')).toHaveAttribute('aria-selected','true');
  await page.keyboard.press('ArrowDown');await expect(selected('Delete')).toHaveAttribute('aria-selected','true');
  await page.keyboard.press('ArrowUp');await expect(selected('Split')).toHaveAttribute('aria-selected','true');
  await page.keyboard.press('ArrowLeft');await expect(selected('Actions')).toHaveAttribute('aria-selected','true');
  await page.keyboard.press('ArrowRight');await expect(selected('Split')).toHaveAttribute('aria-selected','true');
  await page.keyboard.press('ArrowDown');await page.keyboard.press(activate);
  await expect(page.getByRole('menu')).toHaveCount(0);
  const edited=await snapshot(page,'Menu keys edited');expect(edited).toContain('activeSegments="2"');
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Menu keys undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Menu keys redo')).toBe(edited);
});

for(const mode of ['LFO','Envelope'])
test(`MSEG linked edge nodes in ${mode} synchronize endpoints and unlink restores independent editing`,async({page})=>{
  await startMSEG(page);
  if(mode==='Envelope')await chooseMode(page,'Edit Mode',mode,'radio');
  let display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  await display.focus();await page.keyboard.press('End');await page.keyboard.press('Enter');
  await expect(page.getByRole('alert').filter({hasText:'No type-in for the final node, use arrow keys!'})).toBeAttached();
  await page.keyboard.press('ArrowUp');
  const before=await snapshot(page,'Edges before');
  display=await openEditor(page);await display.focus();await page.keyboard.press('Home');await page.waitForTimeout(250);
  await segmentMenuCommand(page,'Settings','Link Edge Nodes');
  let linked=await snapshot(page,'Edges linked');
  const expected=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml'),segments=m.querySelectorAll('segment');
    m.documentElement.setAttribute('endpointMode','1');
    segments[segments.length-1].setAttribute('nv1',segments[0].getAttribute('v0'));
    return new XMLSerializer().serializeToString(m.documentElement);
  },before);
  expect(linked).toBe(expected);expect(linked).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges link undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges link redo')).toBe(linked);
  display=await openEditor(page);await display.focus();await page.keyboard.press('End');await page.waitForTimeout(250);
  await page.keyboard.press('Enter');
  await expect(page.getByRole('alert').filter({hasText:'Editing start node value, linked edge nodes'})).toBeAttached();
  await page.getByRole('textbox',{name:'New Value',exact:true}).fill('1/4');await page.keyboard.press('Enter');
  const linkedEdit=await snapshot(page,'Edges linked edit');
  const changedEndpoints=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml'),segments=m.querySelectorAll('segment');
    segments[0].setAttribute('v0','0.250000');segments[segments.length-1].setAttribute('nv1','0.250000');
    return new XMLSerializer().serializeToString(m.documentElement);
  },linked);
  expect(linkedEdit).toBe(changedEndpoints);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges linked edit undo')).toBe(linked);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges linked edit redo')).toBe(linkedEdit);
  linked=linkedEdit;
  display=await openEditor(page);await display.focus();await page.waitForTimeout(250);
  await segmentMenuCommand(page,'Settings','Link Edge Nodes');
  const unlinked=await snapshot(page,'Edges unlinked');
  expect(unlinked).toBe(linked.replace('endpointMode="1"','endpointMode="2"'));
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges unlink undo')).toBe(linked);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');expect(await snapshot(page,'Edges unlink redo')).toBe(unlinked);
  display=await openEditor(page);await display.focus();await page.keyboard.press('End');await page.keyboard.press('ArrowUp');
  const edited=await snapshot(page,'Edges independent');
  const independent=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml'),segments=m.querySelectorAll('segment');
    segments[segments.length-1].setAttribute('nv1','0.300000');
    return new XMLSerializer().serializeToString(m.documentElement);
  },unlinked);
  expect(edited).toBe(independent);
});

for(const mode of ['LFO','Envelope'])
for(const {label,kind,count} of creations)
test(`MSEG Create ${label} from ${mode} serializes its shape and supports undo/redo`,async({page})=>{
  await startMSEG(page);
  if(mode==='Envelope')await chooseMode(page,'Edit Mode',mode,'radio');
  const before=await snapshot(page,'Create before');
  const display=await openEditor(page);await display.focus();await page.waitForTimeout(250);
  await segmentMenuCommand(page,'Create',label);
  const edited=await snapshot(page,'Create edited');expect(edited).not.toBe(before);
  const model=await page.evaluate(xml=>{
    const m=new DOMParser().parseFromString(xml,'text/xml').documentElement;
    const numeric=e=>Object.fromEntries([...e.attributes].map(a=>[a.name,Number(a.value)]));
    return {...numeric(m),segments:[...m.querySelectorAll('segment')].map(numeric)};
  },edited);
  const env=mode==='Envelope',size=kind==='saw'?count*2-Number(env):count;
  expect(model.activeSegments).toBe(size);expect(model.segments).toHaveLength(size);
  expect(model.editMode).toBe(['minimal','voice'].includes(kind)?0:kind==='scene'?1:Number(!env));
  expect(model.loopMode).toBe(kind==='voice'?3:2);
  expect(model.loopStartPoint).toBe(kind==='voice'?2:0);
  expect(model.loopEndPoint).toBe(kind==='voice'?2:size-1);
  for(const [i,s] of model.segments.entries()){
    expect(s.type).toBe(typeIds.get(kind==='step'?'HOLD':'LINEAR'));
    expect(s.useDeform).toBe(1);expect(s.retriggerAEG).toBe(0);expect(s.retriggerFEG).toBe(0);
    expect(s.invertDeform).toBe(kind==='scene'?i%2:0);
    const duration=kind==='step'?(env?1:1/count):kind==='saw'?(i%2?0:env?1:1/count):kind==='sine'?1/count:kind==='scene'?0.25:1;
    const value=kind==='step'?i/(count-1):kind==='saw'?(i%2?-1:1):kind==='sine'?Math.sin(i*2*Math.PI/count):kind==='scene'?[0,1,0,-1][i]:kind==='voice'?[0,1,0.5,0.5][i]:1;
    expect(s.duration).toBeCloseTo(duration,5);expect(s.v0).toBeCloseTo(value,5);
  }
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Create undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Create redo')).toBe(edited);
});

for(const route of ['menu','keyboard','checkbox'])
for(const {submenu,label,key,attribute,control} of [
  {submenu:'Settings',label:'Use Deform for Segment',key:'d',attribute:'useDeform',control:'Use'},
  {submenu:'Settings',label:'Invert Deform Value',key:'v',attribute:'invertDeform',control:'Invert'},
  {submenu:'Trigger',label:'Filter EG',key:'f',attribute:'retriggerFEG',control:'Filter'},
  {submenu:'Trigger',label:'Amp EG',key:'a',attribute:'retriggerAEG',control:'Amp'},
])test(`MSEG ${route} ${label} toggles only the selected segment and preserves undo/redo`,async({page})=>{
  await startMSEG(page);
  let before=await snapshot(page,'Flag before');
  for(let round=0;round<2;round++){
    const display=await openEditor(page);await display.focus();
    await page.keyboard.press('Home');await page.keyboard.press('Tab');
    await page.waitForTimeout(250); // Separate native 200 ms undo coalescing groups.
    if(route==='menu')await segmentMenuCommand(page,submenu,label);
    else if(route==='checkbox')await page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('checkbox',{name:control,exact:true}).dispatchEvent('click');
    else await page.keyboard.press('Alt+'+key);
    const edited=await snapshot(page,`Flag ${round}`);
    const expected=await page.evaluate(({xml,attribute})=>{
      const m=new DOMParser().parseFromString(xml,'text/xml'),segment=m.querySelectorAll('segment')[1];
      segment.setAttribute(attribute,segment.getAttribute(attribute)==='1'?'0':'1');
      return new XMLSerializer().serializeToString(m.documentElement);
    },{xml:before,attribute});
    expect(edited).toBe(expected);expect(edited).not.toBe(before);
    await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Flag ${round} undo`)).toBe(before);
    await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Flag ${round} redo`)).toBe(edited);
    before=edited;
  }
});

test('MSEG trigger All and Nothing affect only the selected segment with undo/redo',async({page})=>{
  await startMSEG(page);let before=await snapshot(page,'Triggers before');
  for(const [label,value] of [['All','1'],['Nothing','0']]){
    const display=await openEditor(page);await display.focus();
    await page.keyboard.press('Home');await page.keyboard.press('Tab');await page.waitForTimeout(250);
    await segmentMenuCommand(page,'Trigger',label);
    const edited=await snapshot(page,`Triggers ${label}`);
    const expected=await page.evaluate(({xml,value})=>{
      const m=new DOMParser().parseFromString(xml,'text/xml'),segment=m.querySelectorAll('segment')[1];
      for(const attribute of ['retriggerFEG','retriggerAEG'])segment.setAttribute(attribute,value);
      return new XMLSerializer().serializeToString(m.documentElement);
    },{xml:before,value});
    expect(edited).toBe(expected);expect(edited).not.toBe(before);
    await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Triggers ${label} undo`)).toBe(before);
    await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Triggers ${label} redo`)).toBe(edited);
    before=edited;
  }
});

for(const initial of ['uniform','mixed','mixed-longest'])
for(const selection of initial==='mixed-longest'?['all']:['range','all'])
for(const route of ['keyboard','checkbox'])
for(const {key,attribute,control} of [
  {key:'d',attribute:'useDeform',control:'Use'},
  {key:'v',attribute:'invertDeform',control:'Invert'},
  {key:'f',attribute:'retriggerFEG',control:'Filter'},
  {key:'a',attribute:'retriggerAEG',control:'Amp'},
])test(`MSEG ${initial} ${selection} selection ${route} ${control} updates the group in one undo operation`,async({page})=>{
  await startMSEG(page);
  if(initial!=='uniform'){
    if(initial==='mixed-longest'){
      await page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name:'Envelope',exact:true}).dispatchEvent('click');
    }
    const display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
    await display.focus();await page.keyboard.press('Home');await page.keyboard.press('Tab');
    if(initial==='mixed-longest'){
      await page.keyboard.press('Shift+F10');await page.getByRole('menuitem',{name:/^Duration:/}).dispatchEvent('click');
      await page.getByRole('textbox',{name:'New Value',exact:true}).fill('2');await page.keyboard.press('Enter');
      await display.focus();
    }
    await page.keyboard.press('Alt+'+key);
  }
  const before=await snapshot(page,'Group before');
  if(initial!=='uniform'){
    const flags=await page.evaluate(({xml,attribute})=>[...new DOMParser().parseFromString(xml,'text/xml').querySelectorAll('segment')].map(s=>s.getAttribute(attribute)),{xml:before,attribute});
    expect(flags[0]).toBe(flags[2]);expect(flags[1]).not.toBe(flags[0]);
  }
  const display=await openEditor(page);await display.focus();await page.keyboard.press('Home');
  if(selection==='all'){
    await page.keyboard.press('Control+a');
    await expect(page.getByRole('alert').filter({hasText:'All nodes selected, 3 nodes'})).toBeAttached();
  }else{
    await page.keyboard.press('Tab');await page.keyboard.press('Alt+s');await page.keyboard.press('ArrowRight');
    await expect(page.getByRole('alert').filter({hasText:'Node 3 selected, 2 nodes selected'})).toBeAttached();
    await page.keyboard.press('Enter');
    await expect(page.getByRole('alert').filter({hasText:'Selection mode off, 2 nodes selected'})).toBeAttached();
  }
  await page.waitForTimeout(250);
  if(route==='keyboard')await page.keyboard.press('Alt+'+key);
  else await page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('checkbox',{name:control,exact:true}).dispatchEvent('click');
  const edited=await snapshot(page,'Group edited');
  const expected=await page.evaluate(({xml,attribute,selection,initial})=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');
    const segments=[...m.querySelectorAll('segment')],selected=selection==='all'?segments:segments.slice(1);
    // These equal-duration fixtures pick the first selected segment. The
    // unequal-duration fixture deliberately makes the middle segment longest.
    const representative=initial==='mixed-longest'?segments[1]:selected[0];
    if(initial==='mixed-longest' && !segments.every((s,i)=>i===1 || Number(s.getAttribute('duration'))<Number(representative.getAttribute('duration'))))throw Error('Expected middle segment to be longest');
    const value=representative.getAttribute(attribute)==='1'?'0':'1';
    selected.forEach(segment=>segment.setAttribute(attribute,value));
    return new XMLSerializer().serializeToString(m.documentElement);
  },{xml:before,attribute,selection,initial});
  expect(edited).toBe(expected);expect(edited).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Group undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Group redo')).toBe(edited);
});

for(const route of ['radio','menu','keyboard'])
test(`MSEG ${route} edit modes normalize LFO duration and restore envelope timing with undo/redo`,async({page})=>{
  await startMSEG(page);
  const mode=name=>page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name,exact:true});
  await mode('Envelope').dispatchEvent('click');
  let display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  await display.focus();await page.keyboard.press('Home');await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:/^Duration:/}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'New Value',exact:true}).fill('2');
  await page.keyboard.press('Enter');
  const envelope=await snapshot(page,'Mode envelope');
  const durations=xml=>page.evaluate(xml=>[...new DOMParser().parseFromString(xml,'text/xml').querySelectorAll('segment')].map(s=>Number(s.getAttribute('duration'))),xml);
  const original=await durations(envelope);expect(original[0]).toBe(2);
  await openEditor(page);await chooseMode(page,'Edit Mode','LFO',route);
  const lfo=await snapshot(page,'Mode LFO');
  const normalized=await durations(lfo),total=original.reduce((a,b)=>a+b,0);
  expect(normalized.reduce((a,b)=>a+b,0)).toBeCloseTo(1,5);
  normalized.forEach((value,i)=>expect(value).toBeCloseTo(original[i]/total,5));
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mode LFO undo')).toBe(envelope);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mode LFO redo')).toBe(lfo);
  await openEditor(page);await chooseMode(page,'Edit Mode','Envelope',route);
  const restored=await snapshot(page,'Mode restored');
  (await durations(restored)).forEach((value,i)=>expect(value).toBeCloseTo(original[i],5));
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mode restore undo')).toBe(lfo);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mode restore redo')).toBe(restored);
});

for(const route of ['radio','menu','keyboard'])
test(`MSEG ${route} loop modes serialize all three choices and restore their exact undo states`,async({page})=>{
  await startMSEG(page);
  await page.getByRole('group',{name:'MSEG Settings',exact:true}).getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name:'Envelope',exact:true}).dispatchEvent('click');
  let before=await snapshot(page,'Mode loop before');
  for(const [name,value] of [['Gated Loop',3],['Off',1],['Loop',2]]){
    await openEditor(page);
    await chooseMode(page,'Loop Mode',name,route);
    await expect(page.getByRole('menu')).toHaveCount(0);
    const edited=await snapshot(page,`Mode loop ${name}`);
    const expected=await page.evaluate(({xml,value})=>{
      const m=new DOMParser().parseFromString(xml,'text/xml');m.documentElement.setAttribute('loopMode',String(value));
      return new XMLSerializer().serializeToString(m.documentElement);
    },{xml:before,value});
    expect(edited).toBe(expected);expect(edited).not.toBe(before);
    await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Mode loop ${name} undo`)).toBe(before);
    await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
    expect(await snapshot(page,`Mode loop ${name} redo`)).toBe(edited);
    before=edited;
  }
});

for(const loopMode of ['Loop','Gated Loop'])
for(const {label,segment,fraction,attribute,value} of [
  {label:'Set Loop Start',segment:1,fraction:0.5,attribute:'loopStartPoint',value:1},
  {label:'Set Loop End',segment:1,fraction:0.5,attribute:'loopEndPoint',value:1},
  {label:'Set Loop End',segment:1,fraction:0.05,attribute:'loopEndPoint',value:0},
])test(`MSEG mouse ${label} at segment ${segment+1} fraction ${fraction} in ${loopMode} preserves undo/redo`,async({page})=>{
  await startMSEG(page);
  const settings=page.getByRole('group',{name:'MSEG Settings',exact:true});
  await settings.getByRole('group',{name:'Edit Mode',exact:true}).getByRole('radio',{name:'Envelope',exact:true}).dispatchEvent('click');
  await settings.getByRole('group',{name:'Loop Mode',exact:true}).getByRole('radio',{name:loopMode,exact:true}).dispatchEvent('click');
  let display=page.getByRole('group',{name:'MSEG Display/Editor',exact:true});
  // Establish an explicit final loop boundary so both mouse branches can edit
  // the middle segment independently of the preset's implicit (-1) markers.
  await display.focus();await page.keyboard.press('End');await page.keyboard.press('Shift+F10');
  await page.getByRole('menuitem',{name:'Set Loop End',exact:true}).dispatchEvent('click');
  const before=await snapshot(page,'Mouse loop before');
  const time=await page.evaluate(({xml,segment,fraction})=>{
    const m=new DOMParser().parseFromString(xml,'text/xml').documentElement;
    const durations=[...m.querySelectorAll('segment')].map(s=>Number(s.getAttribute('duration')));
    const t=durations.slice(0,segment).reduce((sum,d)=>sum+d,0)+durations[segment]*fraction;
    return (t-Number(m.getAttribute('axisStart')))/Number(m.getAttribute('axisWidth'));
  },{xml:before,segment,fraction});
  display=await openEditor(page);
  const bounds=await display.boundingBox();expect(bounds).not.toBeNull();
  // MSEGCanvas::getDrawArea: inset X=10, left axis=18, right inset=10.
  // The Y midpoint avoids nodes and loop-marker handles. Use native pointer
  // events on the canvas rather than an accessibility context-menu action.
  await page.mouse.click(bounds.x+28+(bounds.width-38)*time,bounds.y+bounds.height*0.4,{button:'right'});
  await page.getByRole('menuitem',{name:label,exact:true}).dispatchEvent('click');
  const edited=await snapshot(page,'Mouse loop edited');
  const expected=await page.evaluate(({xml,attribute,value})=>{
    const m=new DOMParser().parseFromString(xml,'text/xml');
    m.documentElement.setAttribute(attribute,String(value));
    return new XMLSerializer().serializeToString(m.documentElement);
  },{xml:before,attribute,value});
  expect(edited).toBe(expected);
  expect(edited).not.toBe(before);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mouse loop undo')).toBe(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');
  expect(await snapshot(page,'Mouse loop redo')).toBe(edited);
});
