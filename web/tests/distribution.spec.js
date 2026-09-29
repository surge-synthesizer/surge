import {test,expect} from './fixtures.js';
import {execFileSync,spawn} from 'node:child_process';
import {mkdtempSync,rmSync,readFileSync,readdirSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {once} from 'node:events';
import {createHash} from 'node:crypto';
const root=fileURLToPath(new URL('../../',import.meta.url));

test('static package rejects corrupt inputs and publishes reproducible version directories',()=>{
  execFileSync('python3',[path.join(root,'web/tests/package-static.test.py')],{timeout:10000});
});

test('source archive preserves changed worktrees and embedded submodules reproducibly',()=>{
  execFileSync('python3',[path.join(root,'web/tests/source-archive.test.py')],{timeout:10000});
});

test('versioned static distribution starts JUCE, fetches a patch and plays through the worklet',async({page})=>{
  test.setTimeout(90000);
  const distributionRoot=process.env.SURGE_DISTRIBUTION_ROOT?path.resolve(process.env.SURGE_DISTRIBUTION_ROOT):root;
  const temporary=mkdtempSync(path.join(tmpdir(),'surge-distribution-'));
  let server;
  try{
    const existing=process.env.SURGE_DISTRIBUTION_PACKAGE?path.resolve(process.env.SURGE_DISTRIBUTION_PACKAGE):null;
    const output=existing?path.dirname(existing):path.join(temporary,'dist');
    const directory=existing || execFileSync('python3',[path.join(distributionRoot,'web/scripts/package-static.py'),'--output',output],{encoding:'utf8',timeout:45000}).trim();
    if(existing)execFileSync('python3',[path.join(root,'web/scripts/package-static.py'),'--verify',existing],{timeout:45000});
    const manifest=JSON.parse(readFileSync(path.join(directory,'distribution.json'),'utf8'));
    expect(manifest.status).toBe('development');
    if(existsSync(path.join(distributionRoot,'SOURCE-DISTRIBUTION.json'))&&!existsSync(path.join(distributionRoot,'.git'))){
      expect(manifest.source.snapshot).toMatch(/^[0-9a-f]{64}$/);
      if(manifest.source.snapshotArchiveIncluded){
        const snapshotBytes=readFileSync(path.join(distributionRoot,'SOURCE-DISTRIBUTION.json'));
        expect(manifest.source.snapshot).toBe(createHash('sha256').update(snapshotBytes).digest('hex'));
        expect(manifest.source.dirty).toBe(JSON.parse(snapshotBytes).dirty);
        expect(existsSync(path.join(directory,manifest.source.archive))).toBe(true);
        expect(Object.keys(manifest.files)).toContain('build-receipt.json');
      }else expect(manifest.source.dirty).toBeNull();
    }
    expect(Object.keys(manifest.files)).not.toContain('surge-web.js');
    expect(Object.keys(manifest.files)).toContain('user-files.js');
    expect(Object.keys(manifest.files)).not.toContain('surge-juce-browser-check.js');
    const source=[
      'import importlib.util, sys',
      'from functools import partial',
      's=importlib.util.spec_from_file_location("serve",sys.argv[1])',
      'm=importlib.util.module_from_spec(s);s.loader.exec_module(m)',
      'server=m.Server(("127.0.0.1",0),partial(m.Handler,directory=sys.argv[2]))',
      'print(server.server_port,flush=True)',
      'server.serve_forever()'
    ].join('\n');
    server=spawn('python3',['-u','-c',source,path.join(root,'web/scripts/serve.py'),output],{stdio:['ignore','pipe','ignore']});
    const port=await new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>reject(Error('Distribution server did not start')),10000);
      server.once('error',error=>{clearTimeout(timer);reject(error);});
      server.once('exit',code=>{clearTimeout(timer);reject(Error('Distribution server exited: '+code));});
      server.stdout.once('data',data=>{clearTimeout(timer);resolve(Number(data.toString().trim()));});
    });
    expect(port).toBeGreaterThan(0);
    const prefix='/'+path.basename(directory)+'/';
    const origin=`http://127.0.0.1:${port}`;
    const requests=[],errors=[],failed=[];
    page.on('request',request=>requests.push(request.url()));
    page.on('pageerror',error=>errors.push(error.message));
    page.on('requestfailed',request=>failed.push({url:request.url(),error:request.failure()?.errorText}));
    const response=await page.goto(origin+prefix);
    expect(response.headers()['cross-origin-opener-policy']).toBe('same-origin');
    expect(response.headers()['cross-origin-embedder-policy']).toBe('require-corp');
    expect(await page.evaluate(()=>crossOriginIsolated)).toBe(true);
    await expect(page.getByRole('button',{name:'Main Menu',exact:true})).toBeAttached();
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init Saw');
    await page.getByRole('button',{name:'User files',exact:true}).click();
    await expect(page.getByRole('dialog',{name:'User files',exact:true})).toBeVisible();
    await page.getByRole('button',{name:'Close user files',exact:true}).click();
    expect(await page.evaluate(()=>Module.ccall('surge_browser_request_patch','number',['string'],['/factory/patches_factory/Templates/Init FM2.fxp']))).toBe(1);
    await expect.poll(()=>page.evaluate(()=>Module.ccall('surge_browser_patch_name','string',[],[]))).toBe('Init FM2');
    const fonts=Object.fromEntries(readdirSync(path.join(distributionRoot,'resources/fonts')).filter(name=>name.endsWith('.ttf')).map(name=>[name,createHash('sha256').update(readFileSync(path.join(distributionRoot,'resources/fonts',name))).digest('hex')]));
    const actualFonts=await page.evaluate(async names=>{
      const entries=await Promise.all(names.map(async name=>[name,Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',Module.FS.readFile('/fonts/'+name))),x=>x.toString(16).padStart(2,'0')).join('')]));
      return Object.fromEntries(entries);
    },Object.keys(fonts));
    expect(actualFonts).toEqual(fonts);
    await page.getByRole('button',{name:'Enable audio',exact:true}).click();
    await expect.poll(()=>page.evaluate(()=>Module._surge_browser_audio_blocks())).toBeGreaterThan(32);
    expect(requests.filter(url=>url.startsWith(origin)).every(url=>new URL(url).pathname.startsWith(prefix))).toBe(true);
    expect(requests.some(url=>url.endsWith('/surge-xt-browser.wasm'))).toBe(true);
    expect(requests.some(url=>url.includes('/library/objects/'))).toBe(true);
    expect(requests.some(url=>new URL(url).pathname.startsWith(prefix+'source/'))).toBe(false);
    expect(errors).toEqual([]);
    // Chrome has occasionally reported a cancelled .data request despite all
    // mounted fonts matching above. Retain the observation; do not excuse any
    // other failed application/library request.
    if(failed.length)await test.info().attach('cancelled-font-request',{body:JSON.stringify(failed),contentType:'application/json'});
    expect(failed.every(item=>item.error==='net::ERR_ABORTED' && item.url===origin+prefix+'surge-xt-browser.data')).toBe(true);
  }finally{
    if(server && server.exitCode===null){const closed=once(server,'close');server.kill();await closed;}
    rmSync(temporary,{recursive:true,force:true});
  }
});
