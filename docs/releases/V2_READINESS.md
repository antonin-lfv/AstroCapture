# AstroCapture — bilan avant V2

État du 10 octobre 2026, après la livraison 1.15.1.

L’utilisateur réserve pour l’instant AstroCapture à son usage personnel.
La qualification de distribution publique reste une étape ultérieure ; elle
n’empêche pas les essais locaux. La 1.15.1 ajoute le contrôle de la fenêtre
complète, de la reprise entre processus et des échecs de sauvegarde/export
sur un volume de test plein. La comparaison finale avec Siril est prévue
avec l’utilisateur demain ; elle n’est pas encore effectuée.

Ce document centralise les travaux restants. Les fonctions livrées sont décrites
dans le [README](../../README.md) et le guide intégré ; les anciens lots sont
consignés dans le [journal de validation](../design/VALIDATION.md), le
[compte rendu M8/M20 et Orion](../VALIDATION_SIRIL.md) et les ADR.
Les priorités et les pistes ci-dessous ne constituent pas des engagements de
livraison ; leurs limites et critères de validation doivent rester explicites.

## Ce qui existe

Capture, traitement planétaire, calibration ciel profond, alignement stellaire,
empilement robuste, drizzle mono/RVB et CFA natif, extraction de gradient,
composition de filtres, résolution WCS guidée/aveugle et SIP, PCC/SPCC,
masters réutilisables, masques, GHS, restauration et séparation paramétrique
étoiles/fond, géométrie, comparaison, exports et projets récupérables sont
implémentés. La 1.12 ajoute les cartes de contribution/rejet exportables et une
fusion HDR linéaire limitée à des entrées comparables préparées par durée.
La 1.13 ajoute la projection WCS/SIP des poses déjà résolues, avec contrôle
relatif des étoiles et sortie TAN sans SIP, en empilement et drizzle.
La 1.14 borne les buffers de rejet à 64 Mio, utilise une archive temporaire
par calcul et contrôle mémoire, espace disque et limites de reprise avant
l’empilement. La limite de sortie de 32 MP demeure : toute la chaîne ne travaille
pas encore par tuiles. Voir [ADR-025](../decisions/ADR-025-integration-resources.md).

La 1.15 ajoute la préparation de masters par lots, séparés par configuration
avec choix des correctifs de flats, vérification des sources, provenance,
recherche/filtrage et bilan d’annulation ou d’échec. La création reste distincte
de l’application aux poses. Voir [ADR-026](../decisions/ADR-026-batch-calibration-masters.md).

« Implémenté et testé » ne signifie pas « validé dans toutes les conditions ».
Les aides et le guide expliquent les réglages ; les preuves d’une livraison sont
consignées dans le journal de validation et les ADR. La V2 ne doit pas revendiquer
une parité scientifique complète avec Siril sans comparaison indépendante.

## Conditions prioritaires pour une V2 publique robuste

| Travail restant | Pourquoi | Preuve attendue avant clôture |
| --- | --- | --- |
| Qualification sur acquisitions réelles | Les scènes synthétiques détectent les erreurs mais ne couvrent pas tous les capteurs et défauts du ciel | Comparaison finale avec Siril prévue avec l’utilisateur : même sélection, référence et intégrations linéaires M8/M20, puis opérations de finition séparées ; séries publiques ou autorisées mono, CFA, RGB, filtres étroits et HDR ; mesures de bruit, flux, couleur, FWHM et astrométrie |
| PCC/SPCC et profils de réponse | Les données Gaia XP, approximations éventuelles et courbes importées ne prouvent pas une justesse absolue pour chaque équipement | Profils caméra/filtres traçables, cibles de référence, erreurs/résidus publiés, cas de filtres atypiques et catalogue incomplet |
| Ressources et grands fichiers | Limite 32 MP : un capteur de 26 MP en drizzle ×2 produit 104 MP et est refusé ; cartes et HDR augmentent la mémoire. Depuis 1.14, rejet borné et contrôle conservateur avant empilement ; finition et résultats restent en mémoire | Traitement par tuiles cohérent de l’entrée à l’export avant hausse du plafond ; mesures sur séries et modes réels, pression mémoire avec les autres espaces ouverts ; qualification des estimations et de la reprise 4 Gio/64 images |
| Endurance et récupération | Les garanties de publication atomique ne remplacent pas des essais longs et interruptions système | Disque plein, fermeture forcée, annulation à chaque phase, fichiers déplacés/corrompus, export interrompu, sessions longues et migrations entre versions |
| Qualification capture matérielle | Une caméra et un mode ne représentent pas les autres pilotes | Caméras réelles, couleurs CFA confirmées, cadence, pertes, longues acquisitions, reconnexion et permissions sur Mac propre |
| Distribution macOS | Le DMG est signé ad hoc ; il manque la confiance Apple pour une distribution normale | Apple Developer ID, notarisation, essai d’installation et mise à jour sur un Mac vierge, procédure de retour à la version précédente |
| Revue utilisateur finale | L’aide présente dans les panneaux ne prouve pas que chaque parcours est compréhensible | Parcours par des utilisateurs venant de Siril : calibration → empilement → couleurs → finition → sauvegarde/export ; français, clavier et trois tailles de fenêtre |

