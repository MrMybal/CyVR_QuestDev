// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
import {readFile,writeFile,mkdir,copyFile} from 'node:fs/promises';
import {dirname,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';

const workspace=dirname(fileURLToPath(import.meta.url));
const root=resolve(workspace,'..');
const version=(await readFile(resolve(root,'VERSION'),'utf8')).trim();
const config=JSON.parse(await readFile(resolve(workspace,'cydoc.config.json'),'utf8'));
config.version=version;
await writeFile(resolve(workspace,'cydoc.config.json'),JSON.stringify(config,null,2)+'\n');
const assets=resolve(workspace,config.assets);
await mkdir(assets,{recursive:true});
await copyFile(resolve(root,'assets/branding/cyvr-questdev-logo-gradient.png'),resolve(assets,'questdev-logo.png'));
// The upstream reader references assets/favicon.svg; supply our existing app logo.
const logo=await readFile(resolve(assets,'questdev-logo.png'));
await writeFile(resolve(assets,'favicon.svg'),`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024"><image width="1024" height="1024" href="data:image/png;base64,${logo.toString('base64')}"/></svg>`);
for(const name of ['console','console-raw','console-sorted','comparison','applications','device-actions','mirroring-settings','files-batch','metrics-curves','ovr-metrics','cyinsight-ovr','assistant','log-settings','github-updates']){
  await copyFile(resolve(root,'docs/images',name+'.png'),resolve(assets,name+'.png'));
}
const cli=resolve(workspace,'node_modules/cydoc/tools/cli.mjs');
const result=spawnSync(process.execPath,[cli,process.argv[2]==='check'?'check':'build',resolve(workspace,'cydoc.config.json')],{cwd:workspace,stdio:'inherit'});
if(result.status!==0)process.exit(result.status??1);
const manifest=JSON.parse(await readFile(resolve(root,'Documentation/DocData/manifest.json'),'utf8'));
if(manifest.defaultLocale!=='en'||manifest.locales.map(x=>x.code).sort().join(',')!=='en,fr')throw Error('The complete EN/FR manual is required');
const notices=resolve(root,'Documentation/DocData/licenses');
await mkdir(notices,{recursive:true});
for(const [name,file] of [['cydoc','LICENSE'],['marked','LICENSE.md'],['prismjs','LICENSE'],['esbuild','LICENSE.md']]){
  await copyFile(resolve(workspace,'node_modules',name,file),resolve(notices,name+'-MIT.txt'));
}
console.log(`QuestDev ${version} manual: ${manifest.pages.length} pages, EN/FR.`);
