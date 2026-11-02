# Validation

## Alpha 0.8.1-1 — journal RAW, tri et rapports OVR

Validation du 2 novembre 2026 : huit suites C++ passent (289 contrôles rapportés), dont des tests ImGui sans fenêtre du suivi, du retour manuel en bas, de l’arrivée de nouvelles lignes RAW et du copier-coller Ctrl+A/Ctrl+C. Le tri couvre les nombres, la gravité, les textes et les clés secondaires. La conservation des lignes originales est incluse dans le budget de rétention.

La découverte OVR couvre les racines actuelle et historique, les sous-dossiers, les extensions majuscules et les erreurs d’accès. Un test exécute le vrai script shell dans des dossiers fictifs contenant espaces et apostrophes. Les tests ADB simulés vérifient l’enregistrement, les courbes, le diagnostic sans sélection et la restauration du CSV. Les tests natifs Core/SDK/MCP vérifient les imports/exports, le CSV qui grandit, les permissions, les annulations et la fermeture propre. Quinze captures des vues RAW, triées et métriques dans les cinq thèmes ; inspection visuelle mid-grey, navy et light-grey. Les 14 tests Python Core et 8 tests updater passent.

Aucun Quest physique n’était connecté pendant cette validation. Le comportement exact de publication des rapports sur le casque de l’utilisateur reste à confirmer ; aucune application VR n’a été lancée ou arrêtée.

## Alpha 0.8.0-1 — journal, applications et transferts préparés

Validation du 31 octobre 2026 : build Release, 228 contrôles C++, 14 tests Python Core et 8 tests updater réussis. Les suites C++ couvrent la rétention modifiable, l’attribution après redimensionnement, les listes PC, les noms et collisions Windows, les transferts préparés séquentiels, le lien au casque, la reprise, les annulations et la finalisation atomique. Un test dédié vérifie qu’un processus ADB parallèle ne peut pas conserver un handle local étranger. Les tests Core et updater restent disponibles.

30 rendus et 5 nouvelles captures finales de la liste : `tools/verify_ui.py --features` rend les applications, réglages de logs, Device Actions, fichiers simples, fichiers côte à côte et lots préparés dans les cinq thèmes. Inspection visuelle navy, mid-grey et light-grey avec des données de démonstration. Quatre démarrages dans trois profils isolés : `tools/verify_settings.py` vérifie les réglages enregistrés, leurs bornes, les valeurs invalides et leur relecture au redémarrage dans des profils isolés. `tools/verify_native.py --fake` vérifie les opérations et permissions, le catalogue de 38 outils MCP, les fichiers, l’annulation et la fermeture propre sur le runtime réel avec ADB simulé.

Ces vérifications ne déclenchent aucun changement des paramètres d’un Quest réel. Les commandes des interrupteurs reprennent les actions déjà présentes ; l’état inconnu et une demande non vérifiée sont explicitement distingués d’un état confirmé. Les copies de dossiers restent hors périmètre et les listes préparées sont conservées dans la session courante.

La transition d’une distribution antérieure peut être testée avec `python tools/verify_updates.py --product <nouveau-produit> --previous <produit-précédent> --output <nouveau-dossier>`, puis l’installateur avec le scénario de [RELEASE.md](RELEASE.md).

## Alpha 0.7.0-2 — installateur Windows

Validation du 31 octobre 2026 : build Release et 178 contrôles C++ réussis ; les 8 tests updater passent. Le test du worker lit désormais `VERSION` au lieu de figer une ancienne Alpha.

L’installateur Inno Setup a été vérifié avec un identifiant et des raccourcis Windows de test distincts : installation par utilisateur sans élévation, chemins avec espaces et accents, menu Démarrer/Bureau, inventaire et SHA-256 de tous les fichiers installés, réinstallation et refus d’un dossier personnel non dédié. Le compilateur officiel 6.7.4 est préparé en mode portable après vérification de son empreinte épinglée et de sa signature Authenticode.

