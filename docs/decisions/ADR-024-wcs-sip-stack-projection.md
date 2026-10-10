# ADR-024 — Projection WCS/SIP des poses à l’empilement

Date : 10 octobre 2026. Livré en 1.13.0 sur opt-in, désactivé par défaut.

## Problème et périmètre

L’alignement stellaire précédent utilise une similitude ; il ignore les
variations de distorsion optique entre poses. Les solutions TAN-SIP étaient
lues, résolues et exportées, mais leurs polynômes ne projetaient pas les pixels.

La nouvelle option exige des FITS TAN ou TAN-SIP déjà résolus, pour **chaque
pose**, référence comprise. Elle ne lance pas automatiquement l’astrométrie
sur une séquence. Toutes les sources sont projetées sur le plan TAN de la
référence, avec mêmes CRVAL, CRPIX et CD initiaux et sans SIP de sortie. La
référence est donc projetée elle aussi. L’échelle drizzle et le recadrage
remappent ensuite CRPIX/CD ; aucun coefficient SIP obsolète n’est réexporté.

## Transformation et échantillonnage

Une homographie entre bases des plans tangents remplace les conversions
trigonométriques par pixel. Les polynômes SIP directs et leurs dérivées sont
évalués en double précision ; l’inverse utilise Newton, vérifié à 10⁻⁶ px.
Tests de géométrie sur une grille 17 × 17, aller-retour ≤ 10⁻⁵ px,
Jacobiens sans repli, rapport de surface 0,1–10, coordonnées bornées. Les
évaluations SIP vérifient aussi leur Jacobien à l’exécution. Les supports
courbes du drizzle vérifient leur convexité et leur aire par morceau.

Empilement classique : échantillonnage inverse bilinéaire RGB Float32, hors
champ explicitement NaN. Pas de contribution d’un voisin de poids nul à un
NaN. Drizzle : quadrilatères subdivisés sur erreur de milieux/centre ≤ 0,001
pixel de sortie ; profondeur maximale 5, échec explicite si dépassée. Les
aires découpées dans chaque pixel de sortie sont normalisées par l’aire
totale du support projeté. Le mode affine précédent garde son chemin exact.
Le Bayer natif conserve un poids par couleur et n’invente pas les canaux absents.

Cette tolérance est celle du critère adaptatif échantillonné ; ce n’est pas
une preuve analytique de borne uniforme pour toute distorsion d’ordre 5.
Les grilles/Jacobiens et les empreintes réduisent le risque de géométrie
aberrante ; les champs réels doivent encore être qualifiés.

Les valeurs conservent la convention d’intensité des sources. L’option ne
corrige pas les flats pour une photométrie en flux par pixel, ne fournit pas
une pixel-area map scientifique et ne calibre pas le flux absolu. Ne pas
revendiquer une photométrie intégrée corrigée uniquement grâce au SIP.

## Contrôle et refus

Les étoiles sont détectées séparément dans la référence et la source. La
projection source → référence **avec son SIP original** doit retrouver au
moins 12 étoiles uniques dans quatre cases d’une grille 3 × 3, couvrant 35 %
de la largeur et de la hauteur. Distance candidate < 1,5 px ; RMS ≤ 0,75 px **de la référence initiale**,
avant passage sur la grille TAN sans SIP et changement d’échelle drizzle.
Le test de la référence sur elle-même vérifie surtout la présence et la
répartition d’étoiles ; son RMS nul ne qualifie pas sa solution absolue.

Les sources incohérentes/absentes sont écartées, jamais remplacées par une
similitude. Deux sources validées au minimum par filtre sont nécessaires.
Si l’intégration échoue ou est annulée, le contrôleur ne publie aucun résultat
partiel et conserve le résultat précédent. Le bilan affiche les étoiles,
RMS et ordre SIP de chaque source acceptée. Les noms et raisons d’exclusion figurent
dans le bilan sauvegardé et dans ses détails dépliables, même après retouche. Deux WCS également faux peuvent rester relativement
cohérents : la sortie garde `verified=false`. Résolution externe de la sortie
requise avant PCC/SPCC.

## Compatibilité et ressources

