import {mkdtempSync,writeFileSync,readFileSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {execFileSync} from 'node:child_process';

// File-backed handoff avoids waiting for EOF on synchronous child-process pipes.
// Keep fixture generation bounded so a tool failure cannot stall Playwright.
export function zipBase64Files(files){
  const directory=mkdtempSync(join(tmpdir(),'surge-test-archive-'));
  try{
    const input=join(directory,'files.json'),output=join(directory,'fixture.zip');
    writeFileSync(input,JSON.stringify(files));
    execFileSync('python3',['-c',
      'import sys,json,zipfile,base64\nwith open(sys.argv[1]) as source, zipfile.ZipFile(sys.argv[2],"w",zipfile.ZIP_DEFLATED) as archive:\n for name,data in json.load(source).items(): archive.writestr(name,base64.b64decode(data))',
      input,output],{stdio:'ignore',timeout:10000});
    return readFileSync(output);
  }finally{
    rmSync(directory,{recursive:true,force:true});
  }
}
