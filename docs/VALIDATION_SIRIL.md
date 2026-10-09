# Validation sur le jeu M8/M20 du tutoriel Siril

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

## État actuel de la transposition

Le guide pratique à jour est TUTORIEL_M8_M20_ASTROCAPTURE.md. Gradient polynomial avec échantillons modifiables, asinh/GHS, SCNR, déconvolution Richardson–Lucy, débruitage multiéchelle et recettes réversibles sont disponibles en 1.4.0.

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
