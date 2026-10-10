# AstroCapture — bilan avant V2

État du 10 octobre 2026, après la livraison 1.12.0.

## Ce qui existe

Capture, traitement planétaire, calibration ciel profond, alignement stellaire,
empilement robuste, drizzle mono/RVB et CFA natif, extraction de gradient,
composition de filtres, résolution WCS guidée/aveugle et SIP, PCC/SPCC,
masters réutilisables, masques, GHS, restauration et séparation paramétrique
étoiles/fond, géométrie, comparaison, exports et projets récupérables sont
implémentés. La 1.12 ajoute les cartes de contribution/rejet exportables et une
fusion HDR linéaire limitée à des entrées comparables préparées par durée.

« Implémenté et testé » ne signifie pas « validé dans toutes les conditions ».
Les aides et le guide expliquent les réglages ; les preuves d’une livraison sont
consignées dans docs/design/VALIDATION.md et les ADR. La V2 ne doit pas revendiquer
une parité scientifique complète avec Siril sans comparaison indépendante.

## Conditions prioritaires pour une V2 publique robuste

| Travail restant | Pourquoi | Preuve attendue avant clôture |
| --- | --- | --- |
| Qualification sur acquisitions réelles | Les scènes synthétiques détectent les erreurs mais ne couvrent pas tous les capteurs et défauts du ciel | Séries publiques ou autorisées mono, CFA, RGB, filtres étroits et HDR ; comparaison indépendante avec Siril ; mesures de bruit, flux, couleur, FWHM et astrométrie |
| PCC/SPCC et profils de réponse | Les données Gaia XP, approximations éventuelles et courbes importées ne prouvent pas une justesse absolue pour chaque équipement | Profils caméra/filtres traçables, cibles de référence, erreurs/résidus publiés, cas de filtres atypiques et catalogue incomplet |
| Ressources et grands fichiers | Limite 32 MP : un capteur de 26 MP en drizzle ×2 produit 104 MP et est refusé ; cartes et HDR augmentent la mémoire | Traitement par tuiles cohérent de l’entrée à l’export, plafonds mesurés, estimation mémoire/disque, contrôle des caches de 4 Gio/64 images |
| Endurance et récupération | Les garanties de publication atomique ne remplacent pas des essais longs et interruptions système | Disque plein, fermeture forcée, annulation à chaque phase, fichiers déplacés/corrompus, export interrompu, sessions longues et migrations entre versions |
| Qualification capture matérielle | Une caméra et un mode ne représentent pas les autres pilotes | Caméras réelles, couleurs CFA confirmées, cadence, pertes, longues acquisitions, reconnexion et permissions sur Mac propre |
| Distribution macOS | Le DMG est signé ad hoc ; il manque la confiance Apple pour une distribution normale | Apple Developer ID, notarisation, essai d’installation et mise à jour sur un Mac vierge, procédure de retour à la version précédente |
| Revue utilisateur finale | L’aide présente dans les panneaux ne prouve pas que chaque parcours est compréhensible | Parcours par des utilisateurs venant de Siril : calibration → empilement → couleurs → finition → sauvegarde/export ; français, clavier et trois tailles de fenêtre |

## Extensions de traitement encore absentes ou limitées

- Projection utilisant les distorsions optiques pendant l’alignement/drizzle et
  mosaïque de champs larges en FITS linéaire avec WCS. Le SIP livré décrit la
  solution astrométrique ; il ne corrige pas automatiquement toute projection.
- Pondérations statistiques par bruit/FWHM ou nombre de poses, cartes bas/hauts
  séparées et contrôle plus poussé du rejet ; les cartes actuelles ne sont pas
  des cartes de variance ni de confiance.
- HDR de séries brutes avec calibration automatique par durée, vérification
  quantitative de linéarité, égalisation de PSF et prise en compte du nombre de
  poses. Le HDR actuel ne garantit pas des raccords parfaits si le ciel ou la
  PSF varie ; la saturation est déclarée, pas mesurée automatiquement.
- Séparation d’étoiles plus générale : les modèles paramétriques actuels ne
  remplacent pas StarNet pour les étoiles saturées, mélangées, halos et aigrettes.
- RAW X-Trans, import AVI et dérotation planétaire : hors périmètre actuellement
  documenté, à prioriser selon les usages plutôt que les annoncer comme livrés.

Ces extensions ne doivent pas toutes bloquer une V2 à périmètre annoncé.
En revanche, les conditions de qualification, intégrité, ressources et
installation ci-dessus doivent être closes ou explicitement restreintes.

## Demande utilisateur conservée pour la planification

Entrer une position terrestre ou un objet, connaître sa visibilité depuis ce
lieu et les régions du monde favorables : **à développer**, sans fonctionnalité
livrée à ce stade. Il faudra préciser date/heure, horizon, altitude minimale,
nuit astronomique et éventuellement influence de la Lune. Ce module peut suivre
la stabilisation du traitement ; il n’est pas une condition de justesse des
images déjà produites.
