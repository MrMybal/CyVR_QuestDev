# Archives Alpha Windows

La release `v0.6.0-alpha.2` propose une archive **Windows x64** avec l’exécutable, les plugins, le moteur de mirroring scrcpy, les guides, les licences et un **Python 3.13.17 embarqué**. Extraire l’archive entière et lancer `CyVR_QuestDev.exe`. ADB et les CLI des fournisseurs IA restent des outils externes. Le runtime embarqué ne remplace pas les installations Python de la machine.

L’archive **Source** comprend le projet et les sources CyUI / Dear ImGui nécessaires à la recompilation, ainsi que les archives sources scrcpy, FFmpeg, SDL, libusb et dav1d. `SOURCE-SHA256.json` identifie tous ses fichiers. Les archives ont des empreintes dans `SHA256SUMS.txt`. Les limites des fonctionnalités Alpha sont décrites dans [VALIDATION.md](VALIDATION.md).

## Produire les archives

Depuis un arbre de sources vérifié, installer les prérequis du [README](../README.md), puis :

```powershell
python tools/build.py --clean --test
python tools/setup_mirroring.py
runtime/python/Scripts/python.exe -m pip download --only-binary=:all: --platform win_amd64 --python-version 3.13 --implementation cp --abi cp313 --find-links runtime/wheels -r runtime/requirements.lock --dest build/release-cache/wheels
python tools/package_release.py --output build/releases/v0.6.0-alpha.2 --cyui ../CyImgui_Interface
```

Le script récupère le Python embarqué officiel s’il manque au cache et vérifie son SHA-256 épinglé depuis le manifeste Python.org. Les versions des dépendances sont celles de `runtime/requirements.lock`. Les roues sont extraites dans un runtime isolé, sans réutiliser l’environnement virtuel local. Un nouveau dossier de sortie est requis à chaque tentative ; les paquets précédents sont préservés.

Avant publication, vérifier le démarrage depuis une extraction dans un autre dossier, le runtime Core/MCP embarqué, les imports natifs Windows, les tests et les contenus réels des archives. Publier la version comme **préversion Alpha**, avec son code source correspondant et ses licences.

`python tools/verify_release.py <archive-Windows.zip> --output <nouveau-dossier>` vérifie une extraction déplacée, avec un PATH réduit, un profil local isolé et ADB simulé : démarrage de l’app, rendu, Core/SDK, 38 outils MCP, lecture/export CSV et fermeture.
