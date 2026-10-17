---
name: astrocapture
description: Traiter des images astronomiques de ciel profond avec AstroCapture en local, depuis un dossier de FITS, RAW, TIFF ou PNG, ou inspecter un projet .astroproject. Utiliser pour les demandes de calibration, d’empilement et de finition de nébuleuses ou galaxies avec AstroCapture, y compris « traite mon dossier de M42 ». Ne pas utiliser pour développer le logiciel, retoucher des photos ordinaires, traiter des vidéos planétaires ou piloter une caméra.
---

# AstroCapture

Utiliser le mode `--agent` d’AstroCapture pour produire des images et des projets
réouvrables, avec le moteur et la précision de l’application.

## Trouver la version et son guide

Lire le guide avant de préparer une recette :
- Dans une copie personnelle installée selon le README, ouvrir
  [references/AGENT_MODE.md](references/AGENT_MODE.md).
- Dans le dépôt, ouvrir [AGENT_MODE.md](../../../AGENT_MODE.md), à la racine
  d’AstroCapture. Résoudre ces liens depuis le dossier de ce skill.

Le guide définit le contrat et les limites du mode agent. Si aucun des deux
fichiers n’est disponible, demander le guide livré avec la version utilisée ;
ne pas inventer de schéma de recette. Pour une mise à jour, prendre le skill et
le guide dans la même version du dépôt.

Trouver l’exécutable à partir du chemin fourni par l’utilisateur ou de son
installation locale. Sur macOS, les emplacements usuels sont
`/Applications/AstroCapture.app/Contents/MacOS/AstroCapture` ou, dans un dépôt
compilé, `build/release/bin/AstroCapture.app/Contents/MacOS/AstroCapture`.
Sur Linux, utiliser l’AppImage indiquée par l’utilisateur, le binaire disponible
dans le PATH ou `build/release/bin/AstroCapture` dans un dépôt compilé. Ce sont
des candidats à vérifier, pas des chemins garantis. Ne pas supposer que le
répertoire courant contient le dépôt ou les images.

Vérifier l’existence du candidat et exécuter `--agent help` avec un délai borné,
par exemple dix secondes. Attendre un événement JSON `help` avec un schéma
compatible avec le guide. Une ancienne version peut ouvrir une fenêtre au lieu
de répondre ; si le processus lancé pour cette vérification ne répond pas,
l’arrêter sans toucher à une instance de l’utilisateur. En l’absence d’une version
compatible, expliquer le blocage et demander l’emplacement d’une version qui
propose `--agent`. Une demande de traitement ne nécessite pas de modifier le
logiciel ni de remplacer son installation.

## Préparer le traitement

- Exécuter `--agent inspect` sur les sources et lire les métadonnées, groupes,
  dimensions, étoiles détectées et éventuelles erreurs. Pour un projet portable,
  utiliser `inspect` afin de vérifier son contenu et sa réouverture.
- Les rôles proposés sont des indices. Vérifier les poses, darks, flats, bias et
  groupes de filtres à partir des données et des indications de l’utilisateur.
  Demander une précision si l’état de calibration reste inconnu : ne jamais déduire
  « déjà calibré » de la seule absence de darks ou de flats.
- Préparer une recette conforme au guide, avec des rôles explicites et un dossier
  de sortie nouveau, de préférence séparé des sources. Employer des chemins absolus
  ou les résoudre relativement au fichier de recette, jamais au répertoire courant
  par supposition. Conserver la recette et les journaux hors du futur dossier de
  sortie, car celui-ci doit être créé par AstroCapture.
- Choisir une première finition mesurée selon le rendu demandé. Le mode actuel
  empile les filtres séparément ; il ne compose pas automatiquement une image
  RVB/SHO et ne prend pas en charge le planétaire, la capture ou les mosaïques.
  Signaler une demande hors de ce périmètre sans la convertir en un traitement
  incompatible.

## Exécuter et évaluer

Lancer `--agent run` avec la recette. Conserver les événements JSON Lines et les
diagnostics de la sortie d’erreur séparément. Suivre l’opération courante ; son
pourcentage n’est pas celui de toute la tâche. Utiliser le code de sortie et
l’événement final pour déterminer la réussite, pas un message traduit ni un
événement `completed` intermédiaire.

Lire `report.json`, notamment les sources retenues/rejetées, leurs motifs, les
groupes et les réglages effectivement appliqués. Ouvrir réellement les aperçus
produits avant de juger la finition : surveiller le fond de ciel, l’écrêtage,
les dominantes, les halos et les artefacts. Ne pas ajouter ou inventer de détails
astronomiques avec un générateur d’images. Si une variante est utile, garder les
résultats existants et utiliser une nouvelle sortie ; tenir compte du coût d’un
nouvel empilement, car la reprise automatique n’est pas encore disponible.

En cas d’annulation demandée, envoyer SIGINT ou SIGTERM au processus de cette tâche
et attendre sa fin. En cas d’échec, lire le bilan et garder les fichiers terminés.
Le checkpoint `stacked.astroproject` permet de reprendre dans l’application ; ne
pas annoncer une reprise automatique en ligne de commande.

Livrer les liens vers le projet réouvrable, le FITS linéaire, le TIFF de finition,
l’aperçu et le rapport. Présenter brièvement les choix et les réserves fondées sur
le bilan ou l’image. Préciser les limites si l’aperçu n’a pas pu être examiné.
