# Captures du README — AstroCapture 1.18.9

Captures du 13 octobre 2026, produites depuis la fenêtre Qt/QML complète en français. Les contrôleurs et le moteur d’AstroCapture chargent les fichiers fournis. Les réglages, caches de projet et de reprise utilisent des dossiers de validation séparés. La session personnelle et la caméra ne sont pas utilisées.

| Page | Données affichées | Calcul |
| --- | --- | --- |
| Traitement | `jupyter.ser`, qui montre Saturne | 8 984 images analysées, 2 246 conservées (25 %), alignement puis ondelettes légères ; résultat de 612 × 608 pixels |
| Ciel profond | Dix FITS « Calibrated color » du dossier M42 du 9 octobre, iTelescope T68 | Poses déjà calibrées, alignement, normalisation, rejet robuste ; 10/10 poses et 300 s ; finition asinh puis tons moyens et atténuation du vert ; 6 244 × 4 170 pixels |
| Mosaïque | Les 70 PNG lunaires du dossier du 30 mai 2023 | Assemblage natif avec harmonisation de luminosité et adoucissement des bords ; aucune vue exclue ; 5 353 × 4 539 pixels |
| Observation | M42 depuis le lieu T68 · Utah | Instant choisi : 13 octobre 2026 à 04 h 30 sur place, 12 h 30 sur le Mac en Europe/Paris. Carte et positions calculées localement ; prévision Open-Meteo récupérée par l’application |

Les trois compositions 1440 × 960, 1100 × 800 et 900 × 700 ont été inspectées. Le cadrage de Saturne a été ajusté avec le zoom natif pour garder l’ensemble de la planète visible. Les vues du README utilisent les captures 1440 × 960, copiées à l’identique dans `assets/screenshots/readme-*.png`.

Les empreintes SHA-256 des 81 fichiers sources (SER, 10 FITS et 70 PNG) sont identiques avant et après les opérations. Les aperçus dans les captures sont distincts des données originales et des résultats en pleine résolution. Les archives de reprise locales ont servi à reprendre le cadrage de Traitement ; la mosaïque montrée a été recalculée depuis ses 70 sources.

`validation.json` conserve les bilans natifs des calculs et l’état de la météo au moment de la capture. Les prévisions de cette illustration correspondent à cette date et à ce lieu ; elles ne décrivent pas la météo actuelle à la lecture du README. Les cadres peuvent être défilés dans l’application : une capture de fenêtre montre la portion visible de la page.
