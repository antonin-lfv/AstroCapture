# ADR 016 — WCS et calibration sur catalogue

10 octobre 2026 · AstroCapture 1.7.0 · implémentation indépendante.

## Astrométrie

Le solveur guidé utilise une projection TAN/ICRS, un centre approximatif en degrés
et un échantillonnage en secondes d’arc/pixel. Les triangles d’étoiles identifient
rotation, translation, échelle et parité ; un ajustement affine RANSAC puis des
associations répétées affinent la matrice CD et CRPIX. Les pixels restent intacts.

La publication exige huit associations cohérentes, une couverture d’au moins
15 % du champ, des résidus individuels ≤ 2,5 px et une RMS ≤ 2 px. Centre et
échelle doivent rester compatibles avec la recherche guidée. L’interface expose
RMS en pixels et secondes d’arc, couverture et repères. Le solveur refuse une
solution insuffisante, sans remplacer le résultat précédent. Ces critères ne
constituent pas une garantie universelle : inspecter les repères dans tout le champ.

Gaia DR3 est interrogé par TAP/ADQL auprès de l’ESA, ou importé en CSV. Les identifiants
64 bits restent des chaînes. Les mouvements propres sont propagés de leur époque
de référence à DATE-OBS, sinon à la date courante, sur la sphère. En-tête CSV :

```csv
source_id,ra,dec,pmra,pmdec,ref_epoch,phot_g_mean_mag,teff_gspphot,has_xp_sampled
```

`source_id,ra,dec,phot_g_mean_mag` sont obligatoires. Les mouvements propres sont
en mas/an, `pmra` inclut cos(dec), les coordonnées en degrés ICRS et Teff en kelvins.
Sans mouvements propres, le catalogue conserve son époque d’origine : privilégier
les exports complets. Au plus 5 000 étoiles et 8 MiB. La requête en ligne couvre
au maximum huit degrés de rayon et transmet des coordonnées, jamais l’image.

L’import FITS accepte TAN sans distorsion, matrice CD ou PC/CDELT, CRPIX à origine
1, degrés et système ICRS. L’application emploie les coordonnées à origine zéro
dans l’ordre réel des lignes stockées ; aucune inversion implicite n’intervient.
Les coordonnées RA/DEC numériques ou sexagésimales et XPIXSZ/FOCALLEN fournissent
des indications initiales. Un WCS importé doit être vérifié avant PCC/SPCC.

Le WCS du premier fichier de référence suit le recadrage commun de l’intégration.
L’export FITS flottant conserve le WCS, y compris après les orientations,
recadrages, rotations libres et changements d’échelle d’une recette. Le calcul
respecte les centres de pixels et les dimensions arrondies d’OpenCV. Une composition
de filtres change l’image et nécessite actuellement une nouvelle résolution.

Limites explicites : recherche aveugle tout ciel, SIP/TPV, catalogue SIMBAD par nom,
annotations d’objets et coordonnées interactives ne sont pas livrés dans ce lot.
Un champ présentant trop de distorsion doit utiliser un solveur externe.

## Couleurs

La calibration est appliquée après extraction du gradient et piédestal, avant
étirement. Elle refuse les masques, les entrées de recettes déjà étirées ou
orientées et les canaux mono dupliqués. L’aperçu final peut rester étiré.

- PCC : spectres de continuum Planck à partir de Teff Gaia, intégrés dans Bessell
  R/V/B ou la réponse importée. C’est une approximation : extinction et raies
  d’absorption ne sont pas modélisées. Ce moteur ne reproduit pas les coefficients
  Siril et ne doit pas être présenté comme une calibration spectrale mesurée.
- SPCC : véritables spectres Gaia DR3 XP échantillonnés, reçus de l’ESA par lots
  de 10, trois requêtes simultanées au maximum, VOTable BINARY2 dans ZIP. Vérification des identifiants, unités, ordre
  spectral, indicateurs nuls, tailles, répertoire ZIP, décompression et CRC.
  L’intégration photonique utilise `∫ Fλ × λ × réponse dλ` sur l’union des grilles
  spectre/réponse, avec intégration de Simpson par segment. Le facteur commun hc
  s’annule dans les rapports de couleurs.

La SPCC exige une réponse **mesurée** de l’équipement : QE du capteur × transmission
des filtres RVB × filtre supplémentaire éventuel. L’import stocke les données
et l’empreinte SHA256 ; il ne dépend plus du chemin du CSV d’origine. Format :

```csv
wavelength_nm,red,green,blue
```

Longueurs strictement croissantes entre 336 et 1 020 nm, valeurs 0–1, au plus
2 000 échantillons, extrémités nulles et couverture complète. Ne pas normaliser
les trois canaux indépendamment. Le profil Bessell est une référence générique
PCC ; aucun profil de caméra n’est inventé. La bibliothèque de profils mesurés,
la composition interactive QE/filtres et le mode bande étroite restent à développer.

