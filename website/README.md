# CyVR QuestDev showcase

A complete English/French static presentation of QuestDev: application logs, OVR curves, device actions, applications, mirroring settings, file batches, plugins, AI/MCP/SDK, retention settings, updates, documentation and downloads.

## Build and open

From the project root, with Python 3.11 or later:

```powershell
python website/build.py
python website/preview.py
```

Open **http://127.0.0.1:50870/questdev/** for English or **http://127.0.0.1:50870/questdev/?lang=fr** for French. The loopback preview serves only the generated artifact, under a project subpath. Stop it with Ctrl+C. Change the port with `--port` or the subpath with `--prefix /another-path/`.

The generated entry is `dist/website/index.html`. Copy the **whole `dist/website/` directory** to another location and open `index.html` directly; the page uses local classic JavaScript, styles, images and original Markdown guides, without fetch, CDNs, package installation or a server dependency. `fr.html` also prerenders French for opening without JavaScript. JavaScript enables language switching, the image viewer and console tabs. Opening an image in the viewer stays within the page.

No application installer is included in this website. Its optional GitHub and download links point to the existing public QuestDev Alpha. Those destinations need a network connection. No hosting or deployment workflow is configured by this showcase.

## Maintained files

| File | Purpose |
| --- | --- |
| `content.json` | Complete EN/FR copy, screenshot catalog, captions, alt text and verified optional destinations |
| `build.py` | Dependency-free generator, English/French prerendering, local reference and translation checks |
| `main.js` | Language URLs, responsive navigation, keyboard tabs and modal image viewer |
| `styles.css` | Responsive graphite/orange appearance using the existing QuestDev branding |
| `preview.py` | Artifact-only loopback preview with static subpath support |
| `../docs/images/` | Existing public captures of the real app in demonstration mode; paths are selected by the catalog |

Generated files belong in `dist/website/` and are ignored by Git. Do not edit them. The build does not require Git metadata or access to GitHub. Override the output with `python website/build.py --output build/website-preview` when needed; only an empty directory or a previous generated showcase is accepted. Rebuild after changing the version or assets.

Styles, scripts, the logo and screenshots use content-hashed names. The HTML is regenerated with the matching references in the same build. A static host should revalidate HTML and may cache hashed assets for longer; cache invalidation of an already cached HTML file remains the host's responsibility.

## Documentation and licensing

QuestDev currently maintains French Markdown guides, not a generated CyDoc manual. The artifact bundles the original supported user guides and their images under `guides/docs/`, with a bilingual guide index. Contextual links open the corresponding local Markdown files. The showcase itself is completely translated; English guide translations and an HTML manual are not supplied. No replacement documentation reader is introduced.

The SDK download example is included as a reference file. Links from the guides to developer packaging instructions point to their actual GitHub source; those supplemental pages require a connection.

Application and showcase are **GPL-3.0-only**, copyright 2026 Cyberalien. The artifact includes `LICENSE.txt`, `COPYRIGHT.txt`, `THIRD_PARTY.md` and the existing component notices. It ships no third-party script, remote font or application runtime.

## Présentation locale

Depuis la racine du projet :

```powershell
python website/build.py
python website/preview.py
```

Ouvrir **http://127.0.0.1:50870/questdev/?lang=fr**. La vitrine présente les fonctions de l’application en français et en anglais. Les captures proviennent du mode démonstration de QuestDev ; les données sont simulées. Les commandes, packages et libellés réellement affichés dans les captures gardent leur langue d’origine.

La sortie complète `dist/website/` peut être déplacée puis ouverte par son fichier `index.html`, sans installation ni serveur. `fr.html` contient aussi le texte français prérendu. Les guides fournis restent les fichiers Markdown français d’origine. Les liens de téléchargement pointent vers la release GitHub existante ; aucun installateur n’est embarqué dans la vitrine. Aucun déploiement n’est effectué par les commandes ci-dessus.
