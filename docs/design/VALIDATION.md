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


## Lot 1.6.1 — légende, exports et familles indépendantes (10 octobre 2026)

L’aperçu est visible à chaque ouverture du dialogue d’export et se réaffiche
lorsque la légende est activée. PNG, TIFF et JPEG gardent le même rendu et la
même précision qu’avant. L’icône de chargement accompagne le bouton durant
l’export de Traitement, de l’image finale du Ciel profond, du FITS linéaire
et de la mosaïque. Les états du moteur distinguent l’export des autres calculs
et se terminent également après une erreur ou une annulation.

Les cinq familles de Finition se déplient indépendamment, à toutes les tailles
de fenêtre. Toutes peuvent être refermées. La comparaison reste dans le pied
de l’inspecteur ; la suppression nomme la famille active et reste annulable.
La composition par filtres reste dans Couleur, et tous les outils avancés restent
accessibles. Le projet et la session conservent les familles ouvertes, y compris
la liste vide. Les vues d’anciens projets ouvrent la famille précédemment choisie.
Les tests font passer ces vues par une sérialisation JSON réelle pour couvrir
les séquences Qt et les tableaux JavaScript.

Le parcours natif utilise le FITS linéaire d’Orion, 6 244 × 4 170 pixels,
en français, à 1440 × 960, 1100 × 800 et 900 × 700. Les clics ouvrent deux
familles puis les referment toutes, sans modifier les paramètres ni les pixels
flottants. L’aperçu, les textes, la zone de comparaison et les boutons restent
dans leurs panneaux. Les captures `previews/native/1.6.1-*` documentent ces états.
Un vrai export PNG avec légende confirme l’animation et son arrêt ; les parcours
d’erreur de destination et d’annulation la font également disparaître.

Les 36 tests passent en Debug (112,82 s), avec les régressions d’export,
d’aperçu, de fermeture complète et de restauration des familles. Le test de
l’espace Ciel profond passe aussi après la dernière correction de restauration
(3,57 s). Les cinq captures fournies sont copiées sans transformation : leurs
empreintes correspondent aux originaux. Toutes les images référencées dans
le README existent ; les captures remplacées et devenues inutilisées sont retirées.


### Livraison 1.6.1 vérifiée

La suite complète Release passe également : 36 tests, 24,26 s, après reconstruction
des exécutables de test. Le code et le README étaient committés (`d1fbfdc`) avant
la création du paquet.

Le DMG contient la version 1.6.1 et mesure 88 557 484 octets
(84,5 Mio). Sa somme de contrôle est valide. Monté en lecture seule, son
exécutable correspond exactement à l’application construite et sa signature
locale passe la vérification profonde et stricte. Le montage de contrôle a été
démonté par son propre chemin, sans toucher aux autres volumes.

SHA-256 du DMG : `b84856b833b137eab4e48f08ad4ebbd9108d4242238ea488c5d25d3f4876445e`.
SHA-256 de l’exécutable : `20ae92d7aa1a52b54cb28e4f8e33340c69d3adcf59de9884f381b2583d39fa62`.

## Lot 1.7.0 — astrométrie et couleurs (10 octobre 2026)

L’accès est dans Ciel profond → Finition → Couleur → Astrométrie · PCC / SPCC.
Le dialogue suit deux étapes : résoudre et vérifier le WCS, puis calibrer les
couleurs. Les réglages photométriques se déplient à la demande. Chaque paramètre
possède une aide pratique ; calcul en cours, progression, annulation et erreurs
conservent une place visible. Le bilan nomme la méthode, le profil et le blanc,
avec étoiles acceptées/rejetées, dispersion, gains et causes de rejet.

La composition est contrôlée en français avec le FITS réel d’Orion, aux tailles
exactes 1440 × 960, 1100 × 800 et 900 × 700. Ces captures utilisent les composants
Qt/QML natifs avec le rendu logiciel hors écran, après un premier contrôle Cocoa.
Les six images `previews/native/1.7.0-wcs-*` et `1.7.0-spcc-*` montrent les deux
étapes. Aucun libellé ni bouton ne chevauche la modale ; le pied reste visible
et le contenu peut défiler sans barre apparente. Les repères d’étoiles sont
facultatifs et désactivés par défaut.

