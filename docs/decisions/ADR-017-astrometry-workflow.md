# ADR 017 — Cibles, profils de matériel et coordonnées

10 octobre 2026 · AstroCapture 1.8.0.

Ce lot prolonge le WCS TAN guidé et PCC/SPCC de l’[ADR 016](ADR-016-astrometry-colors.md).
Il ne change ni le solveur, ni les algorithmes de calibration, ni les pixels
linéaires lors d’une recherche ou d’un choix de profil.

## Recherche d’une cible

Dans Ciel profond → Finition → Couleur → Astrométrie · PCC / SPCC, l’étape WCS
accepte un nom de catalogue, par exemple M42 ou NGC 7000. CDS Sesame interroge
SIMBAD, NED et VizieR et propose une position J2000. Cette position renseigne
les champs AD/Déc de la recherche guidée ; elle ne prouve pas que l’objet est
dans l’image. Renseigner l’échantillonnage, résoudre, puis inspecter les repères
et les diagnostics. Le WCS vérifié et les réglages précédents restent intacts.

Seul le nom, limité à 120 caractères, est transmis par HTTPS ; l’image n’est
jamais envoyée. Le nom entier est encodé comme une seule requête. Réponses XML
limitées à 64 KiB, sans DTD ni entités, un seul nom correspondant à la requête
et une seule solution complète RA/Déc finie dans les bornes astronomiques.
Un résolveur indisponible peut être suivi d’un résolveur valide ; des coordonnées
partielles réparties entre résolveurs ou plusieurs solutions sont refusées.

La requête est annulable, limitée à 15 s d’inactivité et 30 s au total. Une erreur
ne publie aucune position. Les réponses réussies sont conservées 30 jours dans
un cache distinct de 1 MiB maximum. La même recherche récente peut fonctionner
hors ligne ; une nouvelle cible exige une connexion. Le nom saisi suit le projet
et la session. La recherche ne remplace pas la saisie manuelle et ne concerne
pas les objets du Système solaire.

## Profils de matériel

Dans l’étape PCC/SPCC, Profils de matériel permet d’importer une réponse photonique
RVB mesurée en CSV, puis de l’enregistrer avec un nom, la caméra/le capteur, les
filtres, la source de mesure et des notes. La courbe doit déjà comprendre toute
la réponse effective : QE × filtres RVB × transmission supplémentaire éventuelle.
Les descriptions ne fabriquent ni ne modifient les courbes. Les canaux doivent
partager la même normalisation relative. Aucune bibliothèque de courbes fabricant
prévalidées ni composition interactive de filtres n’est fournie par ce lot.

Utiliser copie les données dans l’état du projet. La calibration déjà appliquée
reste identique jusqu’à un nouveau lancement explicite de PCC/SPCC. Retirer le
profil global ne retire ni la réponse du projet ni les références de calibration.
Un import CSV réussi remplace la réponse chargée ; un import refusé la conserve.
Enregistrer un profil existant nécessite la commande distincte Remplacer.

La bibliothèque globale est conservée séparément des autres préférences. Importer
et Exporter sauvegardent un fichier JSON versionné, limité à 32 profils et 4 MiB.
Le fichier contient `version: 1` et `profiles`, chaque profil conservant :

- `version`, `name`, `camera`, `filters`, `source`, `notes`, `fileName` ;
- `responseHash`, SHA256 du fichier CSV original ;
- `dataHash`, SHA256 du JSON compact des échantillons numériques normalisés ;
- `response`, liste de lignes `[longueur_nm, rouge, vert, bleu]`.

Noms limités à 80 caractères, autres textes à 512, courbes de 3 à 2 000 lignes,
longueurs strictement croissantes dans 336–1 020 nm, réponses 0–1 et extrémités
nulles ; chaque canal doit contenir un signal. Avant utilisation, l’intégrale
photonique est vérifiée par le moteur scientifique. Les empreintes contrôlent
l’intégrité, sans authentifier le matériel ni la source de mesure.

Les doublons de nom sont comparés sans distinction de casse. Un conflit de données
interrompt l’import entier ; un profil identique peut être réimporté. Le JSON est
vérifié avant publication, l’export est atomique et une erreur de sauvegarde des
préférences conserve la bibliothèque active. Un ancien projet contenant une
réponse CSV reste utilisable et peut l’ajouter à la bibliothèque. Le projet portable
conserve la courbe et ses métadonnées indépendamment de la bibliothèque locale.

## Coordonnées au survol

Après vérification du WCS, le résultat affiche sous le pointeur les coordonnées
ICRS du centre du pixel : AD en heures/minutes/secondes, Déc en degrés/minutes/
secondes. L’icône de copie reprend la dernière position de l’image, également en
degrés décimaux, sans la remplacer par la position du bouton.

La conversion utilise le WCS de la recette complète, en respectant les centres
de pixels, dimensions arrondies, recadrage, rotation libre, miroirs et changement
d’échelle. Le zoom et le déplacement de la vue ne changent pas la position céleste.
Avant/Résultat/Partagé utilisent la même géométrie de finition. Les coordonnées
sont masquées sur la source brute, le masque, le modèle de fond, pendant un calcul
ou en dehors de l’image. Le nombre de décimales ne mesure pas l’exactitude :
celle-ci dépend du WCS ajusté et de ses résidus. TAN reste le seul modèle fourni ;
SIP, résolution aveugle et annotations d’objets restent dans la feuille de route.

## Références et validation

- [CDS Sesame — documentation officielle](https://cds.unistra.fr/doc/sesame.htx).
- Réponse réelle M42 utilisée comme fixture :
  `tests/unit/fixtures/cds-sesame-m42.xml`, reçue le 10 octobre 2026.
- [Contrôle sur Orion, persistance et tests](../VALIDATION_SIRIL.md).
- [Validation de l’interface aux trois tailles](../design/VALIDATION.md).

L’implémentation est indépendante. La comparaison scientifique avec Siril reste
un travail distinct ; la provenance saisie ne certifie pas une réponse instrumentale.
