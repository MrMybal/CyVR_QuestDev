# Installer CyVR QuestDev sur Windows

Depuis les [releases GitHub](https://github.com/MrMybal/CyVR_QuestDev/releases), télécharger `CyVR_QuestDev-0.8.1-alpha.1-Windows-x64-Setup.exe`, puis suivre l’assistant en français ou en anglais. Windows 10/11 x64 est requis. Le dépôt privé nécessite un compte GitHub autorisé pour télécharger les fichiers.

L’installation est propre au compte Windows courant et ne demande pas de droits administrateur. Le dossier par défaut est `%LOCALAPPDATA%/Programs/CyVR QuestDev`. Un raccourci est ajouté au menu Démarrer ; celui du Bureau est facultatif. Le logo de l’app est utilisé pour le Setup, les raccourcis et la désinstallation. Fermer QuestDev avant de réinstaller ou désinstaller.

Le runtime Python, CyToolsCore, MCP, les plugins et scrcpy sont inclus. ADB reste externe : installer les Android SDK Platform-Tools officiels si nécessaire. Activer le mode développeur du Quest et accepter le débogage USB dans le casque.

**Réglages → Mises à jour GitHub** fonctionne aussi pour une app installée. La mise à jour conserve les composants de désinstallation et les raccourcis. La désinstallation retire les fichiers de l’app, y compris ceux ajoutés par des versions ultérieures. Les préférences sous `%LOCALAPPDATA%/CyVR/QuestDev`, captures, fichiers personnels et sauvegardes créées par l’updater restent en place. L’installateur ne supprime aucun dossier personnel de façon récursive.

La version ZIP portable reste disponible. Une Alpha déjà extraite peut être conservée ; lancer ensuite l’app depuis son raccourci Windows. Les réglages sont communs au compte Windows. Un dossier contenant une copie de sources, un lien/jonction ou d’autres fichiers sans installation QuestDev est refusé comme destination.

Le Setup n’est pas signé avec un certificat de signature de code Cyberalien. Ses empreintes et celles des ZIP sont publiées dans `SHA256SUMS.txt` de la même release.
