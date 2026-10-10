# AstroCapture — prochaines étapes

État après le lot 1.9.0 du 10 octobre 2026. Cette liste distingue ce qui est livré
et les prochains travaux ; elle ne promet pas une équivalence complète avec Siril.

## Déjà livré

- [x] WCS TAN/ICRS guidé avec Gaia DR3 et catalogue CSV local, vérification des
  associations, repères et RMS ; WCS FITS propagé avec les transformations.
- [x] PCC par modèle de températures Gaia et SPCC par spectres XP mesurés,
  réponse de l’équipement importable, blancs CALSPEC/AB, diagnostics et
  références liées à l’image ; projets, session, versions A/B et Cmd+Z.
- [x] Recherche du nom de cible via CDS Sesame pour préparer le centre WCS,
  avec conservation du WCS existant et cache des réponses réussies.
- [x] Bibliothèque de réponses RVB mesurées importées : caméra, filtres,
  provenance, notes, copie dans le projet et sauvegarde JSON.
- [x] Coordonnées ICRS au survol du résultat et copie, avec géométrie,
  zoom et comparaison avant/après.

- [x] Aperçu de légende affiché dès l’ouverture, indicateur animé d’export,
  familles de Finition indépendantes et état conservé après réouverture.
- [x] README illustré avec les cinq captures fournies : Ciel profond,
  Traitement (Saturne et Lune), Mosaïque et Historique.

- [x] Sauvegarde automatique et reprise de Traitement, Ciel profond et Mosaïque,
  avec résultats calculés en flottants, réglages, légendes, versions A/B et vue.
- [x] Export des résultats sauvegardés même si un fichier source manque.
- [x] Protection à la fermeture, y compris pour une image PNG/TIFF seule ;
  sauvegarder la session et quitter attend la dernière sauvegarde.
- [x] Recadrage interactif avec déplacement, poignées, proportions et coordonnées
  en pixels ; rotation libre, interpolation et changement d’échelle.
- [x] Masques peints et importés FITS/PNG/TIFF, dureté, opacité, flou du bord,
  inversion et portée par type de traitement. Masques persistants dans les
  recettes, projets, préréglages, versions A/B et sessions.
- [x] Page « Repères Siril » dans le guide et installateur 1.5.0.

- [x] Projets Ciel profond portables : sources et calibrations, masques importés,
  empilements par filtre et résultats flottants exacts, légende, réglages et A/B.
  Taille estimée, progression, annulation et publication atomique.
- [x] Réassociation des sources déplacées par contenu ; comparaison explicite
  des métadonnées pour les anciens projets, refus des cas ambigus.
- [x] Poids des caches et de la reprise automatique ; nettoyage des caches
  inutilisés avec protection du travail ouvert et de la session sur disque.

La session conserve les résultats terminés. Elle ne reprend pas un calcul
interrompu ni une acquisition caméra ; la pile d’annulation ne traverse pas le
redémarrage. Hors projet portable, les masques importés restent référencés par leur chemin.
Le projet portable ne sauvegarde pas la pile d’annulation ni un calcul en cours.
Il conserve les empilements terminés par filtre ; les caches internes des étapes
de recette sont reconstruits à la prochaine modification.

- [x] Résolution aveugle Astrometry.net locale, index grand champ téléchargeable,
  vérification Gaia et SIP 2–5 conservé par les transformations et les FITS.
- [x] Bibliothèque de masters avec création robuste, provenance FITS, contrôle
  de compatibilité, statistiques et intégration sans double calibration des flats.
- [x] Couches étoiles/fond par PSF native, vues séparées, réglages indépendants,
  recombinaison neutre exacte et exports Float32 ; persistance portable et A/B.
  Le résidu conserve les étoiles rejetées : ce moteur n’est pas StarNet.

## Priorités suivantes

1. **Approfondir l’astrométrie et les profils spectraux.** Annotations d’objets, profils fabricant prévalidés et
   composition interactive des réponses QE/filtres. Comparaison scientifique
   avec des étalons et Siril ; modes bande étroite distincts.
