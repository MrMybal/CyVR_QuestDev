# Architecture CyUI de QuestDev

QuestDev utilise [CyUI 1.2](https://github.com/MrMybal/CyUI) au commit `ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3`. La révision est définie dans `cmake/CyUI.cmake`. Le backend reste Windows / Win32 / DirectX 11 ; cette migration n’ajoute pas de backend Linux ou macOS.

## Modules et frontières

| Module | Utilisation réelle dans QuestDev |
| --- | --- |
| Core | Thèmes, polices, icônes, notifications, contrôles, propriétés, layout et catalogue EN/FR |
| Plots | Courbes OVR, axes, grille, pan/zoom, survol et séries du curseur temporel partagé |
| Images | Aperçu des captures avec ajustement, zoom et déplacement |
| DocumentViews | Transcript IA, Markdown natif, tableaux, code, liens et copie de messages |
| CodeEditor | RAW Logcat, sortie ADB et JSON du service de plugin, tous en lecture seule |
| Canvas / Code | Dépendances automatiques des modules précédents |

La cible `quest_text_views` contient l’adaptation CodeEditor en C++20. Son interface publique reste compatible C++17 : le programme principal et les moteurs n’héritent pas de l’en-tête TextEditor ou du standard C++20. `quest_plot_views` adapte les données OVR aux modèles de Plots.

`quest_imgui` est la seule instance de Dear ImGui ; elle est fournie à CyUI via `CYUI_IMGUI_TARGET`. Les backends Win32/DX11 utilisent cette même instance. Les cibles explicites remplacent `CyUI::CyUI`. Audio, AudioViews, Gallery, CEF, Web et les autres modules inutilisés ne sont pas compilés. `imgui_demo.cpp` est supprimé du build.

## Présentations remplacées

- Le dessin local des courbes, axes, grille et la navigation de leur canvas sont remplacés par `cyui::plot`. Le parsing CSV, les unités, les statistiques, la sélection des métriques, la plage commune, le curseur fixé et l’enregistrement restent dans QuestDev. Les points sont préparés et mis en cache par révision/plage/résolution, jamais réduits silencieusement par le dessin.
- Les enveloppes OVR conservent les pics et les valeurs mesurées voisines d’un manque, avec une rupture conservatrice des segments. Ce besoin est plus précis que le réducteur générique de CyUI, qui peut supprimer un bucket contenant un manque. L’adaptation produit au plus six points par pixel et reste sous la limite Plots.
- La boucle de paragraphes de discussion est remplacée par `cyui::transcript`, avec des identifiants 64 bits monotones, des caches séparés pour la fenêtre principale et la fenêtre flottante, et l’actualisation des contenus diffusés. Les fournisseurs, permissions, appels MCP et données de conversation restent dans l’app. Les demandes de liens HTTP(S) sont présentées à l’utilisateur avant ouverture externe.
- Les champs multilignes de lecture RAW/ADB/JSON sont remplacés par CodeEditor. La pagination, le budget du journal, le texte original, la capture, la pause et la politique de suivi restent dans l’app. Les raccourcis de l’app ne prennent pas Ctrl+C au panneau RAW focalisé. Une sélection RAW conserve ses octets pendant l’arrivée de nouvelles lignes.
- Le calcul local de mise à l’échelle de la capture est remplacé par `imageViewport`. WIC, les textures DirectX et leur durée de vie restent dans `Preview` ; une nouvelle capture réinitialise la vue.

Ces panneaux n’éditent aucun document métier. Il n’y a donc pas de nouvelle transaction Undo à enregistrer. Le champ de saisie de discussion conserve son édition ImGui et son Undo ; le moteur CodeEditor est explicitement en lecture seule. Les gestes des courbes et des images modifient uniquement l’état de vue.

## Composants spécialisés conservés

`Diagnostics::logView` ne propose pas les six niveaux Android, l’attribution par durée de vie des PID, PID/TID, les colonnes triables, les onglets par package ou le RAW original. Le tableau spécialisé et `LogStore` sont conservés ; leur surface RAW utilise le module commun.

`Assets::assetBrowser` ne porte pas les colonnes taille/date, les règles Android/Windows ni le lot de transferts PC/Quest. Les listes spécialisées et la file restent dans `FileWorkspace` ; les propriétés et contrôles utilisent Core. Les formulaires déjà basés sur `beginProperties` restent dans Core ; aucun Inspector artificiel n’est ajouté. Aucun besoin Web, CEF, Unreal, Audio, Diff, Timeline ou édition de courbes n’a été identifié.

Le Markdown de DocumentViews est un sous-ensemble natif, sans HTML ni scripts. Code/CodeEditor incluent le catalogue lexical commun et ses dépendances Lexilla ; cette révision de CyUI n’expose pas une sélection de lexers par application.

## Dépendance reproductible

```powershell
python tools/build.py --test --build-dir build/cyui-migration --cyui ../CyUI
# Ou le wrapper PowerShell :
.\build.ps1 -Test -CyUIRoot ../CyUI -BuildDirectory build/cyui-migration
```

Le script lit l’objet Git exact et matérialise les sources dans `build/dependencies/CyUI-<commit>`. Il ne modifie pas le dépôt partagé et ne compile pas ses modifications de travail. Le cache inclut les empreintes de tous les fichiers ; une altération est refusée. Une archive Source fournit le même snapshot avec les notices TextEditor, Lexilla, Scintilla et SciTE.

En CMake direct, omettre `CYUI_ROOT` récupère le commit par FetchContent. Un dépôt Git privé nécessite l’authentification Git habituelle. Pour un build hors ligne, fournir le snapshot ou un checkout propre de la révision épinglée avec `-DCYUI_ROOT=<dossier>`.

`tools/build.py` écrit `UI-BUILD.json`, avec la révision, les modules et le SHA-256 de l’exécutable. Le packager refuse un exécutable ancien ou un manifeste incohérent. Utiliser `tools/package_release.py --build-dir build/cyui-migration --cyui <snapshot> --output <nouveau-dossier>` après revue des sources à livrer.

## Langue, densité et DPI

Le catalogue de l’application complète celui de Core pour les vues migrées et les contrôles communs. Les labels utilisent des identités traduites stables ; chemins, logs, métriques et messages de conversation restent des données. Les anciens libellés et messages de validation propres à l’app restent disponibles en français. **Réglages → Apparence → Langue** propose EN/FR. Une nouvelle configuration démarre en anglais ; une configuration française antérieure conserve le français. Langue et densité sont sauvegardées.

Les cinq thèmes et les deux densités restent disponibles. Le host traite `WM_DPICHANGED`. Les dimensions des conteneurs dont l’API attend des unités de conception ne reçoivent plus une seconde mise à l’échelle `dp()`.

## Vérification

```powershell
python tools/build.py --test --build-dir build/cyui-migration
python tools/verify_cyui_migration.py --build-dir build/cyui-migration
$env:QUESTDEV_BUILD_DIR = (Resolve-Path build/cyui-migration).Path
.\runtime\python\Scripts\python.exe tools/verify_metrics.py
.\runtime\python\Scripts\python.exe tools/verify_native.py --fake
.\runtime\python\Scripts\python.exe tools/verify_settings.py --output build/nouveaux-profils-de-test
```

Les tests natifs requièrent l’absence d’une autre instance non démonstration ; utiliser un profil `LOCALAPPDATA` de test. Ils n’appellent aucun fournisseur IA. Le mode ADB simulé ne change pas l’état d’un casque.

Validation locale : Windows x64 / MSVC 2022, neuf suites C++ (303 contrôles rapportés), Core/SDK/MCP et CSV avec ADB simulé. La matrice produit 69 rendus synthétiques : RAW, courbes, transcript, aperçu, ADB et JSON dans cinq thèmes/deux densités ; cas complémentaires à 125 % et 150 % et logs comparés. Les tests ImGui vérifient le suivi, la sélection/copie RAW, le focus clavier, les mises à jour diffusées, la conservation des IDs lors du changement de langue, le zoom et la réduction des métriques. Quatorze tests Python Core et quatre démarrages avec préférences sauvegardées passent. La copie des 208 fichiers CyUI nécessaires conserve ses empreintes ; les essais refusent un fichier altéré, un fichier supplémentaire et le packaging d’un ancien binaire. Linux/macOS ne sont pas des plateformes prévues par le host actuel et n’ont pas été testés.
