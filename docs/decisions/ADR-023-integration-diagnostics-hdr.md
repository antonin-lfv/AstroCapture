# ADR-023 — Diagnostics d’intégration et HDR linéaire

Date : 10 octobre 2026. Livraison : 1.12.0.

## Contrat des cartes

L’option de conservation est explicite, désactivée par défaut et enregistrée
avec les paramètres d’empilement. Elle crée trois images RVB Float32 sur la
même grille recadrée que le résultat :

- **Poids** : somme des poidsgéométriques des échantillons conservés après
  rejet. En empilement classique, chaque échantillon vaut 1. En drizzle, le
  poids provient du recouvrement projeté ; en Bayer natif il est indépendant
  pour R, V et B. Ce n’est pas une variance inverse, un SNR ou une qualité.
- **Contributeurs** : nombre d’échantillons conservés par pixel et canal après
  rejet. Le bilan drizzle existant indique les contributions **avant** rejet.
- **Rejets** : nombre d’échantillons rejetés par la règle médiane/MAD, réunissant
  les valeurs trop basses et trop hautes. Les poses rejetées avant l’intégration
  ne figurent pas dans cette carte. À moins de trois contributions ou avec le
  rejet désactivé, aucun rejet statistique n’est appliqué.

La consultation choisit R, V, B ou leur maximum. L’aperçu est réduit, sans
étirement astrophotographique, avec noir=0 et blanc=maximum de la carte
sélectionnée. Une carte nulle est noire ; une carte constante positive est
blanche. Le min/max et la proportion de pixels positifs concernent la grille
complète, pas l’échantillonnage de l’aperçu. L’export conserve les **trois** canaux,
les valeurs originales Float32 et le WCS/SIP du résultat source, sans finition.

Coût supplémentaire : 36 octets par pixel de sortie et par groupe, hors aperçu
et buffers. À 32 MP : 1,152 Go décimaux. La limite existante de récupération de
4 Gio et celle de 64 images de cache demeurent ; de nombreux groupes peuvent
les atteindre. Les anciennes sessions sans cartes restent compatibles.

## HDR : conditions préalables

La fusion accepte 2 à 8 Lights FITS sélectionnées d’un même groupe, déjà
calibrées et dématricées, avec les mêmes dimensions et canaux. Il est préférable
de préparer **un empilement moyen par durée**. Il faut vérifier que les images
restent linéaires, que le biais/offset instrumental a été soustrait et que la
calibration de chaque série correspond à son exposition. Aucun étirement,
normalisation photométrique variable ou somme d’empilement non divisée ne doit
être confondu avec ces données linéaires comparables.

Les divergences connues de caméra, filtre, gain, offset, binning ou échelle des
échantillons bloquent la fusion. Une métadonnée manquante ne prouve pas la
compatibilité : l’utilisateur confirme explicitement calibration, linéarité,
durées et unités. Ce contrôle n’est pas une analyse de la linéarité du capteur.

La durée saisie est la durée **individuelle** représentée par les pixels d’un
empilement moyen. Les exports d’empilement peuvent stocker le temps cumulé dans
EXPTIME : le champ est prérempli mais doit être vérifié. Les durées doivent être
finies, positives, ≤ 10⁷ secondes, avec un rapport long/court ≥ 1,1. Le seuil
saisi, fini et positif ≤ 10¹², est exprimé dans les échantillons normalisés
réellement importés. **1 n’est jamais déduit comme seuil universel du FITS
flottant.** Un seuil prudent sous le plateau de saturation est recommandé.

## Calcul effectivement livré

La référence est l’exposition individuelle la plus longue. Les autres images
sont alignées par les étoiles avec une transformation de similitude : au moins
six correspondances et résidu RMS ≤ 1 pixel. Le WCS de la référence est conservé ;
aucune correction optique dynamique SIP ni égalisation de PSF n’est appliquée.

Pour une entrée de durée t, référence T, seuil S et transition q :

