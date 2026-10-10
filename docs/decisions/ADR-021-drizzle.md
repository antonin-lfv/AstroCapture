# ADR-021 — Drizzle mono/RVB contrôlé

Date : 10 octobre 2026. Livré dans AstroCapture 1.10.0.

## Utilisation

Dans Ciel profond → Empilement → Réglages avancés, activer **Drizzle · mono / RVB**.
Choisir ×1, ×1,5 ou ×2 et une fraction de pixel entre 0,5 et 1 ; 0,8 est le
point de départ proposé. L’option est désactivée dans les anciens projets et
pour une nouvelle session. Les réglages, dimensions et avertissements sont
traduits ; chaque réglage possède une aide au survol.

Le gain dépend du sous-échantillonnage des poses et de leurs décalages
sous-pixel. Une grille ×2 contient quatre fois plus de pixels, sans garantir
quatre fois plus de détails. Plusieurs phases, suffisamment de poses et du
signal sont nécessaires. L’autoguidage/dithering reste externe à AstroCapture.

La base scientifique est la reconstruction par empreintes réduites décrite
par [Fruchter et Hook](https://arxiv.org/abs/astro-ph/9808087).
Le [guide Siril](https://siril.readthedocs.io/fr/stable/preprocessing/drizzle.html)
présente aussi les compromis entre échelle, fraction de pixel et couverture.
L’implémentation AstroCapture est indépendante ; une équivalence numérique
avec Siril n’est pas annoncée.

## Projection et intensité

Chaque pose est calibrée à sa résolution native. Les sources Bayer sont
**dématricées avant la projection**. L’extraction facultative du gradient et la
normalisation globale se font sur l’image native. L’alignement stellaire
fournit sa transformation affine vers la référence. Aucun rééchantillonnage
bilinéaire de l’image alignée ne précède le drizzle.

Un carré centré sur chaque pixel source est réduit par la fraction de pixel,
transformé vers la grille de sortie, puis intersecté exactement avec chaque
pixel de sortie concerné. Les rotations utilisent un découpage de polygone ;
les transformations sans rotation utilisent une intersection rectangulaire.
Les aires sont calculées en double précision.

Pour chaque pose et pixel de sortie, la somme pondérée des intensités est
divisée par la surface couverte. Cette valeur et son poids sont stockés dans
un fichier temporaire. L’intégration lit des bandes, rejette les contributions
aberrantes autour de la médiane (1,4826 × MAD, seuil σ), puis calcule la moyenne
pondérée des survivants. Le rejet exige **trois contributions disponibles à ce
pixel**, et pas seulement trois fichiers sélectionnés.

La convention reste celle des sources : **intensité linéaire par échantillon**,
pas comptage de photons par nouveau pixel. Les valeurs 0, négatives ou au-delà
de 1 ne sont pas écrêtées. Une photométrie intégrée doit tenir compte de la
surface angulaire des pixels ; ×2 divise cette surface par quatre. Les poids
et la couverture doivent être considérés lors d’une analyse du bruit corrélé.
Les cartes complètes de poids ne sont pas encore exportables.

## Contrôles et compatibilité

- Au moins deux poses sélectionnées, puis utilisables, par groupe de filtre.
- Limite **32 MP** sur la grille avant recadrage, annoncée et contrôlée avant
  lancement. Un FITS T68 de 6 244 × 4 170 px est compatible avec ×1, pas ×2.
- Bordures géométriques communes recadrées. Les trous intérieurs sont exclus
  par leurs poids ; s’il reste un pixel sans contribution, l’opération échoue
  avec un message explicite. Aucun trou n’est remplacé silencieusement par zéro.
- Erreur, espace temporaire insuffisant ou annulation conserve le résultat
  précédemment utilisable. Les fichiers temporaires sont libérés par RAII.
- Bilan conservé dans Empilement : dimensions, échelle, minimum et moyenne du
  nombre de poses contributrices. Avertissement si les phases estimées restent
  proches ou si certains pixels ont moins de trois contributions. La diversité
  est un diagnostic géométrique approximatif, pas une mesure de résolution.
- Intensités et résultat sauvegardés en Float32 ; WCS TAN/SIP et
  échantillonnage suivent le changement d’échelle et le recadrage. Le centre
  d’un pixel est transformé par `x_sortie = s × (x_reference + 0,5) − 0,5`.
- Le bilan et les réglages persistent dans les sessions et projets portables.
  Un projet lié conserve les réglages et doit recalculer ses résultats.
- Drizzle désactivé : la chaîne habituelle conserve son interpolation et sa
  normalisation. Le stockage et la lecture temporaires sont communs ; un
  manque d’espace est désormais toujours fatal au calcul.

## Validation

Tests d’aires sur un décalage analytique de 0,25 pixel ; conservation des zéros,
valeurs négatives et HDR ; carré tourné à 45° ; reconstruction d’un motif
analytique à quatre phases sous-pixel, comparée à son interpolation bilinéaire.
Tests d’intégration avec aberration isolée, diagnostic de couverture, limite
32 MP, trous intérieurs et annulation pendant la dernière pose. WCS/SIP,
échantillonnage et Float32 relus après export FITS.

Le test d’interface utilise l’action QML réelle, vérifie un projet portable et
la conservation du résultat après erreur. Composition native française
vérifiée à 1 440 × 960, 1 100 × 800 et 900 × 700 avec un extrait de l’Orion réel.
Deux copies identiques servent à valider le parcours et son avertissement ;
ce contrôle visuel n’est pas une preuve de gain de résolution astronomique.

## Travaux suivants

Drizzle CFA/Bayer sans dématriçage, cartes de poids/rejet exportables,
reprojection de distorsions plutôt que l’affine seule, traitement de très
grandes images avec des limites cohérentes dans toute la finition, et
comparaison quantitative sur des poses réelles avec dithering connu.
