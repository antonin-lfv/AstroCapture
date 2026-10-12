# AstroCapture — bilan avant V2

État du 12 octobre 2026, après les contrôles de la livraison 1.18.9.

L’utilisateur réserve pour l’instant AstroCapture à son usage personnel.
La qualification de distribution publique reste une étape ultérieure ; elle
n’empêche pas les essais locaux. La 1.15.1 ajoute le contrôle de la fenêtre
complète, de la reprise entre processus et des échecs de sauvegarde/export
sur un volume de test plein. La comparaison finale avec Siril reste à effectuer
avec l’utilisateur, qui l’envisage pour le 11 octobre 2026.

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
l’empilement. Voir [ADR-025](../decisions/ADR-025-integration-resources.md).
La 1.16 porte le plafond à 128 MP / 32 700 pixels par dimension, avec projection
drizzle bornée à 64 Mio, FITS/TIFF par blocs et contrôle de la finition et de la
reprise. Les résultats restent complets en RAM : les combinaisons dépassant les
budgets sont refusées. Voir [ADR-027](../decisions/ADR-027-large-images.md).

La 1.17 simplifie la composition multifiltre : palettes RGB/LRGB/HOO/SHO,
attribution automatique des couches empilées, contrôle des filtres manquants,
mode personnalisé et apport de luminance 0–100 %. Les choix sont sauvegardés ;
la qualification sur une vraie acquisition T33 reste distincte des contrôles
du moteur et de l’interface. Voir [ADR-028](../decisions/ADR-028-channel-composition.md).

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
| Ressources et grands fichiers | Plafond 128 MP / 32 700 pixels par dimension et budgets : un capteur de 26 MP en drizzle ×2 peut produire 104 MP ; cartes, filtres, couches et HDR peuvent dépasser mémoire/reprise. Rejet et projection drizzle bornés, mais résultats et intermédiaires avancés restent en RAM | Qualification sur vraies séries décalées mono/CFA/WCS-SIP de grande taille ; pression mémoire avec les autres espaces ouverts ; contrôle des estimations et de la reprise 4 Gio/64 images ; moteur intégralement sur disque pour aller au-delà des budgets actuels |
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

## Préparation des observations — livrée en 1.18.0

L’onglet indépendant Observation couvre les objets OpenNGC, la Lune et sept
planètes, le lieu et la date locale, les horaires du Soleil, les créneaux de nuit,
la hauteur/obscurité, les favoris et des suggestions calculées sans connexion.
Nuit, horizon et carte régionale partagent le même instant UTC ; les informations
lunaires et les limites des calculs sont explicites. Les recherches de noms et de
villes en ligne sont facultatives et signalent les échecs sans effacer les résultats.

La 1.18.1 ajoute les prévisions nuageuses horaires Open-Meteo, avec cache, attribution,
fraîcheur et états de connexion. Les nuages sont une prévision de modèle distincte
des calculs géométriques. Les autres données météo, la turbulence, la pollution
lumineuse, les masques d’obstacles locaux, les réservations
et le pilotage d’acquisition restent des prolongements possibles. Le module n’est
pas une condition de justesse des images déjà produites.

La 1.18.2 simplifie le parcours Observation : sélection unique, accueil progressif,
protection contre les nuits terminées et vue Monde centrée sur la visibilité de
l’objet. La consultation des dates passées et tous les détails restent accessibles.

La 1.18.3 actualise les vues pendant le glissement du curseur, précise la vue
Ciel et le lieu par défaut, et rend la nomination des positions explicite.

La 1.18.4 ajoute l’heure de référence Mac/télescope, la conversion des créneaux
d’enregistrement et la saisie d’un instant partagé. Elle prépare un lancement
manuel ; elle n’ajoute pas de programmation ou de pilotage du télescope.

La 1.18.5 réunit Observation sur une page et ajoute la carte jour/crépuscule/nuit
hors connexion, avec suivi de l’heure réelle et couche de visibilité de l’objet.

La 1.18.6 commence par la situation du lieu choisi : heure, jour/nuit, prévision
nuageuse et prochains événements solaires. Les recommandations distinguent
l’instant affiché de la nuit entière ; une commande prépare la nuit sombre en cours
ou la suivante. Tous les lieux restent utilisables. T68 · Utah est seulement un
préréglage géographique, sans intégration avec un opérateur. L’ajout de liens
personnels sur les lieux reste une possibilité ultérieure.

La 1.18.7 supprime la sidebar d’Observation. La fiche du lieu et la carte jour/nuit
occupent 25/75 de la première ligne ; les commandes d’heure et d’objet viennent
ensuite, puis visibilité/boussole en 75/25 et hauteur/nuages en 50/50. Les favoris
de lieux passent en premier, avec tri alphabétique. Les coordonnées, la recherche
de ville et les réglages avancés utilisent des fenêtres. Les points de carte
recherchent leur fuseau en ligne, avec UTC conservé et repli explicite en cas
d’échec ; l’horloge réelle du Mac reste distincte de l’heure explorée.

La 1.18.8 empile le lieu et l’heure à gauche de la carte jour/nuit, rassemble les
recommandations instantanées et de toute la nuit dans le choix de l’objet, puis
supprime les deux sections redondantes du bas. Les prochains lever/coucher sont
affichés dans le fuseau du Mac. La reprise d’une session ouvre le dernier espace
de travail contenant un projet, même après sauvegarde depuis Accueil. Le cadre
macOS est sombre avec titre centré ; la version est au centre du pied de fenêtre.
Les 53 tests Release, les vérifications françaises aux trois tailles et la reprise
sur Orion passent. Les traitements et formats de projet restent compatibles.

La 1.18.9 synchronise les deux heures affichées avec l’instant choisi sur les
graphiques et les garde visibles au-dessus du défilement. Les créneaux utilisent
l’heure locale du Mac en premier, sans suffixe GMT. L’absence de recommandation
est indiquée dans le menu, sans effacer la cible affichée. La boussole rejoint
le lieu et l’heure dans la colonne gauche ; jour/nuit, objet et visibilité
occupent la colonne droite, puis hauteur/nuages prennent chacun une moitié de ligne.
Les UTC exacts, règles de changement d’heure, calculs et projets restent conservés.