Le choix utilisateur d’une méthode rétablit 60 candidates en SPCC, 150 en PCC ;
la réouverture restaure la limite mémorisée. La régression couvre une affectation
préalable du champ, qui casse sa liaison QML initiale. Les longues listes de
références ne sont plus enveloppées à répétition lors de la comparaison des réglages.
Une seconde calibration identique termine normalement. Le catalogue et la réponse
complets restent conservés dans le projet, indépendamment de la vue légère du dialogue.

La sauvegarde/réouverture portable, l’annulation et le FITS WCS sont vérifiés sur
Orion. Les résultats et leurs limites scientifiques sont consignés dans
[VALIDATION_SIRIL.md](../VALIDATION_SIRIL.md) : les captures SPCC utilisent une
réponse virtuelle de contrôle, pas un profil mesuré de la caméra.

### Livraison 1.7.0 vérifiée

Le DMG final est reconstruit après `d99fd9b`, y compris le dernier changement
du choix PCC/SPCC. Sommes de contrôle valides, version 1.7.0, exécutable identique
à l’application construite et signature locale vérifiée. Les composants réseau
et TLS ainsi que les notices des données scientifiques sont inclus.
L’image disque mesure 85 394 898 octets (81,44 Mio).

SHA-256 : `e5e3cbbbd4dfcc64ad3f3c6088658f46d0f44fc329b02476a215e33311d4fbce`.

## Lot 1.8.0 — cibles, profils et coordonnées (10 octobre 2026)

Le point de départ est sauvegardé par `5a2395e`, sur la branche
`codex/astrometry-workflow`. Le parcours est intégré à Ciel profond → Finition →
Couleur → Astrométrie · PCC / SPCC. La recherche de cible et les profils complètent
les deux étapes existantes ; les coordonnées apparaissent directement sur l’image.
Chaque nouveau réglage dispose d’une aide pratique « ? » ; la commande de copie
a un nom accessible et une infobulle avec unités et conditions de disponibilité.

Contrôle avec les vrais composants Qt/QML et le projet portable d’Orion 1.7.0,
en français, rendu logiciel hors écran, exactement à 1440 × 960, 1100 × 800 et
900 × 700. Les neuf captures `previews/native/1.8.0-*` montrent la recherche M42,
la bibliothèque de profils et les coordonnées après transformation. Le rendu
violet vient de la réponse Bessell virtuelle du contrôle précédent : il ne valide
pas les couleurs du matériel réel et aucun original n’est modifié.

- Les champs français, actions et pied des dialogues restent accessibles aux
  trois tailles. Le contenu peut défiler sans barre de défilement apparente.
  Les champs de provenance longs sont éditables horizontalement et s’ouvrent
  au début du texte ; l’empreinte SHA256 se replie sur plusieurs lignes.
- La recherche M42 sur CDS renseigne les indications sans effacer le WCS vérifié.
  Le nom du service est lisible (« SIMBAD · CDS »), sans texte du protocole.
  Recherche absente et annulation conservent les résultats précédents.
- Les profils se sauvegardent depuis le dialogue, se sélectionnent explicitement
  et conservent caméra, filtres, source et notes dans le projet. Le retrait global
  et le retour par sauvegarde JSON ne changent pas la calibration existante.
- Le curseur désigne le même pixel au zoom Adapter, 100 % et 170 %, avec déplacement.
  La copie conserve le dernier pixel même quand le pointeur rejoint le bouton.
  Avant/Résultat/Partagé utilisent la même géométrie et les mêmes coordonnées.
- Les confirmations sont masquées quand un dialogue est ouvert. Sur le résultat,
  elles restent dans la zone d’image, au-dessus des coordonnées : elles ne masquent
  ni le bouton de copie ni les réglages de l’inspecteur.

Les 39 tests Release passent (26,01 s). Après le dernier ajustement de placement
des confirmations, le test de l’espace Ciel profond repasse (3,35 s) et le parcours
natif est contrôlé de nouveau. Les vérifications de précision et de sauvegarde
sont consignées dans [VALIDATION_SIRIL.md](../VALIDATION_SIRIL.md).

### Livraison 1.8.0 vérifiée