Paramètre ajouté en fin d’API avec valeur par défaut false. Les projets
anciens restent dans le mode antérieur. Option, bilan et image exacte sont
conservés dans les projets portables et les sessions. Contrôles de bornes,
RMS, ordre SIP et unicité des entrées lors de la restauration du bilan.
Aucune nouvelle copie pleine résolution par pose conservée en mémoire ;
seul le masque de couverture temporaire ajoute un octet par pixel en drizzle
WCS. Plafonds précédents : 32 MP avant recadrage, 500 poses par filtre,
4 Gio pour le projet portable et 64 références d’image.

## Fondements

- [Convention SIP enregistrée par la NASA](https://fits.gsfc.nasa.gov/registry/sip.html) : polynômes de distorsion et coordonnées TAN-SIP.
- [STScI — concept du drizzle](https://hst-docs.stsci.edu/drizzpac/chapter-3-description-of-the-drizzle-algorithm/3-2-drizzle-concept) : transformation du support de pixel avant calcul des recouvrements.
- [STScI — données de distorsion](https://hst-docs.stsci.edu/drizzpac/chapter-4-astrometric-information-in-the-header/4-3-distortion-information-in-pipeline-calibrated-images) : coordonnées de sortie cohérentes avec la projection appliquée.

## Validation

- Compilation release réussie ; **47/47 tests réussis**, 21,86 s
  (`build/wcs-projection-final-tests.log`).
- Transformations comparées à l’évaluation sphérique indépendante de
  `CelestialWcs` : centres TAN différents, rotation/parité, passage RA 0°,
  pôle proche, SIP jusqu’à l’ordre 5, erreur < 10⁻⁶ px.
- Champ analytique distordu : contrôle des étoiles et erreur RGB RMS < 0,002
  en unités des échantillons après empilement/reprojection. Couverture et
  diagnostics en empilement ordinaire/drizzle ; CFA natif sur huit poses.
- Pixel isolé à support SIP courbe : poids total unité ; recouvrement comparé
  à une quadrature indépendante 400 × 400 avec Jacobien analytique. Valeurs
  négatives, nulles et HDR, aucune contamination des couleurs pour les quatre Bayer.
- Refus des WCS absents/incohérents, replis SIP, étoiles insuffisantes,
  annulation ; résultat précédent conservé après échec du contrôleur.
- Action QML réelle : options, bilan acceptés/exclus, sauvegarde/réouverture
  portable avec pixels identiques et export FITS Float32 sans SIP.
  Bilans corrompus refusés, projets anciens toujours compatibles.
- **18 captures** de l’interface QML réelle en français avec Qt, rendu logiciel
  à dimensions exactes 1440 × 960, 1100 × 800 et 900 × 700 : réglages, aide
  entièrement contenue dans la fenêtre (bornes contrôlées), bilan fermé/ouvert,
  finition et guide. `docs/design/previews/native/1.13.0-*.png`.
- Image réelle d’Orion recadrée à 1024 × 768, avec distorsion SIP simulée dans
  une source et WCS volontairement faux dans une autre : deux sources retenues,
  128 correspondances, RMS 0,296005 px ; une source exclue avec sa raison.
  Sortie TAN 1017 × 765 ; environ 1,13 s dans cette fixture sur le Mac de
  développement. **Ce ne sont pas des acquisitions indépendantes** : cela
  contrôle l’intégration et l’interface, pas le gain scientifique sur une
  vraie série avec dither ni une calibration optique de l’instrument.

La comparaison finale sur acquisitions indépendantes avec Siril reste prévue
avec l’utilisateur. Le traitement photométrique de surface, les grands champs,
l’endurance et la qualification des ressources restent dans le bilan V2.


## Distribution locale vérifiée

`build/release/AstroCapture-1.13.0.dmg`, **118 555 868 octets**.

- SHA-256 du DMG : `71dc73638e0e0be31b684b83a631810b260a455e665e1be4002eb6f92b669945`.
- SHA-256 de l’exécutable : `3d9010b088e94d658dc0ed836275a3a5e8b49281de6db8ce658739f56f7a1fa2`.
- Image disque vérifiée, montée en lecture seule ; version 1.13.0 et
  exécutable identique à l’application reconstruite. Signature ad hoc
  vérifiée en profondeur et mode strict ; volume démonté après contrôle.
- Quatre auxiliaires Astrometry démarrent ; archives sources de leurs
  dépendances présentes. 200 fichiers Mach-O contrôlés sans dépendance
  absolue vers Homebrew, `/usr/local` ou un dossier utilisateur.
- Absence de Developer ID/notarisation Apple : la distribution publique et
  l’installation sur un Mac vierge restent des conditions du bilan V2.
