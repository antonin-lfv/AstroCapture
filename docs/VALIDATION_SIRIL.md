# Validation sur le jeu M8/M20 du tutoriel Siril

Ce compte rendu conserve les observations datées des lots testés. Pour les
fonctions et commandes actuelles, consulter le [README](../README.md) et le guide
intégré. Le [bilan V2](releases/V2_READINESS.md) centralise les contrôles restants,
dont la comparaison indépendante finale avec Siril.

7 octobre 2026. Source : https://siril.org/fr/tutorials/tuto-scripts/

## Données et résultat

Validation sur les fichiers fournis localement dans le dossier Siril Tuto, sans modification des originaux :

- 15 brutes de 180 s, 15 darks de 180 s, 15 flats de 3 s et 15 darks de flats de 3 s (archive nommée Offsets_3s).
- Calibration brute, correction des pixels chauds, alignement des étoiles, normalisation et rejet médiane/MAD à 3 sigma.
- 15 poses acceptées, aucune pose exclue ; exposition cumulée 2700 s (45 min).
- Résultat RGB linéaire de 6244 × 4172 pixels après recadrage commun.
- 29 396 561 échantillons rejetés lors de l’intégration. Il s’agit de valeurs de pixels/canaux, pas de poses exclues.
- Résultat de validation : build/siril-validation/siril-validation-linear.fits. Aperçu réduit : build/siril-validation/stack-preview.png.

Cela vérifie la chaîne sur ce jeu réel, sans établir une équivalence de précision ou de qualité avec Siril. L’aperçu applique l’étirement adaptatif et des gains estimés sur le fond de ciel ; les couleurs ne sont pas calibrées photométriquement.

## Défauts corrigés

Le mode « Poses déjà calibrées » était activé par défaut : des fichiers de calibration pouvaient être ignorés. Le mode brut est désormais le défaut ; les réglages contradictoires sont signalés.

L’étirement initial fixe était trop faible pour cette série. Les étoiles les plus brillantes imposaient un point blanc élevé et les nébulosités restaient sombres. L’étirement adapte maintenant les tons moyens au fond, sans réduire le point blanc ; les gains initiaux du fond sont visibles et modifiables.

Un clic sur une pose peut remplacer l’aperçu par une source sans modifier le résultat exporté. L’interface identifie désormais la vue affichée et permet de revenir au résultat.

## Transposition documentée pour les lots 1.4.0 et 1.7.0

Le tutoriel local TUTORIEL_M8_M20_ASTROCAPTURE.md décrit le parcours 1.4.0 ;
ses anciens noms de commandes ne constituent pas le guide de la version actuelle.
Gradient polynomial avec échantillons modifiables, asinh/GHS, SCNR, déconvolution
Richardson–Lucy, débruitage multiéchelle et recettes réversibles sont disponibles
dans ce lot.

La photométrie par CSV de références déjà identifiées reste disponible. En 1.7.0,
AstroCapture ajoute un solveur WCS TAN guidé, une PCC par modèle de températures
Gaia et une SPCC par spectres XP mesurés avec réponse de l’équipement importée.
Ces implémentations indépendantes ne sont pas les algorithmes de Siril et leur
équivalence numérique n’a pas été établie. Comparer d’abord les intégrations
linéaires, puis les opérations de finition séparément.

## Validation 1.4.0 — 8 octobre 2026

30 tests passent. Les fixtures couvrent GHS harmonique analytique/inverse/monotonie, PSF normalisées, neutralité et restauration d’étoiles synthétiques, annulation, réduction de l’erreur sur bruit connu, masques exacts et gains photométriques connus, rejet des références saturées/bords, garde des recettes et persistance. L’export FITS traité conserve exactement les flottants supérieurs à 1.

Sur Light_M20_180s_1x1_gain100_20200818-222530_-9.8C_0001.fit (26 MP), validation d’ouverture/aperçu et nouvelle interface française : GHS + bruit deux échelles + masque circulaire, 3311 ms. PSF gaussienne estimée 3,63704 px sur 386 étoiles, après élargissement à 1500 candidates pour éviter la sélection exclusive des étoiles les plus brillantes/saturées. Richardson–Lucy manuel FWHM 3 px, deux itérations, amortissement 0,002 : 3011 ms. Temps ponctuels sur ce Mac, pas un benchmark comparatif. La brute n’a pas été calibrée pour ce test de chargement/traitement ; ce n’est pas une validation scientifique complète d’acquisition. Pas de validation de couleurs sur catalogue réel ni de validation matérielle nouvelle.

## Vérification du dépôt le 9 octobre 2026

Compilation Release réussie et 30 tests sur 30 réussis. Cette vérification ne relance pas l’intégration M8/M20 ni la comparaison Siril. Le DMG 1.4.0 est présent ; aucun nouvel installateur n’a été créé pendant cette vérification.

## Validation 1.7.0 — 10 octobre 2026

### Astrométrie et calibration sur Orion

