# AstroCapture

## Travail et sauvegarde

- Avant toute nouvelle phase de modification, faire un commit des changements existants,
  comme demandé par l'utilisateur. Ne pas supprimer de travail pour obtenir un état propre.
- Préserver les traitements, la précision des images et la compatibilité des projets.
- Utiliser une branche `codex/` pour une nouvelle phase de travail.

## Interface

- La direction visuelle est décrite dans `docs/design/DESIGN_SYSTEM.md` ; les valeurs
  de référence sont dans `docs/design/tokens.json`.
- La direction Atelier et la maquette de Finition ont été validées par l'utilisateur
  le 9 octobre 2026. Les captures natives dans `docs/design/previews/native/` documentent
  leur intégration. Toute nouvelle refonte doit rester dans la demande en cours ;
  ne pas confondre prototype et fonctionnalités livrées.
- Pour toute migration autorisée, réutiliser des composants QML communs et des noms de
  couleurs sémantiques plutôt que dupliquer des styles par écran.
- Donner la priorité à l'image, aux valeurs précises, à la stabilité des commandes et
  à la navigation clavier. Conserver l'accès aux fonctions avancées.
- Définir les états survol, focus, sélection, désactivation et activité des contrôles.
- Garder un nom accessible sur les commandes à icône seule et traduire les libellés visibles.
- Vérifier visuellement la composition avec une vraie image, en français, aux tailles
  1440 × 960, 1100 × 800 et 900 × 700. Corriger les chevauchements et les libellés coupés.
