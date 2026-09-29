import {test,expect} from './fixtures.js';
import {readFileSync} from 'node:fs';
const configuration=readFileSync(new URL('../../resources/surge-shared/configuration.xml',import.meta.url),'utf8').match(/<fx>([\s\S]*?)<\/fx>/)[1];
const types=[...configuration.matchAll(/<type i="(\d+)" name="([^"]+)"/g)].map(([,id,name])=>({id:Number(id),name}));
const maximum=Math.max(...types.map(t=>t.id));
for(const rate of [44100,48000])test(`direct effect-type edits retain committed metadata until adoption at ${rate} Hz`,async({page})=>{
  await page.goto('/surge-xt-browser.html');
  const result=await page.evaluate(async({rate,types,maximum})=>{
    const {default:create}=await import('/surge-web.js');const m=await create();
    m.FS.mkdirTree('/factory');m.FS.mkdirTree('/user');
    const createEngine=()=>m.ccall('surge_create','number',['number','string'],[rate,'/factory']);
    let actual=createEngine();const out=m._malloc(256);
    if(!actual)throw Error(m.ccall('surge_error','string',[],[]));
    const info=(engine,id)=>JSON.parse(m.ccall('surge_parameter_info','string',['number','number'],[engine,id]));
    try{
      const ids=[];
      for(let id=0;id<m._surge_parameter_count(actual);id++)if(info(actual,id).name.endsWith('FX Type'))ids.push(id);
      if(ids.length<2)throw Error('Effect type metadata was not found');
      const first=ids[0],end=ids[1],checked=[],expected=[];
      const metadata=()=>Array.from({length:end-first},(_,i)=>info(actual,first+i));
      // One engine per module: capture immediate adoption, then replay direct
      // queued edits. A second concurrent engine can exhaust the pthread pool.
      for(const type of types){
        if(!m._surge_set_effect_type(actual,0,type.id))throw Error('Reference edit rejected');
        expected.push(metadata());
      }
      m._surge_destroy(actual);actual=0;actual=createEngine();
      if(!actual)throw Error('Unable to recreate engine');
      for(const type of types){
        const before=info(actual,first);
        if(!m._surge_set_parameter(actual,first,type.id/maximum))throw Error('Direct edit rejected');
        if(JSON.stringify(info(actual,first))!==JSON.stringify(before))throw Error('Unadopted type changed live metadata');
        if(!m._surge_render(actual,0,0,out,out+128,32))throw Error('Adoption render failed');
        for(let id=first;id<end;id++){
          if(JSON.stringify(info(actual,id))!==JSON.stringify(expected[checked.length][id-first]))
            throw Error(`Effect ${type.name} parameter ${id} differs after adoption: ${JSON.stringify(info(actual,id))} vs ${JSON.stringify(expected[checked.length][id-first])}`);
        }
        checked.push(type.name);
      }
      return checked;
    }finally{m._free(out);m._surge_destroy(actual);}
  },{rate,types,maximum});
  expect(result).toEqual(types.map(t=>t.name));
});
