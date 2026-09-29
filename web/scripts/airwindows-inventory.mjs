// Source-derived streaming IDs. Display ordering must never be used as a patch ID.
import {readFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../../',import.meta.url));
export function airwindowsInventory(){
  const source=readFileSync(root+'libs/airwindows/src/AirWinBaseClass_pluginRegistry.cpp','utf8');
  const section=source.split('#if !SURGE_INCLUDE_AIRWINDOWS')[1]?.split('#else')[1]?.split('#endif')[0];
  if(!section)throw Error('Review the Airwindows registration branches');
  const groups=new Map([...section.matchAll(/std::string\s+(\w+)\s*=\s*"([^"]+)"/g)].map(([,key,value])=>[key,value]));
  const entries=[...section.matchAll(/reg\.emplace_back\(create<([^>]+)>, id\+\+,\s*(-?\d+),\s*(\w+),\s*"([^"]+)"\)/g)]
    .map(([,implementation,displayOrder,group,name],id)=>({id,name,implementation,displayOrder:Number(displayOrder),group:groups.get(group),noOp:implementation==='AirWindowsNoOp'}));
  if(entries.length<2 || entries.length!==(section.match(/reg\.emplace_back\(/g)||[]).length || entries.some(entry=>!entry.group))
    throw Error('Review the changed Airwindows registration format');
  return entries;
}
export function airwindowsFixture(entry,inventory=airwindowsInventory()){
  // The adapter reserves parameter zero for the sub-effect selector and maps
  // the original effect's parameters to slots i + 1.
  if(entry.id===0){
    if(entry.implementation!=='ADClip7::ADClip7')throw Error('Review the default Airwindows fixture');
    const header=readFileSync(root+'libs/airwindows/src/ADClip7.h','utf8');
    const boost=header.match(/kParamA\s*=\s*(\d+)/);
    const adapter=readFileSync(root+'src/common/dsp/effects/airwindows/AirWindowsEffect.cpp','utf8');
    if(!boost || !adapter.includes('fxdata->p[i + 1].set_name(txt)'))throw Error('Review Airwindows parameter mapping');
    return {index:Number(boost[1])+1,value:1};
  }
  return {index:0,value:entry.id/(inventory.length-1)};
}

// Edited fixtures exercise effects whose defaults are effectively bypassed by
// the FM2 note. Derive adapter parameter slots from the original declarations.
export function airwindowsActiveParameters(entry){
  const choices={
    'Surge':[['kParamA',0.8]], 'Crunchy Groove Wear':[['kParamA',0.8]],
    'Deck Wrecka':[['kParamA',0.8]], 'Groove Wear':[['kParamA',0.8]],
    'De-Bess':[['kParamA',1],['kParamB',0]], 'Single-Ended Triode':[['kParamA',0.8]],
    'Slew 1':[['kSlewParam',0.8]], 'Slew 2':[['kParamA',0.8]],
    'YHighpass':[['kParamB',0.2]],
  };
  const edits=choices[entry.name];
  if(!edits)return [];
  const implementation=entry.implementation.split('::').at(-1);
  const header=readFileSync(root+'libs/airwindows/src/'+implementation+'.h','utf8');
  return edits.map(([member,value])=>{
    const index=header.match(new RegExp(member+'\\s*=\\s*(\\d+)'));
    if(!index)throw Error('Review active Airwindows fixture parameter: '+entry.name);
    return {index:Number(index[1])+1,value};
  });
}

export function summarizeAirwindows(report){
  const summary={activeFixtures:[],retiredSilence:[],pending:[]};
  for(const entry of report.inventory.filter(entry=>report.selectedFamilies.includes(entry.name))){
    const cases=report.cases.filter(item=>item.streamingId===entry.id);
    if(cases.length!==2 || ![44100,48000].every(rate=>cases.some(item=>item.rate===rate)))
      throw Error('Incomplete Airwindows survey: '+entry.name);
    const active=cases.every(item=>['matched-default-fixture','matched-parameter-fixture'].includes(item.status) &&
      item.comparison.energy>1 && item.comparison.relativeRMS<1e-5 && item.dry.relativeRMS>=1e-3 &&
      item.nativeRepeat.relativeRMS<1e-5 && item.wasmRepeat.relativeRMS<1e-5);
    const silent=entry.noOp && cases.every(item=>item.status==='matched-retired-silence' &&
      item.comparison.energy===0 && item.comparison.error===0 && item.nativeRepeat.error===0 && item.wasmRepeat.error===0);
    if(active)summary.activeFixtures.push(entry.name);
    else if(silent)summary.retiredSilence.push(entry.name);
    else{
      const reason=cases.some(item=>item.status==='error')?'render-error':
        cases.some(item=>item.status==='needs-fixture')?'needs-active-fixture':
        cases.some(item=>item.nativeRepeat?.relativeRMS>=1e-5 || item.wasmRepeat?.relativeRMS>=1e-5)?'needs-statistical-comparison':'native-wasm-mismatch';
      summary.pending.push({id:entry.id,name:entry.name,reason});
    }
  }
  return summary;
}
