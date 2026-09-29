import {test,expect} from './fixtures.js';
async function open(page){
  page.setDefaultTimeout(10000);await page.goto('/surge-xt-browser.html');
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await page.getByRole('button',{name:'Oscillator Type',exact:true}).dispatchEvent('click');
  await page.getByRole('menuitem',{name:'Alias',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Scene A Osc 1 Wrap',exact:true})).toBeAttached();
  await page.getByRole('slider',{name:'Scene A Osc 1 Shape',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name:'Additive',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await expect(page.getByRole('slider',{name:'Harmonic 1',exact:true})).toBeAttached();
}
const harmonics=page=>page.getByRole('slider',{name:/^Harmonic \d+$/}).evaluateAll(nodes=>nodes.map(n=>Number(n.getAttribute('aria-valuenow'))));

test('Alias edited harmonics survive persisted patch reload and FXP import',async({page})=>{
  await open(page);
  await command(page,'Triangle');
  await page.getByRole('slider',{name:'Harmonic 2',exact:true}).press('End');
  await page.getByRole('slider',{name:'Harmonic 4',exact:true}).press('Shift+ArrowUp');
  const edited=await harmonics(page);
  expect(edited).toHaveLength(16);
  expect(edited[1]).toBe(-1);expect(edited[3]).toBeCloseTo(0.01,6);
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.getByRole('button',{name:'Save Patch',exact:true}).dispatchEvent('click');
  await page.getByRole('textbox',{name:'patch name',exact:true}).fill('Alias browser edit');
  await page.getByRole('textbox',{name:'patch category',exact:true}).fill('Browser Tests');
  await page.getByRole('button',{name:'OK',exact:true}).dispatchEvent('click');
  const saved='/user/Patches/Browser Tests/Alias browser edit.fxp';
  await expect.poll(()=>page.evaluate(path=>Module.FS.analyzePath(path).exists,saved)).toBe(true);
  const bytes=await page.evaluate(path=>Array.from(Module.FS.readFile(path)),saved);
  await page.evaluate(()=>SurgeBrowser.flush());await page.reload();
  await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
  expect(await page.evaluate(path=>Array.from(Module.FS.readFile(path)),saved)).toEqual(bytes);
  expect(await page.evaluate(path=>Module.ccall('surge_browser_request_patch','number',['string'],[path]),saved)).toBe(1);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Alias browser edit');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  const verify=async()=>{
    await expect.poll(()=>harmonics(page).then(a=>a.length)).toBe(16);
    const restored=await harmonics(page);
    // Native FXP XML float formatting may round the stored coefficients.
    for(let i=0;i<edited.length;i++)expect(restored[i]).toBeCloseTo(edited[i],6);
  };
  await verify();
  await command(page,'Sine');await expect.poll(()=>harmonics(page)).not.toEqual(edited);
  await page.getByRole('button',{name:'Close Custom Editor',exact:true}).dispatchEvent('click');
  await page.evaluate(bytes=>{
    const transfer=new DataTransfer();
    transfer.items.add(new File([new Uint8Array(bytes)],'Imported Alias.fxp'));
    document.querySelector('canvas').dispatchEvent(new DragEvent('drop',{
      dataTransfer:transfer,clientX:100,clientY:100,bubbles:true,cancelable:true}));
  },bytes);
  await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Imported Alias');
  await page.getByRole('button',{name:'Open Custom Editor',exact:true}).dispatchEvent('click');
  await verify();
});
async function command(page,name){
  await page.getByRole('slider',{name:'Harmonic 1',exact:true}).press('Shift+F10');
  await page.getByRole('menuitem',{name,exact:true}).dispatchEvent('click');
  await expect(page.getByRole('menuitem')).toHaveCount(0);
}
for(const shape of ['Sine','Triangle','Sawtooth','Square'])test(`Alias ${shape} preset preserves harmonic values through undo and redo`,async({page})=>{
  await open(page);
  await page.getByRole('slider',{name:'Harmonic 2',exact:true}).press('End');
  const before=await harmonics(page);expect(before.length).toBeGreaterThan(8);
  // Separate the setup edit from the preset under the native 200 ms undo coalescing rule.
  await page.waitForTimeout(225);
  await command(page,shape);
  const expected=before.map((_,i)=>shape==='Sine'?(i===0?1:0):shape==='Sawtooth'?1/(i+1):shape==='Square'?(i%2===0?1/(i+1):0):(i%2===0?(i%4===2?-1:1)/((i+1)**2):0));
  await expect.poll(()=>harmonics(page)).not.toEqual(before);
  const after=await harmonics(page);for(let i=0;i<after.length;i++)expect(after[i]).toBeCloseTo(expected[i],6);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(after);
});

test('Alias harmonic keyboard editing uses native coarse fine bounds and reset commands',async({page})=>{
  await open(page);const slider=page.getByRole('slider',{name:'Harmonic 4',exact:true});
  const value=()=>slider.getAttribute('aria-valuenow').then(Number);
  await slider.press('Delete');await expect.poll(value).toBe(0);
  await slider.press('ArrowUp');await expect.poll(value).toBeCloseTo(0.05,6);
  await slider.press('Shift+ArrowDown');await expect.poll(value).toBeCloseTo(0.04,6);
  await slider.press('Home');await expect.poll(value).toBe(1);
  await slider.press('ArrowUp');await expect.poll(value).toBe(1);
  await slider.press('End');await expect.poll(value).toBe(-1);
  await slider.press('ArrowDown');await expect.poll(value).toBe(-1);
  // Reset should form a separate native undo gesture.
  await page.waitForTimeout(225);
  await slider.press('Delete');await expect.poll(value).toBe(0);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(value).toBe(-1);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(value).toBe(0);
});

for(const input of ['accessible','keyboard'])test(`Alias editor ${input} toggle labels its next action and preserves harmonics`,async({page})=>{
  await open(page);const slider=page.getByRole('slider',{name:'Harmonic 3',exact:true});
  await slider.press('End');const before=await harmonics(page);
  const activate=async name=>{const button=page.getByRole('button',{name,exact:true});if(input==='keyboard')await button.press('Enter');else await button.dispatchEvent('click');};
  for(let cycle=0;cycle<2;cycle++){
    await activate('Close Custom Editor');
    await expect(page.getByRole('slider',{name:'Harmonic 1',exact:true})).toHaveCount(0);
    await expect(page.getByRole('button',{name:'Open Custom Editor',exact:true})).toBeAttached();
    await activate('Open Custom Editor');
    await expect(page.getByRole('button',{name:'Close Custom Editor',exact:true})).toBeAttached();
    await expect.poll(()=>harmonics(page)).toEqual(before);
  }
});

const transformations={
  'Shift Left':a=>[...a.slice(1),a[0]],
  'Shift Right':a=>[a.at(-1),...a.slice(0,-1)],
  'Keep Positive':a=>a.map(x=>Math.max(0,x)),
  'Keep Negative':a=>a.map(x=>Math.min(0,x)),
  'Keep Even Harmonics':a=>a.map((x,i)=>i>0&&i%2===0?0:x),
  'Keep Odd Harmonics':a=>a.map((x,i)=>i%2===1?0:x),
  Absolute:a=>a.map(Math.abs),
  Invert:a=>a.map(x=>-x),
  // Preserve the native menu's index convention and unchanged fundamental.
  'Invert Even Harmonics':a=>a.map((x,i)=>i>0&&i%2===0?-x:x),
  'Invert Odd Harmonics':a=>a.map((x,i)=>i%2===1?-x:x),
  Reverse:a=>a.toReversed(),
  Soften:a=>{let previous=0;return a.map(x=>previous=Math.fround(previous+Math.fround(Math.fround(x-previous)/2)));},
  'Window (Linear)':a=>a.map((x,i)=>x*(a.length-i)/a.length),
  'Window (Cosine)':a=>a.map((x,i)=>x*Math.cos(i/a.length))
};
for(const [label,transform] of Object.entries(transformations))test(`Alias ${label} transforms harmonics and restores undo history`,async({page})=>{
  await open(page);
  for(const index of [3,5])await page.getByRole('slider',{name:`Harmonic ${index}`,exact:true}).press('End');
  await expect.poll(()=>harmonics(page).then(a=>[a[2],a[4]])).toEqual([-1,-1]);
  const before=await harmonics(page);await page.waitForTimeout(225);
  await command(page,label);const expected=transform(before);
  await expect.poll(()=>harmonics(page)).not.toEqual(before);
  const after=await harmonics(page);expect(after.length).toBe(expected.length);
  for(let i=0;i<after.length;i++)expect(after[i]).toBeCloseTo(expected[i],6);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(after);
});
test('Alias Random keeps bounded harmonics and restores the realized values on undo and redo',async({page})=>{
  await open(page);const before=await harmonics(page);
  await command(page,'Random');await expect.poll(()=>harmonics(page)).not.toEqual(before);
  const first=await harmonics(page);expect(first.every(v=>Number.isFinite(v)&&v>=-1&&v<=1)).toBe(true);
  await page.waitForTimeout(225);await command(page,'Random');await expect.poll(()=>harmonics(page)).not.toEqual(first);
  const second=await harmonics(page);expect(second.every(v=>Number.isFinite(v)&&v>=-1&&v<=1)).toBe(true);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(first);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(second);
});

async function harmonicPoint(page,index,fraction=0.5){
  const box=await page.getByRole('slider',{name:`Harmonic ${index}`,exact:true}).boundingBox();expect(box).not.toBeNull();
  return {x:box.x+box.width/2,y:box.y+box.height*fraction,tolerance:2/box.height};
}
test('Alias pointer drag edits one harmonic and undoes the complete gesture',async({page})=>{
  await open(page);const before=await harmonics(page),start=await harmonicPoint(page,3,0.25),end=await harmonicPoint(page,3,0.75);
  await page.mouse.move(start.x,start.y);await page.mouse.down();await page.mouse.move(end.x,end.y,{steps:8});await page.mouse.up();
  await expect.poll(()=>harmonics(page).then(a=>Math.abs(a[2]+0.5))).toBeLessThan(end.tolerance);
  const after=await harmonics(page);for(let i=0;i<after.length;i++)if(i!==2)expect(after[i]).toBe(before[i]);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(after);
});
for(const action of ['invert','reset','double click'])test(`Alias pointer ${action} preserves native harmonic behavior`,async({page})=>{
  await open(page);const before=await harmonics(page),p=await harmonicPoint(page,3,0.25);
  if(action==='double click')await page.mouse.dblclick(p.x,p.y);
  else{
    const modifier=action==='invert'?'Alt':'Meta';await page.keyboard.down(modifier);
    await page.mouse.click(p.x,p.y);await page.keyboard.up(modifier);
  }
  const expected=action==='invert'?-before[2]:0;
  await expect.poll(()=>harmonics(page).then(a=>a[2])).toBe(expected);
  const after=await harmonics(page);for(let i=0;i<after.length;i++)if(i!==2)expect(after[i]).toBe(before[i]);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(before);
});

test('Alias horizontal drag edits each visited harmonic as one undo gesture',async({page})=>{
  await open(page);const before=await harmonics(page),start=await harmonicPoint(page,2,0.75),end=await harmonicPoint(page,6,0.75);
  await page.mouse.move(start.x,start.y);await page.mouse.down();await page.mouse.move(end.x,end.y,{steps:16});await page.mouse.up();
  await expect.poll(()=>harmonics(page).then(a=>Math.max(...a.slice(1,6).map(v=>Math.abs(v+0.5))))).toBeLessThan(end.tolerance);
  const after=await harmonics(page);for(let i=0;i<after.length;i++)if(i<1||i>5)expect(after[i]).toBe(before[i]);
  await page.getByRole('button',{name:'Undo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(before);
  await page.getByRole('button',{name:'Redo',exact:true}).dispatchEvent('click');await expect.poll(()=>harmonics(page)).toEqual(after);
});
test('Alias wheel fine adjustment is one tenth of coarse adjustment and leaves other harmonics unchanged',async({page})=>{
  await open(page);const slider=page.getByRole('slider',{name:'Harmonic 4',exact:true});
  await slider.press('Delete');await expect.poll(()=>harmonics(page).then(a=>a[3])).toBe(0);
  const baseline=await harmonics(page),p=await harmonicPoint(page,4);
  await page.mouse.move(p.x,p.y);await page.mouse.wheel(0,10);
  await expect.poll(()=>harmonics(page).then(a=>Math.abs(a[3]))).toBeGreaterThan(0);
  const coarse=(await harmonics(page))[3];
  await slider.press('Delete');await expect.poll(()=>harmonics(page).then(a=>a[3])).toBe(0);
  await page.keyboard.down('Shift');await page.mouse.wheel(0,10);await page.keyboard.up('Shift');
  await expect.poll(()=>harmonics(page).then(a=>Math.abs(a[3]))).toBeGreaterThan(0);
  const after=await harmonics(page);expect(after[3]).toBeCloseTo(coarse/10,6);
  for(let i=0;i<after.length;i++)if(i!==3)expect(after[i]).toBe(baseline[i]);
  await page.mouse.wheel(0,10);
  await expect.poll(()=>harmonics(page).then(a=>a[3])).toBeCloseTo(after[3]+coarse,6);
});
