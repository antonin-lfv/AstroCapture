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


## 1.13.0 — projection WCS/SIP à l’empilement

Rendu QML réel en français avec une image d’Orion (recadrage 1024 × 768),
distorsion simulée sur une pose et coordonnées volontairement incohérentes
sur une troisième. Pas de maquette et pas de qualification sur une série
astrophotographique indépendante. Grille de sortie TAN 1017 × 765 ;
128 correspondances, RMS 0,296005 pixel de la référence initiale.

Réglages, aide, bilan compact/déplié, finition et carte du guide contrôlés
à **1440 × 960, 1100 × 800, 900 × 700** en rendu Qt logiciel à dimensions
exactes. Aide entièrement dans la fenêtre, avec test de ses bornes ; textes
repliés et exclusion sauvegardée lisible. Bilan dépliable utilisant le bouton
commun Atelier, option hors des réglages de finition, désactivée par défaut.
18 captures : `previews/native/1.13.0-*.png`.

47 tests automatisés passent ; options et bilan réouverts en projet portable
avec pixels identiques, sortie FITS Float32 TAN sans SIP. Détails techniques
et limites dans `docs/decisions/ADR-024-wcs-sip-stack-projection.md`.


## 1.14.0 — Ressources avant empilement (10 octobre 2026)

Phase sauvegardée par le commit `4d1ccff`, branche `codex/large-image-resources`.
La direction Atelier et la limite de sortie de 32 MP restent en place.

### Calcul et protection des résultats

- **48/48 tests Release passent**, 10,26 s (`build/resource-final-tests.log`).
- Le dimensionnement des tuiles vérifie 1, 12 et 500 poses, poids classiques,
  drizzle RVB et Bayer, y compris une ligne dépassant 64 Mio ; refus des
  paramètres invalides, débordements et sortie drizzle au-delà de 32 MP.
- Les calculs sur tuiles forcées donnent des pixels Float32, cartes de poids,
  contributions et rejets **identiques** au parcours de référence, en classique,
  drizzle et Bayer natif avec calibration/normalisation. Le plafond de 64 Mio
  s'applique aux buffers d'échantillons, pas à toute la mémoire du processus.
- Master robuste de 300 sources contrôlé avec la limite de descripteurs ramenée
  à 32. Lecture d'offsets, conservation des valeurs négatives/HDR, refus de
  lecture hors archive et nettoyage après annulation sont vérifiés. Un enfant
  POSIX termine par `_exit` sans destructeur : aucun fichier de travail ne reste.
- Dossier temporaire absent et réserve de disque impossible refusés ; l'ancien
  résultat reste disponible. Le dossier choisi contenant espaces, accent et `#`
  conserve son fichier témoin ; la préférence locale est persistante et le retour
  au dossier système fonctionne. Le dépassement de 64 références de reprise
  est aussi refusé avant le calcul.

### Image réelle et interface

Contrôle QML natif en français aux tailles **1440 × 960, 1100 × 800, 900 × 700**
avec un recadrage 1024 × 768 d'Orion : ressources, refus expliqué, aide au focus
clavier et carte du guide. Les bornes du texte d'aide et des estimations sont
contrôlées ; les actions restent accessibles dans l'inspecteur défilant.
Treize captures : `previews/native/1.14.0-resources-*.png`.
Le guide et les aides donnent unités, budget, réserve disque et limites de reprise.

Un essai séparé utilise l'image réelle d'Orion **6244 × 4170** complète, soit
26 037 480 pixels. Une copie temporaire du FITS est intégrée comme pose déjà
calibrée, sans normalisation ni correction de pixels : les échantillons Float32
linéaires de sortie sont exactement identiques à ceux de l'entrée (SHA-256).
Ce parcours à une pose contrôle les dimensions, allocations et la précision ;
il ne qualifie pas une série indépendante ni un gain de bruit ou une parité Siril.