Le test effectue ensuite une mise à jour, une restauration et une nouvelle mise à jour avec le bootstrap embarqué et de vrais redémarrages de l’app (Core et worker prêts). Un composant fictif ajouté par la nouvelle version permet de vérifier sa suppression par le désinstalleur d’origine. La désinstallation retire aussi les caches Python des modules livrés, l’entrée Windows et les raccourcis ; les fichiers personnels et préférences fictives restent présents. Aucun dossier personnel n’est supprimé récursivement. Le Setup de distribution est compilé depuis le même payload vérifié ; seul l’identifiant et le nom de l’installation de test diffèrent.

Reproduire : `tools/setup_installer.py`, `tools/build_installer.py --test` puis `tools/verify_installer.py` ; paramètres dans [RELEASE.md](RELEASE.md). Le Setup est livré sans signature de code Cyberalien.

## Version 0.7 — mises à jour GitHub

Validation du 31 octobre 2026. Build Release réussi, 178 contrôles C++ et 14 tests Python Core réussis. Le test CSV temporisé a été relancé isolément après contention avec les captures UI ; la suite complète suivante passe.

- **8 tests updater** : ordre des versions et canaux, redirections sans transmission du jeton au CDN, téléchargement incomplet/altéré, chemins ZIP et doublons Windows, manifeste et SHA-256, conservation des fichiers personnels, refus des sources et fichiers gérés modifiés, identité du processus parent, remplacement réel et restauration après échec du nouvel exécutable. Le protocole du worker vérifie aussi les préférences et son état.
- **47 rendus UI**, dont la page des mises à jour dans les cinq thèmes ; inspection navy/light-grey.
- **Transaction portable réelle** sur copies jetables : bootstrap Python embarqué déplacé, attente du parent, remplacement, redémarrage Win32 avec Core et worker disponibles, sauvegarde et restauration explicite. Les préférences personnelles et fichiers ajoutés après installation sont encore présents après restauration.
- Accès au catalogue privé GitHub validé avec la connexion Git existante. Stockage/lecture/suppression d’un jeton fictif dans une entrée Windows de test unique.

Reproduire : tests C++ et Core habituels, `python -m unittest discover -s updater/tests -v`, `python tools/verify_ui.py`, puis `python tools/verify_updates.py --product <dossier-portable> --output <nouveau-dossier>`. Les essais ne remplacent pas le checkout ni l’installation utilisée par l’utilisateur.


## Alpha 0.6.0-2 — identité de l’application

Validation du 31 octobre 2026 : build Release réussi, logo affiché dans les cinq thèmes (inspection navy et light-grey), PNG embarqué identique à l’asset approuvé et groupe d’icônes Windows en dix résolutions. Une fenêtre réelle expose les icônes grande et petite. Les 178 contrôles C++ existants passent. Le README utilise le même PNG et une capture de démonstration actualisée.

## Version 0.6 — mirroring vidéo/audio

Validation du 30 octobre 2026, build Release Windows x64.

- **178 contrôles C++** réussis, dont 29 consacrés au mirroring : paramètres, audio, recadrage, chemin MKV Unicode, protection contre remplacement, environnement ADB enfant, acquittement après texture/fenêtre, erreur encodeur, annulation du démarrage, arrêt et fermeture avec finalisation.
- **14 tests Python** sur le vrai Core, dont permission du mirroring, bornes vidéo/audio et refus des paramètres étrangers.
- **42 rendus UI** réussis, dont le panneau de mirroring dans les cinq thèmes ; inspection navy et light-grey.
- Vérification native Core/SDK et 38 outils MCP : import, lecture et export métriques, suivi CSV et confirmation/annulation des actions.
- **Quest 3 réel** : flux H.264 à **1600 × 864, environ 60 FPS**, décodage matériel Direct3D 11, piste AAC présente dans le MKV et fichier finalisé après arrêt. Aucun jeu lancé et aucun changement des réglages réseau/alimentation pendant ce test. Le test confirme la présence de la piste audio ; il ne mesure pas sa qualité audible ni sa disponibilité dans chaque application.

