// SPDX-License-Identifier: GPL-3.0-or-later
// Development fixtures use the original FXP/XML format and both real engines.
import {readFileSync} from 'node:fs';
const root=new URL('../../',import.meta.url);
const storage=readFileSync(new URL('src/common/SurgeStorage.h',root),'utf8');
function sequentialEnum(source,name,start){
  const body=source.match(new RegExp(`enum ${name}\\s*\\{([\\s\\S]*?)\\}`))?.[1];
  if(!body)throw Error('Missing enum '+name);
  const members=body.replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g,'').split(',').map(x=>x.trim()).filter(Boolean);
  if(members.some((s,i)=>!(i? /^\w+$/:new RegExp(`^\\w+\\s*=\\s*${start}$`)).test(s)))throw Error('Review enum layout '+name);
  return members.map((s,i)=>({name:s.split(/\s*=/)[0],id:i+start}));
}
export const sourceIds=sequentialEnum(readFileSync(new URL('src/common/ModulationSource.h',root),'utf8'),'modsources',0);
const source=sourceIds.find(x=>x.name==='ms_lfo1').id;
const shape=sequentialEnum(storage,'lfo_type',0).find(x=>x.name==='lt_mseg').id;
export const msegFixtureInfo={source,shape};
export const msegTypes=sequentialEnum(storage.slice(storage.indexOf('struct MSEGStorage')),'Type',1).filter(x=>x.name!=='NONE');
const template=readFileSync(new URL('resources/data/patches_factory/Templates/Init FM2.fxp',root));
if(template.subarray(0,4).toString()!=='CcnK'||template.subarray(60,64).toString()!=='sub3')throw Error('Review FXP fixture header');
const size=template.readUInt32LE(64),original=template.subarray(92,92+size).toString().replace(/\0+$/,'');
export function msegFixture(type,{modulated=true,introHold=false}={}){
  if(!msegTypes.some(x=>x.id===type))throw Error('Unknown MSEG fixture type');
  let xml=original;
  const parameter=(name,value)=>{
    const pattern=new RegExp(`(<${name}\\s+[^>]*?value=")[^"]*("[^>]*?/>)`);
    if(!pattern.test(xml))throw Error('Missing fixture parameter '+name);
    xml=xml.replace(pattern,(_,a,b)=>a+value+b);
  };
  // Streamed parameter names are zero-indexed; ms_lfo1 routes voice LFO 0.
  parameter('a_lfo0_shape',shape);parameter('a_lfo0_rate',1);parameter('a_lfo0_trigmode',1);
  parameter('a_lfo0_magnitude',1);parameter('a_lfo0_deform',0.2);parameter('a_lfo0_unipolar',0);
  if(modulated){
    const pitch=/<a_osc1_pitch\s+([^>]*?)\/>/;
    if(!pitch.test(xml))throw Error('Review fixture pitch routing');
    xml=xml.replace(pitch,(_,attrs)=>`<a_osc1_pitch ${attrs}><modrouting source="${source}" depth="6" muted="0" source_index="0"/></a_osc1_pitch>`);
  }
  const values=[-0.75,0.5,-0.25],durations=[0.25,0.375,0.375];
  let segments=values.map((v,i)=>`<segment duration="${durations[i]}" v0="${v}" nv1="${values[(i+1)%3]}" cpduration="0.35" cpv="0.25" type="${type}" useDeform="1" invertDeform="0" retriggerFEG="0" retriggerAEG="0"/>`).join('');
  if(introHold)segments=`<segment duration="0.125" v0="-0.75" nv1="-0.75" cpduration="0.5" cpv="0" type="${msegTypes.find(t=>t.name==='HOLD').id}" useDeform="0" invertDeform="0" retriggerFEG="0" retriggerAEG="0"/>`+segments;
  if(!/<msegs\s*\/>/.test(xml))throw Error('Review template MSEG storage');
  xml=xml.replace(/<msegs\s*\/>/,`<msegs><mseg scene="0" i="0" activeSegments="${introHold?4:3}" endpointMode="1" editMode="1" loopMode="2" loopStartPoint="0" loopEndPoint="${introHold?3:2}"><segments>${segments}</segments></mseg></msegs>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92)),tail=template.subarray(92+size);
  header.writeUInt32LE(payload.length,64);
  header.writeUInt32BE(32+payload.length+tail.length,56);
  header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
