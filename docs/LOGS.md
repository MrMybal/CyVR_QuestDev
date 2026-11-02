# Journal : suivi, RAW et tri

Dans **Console**, chaque vue, y compris en comparaison côte à côte, propose **Colonnes** et **RAW**. Les filtres, la pause et les limites de rétention s’appliquent aux deux modes.

## Suivre les nouveaux messages

Le suivi s’arrête quand vous remontez dans le journal. Revenir manuellement en bas réactive le suivi. L’interrupteur **Suivre le direct** permet aussi de rejoindre les dernières lignes. La pause suspend seulement l’affichage ; la capture continue dans les limites du journal.

## Copier le texte RAW

**RAW** affiche la ligne originale reçue de Logcat : horodatage, espaces, PID, TID, niveau, tag et message. Sélectionnez du texte à la souris, puis utilisez **Ctrl+C**. **Ctrl+A** sélectionne la page RAW affichée. La sélection fige le texte pour éviter qu’un nouvel arrivage déplace les caractères sélectionnés ; **Libérer la sélection** reprend son actualisation.

La vue RAW utilise des pages de 10 000 lignes ou 2 Mio maximum pour conserver une sélection réactive même avec une longue capture. **Précédentes** et **Suivantes** permettent de parcourir les lignes retenues. Le bouton **Copier toutes les lignes filtrées** et l’export texte incluent toute la vue filtrée, même si elle dépasse une page. Le texte RAW suit l’ordre de réception. Un journal exporté en JSON Lines conserve sa structure habituelle.

## Classer les colonnes

Cliquez un en-tête pour choisir le tri croissant ou décroissant. Maintenez **Maj** en cliquant pour ajouter une clé de tri. Les colonnes Heure locale, Niveau, PID / TID, Application, Tag et Message sont triables. Les niveaux suivent leur gravité ; PID et TID sont comparés comme des nombres. Les égalités conservent l’ordre de réception.

Le copier et l’export du tableau respectent son tri. Le suivi du direct est disponible avec **Heure locale croissante** ; les autres tris restent stables pour l’analyse.
