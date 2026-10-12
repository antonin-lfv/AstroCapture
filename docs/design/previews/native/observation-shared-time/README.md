# Observation — version 1.18.9

Captures des composants QML réels en français, à **1440 × 960, 1100 × 800 et
900 × 700**, avec le moteur Qt logiciel et des préférences de test isolées.

- `observation-shared-top-*`, `controls-*`, `compass-*` et `visibility-*` : colonne
  de 25 % à gauche (lieu, heure, boussole), colonne de 75 % à droite (jour/nuit,
  objet, visibilité). Les courbes de hauteur et de nuages restent en 50/50 dessous.
- Le bandeau **Chez moi / Sur place** reste au-dessus du défilement et affiche
  toujours l’instant choisi dans les deux fuseaux. La fiche du lieu reprend ces
  mêmes heures ; aucune horloge réelle indépendante ne remplace la conversion.
- `observation-shared-utah-*`, `utah-cursor-900` et `recording-900` : lieu en Utah
  avec America/Denver et Mac dans Europe/Paris. Après un clic sur la courbe,
  **07:12 sur place correspond à 15:12 chez moi** le 13 décembre 2026. Les
  créneaux indiquent d’abord l’heure locale du Mac, puis celle du télescope,
  sans suffixe GMT. Ces dates choisies servent à contrôler la conversion.
- `observation-shared-no-target-now-900`, `no-target-night-900` : liste vide
  explicitement nommée et désactivée, objet précédemment choisi conservé dans
  les cartes et nommé **Objet affiché**. Le scénario de nuit utilise un seuil
  de 90° pour contrôler l’absence de recommandation ; le catalogue reste accessible.
- `observation-shared-forecast-*`, `clouds-*`, `offline-900` : prévision Open-Meteo
  enregistrée pour Paris, puis perte de connexion. Ce sont des données de test
  du cache ; elles ne constituent pas une mesure récente. Les données absentes
  restent distinctes de 0 % de nuages.
- `coordinates-900`, `favourites-900`, `time-dialog-900`, `night-choice-900` :
  fenêtres, tri des lieux et choix de la période. Un point non confirmé continue
  d’afficher **UTC provisoire**, y compris dans le bandeau des heures.
- `window-*`, `quit-*`, `restored-window` : Orion HDR 1024 × 768, traitement,
  sauvegarde depuis Accueil et reprise dans Ciel profond. Les pixels flottants
  et les exports restent identiques entre les deux processus.

Les tests couvrent les clics sur les courbes, les glissements continus, le
changement de date/heure, les conversions et les deux messages de liste vide.
La barre macOS est contrôlée séparément par AppKit ; le test respecte la surface
réellement disponible sur l’écran, tandis que les captures QML couvrent les trois
tailles demandées exactement.
