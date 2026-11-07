---
id: troubleshooting
title: Dépannage
description: Diagnostiquez l'autorisation du casque, les logs manquants, les CSV OVR et les captures.
section: Assistance
order: 90
sourceRevision: 1
---

## Aucun casque utilisable {#connection}

Vérifiez que le câble USB transporte les données, que le mode développeur est actif et que le casque a autorisé le débogage USB de ce PC. Vérifiez `adb.exe`, actualisez avec F5 et choisissez le bon appareil. `unauthorized` demande une autorisation dans le casque ; `offline` n'est pas une connexion utilisable. En Wi-Fi, contrôlez l'adresse et le transport réellement courants avant de réessayer.

Ne confondez pas le mode démonstration avec une connexion réelle. Les données de démonstration sont simulées et les actions peuvent être désactivées. Consultez la [première connexion](doc:quickstart#connect).

## Journaux vides ou inattendus {#logs}

Lancez l'application cible et vérifiez le package exact, la catégorie active/installée/système et les filtres. Consultez Système/non attribué lorsqu'une origine ne peut pas être établie. La capture commence au présent ; une app silencieuse peut n'avoir aucun message. Les lignes retirées par la rétention ou manquées pendant une déconnexion ne sont pas reconstruites.

Si le suivi s'arrête, revenez en bas ou activez Suivre le direct. En RAW, libérez une sélection figée pour reprendre les mises à jour. Un tri de colonnes autre que l'heure conserve volontairement un ordre d'analyse stable. Consultez [Console](doc:console#follow).

## Aucune courbe OVR {#metrics}

Installez OVR Metrics Tool, laissez tourner une application VR, actualisez les rapports et contrôlez le mode CSV et la sélection des statistiques dans le plugin OVR. Consultez les refus d'accès plutôt que d'interpréter une liste vide comme un succès. Le firmware peut publier le CSV seulement à la fin de la session VR ; l'activation CSV ne garantit pas du direct. Un CSV local permet de vérifier les courbes indépendamment de la collecte sur casque.

Les compteurs manquants ne sont pas inventés. Vérifiez le format CSV, l'ordre des timestamps et les [limites d'import](doc:metrics#csv). Reconnectez et relisez l'état du plugin après un enregistrement interrompu pour contrôler sa configuration CSV.

## Image, son ou capture complète manquants {#media}

Pour le mirroring, consultez le journal, l'appareil ADB choisi et les éventuels refus de recadrage. Réduisez débit ou résolution si la liaison sature. La duplication audio nécessite Android 13+ et une app autorisant la capture ; le silence peut être propre à l'application. Un arrêt forcé peut laisser un MKV incomplet. Vidéo Android MP4 et MKV de mirroring sont deux modes distincts.

Pour CyInsightOVR, vérifiez l'initialisation du collecteur, le package exact et la correspondance du `capture_id`. Un diagnostic sans tuiles n'est pas une capture mesurée exportable. Relisez l'état après un changement d'outil HUD. Consultez [Mirroring](doc:mirroring#audio) et [Plugins](doc:plugins#cyinsight).

## Commandes, transferts ou actions IA refusés {#commands}

Lisez la sortie et le code de retour de la tâche dans ADB. Les permissions Android et capacités du firmware s'appliquent toujours. Une action annulée peut avoir déjà modifié l'état ; relisez-le. Pour les fichiers, contrôlez l'autorisation de remplacement, les collisions de destination, la sélection de fichiers réguliers et les contraintes de noms Windows. Reconnectez le casque d'origine avant de relancer un lot suspendu.

Pour l'IA, appliquez explicitement le profil après un changement de modèle ou d'URL. Vérifiez le CLI/compte ou modèle local nécessaire, le contexte partagé et la politique d'action. Pour les outils externes, gardez QuestDev ouvert et régénérez la configuration MCP après son redémarrage. Exportez les fichiers des jobs avant leur nettoyage.

Pour signaler un problème reproductible, indiquez version QuestDev, casque/firmware, étapes et erreurs utiles dans le [suivi des problèmes](https://github.com/MrMybal/CyVR_QuestDev/issues). Relisez les journaux et retirez les informations de compte ou identifiants avant de les partager.