Le blanc est soit le spectre solaire CALSPEC, soit Fν constant (AB, donc
Fλ ∝ 1/λ²). Les flux synthétiques sont divisés par ceux du blanc, puis normalisés
au vert. La photométrie d’ouverture soustrait la médiane locale de l’anneau dans
chaque canal. Elle rejette bords, saturation, flux faibles (SNR robuste > 5) et
rapports aberrants à 3 MAD. Les médianes des rapports logarithmiques donnent les
gains R/B ; G reste 1. Au moins huit références acceptées sont nécessaires pour
PCC/SPCC. Le nombre maximal de candidates est réglable de 8 à 150 : 60 par défaut
pour SPCC, 150 pour PCC. Le bilan distingue les causes de rejet et affiche dispersion en magnitudes
et gains. Il s’agit de couleurs relatives, pas de magnitudes absolues.

Les gains manuels RVB initiaux sont remis à 1 après préparation de la calibration,
pour ne pas superposer la neutralisation automatique du fond. Les réglages restent
réversibles par Cmd+Z et A/B. Les références sont liées au SHA256 des pixels linéaires
et des dimensions : un préréglage d’une autre image est refusé tant que sa calibration
n’a pas été recalculée. Les anciens CSV `x,y,red,green,blue` restent utilisables.

## Persistance, réseau et validation

WCS, catalogue, réponse, diagnostics, références et provenance suivent la session
et le projet portable, avec validation avant restauration. Le projet JSON conserve
le WCS et les références ; ses résultats doivent être reconstruits depuis les
sources. Le WCS sauvegardé ne peut être repris que si le résultat recréé possède
exactement la même empreinte. Les flottants négatifs et HDR restent intacts.

Les requêtes sont bornées à 120 s au total, 45 s d’inactivité et 8 MiB, annulables, sans publier de résultat
partiel. Cache Gaia de 64 MiB maximum : catalogues sept jours, spectres DR3
immuables. Les données déjà reçues permettent une répétition hors ligne de la
même requête ; un nouveau champ ou un nouveau lot requiert internet.

Tests : projection analytique et inverses (passage RA=0, haute déclinaison),
rotation et miroir avec catalogue contaminé, refus d’un autre champ, annulation,
intégrale spectrale connue, blanc solaire neutre, gains caméra connus de bout
en bout, identifiants Gaia 64 bits, mouvements propres, VOTable réel, lots ZIP
réels/corrompus, propagation géométrique et FITS Float32/WCS sans perte.
Le bilan natif sur Orion et la vérification de l’installateur sont consignés dans
`docs/VALIDATION_SIRIL.md` après leur exécution.

## Sources primaires et données

- [Normes WCS FITS, NASA](https://fits.gsfc.nasa.gov/fits_wcs.html).
- [Solveur Siril](https://siril.readthedocs.io/en/stable/astrometry/platesolving.html),
  [PCC](https://siril.readthedocs.io/en/stable/processing/color-calibration/pcc.html),
  [SPCC](https://siril.readthedocs.io/en/stable/processing/color-calibration/spcc.html).
- [ESA Gaia DR3 et crédits](https://www.cosmos.esa.int/web/gaia-users/credits),
  [modèle de données](https://gea.esac.esa.int/archive/documentation/GDR3/Gaia_archive/chap_datamodel/sec_dm_main_source_catalogue/ssec_dm_gaia_source.html).
- [CALSPEC solaire STIS 002](https://ssb.stsci.edu/cdbs/current_calspec/sun_reference_stis_002.fits).
  SHA256 original : `20133cdf2a402655e3726de6b334bf09d05c9429ebd27ebf5492dc37fc89cfd8`.
  Resampling linéaire à 2 nm ; seule la forme spectrale relative intervient.
- [SVO Bessell R](https://svo2.cab.inta-csic.es/theory/fps/fps.php?ID=Generic/Bessell.R),
  [V](https://svo2.cab.inta-csic.es/theory/fps/fps.php?ID=Generic/Bessell.V),
  [B](https://svo2.cab.inta-csic.es/theory/fps/fps.php?ID=Generic/Bessell.B).
  Échantillons de transmission conservés et grilles réunies en nm.
  Empreintes XML originales R/V/B :
  `322c3ec16ff9de78512627e9fe9e036709dd272f1ca42316e32c0f6df6d5568b`,
  `2d589dbb08c19da66589b0ad97b934bac7b171eafc774d6c6d8af90393974954`,
  `2b4f1de5b4fe7a9c07fd629c7001b0730493d45242da783d7ddef0ffd37d2291`.

Les notices de données sont incluses dans l’application. Aucun code de Siril n’a
été repris. Les paramètres sont des repères familiers, sans promesse d’équivalence.
