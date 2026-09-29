// SPDX-License-Identifier: GPL-3.0-or-later
import {readFileSync} from 'node:fs';
import {sourceIds} from './mseg-fixtures.mjs';
const root=new URL('../../',import.meta.url);
const storage=readFileSync(new URL('src/common/SurgeStorage.h',root),'utf8');
const members=storage.match(/enum lfo_type\s*\{([\s\S]*?)\}/)?.[1]
  .replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g,'').split(',').map(x=>x.trim()).filter(Boolean);
if(!members || !/^lt_sine\s*=\s*0$/.test(members[0]) || members.slice(1).some(x=>!/^\w+$/.test(x)))
  throw Error('Review formula LFO shape metadata');
const shape=members.indexOf('lt_formula'),source=sourceIds.find(x=>x.name==='ms_lfo1').id;
if(shape<0)throw Error('Missing formula LFO shape');
const interpreter=Number(storage.match(/enum Interpreter\s*\{\s*LUA\s*=\s*(\d+)/)?.[1]);
if(!Number.isInteger(interpreter))throw Error('Missing formula interpreter metadata');
export const formulaCases=[
  {name:'phase sine',code:'function process(s) s.output = sin(s.phase * 2 * pi) return s end'},
  {name:'BitOp counter',code:'function init(s) s.n = 0 return s end function process(s) s.n = s.n + 1 s.output = bit.band(s.n,31) / 16 - 1 return s end'},
  {name:'vector second output',index:1,code:'function process(s) s.output = {0, cos(s.phase * 2 * pi)} return s end'},
  {name:'shared initialization',code:'function init(s) shared.n = (shared.n or 0) + 1 s.n = shared.n return s end function process(s) s.output = s.n * 0.25 return s end'}
];
export function formulaFixture(fixture,{modulated=true}={}){
  const template=readFileSync(new URL('resources/data/patches_factory/Templates/Init FM2.fxp',root));
  const size=template.readUInt32LE(64),tail=template.subarray(92+size);
  let xml=template.subarray(92,92+size).toString().replace(/\0+$/,'');
  for(const [name,value] of [['shape',shape],['rate',1],['magnitude',1],['trigmode',1]]){
    const pattern=new RegExp(`(<a_lfo0_${name}\\s+[^>]*?value=")[^"]*("[^>]*?/>)`);
    if(!pattern.test(xml))throw Error('Missing formula fixture parameter '+name);
    xml=xml.replace(pattern,(_,a,b)=>a+value+b);
  }
  if(modulated){
    const pitch=/<a_osc1_pitch\s+([^>]*?)\/>/;
    if(!pitch.test(xml))throw Error('Missing formula fixture pitch');
    xml=xml.replace(pitch,(_,attrs)=>`<a_osc1_pitch ${attrs}><modrouting source="${source}" depth="6" muted="0" source_index="${fixture.index||0}"/></a_osc1_pitch>`);
  }
  if(!/<formulae\s*\/>/.test(xml))throw Error('Review template formula storage');
  xml=xml.replace(/<formulae\s*\/>/,`<formulae><formula scene="0" i="0" formula="${Buffer.from(fixture.code).toString('base64')}" interpreter="${interpreter}"/></formulae>`);
  const payload=Buffer.from(xml),header=Buffer.from(template.subarray(0,92));
  header.writeUInt32LE(payload.length,64);header.writeUInt32BE(32+payload.length+tail.length,56);
  header.writeUInt32BE(84+payload.length+tail.length,4);
  return Buffer.concat([header,payload,tail]);
}
