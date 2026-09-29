// SPDX-License-Identifier: GPL-3.0-or-later
// Derive the public filter-selection matrix from the engine's own definitions.
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {resolve} from 'node:path';
const sourcePath='libs/sst/sst-filters/include/sst/filters/FilterConfiguration.h';
const source=readFileSync(new URL('../../'+sourcePath,import.meta.url),'utf8');
const clean=text=>text.replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\//g,'');
const body=pattern=>{const match=source.match(pattern);if(!match)throw Error('Filter declaration changed');return clean(match[1]);};
const symbols=body(/enum FilterType\s*\{([\s\S]*?)\}/).split(',').map(s=>s.trim()).filter(Boolean);
if(symbols.pop()!=='num_filter_types'||symbols.some((s,i)=>!(i?/^fut_\w+$/:/^fut_none\s*=\s*0$/).test(s)))throw Error('Review filter enum ordering');
const names=[...body(/filter_type_names\[num_filter_types\]\[32\]\s*=\s*\{([\s\S]*?)\}/).matchAll(/"([^"]+)"/g)].map(m=>m[1]);
const counts=body(/fut_subcount\[num_filter_types\]\s*=\s*\{([\s\S]*?)\}/).split(',').map(s=>s.trim()).filter(Boolean);
if(names.length!==symbols.length||counts.length!==symbols.length||counts.some(s=>!/^\d+$/.test(s)))throw Error('Review filter names and subtype counts');
export const filterTypes=symbols.map((symbol,id)=>({id,symbol:symbol.split(/\s*=/)[0],name:names[id],subtypeCount:Number(counts[id])}));
export const filterModes=filterTypes.flatMap(type=>Array.from({length:Math.max(1,type.subtypeCount)},(_,subtype)=>({type:type.id,subtype})));
export const filterInventory={schema:1,source:sourcePath,sourceSha256:createHash('sha256').update(source).digest('hex'),
  scope:'Public filter type and subtype selectors. Zero-subtype types use subtype 0. This matrix is unverified: it does not prove DSP or UI parity. Internal subtype masks, morph parameters, cutoff/resonance ranges, routing, modulation and live edits need additional coverage.',
  rates:[44100,48000],types:filterTypes,modes:filterModes};
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  if(process.argv.length!==3)throw Error('Usage: node web/scripts/filter-inventory.mjs OUTPUT.json');
  writeFileSync(process.argv[2],JSON.stringify(filterInventory,null,2)+'\n');
  console.log(`${filterTypes.length} types, ${filterModes.length} public type/subtype combinations, ${filterModes.length*2} rate cases; none marked verified`);
}
