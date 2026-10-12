# Observation et fenêtre — version 1.18.8

Composition QML de l’application en français à **1440 × 960, 1100 × 800 et
900 × 700**, avec les contrôles réels. La barre native est vérifiée séparément
sur macOS avec AppKit ; les autres captures utilisent le moteur Qt logiciel.

- `observation-polish-top-*` : lieu et heure empilés à gauche, carte jour/nuit
  et objet à droite. Les menus de recommandations et d’objet sont alignés.
- `observation-polish-controls-*`, `time-dialog-900` et `night-choice-900` :
  heure, calendrier/fuseau en fenêtre, recommandations de toute la nuit dans
  le même sélecteur d’objet. Les deux sections redondantes du bas ont disparu.
- `observation-polish-utah-*` : position en Utah avec fuseau America/Denver,
  même instant UTC et prochains lever/coucher convertis à l’heure du Mac.
  Le Mac est ici dans Europe/Paris ; le changement de date est visible.
- `observation-polish-visibility-*` et `curves-*` : proportions 75/25 puis
  50/50, direction lisible et créneaux de lancement toujours accessibles.
- `observation-polish-coordinates-900`, `favourites-900` : saisie en fenêtre,
  confirmation explicite, favoris avant les lieux proposés. Le fuseau d’un
  point non confirmé reste signalé ; ce scénario fonctionne hors réseau.
- `observation-polish-forecast-*`, `clouds-*`, `offline-900` : prévision
  Open-Meteo enregistrée pour Paris et conservation du résultat après perte
  de connexion. Ce sont des données de test du cache, pas une mesure récente.
- `window-*`, `quit-*`, `restored-window` : Orion HDR 1024 × 768, dérivé de
  l’image réelle. La session est sauvegardée depuis Accueil, puis restaurée
  dans Ciel profond. Les pixels flottants et les exports restent identiques.
- `native-titlebar-900` : cadre macOS réel avec fond sombre, boutons natifs
  et titre centré. Le test vérifie aussi le centrage après redimensionnement
  à 1100 × 800 et 1440 × 960, sans dessiner le contenu Qt sous les boutons.
  Le contenu uni de cette capture sert uniquement au contrôle du cadre.

Le pied de fenêtre affiche **1.18.8** au centre, indépendamment des longueurs
du message d’état et du nom de caméra. Le numéro n’est plus dans le titre.

Les 53 tests Release passent. Les contrôles supplémentaires sur Orion et les
510 messages actifs de Main, Observation, contrôleur et Guide passent également.