Le DMG est reconstruit après le commit `bffd7f5`. Il mesure **88 051 365 octets**
(83,97 Mio). Sommes de contrôle de l’image disque valides ; montage en lecture
seule, version 1.8.0, exécutable identique à l’application Release construite
et signature locale vérifiée en profondeur. QtNetwork et les trois extensions
TLS sont présents. La notice incluant CDS Sesame correspond exactement au dépôt.
Les 193 binaires Mach-O inspectés ne conservent aucune dépendance Homebrew absolue.
Le montage de contrôle est démonté. Developer ID et notarisation restent à fournir.

SHA256 du DMG : `80140f261bf1e40a60ae24b8de98d6607f7adb84086cbd123bedc4bcc4a76895`.


## Lot 1.9.0 — résolution aveugle/SIP, masters et couches étoiles/fond

Les phases sont sauvegardées séparément : `b8127e8` avant modification,
`6e470b3` pour la résolution aveugle/SIP, `d165f08` pour la bibliothèque de
masters et `a84514c` pour les couches étoiles/fond. Les réglages précédents
et les projets sans ces nouvelles options restent compatibles.

Les **44 tests Release passent** (27,30 s, après la correction du parcours natif), dont les nouvelles suites SIP,
résolution aveugle, bibliothèque et couches. Ils couvrent les inverses WCS,
les transformations et les FITS Float32, le rejet robuste, les métadonnées
manquantes/incompatibles, les contrôles d’empreintes et la non-répétition de
la calibration des flats. La neutralité des couches est vérifiée à l’identique
après étirement ; leurs exports et projets conservent les flottants signés,
la géométrie et le SIP. L’annulation conserve les résultats précédents.

Le FITS réel d’Orion, 6 244 × 4 170 pixels, est résolu sans coordonnées
ni échelle préalables, avec SIP d’ordre 3. La vérification indépendante Gaia
associe **675 étoiles**, avec une RMS de **0,523 pixel** et une échelle de
**1,251 seconde d’arc/pixel**. Les indications volontairement erronées du
contrôle ne changent pas la solution aveugle. Les pixels linéaires sont
conservés à l’identique. Le moteur externe travaille localement ; les requêtes
Gaia restent explicites et distinctes de la résolution.

Les vrais panneaux Qt/QML sont contrôlés en français sur ce FITS, avec Cocoa
et le rendu logiciel, à **1440 × 960, 1100 × 800 et 900 × 700**. Les captures
`previews/native/1.9.0-*` montrent le mode aveugle, la bibliothèque, le résultat
combiné et le fond estimé. Les éléments restent dans leurs panneaux et les
réglages défilent sans barre apparente. Ouvrir une famille amène ses contrôles
dans la zone visible. La bibliothèque de contrôle utilise un master synthétique
incompatible de 64 × 64, sans écrire dans la bibliothèque personnelle.

Sur Orion, l’ajustement accepte **1 672 profils parmi 1 819 candidates**,
rejette 147 candidates et produit une RMS normalisée moyenne de **0,0679**.
Il s’agit d’une estimation PSF gaussienne/Moffat, pas d’un retrait neuronal
complet : les étoiles saturées, proches ou mal ajustées restent dans le fond.
Le réglage de réduction diminue la luminosité des profils acceptés, pas leur
rayon. Les structures diffuses ne sont pas classées automatiquement comme
étoiles. L’export final utilise toujours l’image recombinée ; chaque couche
peut être inspectée et exportée séparément en FITS flottant.

Le solveur embarque uniquement les quatre helpers natifs nécessaires, exécutés
dans des processus séparés. GSL et les helpers sont accompagnés des sources,
recettes et licences. WCSLIB 8.10 est recompilé dans un dossier privé à partir
de l’archive officielle contrôlée, car l’archive actuelle ne correspond plus
à l’empreinte de la bouteille Homebrew installée. Ses tests amont passent.
Le pré-inclusion de `stdio.h` dans les options de compilation corrige une
déclaration manquante dans un test amont avec le compilateur macOS actuel.
Aucune bibliothèque Homebrew installée n’est modifiée.

Le contrôle sans Homebrew dans le chemin des outils a détecté les filtres
Python optionnels `removelines` et `uniformize`. Le parcours FITS natif les
désactive explicitement (`--no-remove-lines --uniformize 0`) ; la vérification
C du moteur conserve sa propre répartition/déduplication et le contrôle Gaia
reste obligatoire. Aucun runtime Python n’est requis sur le Mac destinataire.

