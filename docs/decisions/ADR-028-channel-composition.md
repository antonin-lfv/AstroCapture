# ADR-028 — Composition multifiltre guidée

Date : 11 octobre 2026. Version : 1.17.0.

## Demande et périmètre

Simplifier la composition déjà présente sans retirer les attributions manuelles :
choix RGB/LRGB/HOO/SHO, filtres intégrés requis visibles, dosage de luminance,
choix sauvegardés et indication fidèle du résultat affiché. Le module de
visibilité et de préparation des observations appartient à la phase suivante.

## Décision

Un composant QML commun `ChannelComposition` ouvre la famille Couleur pour les
projets multifiltres. Un groupe couleur RGB seul garde son parcours de finition.
Le plan calculé par le contrôleur et l’exécution utilisent les mêmes règles :
RGB = R/G/B, LRGB = R/G/B + L, HOO = HA/OIII/OIII,
SHO = SII/HA/OIII. Les alias de filtre sont résolus seulement lorsqu’ils
désignent une couche intégrée unique. Un filtre absent ou ambigu bloque le
bouton et nomme la couche à empiler ; il ne remplace pas cette couche par la
première disponible. Le mode Personnalisée conserve l’attribution de chaque
canal et la luminance facultative. Modifier une attribution passe dans ce mode.

Les changements de choix n’altèrent pas l’image calculée : la commande Composer
l’image déclenche le recalage stellaire et la composition. Les indications
distinguent les choix du résultat effectivement composé. Un échec ou une
annulation conserve le résultat précédent ; les empilements par filtre sont
conservés. Choisir un empilement individuel retire l’indication de composition.
Les budgets mémoire et de reprise (4 Gio) sont vérifiés avant le calcul.

## Luminance

Pour l’apport a entre 0 et 1, la luminosité RGB Y est la moyenne des trois canaux
et L est la moyenne de la couche de luminance recalée. Lorsque Y > 1e-8,
chaque canal est multiplié par `1 + a × (L/Y − 1)`. À 100 %, le chemin utilise
exactement le facteur historique `L/Y`. À 0 %, L ne participe ni au recalage,
ni au recadrage commun, ni aux métadonnées d’exposition. Le cas sans L conserve
le comportement RGB. Les sources hors couverture sont recadrées comme auparavant.

Ce dosage conserve les rapports des canaux à moyenne positive ; il ne garantit
pas une colorimétrie physique, ne compense pas des durées/unités différentes et
ne récupère pas les pixels saturés. HOO/SHO sont des correspondances de signaux
vers une palette. PCC/SPCC reste destiné aux compositions RGB à large bande.

## Projets et astrométrie

Les choix entrent dans les options existantes des projets/session ; l’état
appliqué est lié à la clé de l’image linéaire. Le format de projet reste en
version 1. Les anciens projets n’ont aucune obligation de posséder ces champs.
Les palettes, noms de couches et valeurs numériques sont validés à l’import ;
une valeur hors 0–100 % est rejetée sans toucher au projet courant.

L’ancien WCS de l’image affichée est retiré après composition : le recalage et
le recadrage commun changent sa grille. Les WCS des empilements sources restent
conservés. Il faut résoudre l’image composée avant d’utiliser ses coordonnées
ou de préparer un nouveau catalogue PCC/SPCC. La propagation exacte du WCS du
canal de référence avec l’offset de recadrage reste une extension distincte.

## Vérification

Les contrôles numériques couvrent 0/50/100 %, le comportement historique,
les limites invalides et l’annulation. Le parcours QML contrôle l’attribution
automatique, le filtre SII manquant, la recomposition, le mode personnalisé,
la conservation du résultat en cas de refus et la sauvegarde/reprise des choix.
Les captures françaises à 1440 × 960, 1100 × 800 et 900 × 700 utilisent une
région de l’image réelle Orion, séparée en couches pour une fixture d’interface.
Les noms HA/OIII de cette fixture ne représentent pas de vraies acquisitions
à bande étroite. La comparaison d’une vraie série T33 avec Siril reste à faire.
Les résultats finaux sont consignés dans le journal de validation.