Dernière exécution : intégration **7 434 ms**, processus complet **21,45 s**,
RSS maximal **2 428 551 168 octets** (2,26 Gio), empreinte mémoire maximale
macOS **2 996 423 704 octets** (`build/resource-full.log`). Le modèle avant calcul
annonce 5 085 369 256 octets, budget 8 Gio, archive 312 449 760 octets et besoin
disque avec réserve 580 885 216 octets. Ces mesures dépendent de la machine,
du mode et des parcours ouverts ; elles ne sont pas un benchmark général ni
une garantie d'absence de manque de mémoire.

Les résultats/finition restent en mémoire complète. Les profils matériel,
la comparaison indépendante avec Siril, les essais de pression/endurance et
le traitement de toute la chaîne par tuiles restent dans le bilan V2.
Voir [ADR-025](../decisions/ADR-025-integration-resources.md).


### Livraison macOS 1.14.0

DMG reconstruit : `build/release/AstroCapture-1.14.0.dmg`.
Contrôle `hdiutil`, montage en lecture seule, version 1.14.0 et exécutable
identique au bundle compilé. La signature locale ad hoc profonde/stricte est
valide ; les quatre helpers Astrometry.net démarrent, les trois archives de
sources sont présentes et les **200 Mach-O** ne référencent aucune dépendance
absolue Homebrew ni dossier utilisateur. Le volume de contrôle est démonté.
Journaux : `build/resource-package.log`, `build/resource-dmg-verification.log`.

- Taille : **117,184,542 octets** (111.76 Mio).
- SHA-256 DMG : `e8ea9227fb27eea0f27ed3f2a8d58cb928b5fe744dba9e25096a853d5f934bf6`.
- SHA-256 exécutable : `4d428948beb9977084cc2b204ac72897a233ea6ad9bb6cc24d7121b19d2d9334`.

La signature Apple Developer ID, la notarisation et l'installation sur un Mac
vierge restent à qualifier avant une distribution publique normale.


## 1.15.0 — Masters de calibration par lots (10 octobre 2026)

Phase sauvegardée par le commit `44502f3`, branche
`codex/batch-calibration-masters`. Direction Atelier conservée.

### Calcul et protection des données

- **49/49 tests Release passent**, 7,96 s (`build/batch-final-tests.log`).
- Regroupement strict testé pour durée, caméra, dimensions, Bayer, binning,
  gain, offset, température, unités et filtre ; les valeurs absentes restent
  distinctes et demandent une revue. Une incompatibilité connue bloque le calcul.
- Masters robustes de durées différentes et rejet d'un échantillon aberrant
  vérifiés. Les flats corrigés/normalisés donnent exactement les mêmes
  échantillons que le parcours de référence, sans double soustraction du piédestal.
- Dark-flat préféré, ambiguïtés explicites et correctifs disponibles directement
  dans la bibliothèque vérifiés. Les alias macOS d'un même chemin ne dupliquent
  pas le candidat. Les masters enregistrés ne sont pas regroupés comme bruts.
- `AC_ORIG` conservé à l'import Float32 ; incompatibilités d'unités refusées.
  Provenance SHA-256 des sources/correctifs et revue des métadonnées relues dans
  l'index, avec compatibilité du format de bibliothèque existant.
- Plan périmé, source modifiée, index corrompu et dossier temporaire absent
  refusés. Annulation immédiate et échec après un premier master publié donnent
  un bilan explicite, conservent les masters terminés et arrêtent les suivants.
  Le résultat linéaire, les sources et les sélections restent inchangés.
- Budgets mémoire/disque partagés avec l'intégration ; le choix du dossier
  temporaire s'applique aussi au calcul des masters et correctifs bruts.
  Publication atomique par entrée, pas transaction de tout le lot.

### Interface, aides et guide

Rendu QML natif en français aux tailles **1440 × 960, 1100 × 800, 900 × 700** :
plan, aide au focus, correctif du flat, bilan de cinq masters, recherche dans la
bibliothèque et carte du guide. Dix-huit captures :
`previews/native/1.15.0-*.png`. Bornes des aides contrôlées ; actions de création,
dossier temporaire et fermeture accessibles, états d'activité/désactivation
vérifiés. Cases à cocher communes avec indicateur sombre, survol et focus Atelier.
Recherche par durée et masquage des incompatibilités contrôlés, application
explicite conservée. Le guide décrit aussi les limites et l'arrêt du lot.

