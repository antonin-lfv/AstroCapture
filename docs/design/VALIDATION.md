# Validation de l'interface Atelier

Direction validée le 9 octobre 2026. L'intégration utilise des composants QML partagés,
les traitements scientifiques existants et les formats de projet existants.

## Contrôle de l'application

L'interface réelle a été rendue en français avec Qt 6.9.3 sur macOS ARM64.
Accueil, Capture, Traitement, Ciel profond, Mosaïque, Historique, Guide et Paramètres
ont été inspectés à 1440 × 960 et 900 × 700. Capture a aussi été vérifiée avec la
caméra simulée de développement, sans enregistrement ni connexion à un appareil réel.

Finition a été vérifiée à 1440 × 960, 1100 × 800 et 900 × 700 avec le TIFF d'Orion
fourni, en résolution native 6244 × 4170. Les captures conservées dans `previews/native/`
montrent l'application, et non le prototype HTML.

- Navigation et barre Images / Empilement / Finition continues.
- Inspecteur de 360 px, réglable entre 320 et 480 px ; préférence conservée.
- Sélecteur de familles en fenêtre étroite ; histogramme repliable sous 1000 px.
- Réglages, histogramme et comparaison accessibles ; zone de zoom avec marges.
- Progression en surimpression : hauteur de la vue conservée pendant et après un
  calcul de finition sur Orion, sans déplacer l'image ni les commandes de zoom.
- Familles Couleur et Transformations avancées inspectées avec les vrais contrôles.
- Menus Projet et Exporter vérifiés ; préréglages de toute la finition dans Projet.
- Export PNG, TIFF ou JPEG avec légende facultative ; FITS linéaire distinct.
- Dialogue d'export compact et aperçu facultatif ; légende utilisable à 900 × 700
  sans barre de défilement affichée. Les champs restent défilables.
- Estimation native du PNG d'Orion sans ajout de légende : environ 144,9 Mo.
- Les valeurs numériques abrégées à l'affichage conservent leur précision interne.

Le TIFF source contient déjà des textes jaunes dans ses pixels. L'option de légende
concerne un ajout. Le texte proposé pour cet ajout est désormais modifiable.

## Informations préremplies de la légende

Le champ commun aux exports de Traitement, Finition et Mosaïque permet de modifier
le texte automatique. Un texte vide et l'absence de correction sont distingués :
vider le champ retire ces informations ; revenir au texte proposé réactive le
contenu calculé par l'application. Désactiver la section conserve la correction.

L'interface française a été inspectée avec Orion à 1440 × 960, 1100 × 800 et
900 × 700, avec et sans aperçu. Les captures `previews/native/caption-*.png`
montrent le champ prérempli et le texte corrigé. Le contrôle dans l'application
complète couvre une vraie saisie clavier, Cmd+Z / Cmd+Maj+Z, la fermeture puis la
réouverture de la modale et la sauvegarde / réouverture / recalcul du projet.

Les six tests ciblés de cette modification couvrent les contrôleurs de traitement
et mosaïque, les interfaces de traitement, ciel profond et mosaïque et l'estimateur
de taille. Ils vérifient la conservation du texte (y compris un champ vide), le
retour aux informations proposées, les anciens documents sans correction et la
persistance dans le projet. Le rendu corrigé est comparé à la même légende saisie
manuellement ; PNG et TIFF utilisent le même aperçu et gardent les échantillons
16 bits hors de la légende.

## Vérifications automatiques

Les 31 tests du projet ont été contrôlés. Les tests d'interface et de bibliothèque
ont été réexécutés après les derniers ajustements. Ils couvrent notamment :

- Conservation des échantillons 16 bits dans les exports PNG et TIFF, avec et sans légende.
- Conservation des données flottantes dans les exports FITS.
- Comparaison Avant / Résultat / Partagé sans modification de l'image à exporter ;
  orientation du comparatif après rotation.
- Historique de finition, familles, projets, préréglages et outils avancés.
- Taille estimée des exports et protection lors de la fermeture.
- Persistance et limites de largeur de l'inspecteur ; fichiers récents de Traitement,
  y compris les PNG et TIFF, avec chemins Unicode.
- Navigation, traitement, mosaïque, glisser-déposer et guide.

## Paquet macOS

Le script `scripts/package_macos.sh` reconstruit le paquet autonome et le DMG 1.5.0.
La vérification de signature du paquet et celle de l'intégrité du DMG sont effectuées
avant livraison. La signature reste locale (ad hoc), comme pour le paquet précédent.

## Lot 1.5.0 — sessions, géométrie et masques

35 tests passent en développement. Les nouveaux contrôles couvrent :

- Sauvegarde atomique, réutilisation des assets inchangés, annulation avant
  engagement, contrôles SHA-256, session endommagée refusée avant restauration,
  conservation exacte des flottants négatifs et HDR et du masque de bord.
- Reprise et export sans la source, avec protection de son chemin original.
- Copie exacte des pixels recadrés, rotations de ±90° comparées à l’orientation
  sans interpolation, les cinq interpolations, dimensions et annulation.
- Opacité appliquée une fois par trait, accumulation entre traits, inversion,
  flou du bord, import 16 bits et FITS flottant 32 bits, refus des mauvaises valeurs.
- Masque limité au débruitage ou à Richardson–Lucy : réglages de couleur encore
  actifs ; masque noir de finition : pixels conservés à l’identique.
- Comparaison suivant le recadrage, géométrie précédente conservée pour l’éditeur,
  et FITS linéaire exporté avec ses dimensions et valeurs d’origine.

L’application complète a été ouverte en français avec le TIFF réel d’Orion
6 244 × 4 170. Le parcours natif couvre une sélection de recadrage à la souris,
son application, la peinture d’un trait, Cmd+Z / Cmd+Maj+Z, l’application du masque,
la sauvegarde, la destruction puis la recréation de l’interface et la reprise.
Les pixels terminés et les paramètres sont comparés à l’identique après reprise.
La boîte de fermeture et « Sauvegarder la session et quitter » ont été contrôlées.

Les captures `previews/native/1.5.0-*` documentent les éditeurs et la reprise aux
formats 1440 × 960, 1100 × 800 et 900 × 700. Le contour de recadrage, les poignées,
les zones protégées en rouge et la barre de zoom sont visibles. La géométrie
et les masques suivent la direction Atelier approuvée.

Cette vérification ne relance pas l’intégration M8/M20 ni une comparaison de
qualité avec Siril ; elle ne valide pas l’astrométrie, SPCC ou de nouveau matériel.
La sauvegarde locale est limitée à 4 Gio ; les acquisitions et calculs interrompus
ne sont pas repris, et les masques importés restent des fichiers externes.

### Vérification de livraison 1.5.0

Les 35 tests passent aussi en Release (24,99 s). Le DMG autonome de 81,1 Mio
a été vérifié par `hdiutil verify`, puis monté en lecture seule. La signature
locale de l’application contenue est valide ; sa version est 1.5.0 et son
exécutable est identique au paquet construit (SHA-256 comparé). Le volume de
vérification a ensuite été démonté.

SHA-256 du DMG :
`6b616089bd84beae658af67314b1f93841efc5db0d02ada18c6a17b245331cd0`.
