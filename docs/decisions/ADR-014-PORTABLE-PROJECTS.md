# ADR-014 — projets portables et sources déplacées

Statut : implémenté dans 1.6.0, 9 octobre 2026.

## Besoin

Le projet JSON existant référence les sources et permet de recalculer une
intégration. Le déplacer sans les images le rendait inutilisable. La session
locale conserve déjà les résultats flottants, mais reste attachée au poste.

## Décision

Conserver les projets JSON version 1, et proposer un projet portable dans un
seul `.astroproject`. Il concerne l'espace Ciel profond : sources et fichiers
de calibration, masques des recettes et des deux versions, intégrations par
filtre, résultat linéaire, résultat fini, aperçus, réglages, légende et cadrage.
Les données flottantes restent en Float32, y compris les valeurs négatives et
supérieures à un. Les sources sont copiées octet pour octet.

Le conteneur version 2 est diffusé sans nouvelle dépendance :

1. Magic ASCII de 8 octets `ACPJ0002`.
2. Longueur uint32 little-endian du manifeste JSON.
3. SHA-256 binaire de 32 octets du manifeste, puis les octets du manifeste.
4. Les fichiers, dans l'ordre du tableau `files`, sans padding ni compression
   supplémentaire. Chaque ligne décrit `name`, `bytes` (entier décimal sous
   forme de chaîne) et `sha256` (hexadécimal).

`inputs` liste les sources sous `inputs/NNNNNN.ext`. `state/session.json` et les
assets UUID utilisent le format SessionStorage version 1. Les chemins de sources
sont remplacés dans l'état par `astroproject:inputs/NNNNNN.ext` ; à l'ouverture,
ils deviennent les chemins du cache local. Les noms d'origine restent dans les
lignes d'images et dans les empreintes. Les alias d'un même fichier canonique
partagent une seule copie. Les images flottantes partagées gardent leur identité
après restauration, ce qui évite de dupliquer les empilements.

Les fichiers sont lus et écrits par blocs de 1 Mio, avec vérification SHA-256,
points d'annulation et QSaveFile. Une source dont l'empreinte a changé depuis
l'import est refusée. L'extraction utilise un dossier temporaire, valide toutes
les données et les réglages, puis publie le cache avant le remplacement atomique
du workspace sur le thread graphique. Le projet courant reste intact en cas
d'erreur ou d'annulation avant publication.

Les noms de fichiers sont bornés et validés, sans chemins libres ni traversée
de dossiers. Les doublons, références de sources externes, données tronquées,
octets supplémentaires, tailles incohérentes et empreintes incorrectes sont
refusés. Limites : manifeste 64 Mio, 4 500 fichiers, archive 128 Gio, résultats
et aperçus 4 Gio selon les limites de SessionStorage. L'estimation de taille
utilise les sources et buffers uniques et une estimation conservatrice des
PNG ; elle reste approximative. La sauvegarde vérifie l'espace du volume cible
avant les fichiers temporaires, puis à nouveau avant l'écriture du conteneur.

## Réassociation

Les nouveaux imports enregistrent taille et SHA-256 hors du thread graphique.
La recherche parcourt le dossier choisi, jusqu'à 50 000 fichiers, et compare
nom, taille puis contenu. Les chemins sont remplacés dans les images, les
références, les masques, les opérations et les versions A/B ; les résultats
calculés restent les mêmes buffers. Les fichiers non résolus restent signalés.

Les anciens projets sans empreinte demandent une option explicite. Seul un
candidat unique de même nom et de mêmes métadonnées enregistrées (dimensions,
canaux, profondeur, Bayer, pose, filtre, gain et température si présents) est
accepté. Cela ne prouve pas l'identité des pixels. Les anciens masques sans
empreinte se rechargent depuis l'outil Masques. Une sauvegarde portable crée
les empreintes de toutes ses sources, même si le projet initial était ancien.

## Cache et reprise

Chaque ouverture portable crée un cache UUID dans le CacheLocation de Qt,
sous `projects`, avec un marqueur `.astrocache` version 1. Les réglages affichent
le poids des caches et de la reprise automatique. Le nettoyage se limite aux
caches possédés et inutilisés : il protège les chemins des trois workspaces,
leurs documents et leur interface, ainsi que ceux du manifeste de reprise sur
disque. Les alias de chemins sont comparés aussi sous leur forme canonique.
Une reprise illisible bloque le nettoyage. Les workspaces et raccourcis d'ouverture
sont verrouillés pendant celui-ci. Les projets sauvegardés et originaux restent
hors de la suppression ; les liens symboliques ne sont pas suivis.

La pile d'annulation et les calculs en cours ne sont pas enregistrés. L'ouverture
et la réassociation restaurent les réglages et A/B et réinitialisent la pile
d'annulation. Les caches internes de recette sont recalculés à la prochaine
modification. Un portable volumineux consomme de l'espace temporaire à la
sauvegarde, puis un cache local à l'ouverture.

## Vérification

`PortableProjectTest` vérifie le transport après retrait des originaux, les
Float32 exacts négatifs/HDR, les empilements partagés, masques importés et peints,
A/B, légende, références, réapplication, composition et FITS linéaire. Il couvre
la corruption, traversée de chemins, doublons, tailles excessives, annulation
pendant le payload, sources modifiées, homonymes, anciens projets, protection
des caches actifs et de reprise, alias canoniques, liens externes et reprise
corrompue. Le parcours natif utilise Orion 6 244 × 4 170 en français aux trois
tailles de référence, avec comparaison des empreintes des buffers flottants.