Reproduire : `python tools/build.py --test`, tests Python, `python tools/verify_ui.py`, puis `runtime/python/Scripts/python.exe tools/verify_metrics.py`. Le test matériel facultatif est décrit dans [MIRRORING.md](MIRRORING.md). Les rendus et essais matériels restent des fichiers locaux exclus du dépôt.


## Version 0.5 — courbes et enregistrements CSV OVR

Validation effectuée le 29 octobre 2026, build Release Win32/DX11.

- **149 contrôles C++** réussis : 39 moteur/backend, 36 plugins, 38 CSV/métriques, 36 fichiers. Les nouveaux contrôles couvrent CSV dynamiques (80 colonnes), timestamps en millisecondes, guillemets/CRLF/BOM, valeurs absentes, pics à faible résolution, fichiers incomplets, import/export asynchrones, enregistrement progressif, mode CSV préexistant, arrêt, annulation, changement de casque et fermeture normale. Un CSV ne contenant que l’en-tête est distingué d’un rapport avec mesures.
- **13 tests Python** réussis avec CyToolsCore réel : nouveaux contrats, limites et permissions, import CSV automatiquement copié avec original préservé, fichiers et dossiers de session privés appartenant au job.
- **37 rendus UI** réussis, dont Métriques dans les cinq thèmes. Inspection visuelle navy/light-grey : courbes continues, pic conservé, sélection latérale et commandes visibles.
- `tools/verify_metrics.py` : application native, vrai Core HTTP/SDK et MCP officiel, ADB simulé. Import, catalogue, plage temporelle, valeurs absentes `null`, export durable après FinishTask, découverte des rapports, suivi d’un CSV qui grandit (ligne partielle différée), pause, confirmation d’enregistrement annulée avant exécution, fermeture et retrait de la connexion. Le catalogue MCP contient **38 outils**.
- **Quest 3 réel** : lecture du mode CSV, enregistrement de deux secondes, finalisation et vérification que le mode CSV initial est restauré. Aucun rapport CSV n’a été produit pendant cette fenêtre. Cette vérification confirme les commandes et le nettoyage matériel ; la réception de mesures d’une application VR et la cadence de flush sur ce firmware restent à vérifier.

Reproduire : `python tools/build.py --test`, `runtime/python/Scripts/python.exe -m unittest discover -s assistant/tests -v`, `python tools/verify_ui.py`, puis `runtime/python/Scripts/python.exe tools/verify_metrics.py`. Le dernier test nécessite qu’aucune autre instance QuestDev ne possède la connexion Core. Test matériel facultatif, application VR déjà ouverte : `quest_metrics_tests.exe --live <adb.exe> <serial>` depuis le dossier de build ; il active le CSV pendant deux secondes et restaure son état initial.

## Version 0.4 — plugins OVR Metrics et CyInsightOVR

Validation effectuée le 29 octobre 2026, build Release Win32/DX11.

- **111 contrôles C++** réussis : 39 moteur/backend, 36 plugins, 36 fichiers. Les nouveaux contrôles couvrent schémas et bornes, métriques inconnues/dupliquées, limites de sélection, packages exacts, erreurs métier malgré ADB=0, LOG_STATE fragmenté/récent, rejet des états périmés et des réglages ignorés, identité des captures et séparation du résultat complet/résumé IA.
- **10 tests Python** réussis avec CyToolsCore réel, dont fichier `.cyovr` privé, propriété du job et suppression à FinishTask.
- **32 rendus UI** réussis : vues existantes, les deux panneaux de plugins dans les cinq thèmes, onglets connexion/rapports et état. Inspection visuelle navy, light-grey et mid-grey ; captures supplémentaires amber-night/violet.
- `tools/verify_plugins.py --fake` : vraie application, vrai Core HTTP/MCP, ADB simulé. Schémas et catalogue, lecture des états, récupération d’une capture de fixture en fichier Core, confirmation puis annulation des mutations, fermeture sans popup résiduel et suppression de la connexion.
- `tools/verify_plugins.py --live` : **Quest 3 réel**, deux applications installées. SDK natif : 193 packages, états OVR et CyInsightOVR, catalogue HUD ; contrôle des paramètres et confirmation/annulation avant exécution. MCP officiel : 35 outils, lecture du contexte, répertoire et état OVR ; fermeture propre.
- Le collecteur officiel CyInsightOVR a été initialisé sur le Quest par le code du plugin, avec appariement privé et réponse `cyinsightovr.bridge/2`. Les compteurs système réels sont reçus. Le collecteur reste actif jusqu’au redémarrage du casque.

