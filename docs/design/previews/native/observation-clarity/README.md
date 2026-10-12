# Observation — version 1.18.2

Captures de la fenêtre native en français : accueil progressif, cible active,
choix unique Catalogue/Favoris, courbe, suggestions, carte et prévisions nuageuses.
Les vues principales sont conservées aux tailles 1440 × 960, 1100 × 800 et
900 × 700. Les noms de fichiers indiquent la vue et la largeur de fenêtre.

M42 et la Lune utilisent les calculs locaux de l’application. Les graphiques
nuageux utilisent les valeurs horaires réelles Open-Meteo archivées dans
`tests/unit/app/observation-weather.json`, avec un cache de validation indépendant
du réseau. Les autres lieux et dates montrent explicitement l’absence de données.

Les panneaux supplémentaires restent dépliables ou accessibles par défilement.
La carte présente la visibilité de la cible au même instant UTC, selon les seuils
de hauteur et d’obscurité ; elle ne constitue pas une prévision météo mondiale.