Le parcours utilise le FITS RGB linéaire d’Orion fourni localement, 6 244 × 4 170
pixels Float32 ; l’original reste intact. Indications initiales : centre ICRS
83,82083° / −5,39°, échantillonnage 1,22134″/pixel, limite Gaia G = 16.
Le catalogue et les spectres XP sont téléchargés auprès de l’ESA Gaia DR3.

Le solveur associe **676 étoiles**, avec RMS **0,535 pixel** (0,669″), couverture
**98 %**, échantillonnage ajusté **1,25099″/pixel** et centre
83,8195363° / −5,3908017°. La résolution conserve exactement les pixels linéaires.

| Méthode | Candidates | Acceptées | Rejetées | Gains R / V / B | Dispersion |
| --- | ---: | ---: | ---: | --- | ---: |
| PCC, températures Gaia | 150 | 128 | 22 | 1,17817 / 1 / 1,48769 | 0,09630 mag |
| SPCC, limite par défaut | 60 | 44 | 16 | 1,12547 / 1 / 1,56666 | 0,16415 mag |
| SPCC, limite portée à 150 | 150 | 120 | 30 | 1,16941 / 1 / 1,48283 | 0,11817 mag |

Les rejets PCC comprennent 21 références incohérentes et une de signal faible.
La SPCC à 150 comprend 28 références incohérentes et deux de signal faible ;
aucun rejet de bord ou de saturation sur cette sélection. Les dispersions sont
des résidus du modèle, pas une mesure de l’exactitude des couleurs de la caméra.

**Limite scientifique du contrôle :** la SPCC utilise les vrais spectres XP,
mais une réponse Bessell virtuelle importée pour tester l’intégration spectrale.
Cette courbe n’est pas une mesure du matériel ayant produit Orion. Le fond de
ciel n’a pas été neutralisé pour ce parcours ; le rendu n’est donc pas une
validation des couleurs physiques d’Orion. Pour une calibration du matériel,
il faut corriger les gradients et importer sa réponse photonique RVB mesurée.
La PCC reste un modèle de continuum ; extinction et raies stellaires ne sont
pas modélisées. Aucune comparaison numérique de PCC/SPCC avec Siril n’est revendiquée.

### Persistance, annulation et performances

- L’annulation d’une résolution conserve le WCS précédent, la finition et
  l’empreinte exacte de l’image Float32.
- L’export FITS linéaire relu conserve chaque échantillon Float32 à l’identique
  et les coordonnées célestes à 10⁻¹⁰ degré sur le point contrôlé.
- Le projet portable est sauvegardé, fermé puis rouvert. WCS, catalogue complet,
  réponse spectrale, références SPCC, réglages et empreinte Float32 sont identiques.
- Une seconde SPCC à 150 références, déjà en cache, conserve exactement les
  réglages et s’achève en 10,607 s en Debug sur ce Mac. Mesure ponctuelle, pas
  un benchmark comparatif ni une promesse de durée pour un téléchargement.
- Les lots de dix spectres et trois téléchargements simultanés évitent
  l’expiration prématurée des réponses ESA progressives. L’annulation reste
  disponible ; les données incomplètes ou corrompues ne sont pas appliquées.
- La comparaison de finition lit chaque carte Qt une seule fois et l’interface
  reçoit une vue légère de l’astrométrie. Les listes scientifiques complètes
  restent dans la session et les projets.

### Tests et interface

Les **38 tests Release passent** (27,33 s). Ils couvrent notamment les inverses
WCS, rotation/miroir, autre champ refusé, géométrie, spectres connus, blanc solaire,
gains photométriques connus, identifiants Gaia 64 bits, mouvements propres,
VOTable/ZIP ESA réels, fichiers corrompus et conservation FITS Float32/WCS.
Après le dernier ajustement d’interface, le test Ciel profond passe de nouveau
(3,49 s), avec une régression du choix PCC → SPCC → PCC : limites 60 puis 150,
même après restauration d’une valeur dans le champ.

L’interface Qt/QML réelle, en français avec Orion, est inspectée à exactement
1440 × 960, 1100 × 800 et 900 × 700 via le rendu logiciel hors écran. Les deux
étapes WCS et couleurs restent lisibles, avec pied fixe, aides et commandes.
Les captures sont dans `docs/design/previews/native/1.7.0-*`.
Le détail des formats et des limites est dans
[ADR-016](decisions/ADR-016-astrometry-colors.md).

### Installateur 1.7.0 vérifié

Le code et les contrôles Orion sont committés (`34b208e`, `d99fd9b`) avant la
reconstruction finale du paquet. Le DMG mesure **85 394 898 octets** (81,44 Mio).
`hdiutil verify` valide toutes ses sommes de contrôle. Monté en lecture seule,
il contient la version 1.7.0 ; son exécutable correspond exactement à celui
de l’application construite. La signature locale passe la vérification profonde
et stricte. QtNetwork, les extensions TLS et la notice Gaia/CALSPEC/SVO sont
présents ; la notice est identique à celle du dépôt. Le contrôle de dépendances
du paquet ne trouve aucune référence absolue à Homebrew dans les binaires.

Le montage de contrôle est démonté par son propre chemin. La signature demeure
locale ad hoc ; Developer ID et notarisation restent dans la feuille de route.