Ces vérifications matérielles n’ont pas appliqué de profil HUD, déclenché de capture GPU ou redémarré de jeu. L’application des réglages et ses refus ont été vérifiés par fixtures ; leur visibilité physique et une nouvelle capture matérielle restent à contrôler dans le casque. Aucun fournisseur IA n’a reçu les données de ces tests ; les parcours SDK/MCP sont locaux.

La validation réelle a révélé le double échappement du script dans `adb exec-out sh -c`. Le constructeur envoie désormais l’argument brut, qu’ADB échappe lui-même. Un test reproduit cet échappement automatique, et la lecture réelle de `/sdcard` retourne 15 entrées. Les vérifications de mutation de fichiers restent celles des fixtures POSIX.

Sauvegarde avant évolution : `runtime/backups/pre-metrics-20261029-220931.zip`, SHA-256 `d7484121671fd068202c9eacd6ac08e215c3b9d6c5fe633bf9c43a2d96f386a8`.

Reproduire : `python tools/build.py --test`, `runtime/python/Scripts/python.exe -m unittest discover -s assistant/tests -v`, `python tools/verify_ui.py`, puis `runtime/python/Scripts/python.exe tools/verify_plugins.py --fake` ou `--live` (collecteur initialisé et deux apps installées). Les vérificateurs natifs nécessitent qu’aucune autre instance QuestDev ne possède la connexion Core.

## Version 0.3 — gestionnaire de fichiers

Validation effectuée le 29 octobre 2026. Le Quest était déconnecté pendant cette itération ; les vérifications suivantes sont locales.

- **74 contrôles C++** réussis : 39 du moteur/backend existant et 35 consacrés aux fichiers. Les commandes réelles ont été exécutées avec le shell POSIX de Git dans des fixtures du dossier de build : noms Unicode, espaces/apostrophes, métacaractères littéraux, création, renommage sans écrasement, refus d’un dossier non vide, envoi avec temporaire, remplacement explicite et nettoyage après annulation. Une source de renommage ou un temporaire d’envoi manquant échoue sans faux succès.
- **9 tests Python** réussis sur CyToolsCore 0.10.1 : contrats et permissions, annulation avec acquittement, isolation, import automatique d’un fichier externe, préservation de l’original, téléchargements privés, suppression des fichiers temporaires et adaptation des noms aux contraintes Windows.
- **18 rendus UI** réussis, dont le gestionnaire de fichiers dans les cinq thèmes CyUI. Inspection visuelle des thèmes navy et light-grey.
- `tools/verify_native.py --fake` : vraie application native, vrai Core HTTP/MCP, faux ADB. Catalogue de **35 outils MCP** ; invocation du contexte et de `list_files` par le client MCP officiel ; pagination, dossier refusé, téléchargement, export SDK durable, confirmation/annulation des écritures, préservation de la source locale, fermeture sans popup résiduel et retrait de la connexion.

Les tests n’ont pas transféré ni supprimé de fichiers sur un casque réel. Ils ne valident pas les permissions ni les utilitaires du firmware actuellement installé. Les nouvelles opérations n’ont pas fait l’objet d’un appel de génération IA réel ; leur protocole MCP et leur exécution native ont été testés séparément.

