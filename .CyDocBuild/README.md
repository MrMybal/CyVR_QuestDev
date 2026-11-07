# QuestDev documentation sources

This private build workspace produces the **same official CyDoc reader** for the native application and GitHub Pages. Maintained input belongs here; `../Documentation/` is a generated export. Build instructions and developer records stay outside the human manual.

## Build

Install Node.js 22+ and Python 3.11+, then run from the repository root:

```powershell
npm ci --prefix .CyDocBuild --no-audit --no-fund
python tools/documentation.py
# The official check also validates and writes the export:
npm run check --prefix .CyDocBuild
```

`tools/build.py`, `website/build.py` and `tools/package_release.py` call the same generator. The native build copies the entire export beside the EXE; packaging includes it in both portable and Source archives. The installer takes its contents from that portable tree. The website copies it unchanged under `manual/`. Reading the installed export requires neither Node.js, Python, a web server nor a network connection.

Open `Documentation/Documentation.html#/en/overview` or `#/fr/overview`. For web preview, run `python website/build.py`, then `python website/preview.py`. Source builds expose contextual **Help / F1**. Previously published 0.9.0 binaries do not contain this new integration.

## Maintain the manual

- Edit matching files in `manual/en/` and `manual/fr/`. English is the source; provide the complete French translation. Keep stable IDs, explicit anchors, parents and corresponding revision/sourceRevision values.
- Follow upstream [AUTHORING.md](https://github.com/MrMybal/CyDoc/blob/93f64486d61dae27a94ae1e5a05a20f6edd99914/ai/AUTHORING.md) and [INTEGRATION.md](https://github.com/MrMybal/CyDoc/blob/93f64486d61dae27a94ae1e5a05a20f6edd99914/ai/INTEGRATION.md). Use `doc:page#section` for local content links. The native route helper is the upstream C++17 example in `third_party/cydoc/`.
- `build.mjs` obtains the application version from `VERSION` and selected public images from `docs/images/`. It creates the asset inputs under the ignored `.cache/`, including an SVG favicon wrapping the existing app logo required by the reader template. Only the image reader module is enabled.
- Rebuild, review both languages, search, links, media, navigation and narrow layouts. The compiler rejects missing pages, outdated translations, invalid links and absent media; translation quality still needs prose review.
- Never edit generated manifests, `data.js`, HTML or reader bundles. Distribute the complete export, including `DocAI.md`, `DocData/`, media scripts, assets and notices. Do not include `.CyDocBuild/` in the portable distribution or public website.

## Pinned compiler

CyDoc 0.1.0 is installed from `vendor/cydoc-0.1.0.tgz`, built with upstream `npm ci`, `npm run build` and `npm pack --ignore-scripts` from the clean revision in `vendor/ORIGIN.json`. That record includes its SHA-256 and MIT license. The lockfile freezes the archive integrity and compiler dependencies. CyDoc is not assumed to be published to npm. A local sibling checkout is not required to build QuestDev's documentation.

To update, build a new official archive from the chosen exact upstream revision, replace the vendor archive and origin record, update the lockfile, and validate the entire application and web export. Retain the CyDoc MIT notice and compiler dependency notices. The manual content remains GPL-3.0-only.