Le parcours réel du contrôleur repasse avec uniquement les helpers du paquet
et les outils système dans PATH : résolution aveugle, 675 associations Gaia,
RMS 0,523018 pixel, export SIP et vérification des pixels linéaires à l’identique.
Une demande SIP invalide conserve ensuite le WCS vérifié et les résultats.

### Livraison 1.9.0 vérifiée

Le DMG final est reconstruit après `0cf560d`. Il mesure **119 036 790 octets**
(113,52 Mio). Son contrôle d’intégrité est valide. Monté en lecture seule, il
contient la version 1.9.0, les quatre helpers natifs et leurs sources ; la
signature locale passe la vérification profonde et stricte. L’exécutable est
identique à l’application Release construite. Les helpers se lancent aussi
depuis le volume monté. Les **200 binaires Mach-O** inspectés ne conservent
aucune dépendance Homebrew absolue. Le volume de contrôle est démonté.
Developer ID et notarisation restent à fournir, comme pour les versions précédentes.

SHA256 du DMG : `d29d0a6dd931eb87b6e64164d83d4032933b10819a2c7128c6a9ce35db359a0d`.
SHA256 de l’exécutable : `38ee02d8fda67d8e31a8cb3312a459e8e9b9c5a673d1ff0c2c78b7d7654b98e8`.


## Guide et drizzle — 1.10.0, 10 octobre 2026

Le guide français intégré et le README reflètent les fonctions livrées :
résolution aveugle/SIP, masters réutilisables, masques peints/importés,
traitement séparé des étoiles et du fond, et exports actuels. Une carte
drizzle décrit quand l’utiliser, ses paramètres et ses limites.

Les **45 tests Release passent** (28,98 s, `build/drizzle-final-tests.log`).
Les nouveaux contrôles portent sur les empreintes sous-pixel analytiques,
rotations à 45°, valeurs signées/HDR et zéros, rejet d’aberrations,
WCS/SIP et échantillonnage relus en FITS, trous de couverture, limite 32 MP
et annulation de la dernière pose. L’action QML réelle, la réouverture
portable du résultat et de son bilan, et la conservation du résultat
précédent après erreur sont contrôlées.

QA native française à **1440 × 960, 1100 × 800 et 900 × 700** : réglages et
résultat sur un extrait 1024 × 768 du vrai Orion, guide et carte drizzle.
La projection ×2 de deux copies de cet extrait aboutit à **2048 × 1536**,
minimum/moyenne de **2 contributions** et un avertissement de phases
similaires. Le parcours complet prend environ **1,15 s** sur ce Mac.
Ce jeu valide l’interface et la stabilité ; les copies identiques ne
démontrent pas un gain de résolution ou de bruit. Les captures 1.10.0
sont conservées dans `docs/design/previews/native/`. Les neuf thèmes du
guide ont aussi été chargés au format minimal du test existant.

Le résultat conserve sa convention d’intensité linéaire et ses données
Float32. L’échelle ×2 reste impossible sur les sources de 26 MP tant que
la finition est limitée à 32 MP ; ×1 est disponible. Le drizzle CFA/Bayer
natif et les cartes de poids exportables restent à développer.


### Livraison 1.10.0 vérifiée

DMG reconstruit après `659b7ec` : **120 452 952 octets** (114,87 Mio).
Le contrôle d’intégrité est valide. Montage en lecture seule : version
1.10.0, signature locale profonde/stricte valide, exécutable identique
au paquet Release, quatre helpers Astrometry.net exécutables et sources
correspondantes présentes. Le volume de vérification est démonté.
Les **200 binaires Mach-O** du paquet n’ont aucune dépendance Homebrew
absolue. La distribution conserve la signature ad-hoc existante ;
Developer ID et notarisation restent à fournir.

## Drizzle Bayer natif — 1.11.0, 10 octobre 2026

