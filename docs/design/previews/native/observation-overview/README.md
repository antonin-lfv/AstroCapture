# Observation — version 1.18.5

Captures de la fenêtre native en français, à 1440 × 960, 1100 × 800 et 900 × 700.
La courbe de hauteur, la boussole, la carte et les nuages sont sur la même page.
La commande horaire du haut reste visible pendant le défilement. Les noms de
capture désignent la partie affichée, sans navigation entre vues dans le produit.

- `observation-overview-*` : courbe de M42 et créneau d’enregistrement.
- `observation-overview-sky-*` et `map-*` : cartes sur deux colonnes en grande
  fenêtre, puis une colonne en fenêtre étroite, avec la légende solaire.
- `observation-overview-now-map-*` : vrai instant de l’horloge système, carte
  jour/crépuscule/nuit et bouton Maintenant actif.
- `observation-overview-object-map-900` : couche de visibilité de M42 ; le titre
  et la légende suivent la couche, sans modifier le temps.
- `observation-overview-remote-*` : 18 h UTC le 12 décembre 2026, soit 19 h à
  Paris et 5 h le lendemain à Sydney. Le changement de lieu conserve l’UTC.
- `observation-overview-clouds-*` : vraie réponse de prévision enregistrée pour
  Paris, avec courbe horaire, source et date. Ces valeurs contrôlent l’affichage
  du cache ; elles ne sont pas une nouvelle mesure de la météo actuelle.
- Les autres captures vérifient les recommandations dépliées, le choix unique
  d’objet, la nuit refusée et le signalement d’une perte de connexion.

Les positions de M42, de la Lune et du Soleil sont calculées par le moteur local.
Le jour suit le bord supérieur du Soleil avec réfraction standard au niveau de
la mer ; la nuit est astronomique (centre sous −18°). La résolution régionale de
la carte est de 6°. Aucun pourcentage nuageux manquant n’est remplacé par zéro.
