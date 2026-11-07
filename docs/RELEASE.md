# Archives Alpha Windows

La release `v0.9.0-alpha.1` propose un **installateur Windows x64** et une archive portable avec l’exécutable, les plugins, le moteur de mirroring scrcpy, les guides, les licences et un **Python 3.13.17 embarqué**. L’installateur crée les raccourcis et la désinstallation pour le compte Windows courant ; le ZIP s’extrait entièrement pour lancer `CyVR_QuestDev.exe`. ADB et les CLI des fournisseurs IA restent des outils externes. Le runtime embarqué ne remplace pas les installations Python de la machine. Voir [INSTALLATION.md](INSTALLATION.md).

L’archive **Source** comprend le projet et les sources CyUI / Dear ImGui nécessaires à la recompilation, ainsi que les archives sources scrcpy, FFmpeg, SDL, libusb et dav1d. `SOURCE-SHA256.json` identifie tous ses fichiers. Les archives ont des empreintes dans `SHA256SUMS.txt`. Les limites des fonctionnalités Alpha sont décrites dans [VALIDATION.md](VALIDATION.md).

Pour l’architecture modulaire, reconstruire avec le commit CyUI épinglé et conserver `UI-BUILD.json`. Le packager inclut également TextEditor, Lexilla, les en-têtes Scintilla, les données générées et leurs notices. Il vérifie le SHA-256 de l’exécutable reconstruit. Voir [CYUI_MIGRATION.md](CYUI_MIGRATION.md).

Les sources actuelles ajoutent le manuel CyDoc EN/FR et l’aide contextuelle F1. Installer Node.js 22+ pour la génération ; `tools/build.py` et le packager appellent `tools/documentation.py` avec le compilateur épinglé dans `.CyDocBuild/`. Le dossier généré `Documentation/` entier est copié dans les archives Windows et Source, puis dans l’installateur. Il contient les scripts, images, données et licences nécessaires à la lecture locale, sans runtime Node.js. Les binaires 0.9.0 déjà publiés précèdent cet ajout. Voir les [instructions de maintenance](../.CyDocBuild/README.md).

Les versions 0.7+ proposent aussi [la mise à jour depuis l’application](UPDATES.md).

## Produire les archives

Depuis un arbre de sources vérifié, installer les prérequis du [README](../README.md), puis :

```powershell
python tools/build.py --clean --test
python tools/setup_mirroring.py
runtime/python/Scripts/python.exe -m pip download --only-binary=:all: --platform win_amd64 --python-version 3.13 --implementation cp --abi cp313 --find-links runtime/wheels -r runtime/requirements.lock --dest build/release-cache/wheels
python tools/package_release.py --output build/releases/v0.9.0-alpha.1 --cyui ../CyUI
python tools/setup_installer.py
python tools/build_installer.py --product build/releases/v0.9.0-alpha.1/CyVR_QuestDev-0.9.0-alpha.1-Windows-x64 --output build/releases/v0.9.0-alpha.1
```

Le script récupère le Python embarqué officiel s’il manque au cache et vérifie son SHA-256 épinglé depuis le manifeste Python.org. Les versions des dépendances sont celles de `runtime/requirements.lock`. Les roues sont extraites dans un runtime isolé, sans réutiliser l’environnement virtuel local. Un nouveau dossier de sortie est requis à chaque tentative ; les paquets précédents sont préservés.

`setup_installer.py` récupère Inno Setup 6.7.4 depuis sa release officielle, vérifie le SHA-256 épinglé et la signature Authenticode Pyrsys B.V., puis prépare le compilateur en mode portable sous `build/toolchain`, sans enregistrer Inno Setup sur la machine. `build_installer.py` peut aussi recevoir `--iscc <ISCC.exe>`. Il refuse un contenu Windows altéré, une version différente ou un inventaire de désinstallation incohérent, compile `installer/questdev.iss`, puis inclut l’EXE dans `SHA256SUMS.txt`. La recette est présente dans l’archive Source. Le binaire Setup est livré sans signature de code Cyberalien.

Pour la vérification d’installation, compiler dans un autre dossier avec `--test` : cette variante possède son propre identifiant Windows et ses propres raccourcis. Exécuter ensuite `python tools/verify_installer.py --setup <Setup-de-test.exe> --product <dossier-Windows> --output <nouveau-dossier>`. Ce contrôle installe, réinstalle, lance l’app avec ses services, applique une mise à jour et une restauration, puis désinstalle en conservant les données fictives personnelles.

Avant publication, vérifier le démarrage depuis une extraction dans un autre dossier, le runtime Core/MCP embarqué, les imports natifs Windows, les tests et les contenus réels des archives. Publier la version comme **préversion Alpha**, avec son code source correspondant et ses licences.

`python tools/verify_release.py <archive-Windows.zip> --output <nouveau-dossier>` vérifie une extraction déplacée, avec un PATH réduit, un profil local isolé et ADB simulé : démarrage de l’app, rendu, Core/SDK, 38 outils MCP, lecture/export CSV et fermeture.
