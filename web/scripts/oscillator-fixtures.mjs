// SPDX-License-Identifier: GPL-3.0-or-later
import {readFileSync} from 'node:fs';
const root=new URL('../../',import.meta.url);
const source=readFileSync(new URL('src/common/SurgeStorage.h',root),'utf8');
const clean=s=>s.replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g,'');
const members=clean(source.match(/enum osc_type\s*\{([\s\S]*?)\}/)[1]).split(',').map(s=>s.trim()).filter(Boolean);
if(members.pop()!=='n_osc_types'||members.some((s,i)=>!(i?/^ot_\w+$/:/^ot_classic\s*=\s*0$/).test(s)))throw Error('Review oscillator enum layout');
const names=[...clean(source.match(/osc_type_names\[n_osc_types\]\[24\]\s*=\s*\{([\s\S]*?)\}/)[1]).matchAll(/"([^"]+)"/g)].map(m=>m[1]);
if(names.length!==members.length)throw Error('Oscillator name/ID mismatch');
export const oscillatorTypes=members.map((s,id)=>({id,symbol:s.split(/\s*=/)[0],name:names[id]}));
// This original patch embeds its wavetable samples, making table-dependent
// oscillators independent of native-only filesystem lookup or lazy downloads.
const template=readFileSync(new URL('resources/data/patches_factory/Templates/Init Wavetable.fxp',root));
const size=template.readUInt32LE(64);
if(template.subarray(0,4).toString()!=='CcnK'||template.subarray(60,64).toString()!=='sub3'||template.length<=92+size)throw Error('Review embedded wavetable fixture');
export function oscillatorFixture({muted=false}={}){
  const bytes=Buffer.from(template),xml=bytes.subarray(92,92+size).toString();
  const required={scene_active:0,scenemode:0,a_drift:0,a_level_o1:1,a_mute_o1:0,a_mute_o2:1,a_mute_o3:1,a_mute_ring12:1,a_mute_ring23:1,a_mute_noise:1};
  for(const [name,value] of Object.entries(required)){
    const actual=xml.match(new RegExp(`<${name}\\s+[^>]*?value="([^"]+)"`));
    if(!actual||Number(actual[1])!==value)throw Error('Review oscillator isolation: '+name);
  }
  let changed=xml.replace(/(<a_osc1_retrigger\s+[^>]*?value=")0("[^>]*\/?>)/,(_,a,b)=>a+'1'+b);
  if(muted)changed=changed.replace(/(<a_mute_o1\s+[^>]*?value=")0(")/,(_,a,b)=>a+'1'+b);
  if(changed===xml||Buffer.byteLength(changed)!==size)throw Error('Review retrigger fixture');
  bytes.write(changed,92,'utf8');return bytes;
}