Le mode **Bayer natif / CFA** de l’empilement projette seulement la couleur
mesurée de chaque pixel calibré. La copie dématricée sert à l’alignement et,
si demandé, à estimer le modèle de gradient ; elle ne fournit pas les couleurs
interpolées au résultat. Couverture, poids et rejet sont séparés en R, V et B.
Les valeurs nulles, négatives et HDR restent valides. WCS/SIP et Float32 sont
conservés. Les réglages existants gardent leur mode mono/RVB par défaut.

Une déclaration explicite peut compléter les matrices manquantes, notamment
les FITS portant seulement `BAYERPAT=VALID`, sans réécrire les fichiers.
Une matrice connue contradictoire et un fichier RVB incompatible bloquent
le calcul. Guide intégré, README et aides des réglages sont à jour en français.

Les **45 tests Release passent** (29,25 s, `build/bayer-final-tests.log`).
Les tests couvrent les quatre matrices, la reconstruction analytique à ×1/×2
avec phases contrôlées, la projection à 45°, la calibration bias/flat,
la normalisation, le rejet d’un défaut, le WCS, les trous de couleur,
l’annulation et la conservation du résultat précédent. Le parcours QML teste
la déclaration d’une matrice absente, les conflits, les réglages et la
restauration exacte des pixels et du bilan dans un projet portable.

La composition native française a été vérifiée à **1440 × 960**, **1100 × 800**
et **900 × 700** : réglages d’empilement, résultat, bilan et guide. En petite
fenêtre, l’interprétation de la source occupe une ligne complète et les deux
contrôles d’aperçu gardent leurs libellés lisibles. Les captures sont dans
`previews/native/1.11.0-*.png`.

La fixture visuelle construit huit mosaïques CFA depuis un extrait du vrai
Orion, à quatre décalages entiers connus : **1021 × 765 px**, minimum
**4 contributions par canal**, moyenne **6,664**, calcul **2,089 s**
(`build/bayer-native.log`). Ce contrôle n’est pas une mesure de gain sur les
poses originales de l’utilisateur ; elles ne sont pas modifiées.

Les limites restent explicites : grille avant recadrage de **32 MP**,
stockage temporaire de **24 octets par pixel et par pose**, nombreux décalages
nécessaires pour une couverture CFA fiable. Une couleur sans données produit
une erreur et conserve le résultat précédent. Les cartes de poids/rejet
exportables et une comparaison quantitative sur de véritables acquisitions
avec dithering documenté restent à développer. Voir l’ADR-022.

SHA256 du DMG : `d36f491245e450dd4ab19b8d9a7ffd0b5653c7293389b1bf02e342eb333ebf1d`.
SHA256 de l’exécutable : `9bf931f63836e442832642cb08d6e39d5df89074e8648a5bfb03b23539f95839`.


### Livraison 1.11.0 vérifiée

Le DMG a été reconstruit et vérifié (`build/bayer-package.log`,
`build/bayer-dmg-verification.log`). Son volume monté en lecture seule contient
la version 1.11.0 et un exécutable identique à celui contrôlé. La signature locale
profonde/stricte est valide ; les quatre helpers Astrometry.net démarrent et les
archives de sources requises sont présentes. Les **200 Mach-O** du bundle ne
référencent aucune dépendance absolue Homebrew ni un dossier utilisateur.
Le volume de vérification est démonté.

- Taille : **118 406 354 octets** (112,92 Mio).
- SHA-256 DMG : `a90542f0ed21772518e305a707ffe4b40fcf44f963d888b29cebe2137f64a109`.
- SHA-256 exécutable : `8d0289f35207dd7b23069aae2b9ae0ad6ed5630ff884451a74181efd90751223`.
- Les deux tests ciblés de livraison passent après la dernière correction des
  messages de validation (5,13 s, `build/bayer-delivery-tests.log`).

La signature Developer ID et la notarisation restent à fournir.

## 1.12.0 — Cartes d’intégration et HDR linéaire (10 octobre 2026)

Phase sauvegardée par le commit `ab4d42e`, branche `codex/integration-maps-hdr`.
La direction Atelier existante est conservée.

### Contrôles du calcul et des projets

- **46/46 tests Release passent**, 20,46 s (`build/maps-hdr-final-tests.log`).
- Les trois contrôles ciblés de livraison passent, 6,25 s, après ajout du contrôle
  WCS/SIP des exports HDR et cartes (`build/maps-hdr-delivery-tests.log`).