## Extensions de traitement encore absentes ou limitées

- Mosaïque de champs larges en FITS linéaire avec WCS ; résolution automatique
  d’une séquence avant projection, et correction photométrique de surface des
  pixels. Depuis 1.13.0, la projection d’empilement WCS/SIP est livrée **sur
  opt-in, pour des poses FITS déjà résolues**, avec contrôle relatif des étoiles
  et sortie TAN sans SIP. Elle ne résout pas les poses à elle seule et ne
  certifie pas les coordonnées absolues. Voir [ADR-024](../decisions/ADR-024-wcs-sip-stack-projection.md).
- Pondérations statistiques par bruit/FWHM ou nombre de poses, cartes bas/hauts
  séparées et contrôle plus poussé du rejet ; les cartes actuelles ne sont pas
  des cartes de variance ni de confiance.
- HDR de séries brutes avec calibration automatique par durée, vérification
  quantitative de linéarité, égalisation de PSF et prise en compte du nombre de
  poses. Le HDR actuel ne garantit pas des raccords parfaits si le ciel ou la
  PSF varie ; la saturation est déclarée, pas mesurée automatiquement.
- Séparation d’étoiles plus générale : les modèles paramétriques actuels ne
  remplacent pas StarNet pour les étoiles saturées, mélangées, halos et aigrettes.
  Étudier un moteur externe optionnel ou l’import de couches externes, avec
  licence des modèles vérifiée et contrôle des artefacts ; une réduction du
  rayon stellaire reste distincte de la réduction de luminosité actuelle.
- PSF elliptique ou variable sur le champ, estimation plus avancée,
  déconvolution régularisée et débruitage plus avancé.
- Retrait de gradient RBF et par division, réglages de grille par séquence ;
  diagnostic exact des pertes finales distinguant saturation des sources et
  écrêtage ajouté par la finition.
- Application de calibrations distinctes à des poses hétérogènes dans une seule
  intégration et pipeline HDR brut complet. Depuis 1.15, les **masters** sont
  regroupés/créés par lots avec recherche et contrôles explicites ; cela
  n’automatise pas toute la préparation des poses. Diagnostics de calibration
  spatiaux plus détaillés et critères de classement avancés restent possibles.
- Normalisation locale, interpolation d’empilement configurable, champ de sortie
  choisi par l’utilisateur et projections grand angle. Le changement d’échelle
  existe déjà en géométrie et dans les modes drizzle documentés.
- Inspection et édition des métadonnées FITS, séparation des canaux RGB et
  diagnostics de normalisation plus détaillés.
- RAW X-Trans, import AVI et dérotation planétaire : hors périmètre actuellement
  documenté, à prioriser selon les usages plutôt que les annoncer comme livrés.

Ces extensions ne doivent pas toutes bloquer une V2 à périmètre annoncé.
En revanche, les conditions de qualification, intégrité, ressources et
installation ci-dessus doivent être closes ou explicitement restreintes.

## Autres pistes conservées

- Fluidité et séries : aperçus sur région d’intérêt, caches ciblés après profilage,
  mesure de chaque phase et accélération des outils avancés ; traitements et
  exports par lots avec état et annulation par tâche.
- Astrométrie et couleurs : élargir la qualification des champs/index du solveur,
  composer les réponses QE/filtres et prévoir des modes bande étroite distincts.
  Les profils fabricant prévalidés relèvent de la qualification PCC/SPCC ci-dessus.
- Annotations NGC, Messier, planètes et cratères, avec confiance et correction
  manuelle ; recherche de cible et coordonnées WCS au survol existent déjà.
- Pilotage de monture, guidage et dithering, actuellement externes à AstroCapture.

Les automatismes doivent rester optionnels, contrôlables et réversibles. Préserver
les sources, la précision et le moteur indépendant ; ne pas annoncer de support
matériel ou de justesse scientifique sans preuve.

## Demande utilisateur conservée pour la planification

Entrer une position terrestre ou un objet, connaître sa visibilité depuis ce
lieu et les régions du monde favorables : **à développer**, sans fonctionnalité
livrée à ce stade. Il faudra préciser date/heure, horizon, altitude minimale,
nuit astronomique et éventuellement influence de la Lune. Ce module peut suivre
la stabilisation du traitement ; il n’est pas une condition de justesse des
images déjà produites.
