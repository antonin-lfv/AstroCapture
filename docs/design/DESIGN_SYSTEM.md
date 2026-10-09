# AstroCapture — Atelier

Direction validée par l'utilisateur le 9 octobre 2026 et intégrée à l'application.
Point de départ sauvegardé : `b18d96a` ; référence approuvée : `dd15949`.
La maquette de Finition reste la référence visuelle des écrans de l'application.

## Intention

AstroCapture est un atelier d'imagerie scientifique. L'image est le document principal.
L'interface doit permettre de travailler longtemps, lire une valeur précisément et
comprendre immédiatement la portée d'une commande. Les fonctionnalités avancées
restent accessibles et portent leurs vrais noms.

## Structure de l'écran

1. Navigation de l'application dans une barre horizontale commune à tous les espaces.
2. En-tête du document : nom, dimensions, options du projet et export.
3. Étapes Images, Empilement, Finition dans une seule barre continue.
4. Image à gauche ; inspecteur de réglages à droite, redimensionnable en production.
5. Zoom dans une barre réservée sous l'image, avec des marges régulières.
6. Histogramme dans l'inspecteur ; aucune deuxième copie permanente sous l'image.
7. Familles de réglages sous forme de sections dépliables : Étirement, Fond de ciel,
   Couleur, Géométrie, Transformations avancées. Les noms restent toujours visibles.
8. Comparaison et suppression des réglages dans le pied de l'inspecteur.

La navigation entre étapes conserve la vue et les réglages. L'ouverture d'une famille
ne réinitialise rien. Une seule famille est dépliée à la fois. En fenêtre étroite,
l'inspecteur passe de 360 à 320 px et un sélecteur rassemble les familles. Sous
1 000 px de largeur, l'histogramme peut être déplié à la demande. Les actions de
comparaison et le zoom conservent leur place. Seuls les contrôles de la famille
peuvent défiler ; les commandes de navigation et de comparaison restent accessibles.

## Fondations

Les valeurs de référence sont dans `tokens.json`. Les noms sémantiques sont communs
à la maquette, au prototype et aux composants QML de production (`app/qml/Atelier.js`).

- Gris neutres pour les surfaces : aucune dominante bleue dans la zone d'image.
- Un bleu doux signale sélection et focus. Un bleu plus soutenu distingue l'export.
- Texte principal clair ; texte secondaire assez contrasté pour les petits libellés.
- Rouge réservé à la suppression des réglages et aux erreurs.
- Police système macOS dans l'application ; SF Pro dans Figma.
- Corps et contrôles à 13 px. Métadonnées à 11 px. En-tête de document à 16 px.
- Grille d'espacement : 4, 8, 12, 16, 24 px.
- Contrôles de 32 px, variante compacte de 28 px. Rayon de 4 px pour les contrôles.
- Panneaux contigus à angles droits. Rayon de 10 px pour les dialogues.
- Séparateurs de 1 px ; pas d'ombre autour de chaque section.

## Composants et hiérarchie

**Action principale** : une commande mise en avant par zone de travail, avec un verbe
explicite. Dans Finition, c'est Exporter, avec la disquette demandée.

**Action secondaire** : bouton discret, ou commande de menu lorsque son usage est
occasionnel. Les options de projet et les préréglages de toute la finition appartiennent
au menu du projet. Une icône seule possède un nom accessible et une infobulle.

**Réglage** : libellé à gauche, valeur numérique à droite, puis curseur sur la largeur
disponible. La saisie numérique expose les limites et l'unité. Les valeurs utilisent
des chiffres tabulaires. Les deux modes d'étirement exclusifs ont un sélecteur explicite.

**Section** : titre, état actif si pertinent, chevron de disclosure. L'aide de la famille
est disponible depuis son en-tête ; les explications complètes ne prennent pas la
place des contrôles. Les commandes rarement utilisées restent dans une section dépliable.

**États** : survol sobre, focus clavier visible, sélection accompagnée d'un repère
géométrique, désactivation lisible. Une opération longue affiche son nom et son
avancement. Une erreur est expliquée près de la commande concernée.

## Comportements

