# Architecture

`main.cpp` héberge Win32, DirectX 11 et Dear ImGui. `app.cpp` dessine les contrôles CyUI et possède le modèle affiché. Le backend et le moteur de logs sont séparés de l’interface.

## Capture et attribution

1. Le worker ADB cherche les appareils toutes les trois secondes. Chaque choix d’appareil crée un nouvel identifiant de session.
2. Le worker lit les packages et leur statut système, l’heure du casque et `ps -A -o PID,UID,NAME`.
3. Un client Logcat suit les quatre buffers depuis l’heure initiale du casque, au format `threadtime,epoch,usec`, avec les niveaux verbose à fatal.
4. Le moteur enregistre les durées de vie des processus. Les événements `am_proc_start` et `am_proc_died` ajoutent ou ferment des intervalles horodatés. Une observation `ps` complète les événements toutes les deux secondes.
5. Chaque message est attribué au processus qui possédait ce PID à son timestamp. Un événement arrivé en retard peut corriger les messages déjà reçus dans son intervalle, sans déplacer ceux des précédentes durées de vie.
6. Le package est reconnu par nom exact ou par le préfixe précédant `:`. Aucun rattachement par UID commun n’est effectué.

Les formats et les événements utilisés suivent les références Android officielles : [Logcat](https://developer.android.com/tools/logcat), [options et timestamps dans AOSP](https://android.googlesource.com/platform/system/logging/+/refs/heads/main/logcat/logcat.cpp), [événements ActivityManager](https://android.googlesource.com/platform/frameworks/base/+/master/services/core/java/com/android/server/am/EventLogTags.logtags).

## Threads et limites

L’interface ne lance aucune commande ADB bloquante. Le worker effectue les requêtes et les actions ; un second thread lit le flux. Les résultats passent par une file synchronisée ; l’interface ignore les événements d’une ancienne session.

La file des lignes est limitée à 16 Mo. Les lignes de plus de 256 Ko sont rejetées. Le journal est limité à 50 000 entrées et 64 Mo comptabilisés. Ses vues utilisent des identifiants stables et un `ImGuiListClipper`, puis construisent leurs index uniquement quand le contenu ou les filtres changent. Les états de pause des deux vues sont indépendants.

Les commandes sont lancées via `CreateProcessW`, sans fenêtre de terminal. Les arguments Windows sont échappés individuellement. Les packages et composants utilisés dans les commandes distantes sont validés. Les appels ont un délai maximal, une annulation à la fermeture et une lecture de sortie bornée. Le client Logcat est arrêté sans tuer le serveur ADB partagé avec les autres outils.

L’export sérialise les lignes actuellement visibles. Le JSONL conserve le timestamp Unix en microsecondes ainsi que l’identité du processus et du package.

## Actions, plugins et assistant

Le backend accepte huit jobs ADB concurrents, indépendants de Logcat, avec session d’appareil, étapes, délai par étape, sortie progressive et annulation. Les fichiers binaires disposent d’un handle stdout séparé du stderr. Les nettoyages vidéo ciblent uniquement le PID dont la commande contient le chemin distant unique du job et précèdent l’événement de fin. Une déconnexion physique peut empêcher le nettoyage et produit un diagnostic.

`plugins.cpp` valide les manifestes et construit les actions/mappings avec `--no-rebind`. `preview.cpp` décode les PNG via WIC et crée une texture DirectX sur le thread graphique. Chaque page possède sa navigation latérale.

`metrics_plugins.cpp` ajoute deux contrôleurs de manifestes validés pour les packages exacts OVR Metrics et CyInsightOVR. Ils partagent les schémas, bornes et constructeurs de jobs entre l’UI et `plugin_action`. Les brouillons locaux et réponses du service restent séparés. Les actions sont sérialisées entre plugins ; la fin d’un job ADB est suivie de la validation de sa réponse métier avant notification de succès. OVR utilise un timestamp du casque et les tags datés de LOG_STATE. CyInsightOVR utilise ShellClient et le JSON base64, sans transférer la capacité du collecteur au PC. Son bootstrap copie le JAR sélectionné via un nom temporaire unique, puis apparie l’app sur le casque.

Les captures CyInsightOVR complètes restent séparées des résumés du contexte IA. Les exports SDK sont écrits dans `context.path('outputs/capture.cyovr')`, sous la propriété du job Core. `capture_id` distingue acceptation, attente et réception du résultat ; aucune mesure n’est déduite d’une simple acceptation. Le contexte UI ne conserve pas les chemins temporaires des jobs.

`assistant_bridge.cpp` lance le Python privé, deux pipes NDJSON UTF-8 et un Job Object Windows pour les enfants. Le compagnon `assistant/service.py` utilise le vrai runtime CyToolsCore et ses adaptateurs HTTP/MCP. Le contrat commun décrit quatorze opérations, leurs schémas et permissions. Le contexte est actualisé chaque seconde. Les lectures natives rejoignent le thread UI ; les actions utilisent les jobs ADB et la politique d’autorisation commune à IA/MCP/SDK.

`files.cpp` normalise les chemins Android, construit les commandes et décode les listes délimitées par NUL. Noms et métadonnées restent séparés, y compris avec espaces, accents et retours à la ligne. La liste binaire utilise un fichier de job ; elle est bornée à 5 000 entrées et paginée pour le MCP. Les racines protégées et réponses incomplètes sont refusées.

Les envois utilisent un temporaire unique puis un renommage sans remplacement par défaut. L’annulation retire le temporaire avant d’acquitter le job. Les téléchargements UI utilisent un `.part` et ne remplacent le fichier local qu’après succès via `MoveFileExW`. Le SDK produit ses fichiers dans la tâche privée Core. `configure_inputs` importe le champ `push_file.sourceFile` sans modifier l’original. Les requêtes de liste sont liées à l’appareil et à la navigation courante.

Le changement d’appareil invalide les demandes de l’ancienne session. Une annulation garde le handler Core actif jusqu’à l’acquittement natif. Les workers ADB sont arrêtés avant le compagnon à la fermeture. Les fichiers utilisent `context.path` et les espaces privés Core. Le MCP ouvre une tâche temporaire par connexion ; le SDK adapte le client HTTP officiel. L’assistant et le client externe possèdent des acteurs distincts ; les clients SDK partageant le même jeton restent un principal unique.

Codex utilise app-server et les outils dynamiques. Les autres transports proposent actuellement du texte contextualisé. Les clés de session ne sont pas sauvegardées dans le profil. Le fichier de connexion local est protégé par les ACL du dossier et son jeton n’est pas copié dans la configuration MCP.
