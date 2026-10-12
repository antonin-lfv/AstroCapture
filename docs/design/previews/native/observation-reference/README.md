# Observation — version 1.18.4

Rendus de la fenêtre native en français aux trois tailles de validation.
`observation-reference-*` montre la saisie de l’heure chez soi et le créneau pour
M42 depuis Paris. `observation-reference-remote-*` compare le Mac en Europe/Paris
et le télescope à Sydney : un même instant peut appartenir à deux dates différentes.
La courbe utilise l’heure du télescope, indiquée explicitement.

Le début d’enregistrement est le premier créneau restant après l’heure réelle,
indépendamment du curseur d’exploration. Il est arrondi vers la minute suivante,
avec une fin arrondie vers la précédente. Les calculs de M42 et de la Lune viennent
du moteur local, avec les coordonnées et les seuils réellement choisis.

Les autres captures documentent le parcours conservé : accueil, objet, favoris,
nuit refusée, ciel, suggestions, carte et saisie d’un nom de lieu.

`observation-location-same-instant-900` montre le changement de Paris à Sydney
par le sélecteur de lieu : le curseur conserve l’instant UTC et l’heure du Mac,
avec conversion de l’heure au télescope. Cette invariance est aussi vérifiée
pendant tous les signaux du contrôleur, y compris les périodes de 23/25 heures.