L'image d'Orion est réelle, recadrée à 1024 × 768, avec des métadonnées de matériel
de démonstration sur sa copie ; les calibrations du contrôle d'interface sont
synthétiques. Il s'agit d'un contrôle de parcours et de précision, pas d'une
qualification indépendante des capteurs ni d'une comparaison avec Siril.

Les images restent complètes en mémoire, limitées à 32 MP. Le lot prépare les
masters ; l'application automatique de calibrations distinctes aux poses
hétérogènes et le pipeline HDR brut complet restent hors de ce périmètre.
Voir [ADR-026](../decisions/ADR-026-batch-calibration-masters.md) et le
[bilan avant V2](../releases/V2_READINESS.md).

### Livraison macOS 1.15.0

Implémentation et preuves sauvegardées par le commit `88e0e87`.
DMG reconstruit : `build/release/AstroCapture-1.15.0.dmg`.
Contrôle `hdiutil`, montage en lecture seule, version 1.15.0 et exécutable
identique au bundle compilé. Signature locale ad hoc profonde/stricte valide ;
les quatre helpers Astrometry.net démarrent et les trois archives de sources
sont présentes. Les **200 Mach-O** ne référencent aucune dépendance absolue
Homebrew ni dossier utilisateur. Le volume de contrôle est démonté.
Journaux : `build/batch-package.log`, `build/batch-dmg-verification.log`.

- Taille : **112 765 247 octets** (107,54 Mio).
- SHA-256 DMG : `29cc9a1ae5601a99a61b766a4aafee165526c09b9d30efa1489aad82c26430d3`.
- SHA-256 exécutable : `975e12d585f52a84f7fbf29df03b6c9e7e1294d4df36815a6a66ca7f1bbca6fa`.

Apple Developer ID, notarisation, installation sur Mac vierge et qualification
indépendante restent à faire ; les contrôles de ce lot ne les remplacent pas.


## 1.15.1 — Contrôle pour usage personnel (10 octobre 2026)

L'utilisateur réserve AstroCapture à son usage personnel et reporte la
comparaison finale avec Siril au lendemain. Les conditions de publication
publique restent consignées dans le bilan V2, sans bloquer ces essais locaux.
Phase sauvegardée par `140e1fc`, branche `codex/personal-validation` ; tests et
correction de traduction sauvegardés par `85c564f`.

### Fenêtre complète et reprise

**50/50 tests Release passent**, 7,90 s (`build/personal-final-tests.log`).
Le nouveau contrôle charge `Main.qml` et tous ses panneaux, avec les vrais
contrôleurs et leurs connexions. Le chargement depuis les sources rend explicite
l'import normalement implicite du module ; seules les destinations de stockage
et les préférences sont redirigées vers des dossiers de test.

- Capture SER simulée, zéro perte dans cet essai court ; quitter et changer
  d'espace de traitement sont bloqués pendant l'enregistrement. Les barres de
  couleur simulées sont correctement refusées en alignement planétaire ; leur
  import à une seule image fonctionne. Cela ne qualifie pas une caméra réelle.
- Une vraie image d'Orion, recadrée à 1024 × 768 sur une copie, est intégrée
  comme image déjà calibrée, sans normalisation ni correction de pixels : les
  échantillons Float32 sont identiques. Réglages de finition via le panneau,
  export FITS linéaire exact, puis légende modifiée et export TIFF/PNG/JPEG.
- TIFF et PNG 16 bits donnent les mêmes pixels, légende comprise ; JPEG est
  relu et reste un format avec pertes. Les exports apparaissent dans l'historique.
- Annulation d'un export préserve le fichier existant ; destination absente
  et annulation d'empilement conservent le résultat précédent. Projet portable
  enregistré, sources de test inchangées.
