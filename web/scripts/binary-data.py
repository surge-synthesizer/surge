#!/usr/bin/env python3
"""Generate JUCE BinaryData symbols for Surge's unchanged embedded UI resources."""
import pathlib
import re
import sys
root, output = map(pathlib.Path, sys.argv[1:])
files = sorted((root/'resources/classic-skin-svgs').glob('*.svg'))
files += sorted((root/'resources/fonts').glob('Lato*ttf'))
files += [root/'resources/fonts'/name for name in ['IndieFlower.ttf','FiraMono-Regular.ttf']]
files += [root/'resources/surge-xt'/name for name in ['memory-skin.xml','wtfile_icon.svg','wtscript_icon.svg']]
output.mkdir(parents=True,exist_ok=True)
header=['#pragma once','namespace SurgeXTBinary {']
source=['#include "SurgeXTBinary.h"','#include <cstring>','namespace SurgeXTBinary {']
names=[]
for path in files:
    name=re.sub(r'[^a-zA-Z0-9_]','_',path.name.replace('-','').replace('.','_'))
    if name in names: raise ValueError(f'Duplicate resource symbol: {name}')
    names.append(name)
    data=path.read_bytes()
    header += [f'extern const char *{name};',f'const int {name}Size = {len(data)};']
    source += [f'static const unsigned char data_{name}[] = {{{",".join(map(str,data))},0}};',f'const char *{name} = reinterpret_cast<const char *>(data_{name});']
header += ['extern const char *namedResourceList[];','extern const char *originalFilenames[];',f'const int namedResourceListSize = {len(files)};','const char *getNamedResource(const char *, int &);','const char *getNamedResourceOriginalFilename(const char *);','}']
source += ['const char *namedResourceList[] = {'+','.join('"'+n+'"' for n in names)+'};','const char *originalFilenames[] = {'+','.join('"'+p.name+'"' for p in files)+'};','const char *getNamedResource(const char *name, int &size) {']
for name in names: source += [f'if(std::strcmp(name,"{name}")==0) {{size={name}Size;return {name};}}']
source += ['size=0;return nullptr;}','const char *getNamedResourceOriginalFilename(const char *name) {for(int i=0;i<namedResourceListSize;++i)if(std::strcmp(name,namedResourceList[i])==0)return originalFilenames[i];return nullptr;}','}']
for name, lines in [('SurgeXTBinary.h', header), ('SurgeXTBinary.cpp', source)]:
    destination = output / name
    content = '\n'.join(lines)
    if not destination.exists() or destination.read_text() != content:
        destination.write_text(content)