- l’intensité utilisée est `v × T/t` ;
- `u = clamp((S − pic)/(S × q), 0, 1)` ;
- le poids vaut `(t/T) × u² × (3 − 2u)` ;
- la sortie est la moyenne pondérée des valeurs ainsi exprimées.

q est compris entre 0,01 et 0,5 (10 % par défaut). Le pic est le maximum RVB des
échantillons sources qui contribuent à l’interpolation bilinéaire. La saturation
est évaluée **avant** interpolation pour éviter qu’un pic écrêté paraisse
artificiellement valide. Un poids commun RVB exclut les trois canaux si l’un
sature et évite les raccords indépendants de couleur. Ce poids proportionnel à
la durée n’est pas une estimation de variance ni un poids tenant compte du
nombre de poses de chaque empilement.

La grille de la référence est conservée. Une source hors champ ne contribue
pas ; le résultat ne prétend donc pas bénéficier de poses courtes partout.
Quand toutes les sources disponibles saturent, l’échantillon disponible de
plus courte durée est conservé, remis aux unités T, et marqué comme encore
saturé. Le calcul n’invente pas de détails. Les données non finies, les valeurs
hors plage et les échecs d’alignement sont des erreurs, sans exclusion silencieuse.
Les pixels négatifs et les valeurs au-delà de 1 restent flottants ; la finition
et l’export classique peuvent les écrêter et doivent être contrôlés séparément.

Deux cartes RVB binaires sont produites : pixel de référence saturé récupéré par
une entrée utilisable ; pixel encore saturé dans toutes les entrées disponibles.
Le bilan conserve paramètres, chemins et alignements. Le temps d’intégration
**total reste inconnu**, car les entrées peuvent être des empilements. L’export
linéaire FITS indique T en EXPTIME pour décrire l’échelle d’exposition du résultat.

## Publication, récupération et refus

Les calculs et exports travaillent en arrière-plan. La publication du résultat
est atomique après réussite et vérification de l’annulation. Les originaux,
masters, masques référencés et fichier projet sont protégés contre l’écrasement ;
les exports passent par une copie atomique. Une erreur ou une annulation conserve
le résultat précédent. Les projets portables transportent les cartes et les
sources ; les projets liés conservent les durées et seuils pour refaire la fusion,
sans conserver son résultat calculé. Les anciens projets sans ces champs restent lisibles.

Les cartes restaurées doivent avoir la grille de leur empilement et des valeurs
finies non négatives ; un bilan HDR doit être lié au résultat correspondant.
La consultation/export d’un WCS absent n’ajoute plus d’entrée WCS invalide au
projet. Ce cas est couvert par une réouverture après export.

## Références et limites de validation

Le principe d’une radiance proportionnelle à l’exposition et l’importance de la
réponse du capteur sont décrits par
[Debevec et Malik, SIGGRAPH 1997](https://www.pauldebevec.com/Research/HDR/debevec-siggraph97.pdf).
Cette implémentation exige une réponse déjà linéaire ; elle ne récupère pas une
courbe de réponse comme leur méthode. Les cartes de rejets sont aussi décrites
[dans la documentation Siril](https://siril.readthedocs.io/en/stable/preprocessing/stacking.html).
AstroCapture fournit ici des rejets bas/hauts réunis et ses propres poids
géométriques ; aucune équivalence numérique avec tous les modes Siril n’est affirmée.

Les tests contrôlent une scène analytique, couleurs indépendantes, saturation
partielle et totale, vrais zéros, pixels négatifs, translations, défaut isolé,
erreurs et annulation. Le contrôle visuel utilise une vraie image de M42 avec
des expositions et saturations **simulées** : il valide le parcours et l’affichage,
pas un gain scientifique mesuré sur une acquisition HDR indépendante.
La qualification sur de vraies séries bracketées, avec transparence du ciel et
PSF variables, reste nécessaire avant de revendiquer une performance de référence.