- « Continuer à travailler » annule la fermeture. « Sauvegarder la session et
  quitter » attend la sauvegarde puis ferme la fenêtre. Un nouveau processus
  restaure les mêmes pixels linéaires, finis et de Traitement (SHA-256), la
  légende corrigée et l'espace actif ; export FITS du résultat repris exact.
- Fenêtre complète et confirmation de sortie contrôlées en français à
  **1440 × 960, 1100 × 800, 900 × 700**. Les noms d'espaces restés en anglais
  dans le choix d'export de sortie sont corrigés et couverts par le test.

### Disque plein réel, limité au test

Un volume HFS+ privé de **64 Mio** est rempli jusqu'à `ENOSPC`, sans remplir le
disque du Mac. Échecs d'export PNG, FITS et projet portable : les fichiers
préexistants restent identiques et aucun signal de réussite n'est émis.
Échec de sauvegarde de session : l'ancien manifeste reste identique, le résultat
linéaire reste disponible, le message d'échec est affiché et la fenêtre reste
ouverte. Le volume temporaire est démonté et supprimé.

Parcours reproductible : `scripts/validate_personal_macos.py` ; journaux dans
`build/personal-validation/`. Ce contrôle ne simule pas une panne du disque
physique, une perte d'alimentation ou une acquisition longue. La comparaison
indépendante avec Siril et les essais de caméra réelle restent à effectuer.

Huit captures archivées : `previews/native/1.15.1-personal-*.png`.
Le parcours complet sur Orion et le test de volume plein passent après la
correction de traduction (`build/personal-validation.log`).

### Livraison et démarrage du DMG 1.15.1

`build/release/AstroCapture-1.15.1.dmg` reconstruit et monté en lecture seule.
Intégrité `hdiutil` et signature locale profonde/stricte valides ; version 1.15.1,
quatre helpers Astrometry.net fonctionnels, trois archives de sources et
**200 Mach-O** sans dépendance absolue Homebrew/utilisateur. Exécutable identique
au bundle compilé. Journaux : `build/personal-package.log` et
`build/personal-dmg-verification.log`.

L'application du DMG démarre en mode **Cocoa natif** et reste active durant le
contrôle de démarrage (5 s), sans erreur de chargement QML. Ses dossiers de
données/cache sont isolés ; écriture dans la bibliothèque utilisateur et accès
au service de préférences interdits pour ce lancement de contrôle. Le processus
de test est arrêté et le volume démonté. Ce démarrage ne remplace pas un essai
matériel ou d'endurance. Journal : `build/personal-validation/dmg-startup.log`.

- Taille : **117 251 893 octets** (111,82 Mio).
- SHA-256 DMG : `c9d7a61a2a2656590f7e73ba6b0fc14d415595e7fbbde4f9098bb3ea708fcac3`.
- SHA-256 exécutable : `405dca616e70edd9debb24b972907255ed3dceaebbacad6a39c7569c0f96e49c`.

La signature reste ad hoc pour cet usage personnel. La signature Apple et la
notarisation ne sont pas annoncées comme réalisées.


## 11 octobre 2026 — Images jusqu’à 128 MP (1.16.0)

Plafond commun de 128 000 000 pixels / 32 700 pixels par dimension, budgets
mémoire et reprise conservés. Voir [ADR-027](../decisions/ADR-027-large-images.md).
Projection drizzle en bandes de 64 Mio ; FITS par blocs de 262 144 échantillons,
TIFF par blocs encodés de 128 Kio. Les images scientifiques restent complètes
en RAM ; les combinaisons avancées peuvent être refusées avant calcul.

Les tests comparent les bandes de drizzle aux projections complètes, avec
égalité des valeurs et poids (NaN comparés comme absence de données) :
translations, rotation, cisaillement, RGGB/BGGR/GRBG/GBRG, TAN-SIP et valeurs
signées. Réservations de l’archive, abandon/troncature, offsets, annulation et
fin du processus sont contrôlés. L’export des cartes utilise le même writer
FITS par vue immuable ; une régression transitoire détectée dans ce chemin a
été corrigée avant livraison. La petite géométrie recadrée d’une grande source
est estimée sur ses propres canevas, au lieu de multiplier toute la source.