Sauvegarde du code avant évolution, sans Git : `runtime/backups/pre-v03-20261029-210107.zip`, SHA-256 `395b134020d8a827beb84b0551c958538262762122b48a2107e1f102d395c230`.

## Version 0.2 — historique

Validation sur Windows x64, le 29 octobre 2026.

### Automatisation

- Build Release C++17/Win32/DX11 : compilation et **39 contrôles C++** réussis. Attribution inter-apps/PID réutilisés/UID partagé, mémoire et JSONL, reconnexion Logcat, arguments Windows/Android, manifestes/ports de plugins, `--no-rebind`, stdout binaire distinct de stderr, concurrence, annulation avec nettoyage et codes d’erreur.
- **5 tests Python** sur le vrai CyToolsCore 0.10.1 : catalogue, paramètres, permissions, appels natifs, exports privés, isolation des acteurs, destruction des fichiers en fin de tâche et attente de l’acquittement natif après annulation.
- `pip check` : aucune dépendance cassée. Versions dans `runtime/requirements.lock`.
- `tools/verify_ui.py` : **13 rendus**, cinq thèmes, comparaison et sept pages. Détection des captures vides et inspection visuelle des pages Appareil, Applications et Assistant.

### IA et MCP réels

`tools/verify_assistant.py --provider` lance un compagnon isolé avec deux constantes fictives, sans accès aux données du Quest. Vérifications : compte Codex authentifié, six modèles découverts, vraie réponse progressive, appels `get_context` et `read_logs` via outils dynamiques et jobs Core, restitution exacte des marqueurs, découverte de **31 outils MCP**, invocation par le SDK MCP officiel, nettoyage des tâches et fermeture.

Le test a permis de corriger un ancien modèle par défaut incompatible et l’encodage UTF-8 des pipes Windows. Les vraies données du casque n’ont pas été envoyées au fournisseur pendant ces vérifications.

### Quest 3 connecté

Le test local a constaté **193 packages** et plus de **4 000 logs** pendant sa fenêtre d’observation. `tools/verify_native.py` vérifie contre l’app ouverte : contexte, applications, logs, export privé, plugins, navigation, demande de lecture ADB en attente de confirmation puis annulée avec acquittement, rendu et fermeture propre. L’inspection visuelle a détecté puis fait corriger un popup resté vide après annulation.

`tools/verify_device.py` a produit sur ce Quest : PNG avec signature valide, trace Perfetto de deux secondes (environ 3,36 Mo), MP4 finalisé après arrêt anticipé avec structures `ftyp` et `moov`. Les fichiers distants uniques du test ont été retirés. Les captures locales restent dans `captures/`, exclu du suivi.

La version 0.1 avait aussi vérifié la séparation de `com.cyvr.player` : cinq lignes du player, **zéro ligne étrangère** dans sa vue. Les tests C++ couvrent cette attribution indépendamment des apps installées.

### Limites de vérification

Les actions GPU, HUD/CSV, Boundary, proximité, veille, redémarrage et passage Wi-Fi/USB sont implémentées ; leurs changements n’ont pas été déclenchés sur ce casque pendant cette validation. Les commandes HUD/CSV suivent la [documentation Meta OVR Metrics](https://developers.meta.com/vr/documentation/native/android/ts-ovrmetricstool/). L’état de proximité a été lu via le service système réel.

Installations, lancements, arrêts et tunnels de plugin ont été vérifiés avec le faux ADB. Aucun service compagnon TCP propriétaire n’était disponible pour vérifier son protocole applicatif. Les autres profils IA nécessitent leurs comptes/serveurs et n’ont pas fait l’objet d’une génération réelle ici. Leur discussion intégrée est textuelle ; Codex et les clients MCP externes disposent des outils QuestDev.

L’aperçu repose sur `screencap`, sans audio ni promesse de faible latence. Les tests de capture valident les fichiers et conteneurs ; ils ne mesurent pas leur qualité visuelle ou la fluidité dans chaque application.
