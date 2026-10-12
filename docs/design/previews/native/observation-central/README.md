# Observation — version 1.18.7

Fenêtre native en français à 1440 × 960, 1100 × 800 et 900 × 700.
La page centrale contient tous les blocs, sans sidebar :

- `observation-central-top-*` : fiche du lieu à gauche sur 25 %, carte jour/nuit
  à droite sur 75 %. Paris par défaut ; horloge du lieu à l’instant exploré et
  horloge réelle du Mac clairement distinguées.
- `observation-central-controls-*` : heure/date, choix du fuseau de référence,
  curseur continu, liste des objets visibles et accès au catalogue/favoris.
  Le premier objet de la liste calculée est sélectionné automatiquement.
- `observation-central-visibility-*` : carte de visibilité à gauche sur 75 %,
  boussole du ciel à droite sur 25 %. Les petits rendus utilisent N/E/S/O ;
  le libellé de direction complet et la hauteur restent au-dessus.
- `observation-central-curves-*` : hauteur et nuages sur deux moitiés égales,
  avec absence de données explicite dans le scénario de décembre.
- `observation-central-coordinates-900` : coordonnées décimales saisies avec
  des virgules dans la fenêtre, avant application. Le lieu affiché derrière
  conserve ses valeurs précédentes jusqu’à « Appliquer la position ».
- `observation-central-favourites-900` : lieu personnalisé en tête du menu,
  puis noms triés alphabétiquement. La recherche du fuseau est désactivée dans
  ce contrôle hors réseau ; l’avertissement UTC reste visible.
- `observation-central-forecast-*`, `clouds-*` et `offline-900` : réponse
  Open-Meteo enregistrée pour Paris, prévision horaire et résultat conservé
  après perte de connexion. Il s’agit d’un contrôle du cache, pas d’une mesure
  nouvelle. Les données manquantes ne valent jamais 0 %.
- `window-*`, `quit-*`, `restored-window` : traitement et reprise sur Orion HDR
  1024 × 768, dérivé de l’image réelle ; pixels et exports contrôlés.

Les calculs restent locaux. La résolution du fuseau d’un point utilise uniquement
ses coordonnées et le paramètre `timezone=auto` documenté par
[Open-Meteo](https://open-meteo.com/en/docs). Le test en ligne séparé vérifie
Europe/Paris et la conservation exacte de l’UTC, puis l’annulation de l’application
d’un résultat devenu obsolète. Les villes, les lieux favoris et les coordonnées
manuelles gardent le fuseau configuré ; aucune intégration d’opérateur n’est ajoutée.
