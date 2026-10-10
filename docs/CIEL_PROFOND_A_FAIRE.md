# Ciel profond et prochaines étapes

État consolidé au 10 octobre 2026, version 1.9.0. Le README décrit les fonctions disponibles ; cette note contient uniquement les travaux restants.

## Validation et fluidité

- Comparer les intégrations linéaires AstroCapture et Siril sur les mêmes données M8/M20. La chaîne AstroCapture a déjà abouti sur les 15 poses ; la comparaison quantitative reste à faire. Voir VALIDATION_SIRIL.md et TUTORIEL_M8_M20_ASTROCAPTURE.md.
- Valider ensuite des acquisitions personnelles et des FITS de télescope distant, en distinguant brutes et données déjà calibrées.
- Mesurer calibration, alignement, intégration, mémoire et espace temporaire ; vérifier annulation et grandes séries. Ajouter aperçus réduits/ROI et améliorer le cache après profilage, avant de nouveaux portages Metal.
- Vérifier les longues poses, niveaux 16 bits et métadonnées sur les caméras physiques prises en charge.

## Traitements à compléter

- Valider PCC/SPCC sur des réponses mesurées du matériel réel ; élargir les champs de contrôle du solveur aveugle/SIP et les jeux d’index. Résolution locale, Gaia DR3 et PCC/SPCC sont disponibles.
- PSF elliptique ou variable sur le champ, estimation plus avancée et déconvolution régularisée. Les masques peints/importés sont disponibles.
- Retrait de gradient RBF et division, réglages de grille par séquence ; contrôle exact des pertes finales et distinction saturation source/écrêtage ajouté.
- Sélection automatique contrôlée des masters par configuration, création par lots et diagnostics de calibration plus détaillés. La bibliothèque avec métadonnées, compatibilité et rejet robuste est disponible.
- Cartes de poids et diagnostics de normalisation/rejet exportables ; inspection/édition des métadonnées FITS et séparation des canaux RGB.
- Débruitage plus avancé et retrait complet des étoiles, éventuellement avec un moteur externe. Le traitement séparé actuel utilise des profils gaussiens/Moffat : les étoiles rejetées restent dans le fond.

## Intégration et grands champs

- Drizzle/Bayer drizzle, correction des distorsions, interpolation configurable et normalisation locale.
- Champ de sortie configurable, changement d’échelle, mosaïques linéaires et gestion de très grands champs.
- HDR, projections grand angle et matrices RAW supplémentaires dont X-Trans.

## Acquisition et distribution à plus long terme

- Pilotage de monture, guidage et dithering.
- Annotations NGC, Messier, planètes et cratères, avec confiance et correction manuelle.
- Visibilité d’une cible depuis une position terrestre et régions du monde favorables à son observation (demande utilisateur conservée).
- Signature Developer ID et notarisation macOS.

Les automatismes restent optionnels, contrôlables et réversibles. Préserver les sources et le moteur indépendant ; ne pas annoncer de compatibilité matérielle ou de précision scientifique sans validation.
