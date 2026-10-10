# ADR-026 — Masters de calibration par lots

Date : 10 octobre 2026. Lot 1.15.0.

## Périmètre

La bibliothèque existante créait un master à la fois et refusait les mélanges de
configurations. Ce lot ajoute un plan visible des **masters**, leur création
séquentielle et une recherche dans la bibliothèque. Il ne transforme pas
l’intégration en pipeline automatique pour des poses de configurations différentes.
Les utilisateurs choisissent toujours les masters à appliquer ; les sources,
sélections, recettes et résultats affichés ne sont pas remplacés par la création.

## Regroupement et revue

- Sources **sélectionnées**, de rôle Bias, DarkFlat, Dark ou Flat, en FITS ou RAW
  natif. Les masters enregistrés ne sont jamais moyennés comme sources brutes ;
  les bias/dark-flats sélectionnés et ceux de la bibliothèque peuvent servir
  de correctifs, sans application automatique aux poses.
- Identité stricte des valeurs enregistrées : rôle, caméra, largeur/hauteur,
  canaux, Bayer, binning X/Y, gain, offset, température, durée et unités de
  normalisation. Les flats séparent aussi filtre et groupe déclaré. Des écarts
  même petits créent des groupes distincts : aucune chaîne de tolérances ne
  fusionne des configurations. Les valeurs manquantes restent distinctes des
  valeurs connues ; elles peuvent encore masquer un mélange réel de matériel.
- Une revue explicite des fichiers et du journal d’observation est nécessaire
  pour les informations manquantes, y compris celles des correctifs. Cette revue
  est conservée dans l’entrée de bibliothèque. Une incompatibilité connue,
  dimension invalide ou métadonnée non finie ne peut pas être forcée.
- Un préfixe facultatif (40 caractères), noms descriptifs (120 caractères) et
  jusqu’à 64 groupes par lancement. Le plafond de 2000 fichiers du projet reste
  en place. Les noms des sources sont dépliables pour inspecter les groupes.

## Correction des flats et calcul

Les correctifs sont proposés selon les contrôles de compatibilité existants :
dimensions/canaux/Bayer, caméra, binning, gain, offset ; pour un dark-flat, durée
à 1 % ou 0,01 s et température à 2 °C. La normalisation d’entrée doit correspondre.
Un dark-flat unique est proposé avant un bias ; plusieurs dark-flats possibles
exigent un choix, sans repli silencieux vers un bias. Si aucun dark-flat adapté
n’est disponible, un bias unique peut être proposé. Des candidats incomplets
restent soumis à revue. Un correctif brut peut être calculé sans enregistrer un
master séparé : décocher sa création n’interdit pas son utilisation pour un flat.

Chaque flat est corrigé du piédestal **une fois**, puis normalisé avant moyenne
robuste. Le master est marqué corrigé/normalisé pour les futures intégrations.
Les groupes passent dans l’ordre Bias, DarkFlat, Dark, Flat, en conservant l’ordre
des sources à l’intérieur d’un groupe. Le moteur existant effectue la moyenne
avec rejet médiane/MAD dès trois sources ; une seule source ne réduit pas le
bruit. Valeurs négatives des darks/bias et précision Float32 sont conservées ;
les masters non finis et les flats invalides sont refusés lors de la publication.

`AC_ORIG` est conservé dans les métadonnées des fichiers Float32 importés :
`AC_SCALE=1` ne doit pas remplacer les unités d’origine par 1 dans la fiche du
master. La compatibilité des darks/bias/dark-flats contrôle ces unités ; les
flats normalisés sont sans dimension et ce contrôle ne s’applique pas à leur
utilisation sur les poses. Les anciens projets restent lisibles. Des unités
absentes demandent une revue ; elles ne prouvent pas l’équivalence des échantillons.

## Ressources et interruption

Le lot partage avec l’intégration le comptage des images/aperçus conservés de
Ciel profond. Le plan compte les buffers complets du master et une marge ; le
budget reste moitié de la RAM physique, au plus 8 Gio, repli à 2 Gio. Il ne mesure
pas la RSS, toutes les allocations internes ou les autres applications.

Le dossier temporaire de la 1.14 s’applique aux archives des masters du lot et
aux correctifs bruts. Le besoin annoncé est conservateur, incluant le plus grand
correctif possible d’un flat. Le disque est contrôlé avant calcul et pendant
l’écriture avec 256 Mio de réserve ; une archive privée par calcul est libérée
à la sortie. Résultats et chargements restent complets en mémoire, **32 MP**.
Les archives de groupes successifs ne sont pas retenues ensemble. Le dossier de
bibliothèque est distinct ; une erreur lors de son écriture arrête le lot.
Les limites existantes de bibliothèque (1000 entrées et index JSON de 2 Mio)
restent appliquées, y compris la nouvelle provenance.

La publication est atomique **par entrée**, pas pour tout le lot. Les SHA-256
sont vérifiés avant et après lecture ; des sources modifiées depuis l’import
sont refusées. Métadonnées décodées et configuration prévue sont comparées.
La révision du plan inclut aussi la bibliothèque ; un plan périmé est refusé
avant toute publication. Un index illisible bloque le plan sans exception
traversant l’interface QML. Sources et correctifs sont identifiés par leurs
empreintes dans l’entrée publiée.
Chaque groupe terminé est conservé. Première erreur ou annulation : arrêt,
bilan du groupe en échec/annulé et des groupes non traités. Une annulation avant
le premier chargement donne aussi ce bilan. L’opération ne signale pas une
réussite si un groupe échoue. Un arrêt forcé entre publication du FITS et de
l’index peut laisser un FITS non référencé ; il ne devient pas une entrée utilisable.

## Bibliothèque et interface

Recherche sur nom, caméra, groupe de filtre, durée, température et gain ; filtre
par rôle et option pour masquer les incompatibilités connues. Les entrées sont
classées compatibles, informations manquantes, incompatibles, puis par nombre
de sources et nom. Ce classement ne certifie ni le bruit ni la qualité du master.
Application explicite, empreinte du FITS contrôlée, règles de désélection et
portabilité inchangées. Les cases communes utilisent les couleurs Atelier dans
les fenêtres modales aussi ; focus, survol, désactivation, clavier et animation
d’activité du bouton sont conservés. Le guide expose le parcours et ses limites.

## Validation

Voir le [journal de validation](../design/VALIDATION.md) et le
[bilan avant V2](../releases/V2_READINESS.md). Les séries de calibration du lot
sont contrôlées/synthétiques ; les captures utilisent une vraie image d’Orion
avec des métadonnées de démonstration sur sa copie. Ce n’est pas une qualification
indépendante d’un matériel ni une comparaison avec Siril.