### Contrôle natif sur 104,15 MP

Source réelle Orion : 6244 × 4170 px, déjà calibrée. Deux fichiers identiques,
dont une copie privée, servent au contrôle des ressources et des pixels.
Aucune prétention de photons supplémentaires ni de gain de résolution par
cette duplication. Originaux préservés, préférences et sorties de test isolées.

- Drizzle RVB ×2, fraction de pixel 1, sans normalisation/rejet :
  **12488 × 8340 px (104 149 920 pixels)**. Calcul mesuré à **27,983 s** sur
  le Mac local de 16 Gio, sans seuil de performance imposé.
- Intensités comparées à toute la source répétée sur la grille ×2 : écart
  inférieur à 2e-6. SHA-256 Float32 linéaire reproductible :
  `0b0ffcd3f95b88cce3dbbfea807015175adcfa9327d6ffd19e561b082fa4a515`.
- Finition demandée via les commandes QML natives ; fichier FITS relu avec
  égalité complète des échantillons Float32, TIFF 16 bits relu avec dimensions
  et quantification vérifiées aux bords/centre. Session sauvegardée et relue :
  égalité complète des images linéaire et finie, validateurs applicatifs valides.
- Combinaison étoiles + débruitage avancé + déconvolution : refusée avant calcul
  par le budget, en français, image finie précédente conservée.
- Parcours complet de contrôle : 174,95 s ; RSS maximal **5 322 686 464 octets**
  (4,96 Gio), empreinte physique maximale **10 175 848 560 octets** (9,48 Gio).
  Le contrôle conserve aussi l’état initial pendant les relectures/décodages
  et la restauration pour les comparer. Ce pic n’est pas une mesure isolée de
  l’empilement : son estimation de **6 863 978 552 octets** (6,39 Gio) couvre
  les buffers modélisés de cette phase, pas les copies du banc de contrôle,
  les autres espaces/applications ni un plafond mesuré de tout le processus.

Journaux : `build/large-image-final-validation.log`,
`build/large-processing-validation.log`, `build/large-processing-ui-validation.log`
et `build/large-resources-ui-validation.log`.

### Traitement, PNG et composition française

TIFF de 104,15 MP importé dans Traitement, ondelettes calculées, **PNG 16 bits**
exporté et relu ; dimensions et valeurs quantifiées vérifiées aux bords/centre.
Le contrôle d’export dure 102,65 s, RSS maximal 5 623 791 616 octets (5,24 Gio),
empreinte maximale 5 920 478 176 octets (5,51 Gio). La sonde de format du plugin
TIFF écrit un diagnostic « bad magic number » ; le décodage effectif et les
vérifications de pixels réussissent. Ce message ne constitue pas une validation
ni un échec d’export. Le parcours visuel séparé utilise les réglages QML et
montre la valeur de l’ondelette appliquée.

Composition vérifiée aux tailles **1440 × 960**, **1100 × 800** et **900 × 700**
avec la vraie image, en français : plan drizzle à 104 MP, Finition, Traitement,
aide des ressources et guide. Pas de chevauchement introduit ; les contrôles
inférieurs restent accessibles par le défilement du panneau compact.
Captures conservées : `previews/native/1.16.0-large-*.png` et
`previews/native/1.16.0-resources-*.png`.

La qualification d’une vraie série dithered mono/CFA/WCS-SIP, la pression
mémoire globale, le matériel réel et la comparaison finale Siril restent des
contrôles distincts. Le plafond de 128 MP n’annonce pas toutes les combinaisons
comme possibles : le budget mémoire et la reprise 4 Gio/64 images priment.

**Vérification Release finale : 50/50 tests, 10,39 s**, journal `build/large-delivery-tests.log`.

### Installateur 1.16.0 et parcours personnel