SHA-256 du DMG : `e5e3cbbbd4dfcc64ad3f3c6088658f46d0f44fc329b02476a215e33311d4fbce`.
SHA-256 de l’exécutable : `e6c4f155a160c37ebe05fcf0e35b6a512c95cd70dd5e66ef14491e494d6c8d5d`.

## Validation 1.8.0 — 10 octobre 2026

Ce lot prolonge le parcours 1.7.0 ; le solveur, PCC et SPCC restent inchangés.
Le contrôle utilise le projet portable WCS/SPCC d’Orion sauvegardé en 1.7.0,
avec ses 6 244 × 4 170 pixels Float32. Le fichier FITS original n’est pas modifié.

### Cible et profils

- La recherche M42 sur le service CDS réel propose **83,8201° / −5,3876°**,
  source SIMBAD. La réponse remplit les indications du dialogue, sans modifier
  le WCS vérifié, les réglages de finition ou l’empreinte de l’image linéaire.
- Une cible absente et une recherche annulée conservent le WCS et le traitement.
  La fixture publique M42 vérifie également l’encodage d’un nom, les erreurs XML,
  champs incomplets, ambiguïtés, nombres non finis et bornes. Un repli NED valide
  est accepté sans mélanger les coordonnées de résolveurs différents.
- La réponse Bessell virtuelle de contrôle est enregistrée depuis le dialogue
  avec caméra, filtres, provenance et notes. Le choix ne change pas les réglages.
  Suppression/réimport JSON et réouverture des préférences retrouvent exactement
  la même courbe, les empreintes et les métadonnées.
- La régression refuse les empreintes corrompues, collisions de noms et imports
  conflictuels sans remplacement partiel. Une courbe structurellement valide dont
  l’intégrale sous-déborde à zéro est refusée avant publication dans le projet.

### Coordonnées et sauvegarde

- Transformation du résultat : quart de tour, miroir horizontal, recadrage
  x = 20, y = 30, 1 800 × 1 500, puis échelle 0,5. Résultat **900 × 750**.
  Le point source (1 234, 1 500) retrouve sa position céleste à **10⁻¹⁰ degré**
  après transformation ; il s’agit d’une tolérance de calcul, pas d’exactitude
  astrométrique physique.
- Le pointeur sur le pixel (450, 300) affiche **AD 05:37:18.77, Déc −04:34:51.5**
  aux trois tailles de fenêtre. Adapter, 100 %, 170 % et déplacement de la vue
  retrouvent le même pixel et les mêmes valeurs. Le vrai bouton de copie conserve
  la dernière position de l’image. Avant/Résultat/Partagé sont cohérents, avec une
  image de comparaison de mêmes dimensions transformées.
- Le projet portable 1.8.0 est sauvegardé, fermé puis rouvert après retrait du
  profil global. Courbe, métadonnées, état astrométrique, références, réglages et
  empreinte Float32 restent identiques ; le nom de recherche M42 est restauré.
- Les tests couvrent aussi la restauration de session, les coordonnées hors image
  et l’absence de coordonnées sur la comparaison brute non transformée.

Les **39 tests Release** passent en **26,01 s**, dont le nouveau résolveur de noms.
Les trois tests concernés passent aussi en Debug (5,80 s). Le dernier ajustement
QML des confirmations repasse le test Ciel profond Release (3,35 s). Le contrôle
natif isolé utilise de vraies instances de l’application en français, aux trois
tailles, avec des préférences et caches de projets temporaires ; il ne touche
pas la bibliothèque personnelle ni le presse-papiers Cocoa de l’utilisateur.

La réponse Bessell du contrôle est **virtuelle**, pas une mesure de la caméra
ayant acquis Orion. Aucune nouvelle validation des couleurs physiques ni
comparaison numérique avec Siril n’est revendiquée. Les fonctionnalités et
formats du lot sont décrits dans l’[ADR 017](decisions/ADR-017-astrometry-workflow.md).

### Installateur 1.8.0 vérifié

Reconstruction après `bffd7f5`, une fois le parcours natif et les contrôles committés.
Le DMG mesure **88 051 365 octets** (83,97 Mio). Toutes les sommes de contrôle
de `hdiutil verify` sont valides. Le montage en lecture seule contient la version
1.8.0 ; son exécutable correspond exactement au paquet Release construit.
La signature locale passe le contrôle profond et strict. QtNetwork, les trois
extensions TLS et la notice Gaia/CALSPEC/SVO/CDS sont présents. La notice est
identique au fichier source. **193 binaires Mach-O** sont contrôlés, sans aucune
dépendance absolue à Homebrew. Le montage est démonté par son propre chemin.
La signature reste ad hoc ; Developer ID et notarisation restent dans la feuille de route.

SHA256 du DMG : `80140f261bf1e40a60ae24b8de98d6607f7adb84086cbd123bedc4bcc4a76895`.
SHA256 de l’exécutable : `79546c94e013eec42d56b6453e9baa7d86f91a6c17f397f573755ad7f8fdac3a`.