2. **Calibration avancée.** Regroupement automatique de séries hétérogènes,
   recherche multicritère et diagnostics approfondis des masters ; validation
   sur de nouvelles caméras et séries réelles.
3. **Extraction stellaire avancée.** Moteur neuronal optionnel et modèles dont
   la redistribution est autorisée, ou import de couches externes ; contrôle des
   halos et aigrettes, réduction du rayon et validation sur champs denses.
4. **Performance et séries.** Aperçu sur région d’intérêt, caches ciblés,
   comparaison de performances, accélération des outils avancés, traitements
   et exports de plusieurs images avec état et annulation par tâche.
5. **Intégration avancée.** Drizzle, pondération plus fine des poses et diagnostics
   FWHM/excentricité/SNR ; définir les unités et valider sur jeux synthétiques
   connus puis brutes réelles.
6. **Compositions scientifiques.** HDR multi-expositions, mosaïques linéaires et
   correction des distorsions, avec propagation cohérente de la couverture
   et de l’astrométrie.
7. **Acquisition et diffusion.** Validation sur les vraies caméras, longues
   sessions, reconnexions et captures interrompues ; signature et notarisation
   Apple de l’installateur.

## Plus tard — préparation des observations

- [ ] **Visibilité des objets depuis la Terre.** Saisir un lieu (ville ou
  latitude/longitude) pour connaître les objets visibles et leurs créneaux
  d’observation ; ou rechercher un objet pour savoir s’il est visible depuis
  un lieu choisi et depuis quelles régions du monde il peut être observé.
  Prévoir une carte mondiale et un choix de date, heure et fuseau horaire,
  avec hauteur au-dessus de l’horizon, direction et périodes de nuit locale.
  Distinguer un objet au-dessus de l’horizon d’un objet observable dans de
  bonnes conditions. Demande ajoutée le 10 octobre 2026 ; fonctionnalité
  planifiée, pas encore implémentée.

## Repères pour un utilisateur de Siril

| Repère | Dans AstroCapture | Point à vérifier |
| --- | --- | --- |
| Calibration, alignement, empilement | Ciel profond → Images / Empilement | Rôles des images, échelle d’import, normalisation et rejet sigma/MAD |
| Gradient | Finition → Fond de ciel | Degré, échantillons manuels, tolérance et modèle extrait |
| Étirement | Étirement et Transformations avancées | Niveaux, asinh, GHS, SP/LP/HP, force, intensité locale et inverse |
| Déconvolution | Transformations avancées → PSF et déconvolution | Richardson–Lucy, PSF, FWHM en pixels, itérations, amortissement, dosage et arrêt |
| Débruitage | Transformations avancées → Débruitage multiscale | Échelles, seuil robuste et dosages luminance/chrominance ; moteur distinct de NL-Bayes/DA3D |
| Masque | Transformations avancées → Masques | Blanc applique, noir protège ; dimensions de l’entrée de recette et portée du traitement |
| Géométrie | Finition → Géométrie | Coordonnées depuis zéro en haut à gauche, dimensions, interpolation et limite de 32 MP |
| Conservation du linéaire | Exporter → FITS linéaire | Intégration d’origine ; Image finale → FITS conserve le résultat traité flottant |
| WCS / PCC / SPCC | Finition → Couleur → Astrométrie | Centre et échantillonnage, Gaia DR3 ou CSV ; PCC continuum, SPCC XP avec réponse mesurée ; inspecter RMS, références et gains |

Les noms et paramètres servent de repères. Les moteurs, valeurs et conventions
ne sont pas tous interchangeables ; la validation numérique compare chaque
opération séparément. La rotation utilise OpenCV ; la limitation des dépassements
est faite sur l’étendue de chaque canal, pas avec le clamping local de Siril.

Références consultées : [géométrie Siril](https://siril.readthedocs.io/en/stable/processing/geometry.html)
et [masques Siril](https://siril.readthedocs.io/en/latest/processing/masks.html).
La page des masques décrit la branche récente de Siril ; les outils accessibles
peuvent dépendre de la version installée.
