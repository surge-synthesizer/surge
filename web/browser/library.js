// SPDX-License-Identifier: GPL-3.0-or-later
// Asset preparation runs on the browser task queue, never inside the audio callback.
export class FactoryLibrary {
  static async open(url = './library/manifest.json', report = () => {}) {
    const location = new URL(url, document.baseURI);
    const response = await fetch(location);
    if (!response.ok) throw Error(`Factory index download failed (${response.status})`);
    return new FactoryLibrary(await response.json(), location, report);
  }
  constructor(manifest, location, report) {
    if (manifest.schema !== 1 || !Array.isArray(manifest.entries)) throw Error('Unsupported factory index');
    this.patchIndex = manifest.patchIndex;
    this.version = manifest.version;
    this.location = location;
    this.report = report;
    this.entries = new Map();
    this.pending = new Map();
    for (const entry of manifest.entries) {
      if (typeof entry.path !== 'string' || entry.path.startsWith('/') ||
          entry.path.split('/').some(part => !part || part === '.' || part === '..') ||
          entry.path.includes('\\') || entry.path.includes('\0') ||
          !/^[a-f0-9]{64}$/.test(entry.sha256) || entry.url !== 'objects/' + entry.sha256 ||
          !Number.isSafeInteger(entry.size) || entry.size < 0 || this.entries.has(entry.path))
        throw Error('Invalid factory index entry');
      this.entries.set(entry.path, Object.freeze({...entry}));
    }
  }
  search(query = '', extension = '') {
    const words = query.toLocaleLowerCase().split(/\s+/).filter(Boolean);
    return [...this.entries.values()].filter(entry =>
      (!extension || entry.extension === extension) &&
      words.every(word => entry.path.toLocaleLowerCase().includes(word)));
  }
  async verify(response, entry) {
    if (!response.ok) throw Error(`Download failed (${response.status}): ${entry.path}`);
    const bytes = new Uint8Array(await response.arrayBuffer());
    if (bytes.byteLength !== entry.size) throw Error(`Incomplete factory asset: ${entry.path}`);
    const hash = [...new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))]
      .map(value => value.toString(16).padStart(2, '0')).join('');
    if (hash !== entry.sha256) throw Error(`Factory asset checksum mismatch: ${entry.path}`);
    return bytes;
  }
  async bytes(path) {
    if (!this.entries.has(path)) throw Error(`Unknown factory asset: ${path}`);
    if (this.pending.has(path)) return this.pending.get(path);
    const pending = this.download(this.entries.get(path));
    this.pending.set(path, pending);
    try { return await pending; }
    finally { this.pending.delete(path); }
  }
  async download(entry) {
    const url = new URL(entry.url, this.location);
    let cache;
    try { cache = await caches.open('surge-factory-v1'); }
    catch (error) { this.report(`Factory cache unavailable; using the downloaded copy. ${error}`); }
    if (cache) {
      try {
        const cached = await cache.match(url);
        if (cached) {
          try { return await this.verify(cached, entry); }
          catch { await cache.delete(url); }
        }
      } catch (error) { this.report(`Factory cache read failed. ${error}`); }
    }
    const response = await fetch(url);
    const bytes = await this.verify(response, entry);
    if (cache) {
      try { await cache.put(url, new Response(bytes, {headers:{'Content-Type':'application/octet-stream'}})); }
      catch (error) { this.report(`Factory cache write failed; the downloaded asset remains available. ${error}`); }
    }
    return bytes;
  }
  installWavetableCatalog(FS) {
    for (const entry of this.entries.values()) {
      if (!/^(wavetables|wavetables_3rdparty)\//.test(entry.path) || !['.wt','.wav','.wtscript'].includes(entry.extension)) continue;
      const destination = '/factory/' + entry.path;
      FS.mkdirTree(destination.slice(0, destination.lastIndexOf('/')));
      if (!FS.analyzePath(destination).exists) FS.writeFile(destination, new Uint8Array());
    }
  }
  installImpulseCatalog(FS) {
    for(const entry of this.entries.values()){
      if(!/^(impulses_factory|impulses_3rdparty)\//.test(entry.path)||!['.wav','.aif','.aiff','.flac'].includes(entry.extension))continue;
      const path='/factory/'+entry.path;
      FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));
      if(!FS.analyzePath(path).exists)FS.writeFile(path,new Uint8Array());
    }
  }
  async installPatchIndex(FS) {
    const index = this.patchIndex;
    if (!index || !/^[a-f0-9]{64}$/.test(index.sha256) || index.url !== 'objects/' + index.sha256)
      throw Error('Invalid patch index');
    const bytes = await this.download(index);
    const stream = new Blob([bytes]).stream().pipeThrough(new DecompressionStream('gzip'));
    const patches = await new Response(stream).json();
    for (const [path, xml] of Object.entries(patches)) {
      const skin = path.startsWith('skins/') && path.endsWith('.surge-skin/skin.xml');
      const preset = /^(fx_presets|modulator_presets)\/.*\.(srgfx|modpreset)$/.test(path);
      if (!this.entries.has(path) || (!/\.(fxp|wtscript)$/.test(path) && !skin && !preset) || typeof xml !== 'string')
        throw Error('Invalid patch catalog entry');
      const destination = '/factory/' + path;
      if (path.endsWith('.wtscript') || skin || preset) {
        FS.mkdirTree(destination.slice(0, destination.lastIndexOf('/')));
        FS.writeFile(destination, xml);
        continue;
      }
      const metadata = '/factory/.metadata/' + path + '.xml';
      FS.mkdirTree(destination.slice(0, destination.lastIndexOf('/')));
      FS.mkdirTree(metadata.slice(0, metadata.lastIndexOf('/')));
      // Directory entries allow the existing native category scanner to operate.
      // Their zero-byte contents are never loaded as patches: browser selection
      // must finish install() before the engine is allowed to consume the queue.
      if (!FS.analyzePath(destination).exists) FS.writeFile(destination, new Uint8Array());
      FS.writeFile(metadata, xml);
    }
  }
  async install(path, FS) {
    // Verify before touching the active filesystem, then publish with one rename.
    const bytes = await this.bytes(path);
    const destination = '/factory/' + path;
    FS.mkdirTree(destination.slice(0, destination.lastIndexOf('/')));
    const temporary = destination + '.download-' + crypto.randomUUID();
    try {
      FS.writeFile(temporary, bytes);
      FS.rename(temporary, destination);
    } catch (error) {
      try { FS.unlink(temporary); } catch {}
      throw error;
    }
    return destination;
  }
  async installSkin(root, FS) {
    if (!root.startsWith('skins/') || !root.endsWith('.surge-skin/') || !this.entries.has(root+'skin.xml'))
      throw Error('Unknown factory skin');
    const entries=[...this.entries.values()].filter(entry=>entry.path.startsWith(root));
    const target='/factory/'+root.slice(0,-1),stage=target+'.download-'+crypto.randomUUID(),backup=stage+'.previous';
    const remove=path=>{
      if(!FS.analyzePath(path).exists)return;
      for(const name of FS.readdir(path))if(name!=='.'&&name!=='..'){
        const child=path+'/'+name;
        if(FS.isDir(FS.stat(child).mode))remove(child);else FS.unlink(child);
      }
      FS.rmdir(path);
    };
    FS.mkdirTree(stage);
    let next=0,saved=false;
    try {
      // Bounded parallel downloads, with no live destination changes until all
      // checksum-verified resources have been written to the staging directory.
      const results=await Promise.allSettled(Array.from({length:4},async()=>{
        while(next<entries.length){
          const entry=entries[next++],bytes=await this.bytes(entry.path);
          const path=stage+'/'+entry.path.slice(root.length);
          FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));FS.writeFile(path,bytes);
        }
      }));
      const failed=results.find(result=>result.status==='rejected');if(failed)throw failed.reason;
      if(FS.analyzePath(target).exists){FS.rename(target,backup);saved=true;}
      try{FS.rename(stage,target);}catch(error){if(saved)FS.rename(backup,target);throw error;}
      remove(backup);
    }finally{remove(stage);}
    return target;
  }
}
