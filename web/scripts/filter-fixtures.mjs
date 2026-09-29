// SPDX-License-Identifier: GPL-3.0-or-later
import {oscillatorFixture,oscillatorTypes} from './oscillator-fixtures.mjs';
import {filterTypes} from './filter-inventory.mjs';
import {readFileSync} from 'node:fs';
const storage=readFileSync(new URL('../../src/common/SurgeStorage.h',import.meta.url),'utf8');
const revision=Number(storage.match(/const int ff_revision\s*=\s*(\d+)/)?.[1]);
if(!Number.isInteger(revision))throw Error('Missing patch streaming revision');
export const inputOscillator=oscillatorTypes.find(t=>t.symbol==='ot_audioinput').id;
// Use with the harness's Audio Input oscillator and deterministic stereo input.
// Serial 1, oscillator routed to filter 1, filter 2 and waveshaper off. These
// fixtures isolate public filter modes; they do not cover the other routings.
export function filterFixture({type,subtype=0,muted=false,cutoff=3,resonance=0.35}){
  const filter=filterTypes[type];
  if(!Number.isInteger(type)||!filter||!Number.isInteger(subtype)||subtype<0||subtype>=Math.max(1,filter.subtypeCount))
    throw Error('Invalid filter type/subtype');
  if(!Number.isFinite(cutoff)||!Number.isFinite(resonance)||resonance<0||resonance>1)
    throw Error('Invalid filter fixture setting');
  const template=oscillatorFixture({muted}),size=template.readUInt32LE(64),tail=template.subarray(92+size);
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  // These are current-mode fixtures, not historical-patch migration fixtures.
  if(!/<patch revision="\d+"/.test(xml))throw Error('Missing patch revision');
  xml=xml.replace(/<patch revision="\d+"/,`<patch revision="${revision}"`);
  const values={a_route_o1:0,a_fb_config:0,a_f_balance:0,a_feedback:0,a_ws_type:0,
    a_filter1_type:type,a_filter1_subtype:subtype,a_filter1_cutoff:cutoff,a_filter1_resonance:resonance,
    a_filter1_envmod:0,a_filter1_keytrack:0,a_filter2_type:0};
  for(const [name,value] of Object.entries(values)){
    const pattern=new RegExp(`(<${name}\\s+[^>]*?value=")[^"]*("[^>]*?/>)`);
    if(!pattern.test(xml))throw Error('Missing filter fixture parameter '+name);
    xml=xml.replace(pattern,(_,a,b)=>a+value+b);
  }
  const header=Buffer.from(template.subarray(0,92)),payload=Buffer.from(xml);
  header.writeUInt32LE(payload.length,64);
  header.writeUInt32BE(32+payload.length+tail.length,56);
  // Native FXP permits byteSize=0; preserve that convention when present.
  if(header.readUInt32BE(4)!==0)header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
