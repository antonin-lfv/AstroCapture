# Traitement séparé des étoiles et du fond

Moteur natif : candidats détectés, centroidage à la résolution d'origine puis
régression RGB d'une PSF circulaire + plan local. Cinq largeurs entre 0,65 et
1,5 × FWHM ; Gaussian ou Moffat. Rejet des bords, mélanges proches, saturation,
amplitudes négatives et RMS normalisée excessive. Il s'agit d'une estimation
inspectable, pas d'une suppression neuronale complète. Les structures étendues
et les étoiles rejetées restent dans le fond. Les nœuds compacts de nébulosité,
les halos, aigrettes et PSF irrégulières exigent une inspection des couches.

Séparation dans le domaine linéaire, après gradient, calibration photométrique
et préparation scientifique. Les recettes doivent placer cette étape avant une
étape non linéaire ou géométrique ; les masques sont désactivés pour cette étape.

Pour conserver une recombinaison neutre exacte malgré l'étirement commun, la
couche stellaire finie est F(original) − F(résidu). Le fond fini reçoit ensuite
ses propres gain, saturation et débruitage ; la couche stellaire ses gain,
saturation et tons moyens. Une réduction de gain agit sur l'intensité, pas sur
le rayon stellaire. Les valeurs neutres renvoient directement F(original).
Les couleurs communes, orientation et géométrie se propagent ; les couches
finales restent additives à l'arrondi Float32 près, sans écrêtage de sauvegarde.

Vues combinée/étoiles/fond indépendantes de l'export principal. Chaque couche
s'exporte en FITS Float32 signé avec WCS/SIP. L'export FITS linéaire conserve
l'intégration source. Paramètres dans préréglages, A/B, annulation et projets ;
couches calculées dans les sessions et projets portables, précision exacte.

Validation : étoiles synthétiques sur gradient et nébulosité connue, émission
étendue seule, saturation, annulation, neutralité bit à bit après étirement,
réduction de la contribution stellaire sans changer la nébulosité distante,
addition des couches, export SIP, inspection des sources, Cmd+Z/A/B,
projet portable et géométrie. Sur Orion 6 244 × 4 170 : 1 672 candidates ajustées
sur 1 819, 147 rejetées, RMS normalisée moyenne 0,0679 (sans calibration couleur).
Contrôle natif français aux trois tailles dans les captures de validation.