DMG recréé après le commit fonctionnel `f7e957c` :
`build/release/AstroCapture-1.16.0.dmg`, **116 660 457 octets**.
SHA-256 : `fda45743c47d7bfd5338443dce252b89e2a687642541042b16bc5cd55df2c659`.
Exécutable : `2f06f571e8bcfd1d1ad5d7007edbbd7f02a3862b9a81d1c8aa68feb5886f749b`,
identique entre le bundle compilé et celui du DMG.

Contrôles réussis : somme de contrôle du DMG, version embarquée 1.16.0,
signature locale vérifiée, **200 fichiers Mach-O** sans dépendance vers
Homebrew ni les dossiers utilisateur, quatre exécutables auxiliaires
d’astrométrie démarrables et trois archives sources tierces présentes.
Journal : `build/large-dmg-verification.log`.

Le parcours `scripts/validate_personal_macos.py` a été relancé avec le
recadrage central 1024 × 768 de la source Orion : fenêtre complète, exports,
projet portable et reprise dans un second processus. Sur un volume privé de
64 Mio rempli, les échecs d’export/sauvegarde préservent les fichiers existants
et les résultats. L’application du DMG démarre avec le backend natif Cocoa,
reste active pendant les cinq secondes de contrôle et ne produit pas d’erreur
de chargement QML. Les préférences utilisateur sont protégées et les volumes
de test ont été démontés. Le seul diagnostic de démarrage concerne l’alias
de police « Monospace ». Journaux : `build/large-personal-validation.log` et
`build/large-personal-validation/`.

Ces essais locaux ne remplacent pas le contrôle d’une caméra réelle, la
comparaison scientifique finale avec Siril ni la qualification de distribution
publique décrite dans le [bilan V2](../releases/V2_READINESS.md).


## 11 octobre 2026 — Composition multifiltre guidée (1.17.0)

La famille Couleur propose RGB/LRGB/HOO/SHO et Personnalisée. Les palettes
attribuent les couches intégrées automatiquement et affichent leurs attributions
sur deux lignes ; les sélecteurs détaillés restent dans Personnalisée. Les
filtres absents/ambigus sont nommés et bloquent la composition. L’inspection
individuelle des empilements reste accessible. Voir [ADR-028](../decisions/ADR-028-channel-composition.md).

L’apport de luminance va de 0 à 100 %. Les tests vérifient le résultat RGB à 0 %
y compris avec une L sans étoiles, le mélange à 50 %, le doublement contrôlé
avec L = 2 × RGB, les rapports des canaux d’une image colorée, l’identité entre
l’ancien défaut et 100 % explicite, les valeurs invalides et l’annulation.
Le parcours natif contrôle les quatre palettes, le refus SHO sans SII puis sa
réussite après ajout/empilement de SII, le mode personnalisé, les choix en attente
sans changement des pixels, l’annulation conservant le résultat précédent et
l’accès à un empilement individuel. La restauration du pourcentage après une
interaction est aussi vérifiée. Les choix sont comparés après sauvegarde/relecture
JSON du projet lié et sérialisation/relecture de la session ; les valeurs
hors plage sont refusées. L’astrométrie de l’image composée est retirée, car
sa grille a changé ; les empilements sources restent disponibles.

**50/50 tests Release passent, 9,20 s** (`build/composition-tests.log`).
Le banc graphique supplémentaire réussit (`build/composition-ui-validation.log`).
Une assertion du banc a été corrigée pour chercher les sélecteurs instanciés
par Repeater dans l’arbre visuel, avec vérification du pointeur avant lecture.
Ce problème du banc de contrôle n’était pas une erreur de traitement.

Composition inspectée en français à **1440 × 960**, **1100 × 800** et
**900 × 700**, avec une région de l’image réelle Orion. États conservés :
LRGB à 50 %, SHO sans SII et Personnalisée. Le panneau compact défile et garde
les commandes de comparaison/zoom dans leurs zones réservées. Neuf captures :
`previews/native/1.17.0-composition-*.png`. Les couches de cette fixture sont
dérivées du RGB ; leurs noms HA/OIII/SII ne prouvent aucune acquisition réelle
en bande étroite. La qualification d’une vraie série T33 et la comparaison
finale Siril restent à faire. Chaque nouveau réglage possède une aide française,
et le guide contient une carte Composition multifiltre.


