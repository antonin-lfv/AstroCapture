# Résolution aveugle locale et SIP

La résolution aveugle utilise Astrometry.net sur une copie de luminance FITS
Float32 sans coordonnées ni échelle. Les index restent locaux. Le moteur natif
est embarqué dans le DMG ; l'index 4109 (47,5 Mio, motifs de 42–60 minutes d'arc)
est téléchargé à la demande avec vérification de taille et du MD5 officiel.
D'autres champs nécessitent d'autres index, sélectionnables par dossier.

Le résultat doit passer une vérification indépendante Gaia : RMS, couverture,
associations et minimum de références adapté à l'ordre SIP. Une annulation, un
délai dépassé ou une solution invalide conserve l'ancien résultat.
Les conversions TAN-SIP (ordre 2 à 5) utilisent les polynômes directs et une
inversion Newton contrôlée. Les transformations affines remappent analytiquement
les coefficients ; projets, sessions, FITS et curseur conservent ce modèle.
Les anciens projets TAN restent compatibles. Les pixels ne sont pas modifiés.

Validation : aller/retour SIP, référence polynomiale indépendante, recadrage,
rotation, miroir, échelle anisotrope, FITS Float32 et annulation. Sur Orion
6 244 × 4 170, sans hints et avec des hints volontairement faux : 675 associations
Gaia, RMS 0,523 px, échantillonnage 1,251″/px, SIP ordre 3.

Références : https://astrometry.net/doc/readme.html ;
https://fits.gsfc.nasa.gov/registry/sip/SIP_distortion_v1_0.pdf ;
https://data.astrometry.net/4100/md5sums.txt.
