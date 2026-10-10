# ADR-022 — Reconstruction drizzle Bayer native

Date : 10 octobre 2026. AstroCapture 1.11.0. Extension de l’ADR-021.

## Usage et choix des sources

Dans **Ciel profond → Empilement → Réglages avancés**, activer **Drizzle**,
puis **Bayer natif / CFA**. L’activation par l’utilisateur propose **×1** et une
fraction de pixel de **1** ; restaurer un projet conserve ses valeurs sauvegardées.
Les deux options sont désactivées par défaut dans un projet ancien ou nouveau.

Seules des images à un canal qui conservent la matrice du capteur conviennent.
Les quatre matrices RGGB, BGGR, GRBG et GBRG sont acceptées ; les RAW X-Trans
et les fichiers RVB déjà dématricés ne conviennent pas. La sortie reste limitée
à 32 MP avant recadrage. Un FITS de 26 MP peut utiliser ×1, mais pas ×2.

La [documentation Siril](https://siril.readthedocs.io/en/stable/preprocessing/drizzle.html#cfa-workflow)
décrit le compromis entre couverture, fraction de pixel et agrandissement.
La reconstruction utilise le principe des empreintes réduites de
[Fruchter et Hook](https://arxiv.org/abs/astro-ph/9808087). L’implémentation est
indépendante et ne promet pas une équivalence numérique avec Siril.

Certains FITS iTelescope portent `BAYERPAT=VALID`, sans ordre des couleurs.
Cette valeur ne suffit pas à connaître la matrice. **Matrice Bayer des métadonnées**
reste le défaut ; une sélection explicite peut compléter la matrice manquante
des poses et calibrations sélectionnées à un canal pendant cet empilement.
L’ordre choisi doit correspondre aux pixels du fichier, après éventuel recadrage,
décalage du capteur ou changement de sens des lignes. Aucun ordre n’est deviné
depuis le nom de caméra. Une matrice reconnue contradictoire bloque le calcul.
La déclaration reste dans les réglages du projet ; les fichiers sont intacts.
L’aperçu individuel antérieur à l’empilement continue à interpréter les métadonnées
du fichier ; la déclaration concerne ce calcul, pas une réécriture de la source.

## Calcul

1. Calibration native : dark/bias, flat et correction des pixels défectueux,
   en respectant les couleurs de la matrice. La déclaration explicite est
   appliquée avant la préparation des calibrations lorsqu’elle est nécessaire.
2. Une copie dématricée permet la détection et l’alignement des étoiles. Si
   l’extraction de gradient par pose est active, cette copie fournit son modèle.
   Au site mesuré, la copie garde exactement la valeur calibrée de sa couleur,
   moins la correction explicite de fond ; les valeurs interpolées des autres
   couleurs ne sont jamais utilisées pour projeter ce site.
3. La normalisation utilise un facteur stellaire commun et un offset estimé
   séparément sur les échantillons réellement mesurés en R, V et B. Elle ne
   constitue pas une calibration photométrique des couleurs.
4. Projection affine des carrés réduits avec le même calcul d’intersection
   rectangulaire/polygonale que l’ADR-021. Chaque pixel ne contribue qu’au canal
   de son filtre CFA. Sommes et aires sont calculées en double précision.
5. Chaque pose stocke trois intensités et trois poids par pixel de sortie.
   Un poids nul signale une contribution absente ; une intensité nulle réelle
   reste valide. Le calcul par bandes ignore les contributions absentes.
6. Rejet médiane/MAD par pixel **et par couleur**, uniquement avec au moins
   trois valeurs disponibles dans ce canal. Moyenne pondérée des survivants.
   Les statistiques de contributions représentent la couverture avant rejet.

L’intensité conserve la convention des sources. Les valeurs négatives ou HDR
ne sont pas écrêtées. La photométrie intégrée doit tenir compte de la surface
angulaire de sortie, divisée par le carré de l’échelle. Le WCS TAN/SIP et
l’échantillonnage suivent exactement l’échelle et le recadrage, comme en mono/RVB.

## Couverture, mémoire et erreurs

Au moins deux poses utilisables sont requises, mais ce minimum ne garantit
aucune couverture CFA complète. Beaucoup de poses avec dithering varié sont
nécessaires, particulièrement pour R/B et lors d’un agrandissement. Les bordures
géométriques communes sont recadrées ; un canal sans contribution à l’intérieur
de cette zone provoque une erreur explicite. Il n’est remplacé ni par zéro ni
par une interpolation. Des copies identiques ne suffisent pas.

Le diagnostic de diversité considère la périodicité de deux pixels du motif
CFA ; un déplacement d’un pixel peut changer les couleurs mesurées même s’il
n’apporte pas de phase sous-pixel à une image mono. Ce diagnostic ne garantit
ni une couverture uniforme ni un gain de résolution.

Le stockage temporaire utilise 24 octets par pixel de sortie et par pose,
contre 16 en mono/RVB. La projection garde en mémoire une pose, sa copie couleur
et la grille accumulée : le plafond de 32 MP ne constitue pas une garantie de
mémoire suffisante. L’intégration relit des bandes plutôt que toutes les poses.
Annulation, manque d’espace ou erreur conserve le résultat précédent et libère
les fichiers temporaires. Mode, déclaration et bilan persistent dans les sessions
et projets ; les pixels des projets portables restent en Float32.

## Vérification

- Quatre matrices, attribution exacte à chaque couleur, vrais zéros,
  négatifs/HDR et absence explicite des couleurs non mesurées.
- Reconstruction analytique de champs couleur indépendants à quatre phases
  entières ; comparaison avec la perte de détails du dématriçage bilinéaire.
- Reconstruction à ×2 et fraction 0,5 avec seize phases contrôlées ; intersection
  d’un pixel isolé tourné à 45°, sans fuite vers les autres couleurs.
- Chaîne réelle avec bias/flat, normalisation activée/désactivée, rejet d’un
  défaut isolé, couverture par canal et WCS. Annulation pendant la dernière pose.
- Parcours QML réel, déclaration d’une matrice manquante, conflit de métadonnées,
  refus des sources mono/RVB incompatibles, erreur de couverture, sauvegarde
  portable et restauration exacte des pixels/réglages/bilan.
- Composition française à 1440 × 960, 1100 × 800 et 900 × 700 avec des mosaïques
  CFA construites depuis un extrait du véritable Orion, à quatre décalages connus.
  Cette fixture vérifie le parcours et la composition ; elle ne démontre pas un
  gain sur une acquisition astronomique réelle avec dithering.

## Évolutions depuis ce lot

Les cartes de contribution/rejet sont livrées en
[1.12.0](ADR-023-integration-diagnostics-hdr.md) et la projection des distorsions
WCS/SIP en [1.13.0](ADR-024-wcs-sip-stack-projection.md). Les métriques avancées,
les grands fichiers et la comparaison quantitative de bruit et résolution sur
acquisitions réelles avec dithering documenté restent dans le
[bilan V2](../releases/V2_READINESS.md).
