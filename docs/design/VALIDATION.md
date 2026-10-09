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

Le script `scripts/package_macos.sh` reconstruit le paquet autonome et le DMG 1.5.1.
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

## Lot 1.5.1 — aide pratique des réglages

Les aides couvrent les réglages numériques, sélecteurs et options de capture,
empilement, finition, géométrie, masques, mosaïque, légendes, export et application.
Les 106 nouveaux textes d’aide ou libellés sont vérifiés dans le catalogue français.
Leur rédaction distingue les effets pratiques des conventions scientifiques.
Les explications numériques existantes des outils avancés sont conservées.

Les quatre tests d’interface concernés passent en développement. La vérification
native avec Orion couvre Traitement en mode Compact, le recadrage, le pinceau et
l’export aux tailles 1440 × 960, 1100 × 800 et 900 × 700. Les options de mosaïque,
la capture simulée et les paramètres de l’application sont aussi inspectés.
Le survol réel, l’aide au focus clavier, les limites de fenêtre, la proximité du
contrôle après défilement et la disparition au changement d’onglet sont contrôlés.
Consulter ces aides ne modifie ni les pixels ni les réglages de traitement.
Les captures sont enregistrées sous `previews/native/1.5.1-*`.

Les quatre tests concernés passent aussi en Release (5,84 s). Le DMG 1.5.1 de
81,3 Mio passe la vérification d’intégrité. Monté en lecture seule, il contient
la bonne version, une signature locale valide et un exécutable identique au paquet
construit. Le volume de vérification a été démonté après contrôle.

SHA-256 du DMG :
`6dad40782d7fa2a327bce0487dbf16800cb6897a07d7020a536fd4155e928ef2`.

## Lot 1.6.0 — projets portables et sources déplacées

Le menu du projet propose une sauvegarde **Portable** ou **Liée**, avec taille
estimée et aide pratique. Le portable conserve les fichiers originaux et de
calibration, les masques importés, les empilements terminés par filtre, les
résultats flottants, la légende, A/B et le cadrage. Les projets JSON précédents
restent lisibles. La recherche des sources déplacées vérifie les empreintes ;
les anciens projets sans empreinte exigent une comparaison explicite des
métadonnées, et les cas ambigus restent manquants.

Les paramètres affichent les caches et la reprise automatique. Le nettoyage
préserve les chemins de tous les workspaces et du manifeste de reprise, y
compris les alias canoniques ; une reprise corrompue bloque le nettoyage.
Les exports protègent aussi le fichier de projet ouvert. Les sources modifiées
depuis l'import bloquent la sauvegarde portable.

Le test dédié vérifie les Float32 exacts négatifs/HDR, les buffers partagés,
masques peints/importés, A/B, légende, références, réapplication, composition et
FITS linéaire ; corruption, traversée de chemins, doublons, bornes, annulation
pendant l'écriture, source modifiée, homonymes, anciens projets, caches protégés
et liens externes. Le test QML couvre aussi un portable enregistré avant
l'intégration avec une recette sans A/B : les contrôles sont restaurés et la
recette s'applique après l'empilement. Les cinq tests concernés passent en
Debug (5,22 s).

Le parcours natif en français utilise Orion 6 244 × 4 170. Il sauvegarde un
portable, le déplace, retire la copie source, puis le recharge : les empreintes
des buffers linéaire et fini, les réglages, la légende corrigée et A/B sont
identiques ; les contrôles de Finition sont synchronisés avec les pixels.
Le fichier complet mesure 880 934 480 octets, pour une estimation prudente
de 1 017 870 000 octets environ. Les dialogues de sauvegarde/réassociation et
le panneau de cache sont contrôlés à 1440 × 960, 1100 × 800 et 900 × 700 ; les
boutons restent dans leurs panneaux, sans chevauchement ni libellé coupé.
Les captures `previews/native/1.6.0-*` documentent ce parcours.

La portabilité concerne Ciel profond. La pile d'annulation et les calculs en
cours ne sont pas enregistrés ; les caches internes des étapes de recette
sont reconstruits lors d'une modification. Les anciens masques sans empreinte
se rechargent depuis l'outil Masques. Le format, les limites et la gestion du
cache sont décrits dans ADR-014.

Les 36 tests passent aussi en suite complète Debug (142,19 s, en parallèle
avec la construction et le parcours natif) et Release (23,37 s). Le code a été
commité avant la reconstruction de l’installateur : `b515323`, puis `8fec824`
pour la traduction française des erreurs du stockage partagé.

### Vérification de livraison 1.6.0

Le DMG mesure 87 384 841 octets (83,3 Mio). `hdiutil verify` valide
ses sommes de contrôle. Monté en lecture seule, il contient la version 1.6.0 ;
l'empreinte de son exécutable est identique à celle de l'application construite
et sa signature locale passe la vérification profonde et stricte. Le montage
de contrôle a été démonté sans toucher aux autres volumes. La signature reste
locale ad hoc ; la signature Developer ID et la notarisation Apple demeurent
dans la feuille de route.

SHA-256 du DMG : `14af7483d9e463952a102e55789863af555fb5071c518c2826339236a0da9201`.
SHA-256 de l'exécutable : `59ad87cf5e8dae41865a72414425a331fbbf13c11653393b22d83fbe061d8c67`.