- Modifier une valeur continue actualise l'aperçu et forme une seule entrée d'historique.
- Cmd+Z annule la dernière modification ; Cmd+Maj+Z rétablit ; Cmd+S sauvegarde le projet.
- Les champs texte et numériques conservent leurs raccourcis d'édition habituels.
- Zoom à la molette ou au trackpad, déplacement de l'image, 100 % et Adapter restent
  disponibles à un emplacement stable. Le zoom ne modifie jamais les pixels exportés.
- La comparaison utilise les mêmes coordonnées, le même zoom et la même convention
  d'affichage. Elle propose Avant, Résultat et Partagé, avec une séparation manipulable.
- Supprimer ces réglages agit sur la famille courante et reste annulable par Cmd+Z.
- Le menu Exporter distingue FITS linéaire et Image finale. L'export classique rassemble
  format, légende facultative, dimensions et poids estimé avant la confirmation.
- PNG 16 bits et TIFF 16 bits sont présentés comme sans perte ; JPEG expose sa perte
  de précision. Une estimation compressée est identifiée comme approximative.
- Fermer l'application conserve la protection existante contre les pertes de travail.

## Vérification de la composition

L'application doit être inspectée à 1440 × 960, 1100 × 800 et 900 × 700. Vérifier les
libellés français, les menus au survol, le focus clavier, les valeurs longues, les
familles avancées, la comparaison, le zoom et la modale d'export. Aucune commande ne
doit chevaucher le bord d'un panneau ou dépendre uniquement de sa couleur.

Le prototype démontre les interactions et l'organisation. Il ne remplace pas les
traitements scientifiques et n'exporte pas les données du projet. Son image d'Orion
vient du moteur actuel d'AstroCapture ; les ajustements de démonstration restent locaux.

Les deux vues de comparaison ont été calculées séparément avec le moteur natif :
sans étirement, puis avec l'étirement suggéré. Les contrôles de la maquette ne
recalculent pas ces vues. Les textes jaunes déjà présents dans le TIFF source
font partie de ses pixels : l'option de légende concerne uniquement un ajout.

Les poids sans légende viennent de l'estimateur natif sur l'image complète
6 244 × 4 170 : PNG ≈ 144,9 Mo, JPEG ≈ 8,3 Mo, TIFF 156,2 Mo,
FITS 312,5 Mo. Le PNG/JPEG avec une nouvelle légende reste indiqué comme
à estimer dans cette maquette. Le TIFF non compressé conserve le même poids.

## Légendes

- [x] **Rendre modifiables les informations préremplies.** Le champ « Informations
  de l'image et du traitement » contient le texte proposé automatiquement et permet
  de corriger librement dimensions, poses, durée, filtres ou toute autre information.
  Les champs existants gardent le titre, l'objet, la date, les notes et le matériel.
  Les corrections manuelles restent présentes lors d'une actualisation et dans le
  projet sauvegardé. L'aperçu et l'export reprennent le même texte corrigé.
  Désactiver ces informations conserve le texte ; le vider retire le texte ajouté.
  Une commande avec infobulle permet de revenir aux informations proposées.
  Les anciens projets continuent à utiliser le texte automatique tant qu'il n'est
  pas modifié. La saisie conserve ses raccourcis d'annulation et de rétablissement.

## Livrables et état

- `atelier-finition.html` : maquette interactive, image et données incluses.
- `previews/` : vues de référence aux trois tailles et parcours d'export.
- `previews/native/` : captures de l'application intégrée, avec Orion.
- `VALIDATION.md` : contrôles de l'application et périmètre de la livraison.
- `tokens.json` : couleurs, typographie, espacements et dimensions de référence.
- `AGENTS.md` à la racine : règles de travail et de cohérence pour les prochaines étapes.

La bibliothèque Figma contient les variables, styles et composants. La composition
de l'écran y reste inachevée : le connecteur a refusé les propriétés texte SF Pro,
puis la limite du forfait Starter a bloqué les appels. La maquette locale constitue
la référence vérifiée de cette phase. La migration QML et un nouveau DMG appartiennent
à la phase de mise en œuvre de cette charte.

## Références

- [Apple — Designing for macOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-macos)
- [Apple — Toolbars](https://developer.apple.com/design/human-interface-guidelines/toolbars)
- [Fondations Figma — composition en attente](https://www.figma.com/design/WWVWutYSBNlgP6v4KhjAVd)