### Livraison macOS 1.17.0

DMG recréé après le commit fonctionnel `1ce20d6` :
`build/release/AstroCapture-1.17.0.dmg`, **114 412 348 octets**.
SHA-256 : `fb0f6a8b1f4ea383a40560e6c303500bda6031939ba62600faf4d55ae3492223`.
Exécutable : `1a7f9982b1f93c521e0005724a4915f8d9bae6729ce079a2776f373386fd76dd`,
identique entre le bundle compilé et celui du DMG.

Somme de contrôle, version 1.17.0 et signature locale vérifiées ; 200 fichiers
Mach-O sans dépendance vers Homebrew ni les dossiers utilisateur, quatre
auxiliaires d’astrométrie démarrables et trois archives sources tierces présentes.
Journal : `build/composition-dmg-verification.log`.

Le parcours personnel a aussi été relancé : sauvegarde et exports, reprise
dans un second processus, protection des fichiers/résultats sur un volume
privé plein de 64 Mio et démarrage natif Cocoa depuis le DMG en lecture seule.
Les contrôles réussissent, les préférences utilisateur sont protégées et les
volumes temporaires sont démontés. Aucun échec de chargement QML ; le seul
diagnostic de démarrage concerne l’alias de police « Monospace ». Journaux :
`build/composition-personal-validation.log` et
`build/composition-personal-validation/`.


## Lot 1.18.0 — Observation

L’onglet natif **Observation**, indépendant de Ciel profond et placé avant Guide,
a été contrôlé en français à 1440 × 960, 1100 × 800 et 900 × 700. Les captures
`previews/native/observation/` montrent les calculs réels de M42 et de la Lune,
les suggestions calculées et un état simulé de perte de connexion. Les composants
Atelier partagés fournissent les commandes, aides, états de focus et contrôles ;
les courbes et la carte utilisent les couleurs sémantiques. Les panneaux défilent
sur la petite taille pour conserver l’accès aux réglages et aux détails horaires.

- Catalogue OpenNGC inclus, Lune et sept planètes, recherche en ligne facultative.
- Favoris d’objets et de lieux enregistrés localement et restaurés, y compris
  les coordonnées d’objets trouvés en ligne ; noms personnalisables pour les lieux.
- Suggestions pour le lieu et la date, avec créneaux, hauteur et obscurité explicites.
  Les limites des suggestions et les critères propres à chaque famille sont affichés.
- Horaires du Soleil, périodes de jour et de nuit astronomique, crépuscules et cas polaires.
- Nuit, horizon et carte mondiale liés au même instant ; date locale, UTC, changement d’heure.
- Messages d’absence/perte/rétablissement de connexion ; conservation des résultats en cas d’échec.

Les positions topocentriques mobiles ont été comparées aux données indépendantes
[NASA/JPL Horizons](https://ssd.jpl.nasa.gov/horizons/) pour cinq cas aux deux
hémisphères et près du cercle polaire : écarts d’altitude et d’azimut inférieurs
à une minute d’arc. Les horaires solaires ont été comparés à
[l’API de l’US Naval Observatory](https://aa.usno.navy.mil/data/api) à Paris et Sydney,
avec une tolérance de 90 secondes sur des valeurs arrondies à la minute ; le jour
polaire à Tromsø a été vérifié. Les réponses de référence sont conservées dans les
fixtures de tests. Ces contrôles ne garantissent pas le pointage du télescope ou
les conditions météorologiques réelles.

Les tests couvrent aussi les changements d’heure (23 et 25 heures), les positions
invalides sans remplacement des résultats, les favoris persistants, les critères
et l’actualisation des suggestions, les transitions de connexion simulées et les
interfaces aux trois tailles. Les 51 tests de régression passent ; les tests ciblés
d’Observation et de la fenêtre complète ont été relancés après les derniers ajustements.
Les recherches réelles de Toulouse et de Vega sont
vérifiées séparément ; les tests ordinaires restent indépendants d’Internet.