- Le nouveau test numérique vérifie radiance analytique, rapports RVB, vrais
  zéros, valeurs négatives, dépassements de 1, saturation partielle/totale,
  translation stellaire, erreurs de paramètres/métadonnées, données non finies,
  défaut isolé et annulation. Les diagnostics facultatifs ne changent pas le
  résultat ; les comptes de rejets correspondent au bilan global.
- Le test du parcours QML vérifie activation volontaire des cartes, export
  Float32 exact, refus de carte corrompue, restitution portable des pixels/cartes,
  confirmation HDR, saisie décimale conservée, paramètres HDR en projet lié,
  sources remappées en projet portable, refus/annulation conservant le résultat,
  EXPTIME du FITS HDR égal à la durée de référence et WCS/SIP conservé.
- La consultation/export d’un WCS absent ne crée plus de valeur WCS invalide dans
  le projet. La réouverture après export est contrôlée. Les messages de calcul
  drizzle/HDR sont traduits dans le contexte C++ effectivement utilisé.

### Interface et guide

Contrôle natif en français aux tailles **1440 × 960, 1100 × 800 et 900 × 700** :
réglages HDR, consultation des cartes, résultat final et cartes HDR. Les fenêtres
modales conservent les actions accessibles, une zone de contenu défilante sans
barre visible si nécessaire et des aides au survol des paramètres. L’aperçu des
cartes conserve les proportions, affiche min/max et échelle, et indique la taille
estimée du FITS. Tous les nouveaux libellés et erreurs HDR ont une traduction.

Le parcours utilise un recadrage **1024 × 768** de la vraie image RVB de M42.
Trois expositions sont **simulées** (120/30/3 s, valeurs multipliées par 4 et
écrêtées à 1 avant fusion), pour vérifier le parcours sans les présenter comme
une acquisition HDR indépendante. Le bilan obtenu : **19 969 pixels récupérés**,
**9 encore saturés**, 131 étoiles correspondantes par source alignée, résidu
maximal **0,120 px**, calcul de 543 ms lors de cette exécution. Ce temps n’est pas
un benchmark général du traitement ni une preuve de gain sur le terrain.

Le guide contient les unités, la préparation par durée, les limites du HDR,
le rôle exact des cartes et leur coût mémoire. La mention de stockage temporaire
Bayer est corrigée : **24 contre 16 octets**, soit 1,5 fois, plutôt que deux fois.
Les cartes du guide sont aussi contrôlées aux trois tailles.

Captures de cette livraison dans `docs/design/previews/native/` :

- `1.12.0-hdr-settings-{1440x960,1100x800,900x700}.png`
- `1.12.0-maps-{1440x960,1100x800,900x700}.png`
- `1.12.0-hdr-result-{1440x960,1100x800,900x700}.png`
- `1.12.0-hdr-map-{1440x960,1100x800,900x700}.png`
- `1.12.0-guide-maps-{1440x960,1100x800,900x700}.png`
- `1.12.0-guide-hdr-{1440x960,1100x800,900x700}.png`

### Paquet macOS

DMG : `build/release/AstroCapture-1.12.0.dmg`. Vérification `hdiutil`, montage en
lecture seule, version 1.12.0, signature ad hoc profonde/stricte valide et
exécutable identique à celui compilé. Les quatre helpers Astrometry.net démarrent,
les archives de sources sont présentes et les **200 Mach-O** n’ont aucune
référence absolue Homebrew ni dossier utilisateur. Volume de contrôle démonté.

- Taille : **116 648 837 octets** (111,25 Mio).
- SHA-256 DMG : `ca2a3f7fb37dda5d4be332fb807ecb680f5b7e1845ddb694cb5fe9ef18ddc949`.
- SHA-256 exécutable : `4d48bbd2015ff10cbe80fbda0bc1a5ac60f19f3ce52fc76c6e2b46fb6413b701`.

La signature Apple Developer ID et la notarisation ne sont pas fournies.
Les limites scientifiques et les conditions avant V2 sont consignées dans
`docs/decisions/ADR-023-integration-diagnostics-hdr.md` et
`docs/releases/V2_READINESS.md`. La qualification sur de vraies séries HDR,
la comparaison indépendante avec Siril et les essais d’endurance demeurent à faire.
