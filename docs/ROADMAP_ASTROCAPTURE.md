# AstroCapture — prochaines étapes

État après le lot 1.6.1 du 10 octobre 2026. Cette liste distingue ce qui est livré
et les prochains travaux ; elle ne promet pas une équivalence complète avec Siril.

## Déjà livré

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

## Priorités suivantes

1. **Astrométrie et calibration des couleurs sur catalogue.** Résolution WCS,
   correspondance des étoiles et calibration PCC/SPCC avec bilans, filtres et
   références explicites. La photométrie actuelle utilise un CSV d’étoiles
   déjà identifiées et des gains relatifs ; elle n’est pas une SPCC.
2. **Bibliothèque de calibration.** Masters dark/flat/bias/dark-flat réutilisables,
   regroupement par caméra, gain, binning, température et exposition ; bilan des
   incompatibilités et contrôle de qualité des masters.
3. **Traitement séparé des étoiles et du fond.** Extraction/recombinaison,
   réduction contrôlée des étoiles et comparaison de l’effet sur les structures.
   Choisir et valider le moteur avant d’exposer une commande de production.
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
| PCC / SPCC automatique | À implémenter | Le CSV actuel est une calibration relative sans résolution WCS ni catalogue automatique |

Les noms et paramètres servent de repères. Les moteurs, valeurs et conventions
ne sont pas tous interchangeables ; la validation numérique compare chaque
opération séparément. La rotation utilise OpenCV ; la limitation des dépassements
est faite sur l’étendue de chaque canal, pas avec le clamping local de Siril.

Références consultées : [géométrie Siril](https://siril.readthedocs.io/en/stable/processing/geometry.html)
et [masques Siril](https://siril.readthedocs.io/en/latest/processing/masks.html).
La page des masques décrit la branche récente de Siril ; les outils accessibles
peuvent dépendre de la version installée.
