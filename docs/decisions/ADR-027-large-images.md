# ADR-027 — Images jusqu’à 128 MP avec budgets explicites

Date : 11 octobre 2026. Livraison : 1.16.0.

## Besoin et portée

Une source de 6244 × 4170 px (26,04 MP) en drizzle ×2 donne 12488 × 8340 px
(104,15 MP). Le plafond précédent de 32 MP empêchait ce parcours. Le nouveau
plafond commun est **128 000 000 pixels**, avec **32 700 pixels par dimension**
pour rester dans les coordonnées prises en charge par les transformations
OpenCV. Il concerne les images scientifiques, images fixes de Traitement,
finition, géométrie, sortie de mosaïque, exports et images sauvegardées.
Les entrées de mosaïque restent limitées à 24 MP ; les limites propres à
l’analyse des vidéos ne sont pas modifiées par cette livraison.

Ce plafond n’est pas une promesse que chaque combinaison passe sur tout Mac.
Le budget est toujours la moitié de la RAM physique, plafonné à **8 Gio**,
ou 2 Gio si la RAM est inconnue. L’estimation avant empilement et les contrôles
avant finition/refinition comprennent les images conservées de leur espace.
Des cartes, plusieurs filtres, couches étoiles/fond ou des traitements avancés
peuvent dépasser ce budget. L’opération est alors refusée avec une explication
et le résultat précédent est conservé. Les autres espaces ouverts, autres
applications et allocations graphiques ne sont pas mesurés exactement.

## Calcul et précision

La projection drizzle accumule maintenant dans des bandes horizontales
complètes. Sommes et poids restent en double précision ; la sortie et les
poids restent Float32. Leur allocation combinée est bornée à **64 Mio**.
L’ordre source y/x est conservé pour chaque pixel destination. Les bords sont
calculés dans les coordonnées globales, puis indexés dans la bande locale.
Les bornes affines inverses réduisent les lignes source parcourues sans
modifier les empreintes. Le cas WCS/SIP parcourt toutes les lignes source par
bande : les empreintes courbes ne sont pas approximées pour gagner du temps,
mais ce mode peut être plus lent sur une très grande image.

Chaque bande est écrite aux offsets RGB puis poids de l’archive de travail
existante. Aucun fichier supplémentaire par pose. Les réservations abandonnées
sont tronquées avant de poursuivre après le rejet d’une pose. Les erreurs de
stockage interrompent le calcul complet, et l’annulation est propagée. Les
contrôles de disque et la réserve de 256 Mio de l’ADR-025 restent applicables.
Le rejet reste borné par tuiles horizontales/verticales de 64 Mio.

La finition Metal utilise déjà des bandes avec recouvrement ; les calculs
CPU et traitements avancés peuvent toujours utiliser des images intermédiaires
complètes. Les résultats scientifiques **restent des vecteurs complets en RAM**.
Cette livraison n’est donc pas un moteur entièrement indépendant de la taille
sur disque. Les combinaisons coûteuses sont contrôlées avant calcul, avec un
modèle conservateur plutôt qu’une promesse de mémoire résidente exacte.

## Import, export et reprise

FITS est lu/écrit par blocs de 262 144 échantillons, sans image planaire complète
supplémentaire. L’export reçoit directement les pixels immuables et des métadonnées
séparées. Le stockage Float32, la normalisation d’entrée, les valeurs signées,
les métadonnées et les checksums FITS sont conservés. TIFF garde la même
organisation et les mêmes échantillons 16 bits, mais encode 128 Kio à la fois,
sans reconstruire le fichier entier en mémoire. La copie atomique finale et
la protection des originaux restent en place. PNG 16 bits reste un export
sans perte via Qt ; son encodage peut demander une image complète.

La limite du décodeur Qt est fixée une seule fois à 1024 Mio, suffisante pour
128 MP en RGBA64. Dimensions et budget sont contrôlés séparément avant lecture.
Les formats des projets et de la reprise restent inchangés. La reprise conserve
son plafond de **4 Gio / 64 références d’images** ; les pixels sont sauvegardés
par blocs, avec vérification des empreintes avant restauration. Le chargement
vérifie le volume total et les dimensions/mémoire décodées des aperçus avant
d’allouer les images. La finition contrôle aussi la taille prévue des nouvelles
couches. Un état trop volumineux n’est pas annoncé comme sauvegardable.

## Qualification et limites

Les tests comparent bandes étroites et calcul complet pixel pour pixel :
mono/RVB, quatre matrices CFA, translations, rotation, cisaillement et SIP,
ainsi que valeurs signées, trous, poids et rejets. Les tests de stockage couvrent
lecture par offsets, réservation abandonnée, annulation et fermeture du processus.
Le contrôle natif optionnel `ASTRO_UI_LARGE_QA` utilise deux copies de la même
image réelle pour isoler les ressources et la précision, sans prétendre à un
gain de résolution, de photons ou à une qualification d’une vraie série drizzle.
Les résultats mesurés et les contrôles de livraison sont consignés dans
[VALIDATION.md](../design/VALIDATION.md).

Les séries réelles avec déplacements subpixel, la pression mémoire avec les
autres espaces et applications, le drizzle WCS/SIP de grande taille et la
comparaison finale Siril restent à qualifier séparément.
