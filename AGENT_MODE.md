# Traitement local par un agent

Depuis la version 1.21.0, AstroCapture propose une entrée `--agent` pour traiter
des images de ciel profond sans ouvrir de fenêtre. Elle réutilise les commandes et le moteur de l'application :
aucun serveur permanent, aucune modification de l'interface, aucun envoi d'images.
Lancer normalement AstroCapture ne démarre pas ce mode.

Cette première version couvre l'inspection, la calibration, l'alignement,
l'empilement par groupe, une finition simple et les exports. Elle ne pilote pas
la capture, le planétaire, les mosaïques, la composition entre filtres ou
l'astrométrie. Le choix artistique reste à évaluer sur les aperçus.

## Lancement

Sur macOS, appeler directement l'exécutable de la version construite avec ce mode :

```sh
APP="/Applications/AstroCapture.app/Contents/MacOS/AstroCapture"
"$APP" --agent help
"$APP" --agent inspect "/chemin/vers/mes-images"
"$APP" --agent run "/chemin/vers/recette.json"
```

Pour un build local, remplacer `APP` par
`build/release/bin/AstroCapture.app/Contents/MacOS/AstroCapture`.
Sur Linux, utiliser `./AstroCapture.AppImage --agent …` ou le binaire construit.
Les versions déjà installées avant l'ajout de ce mode ne disposent pas de ces commandes.

Le processus utilise Qt en mode `offscreen` pour les services d'image, sans charger
l'interface QML. Les paquets doivent inclure le plugin Qt `offscreen` ; les scripts
de distribution Mac et Linux le prévoient. Il consomme les ressources nécessaires
au traitement, avec les limites mémoire habituelles du moteur.

## Inspecter avant de traiter

`inspect` accepte un fichier image ou un dossier parcouru récursivement.
Les liens symboliques rencontrés dans un dossier ne sont pas parcourus. Les formats
sont FITS (`fit`, `fits`, `fts`, `fz`), TIFF, PNG et RAW Bayer pris en charge.
Le dernier événement contient `frames` : chemins, empreintes des sources,
dimensions, rôle proposé, groupe, métadonnées disponibles, nombre d'étoiles et
FWHM lorsqu'elle est mesurable. Cette inspection décode les images et mesure les
étoiles ; ce n'est pas une simple liste de fichiers.

Les rôles déduits des noms et métadonnées sont des **suggestions**. L'agent doit
les vérifier avant de préparer une recette. Un fichier illisible fait échouer
l'inspection ou le traitement ; il n'est pas ignoré silencieusement.
Le plafond est de 2 000 images par tâche. Éviter de mélanger sources et anciens
exports dans le même dossier : tous les fichiers de formats reconnus sont lus.

`inspect resultat.astroproject` vérifie aussi la réouverture d'un projet portable
et renvoie ses groupes, dimensions, réglages de finition et bilan d'empilement.
Les sources embarquées sont extraites dans un cache temporaire supprimé à la fin.

## Recette version 1

```json
{
  "version": 1,
  "inputs": [
    { "path": "images/lights", "role": "Light", "group": "RGB" }
  ],
  "calibrated": true,
  "output": "resultats-001",
  "sigma": 3,
  "normalize": true,
  "correctBadPixels": true,
  "finish": {
    "saturation": 1.0,
    "denoise": 0.0
  }
}
```

Les chemins relatifs sont résolus depuis le dossier de la recette. Le dossier
parent de `output` doit exister ; `output` lui-même doit être nouveau, même si un
dossier existant est vide. Les sources et les précédents résultats sont préservés.
Les champs inconnus, les doublons et les types incorrects sont refusés.

- `version` : obligatoire, `1`.
- `inputs` : liste non vide de fichiers ou dossiers. Chaque entrée exige `path`
  et un `role` explicite : `Light`, `Dark`, `Flat`, `Bias`, `DarkFlat`, `BadPixels`.
  `group` est facultatif ; son absence utilise le regroupement habituel selon le
  filtre et les métadonnées. Les masters reconnus par leurs métadonnées gardent
  leur rôle et les validations existantes du moteur.
- `calibrated` : booléen obligatoire. `true` uniquement pour des poses déjà
  calibrées ; aucune entrée de calibration ne doit alors être fournie.
  Pour des poses brutes, mettre `false` et ajouter les calibrations adaptées.
  L'absence de calibration ne signifie pas que les poses sont déjà corrigées.
- `sigma` : seuil de rejet, entre 1 et 10, défaut 3.
- `normalize`, `correctBadPixels` : booléens, défaut `true`.
- `finish` : ajustements facultatifs sur l'étirement proposé par AstroCapture.
  Champs disponibles : booléens `stretch`, `background` ; nombres `black`, `white`,
  `midtones`, `red`, `green`, `blue`, `denoise`, `saturation`.
  Ils sont validés par les mêmes règles que l'application. Le bilan contient
  tous les réglages réellement appliqués, y compris les valeurs automatiques.

Les groupes sont empilés et exportés séparément ; aucune composition RVB/SHO/Ha
n'est déduite automatiquement. Les contrôles de calibration du moteur restent actifs.

## Résultats et reprise

Le nouveau dossier contient :

- `report.json` : recette, version du logiciel, chemins résolus et empreintes des
  sources importées, estimation des ressources, diagnostics d'empilement, réglages
  effectivement appliqués, résultats et durée totale.
- `stacked.astroproject` : sauvegarde portable après l'empilement, avant la finition.
- `group-001/`, `group-002/`, etc. : le rapport associe chaque numéro à son groupe.
  Chaque dossier contient `linear.fits` (Float32), `finished.tiff` (16 bits),
  `preview.jpg` et `result.astroproject` (résultat et finition réouvrables).

Chaque projet portable inclut les sources et les empilements des groupes, avec
la finition active du groupe concerné : plusieurs groupes peuvent donc produire
un volume important. Les calculs conservent leur précision flottante ; le JPEG
est un aperçu. Les TIFF sont les exports de finition, pas les données linéaires.
Le JPEG est exporté à pleine résolution, sans légende ; la recette ne propose
pas encore de redimensionnement ni de légende. Ces étapes nécessitent les
fonctions de l’application ou un auxiliaire utilisant son moteur.

Le rapport est écrit atomiquement aux étapes principales. En cas d'échec ou
d'annulation, les fichiers déjà terminés sont conservés. Le checkpoint permet
une reprise dans l'application ; la reprise automatique d'une tâche en ligne de
commande n'est pas encore proposée. Une nouvelle exécution exige un autre dossier.
Un arrêt forcé (`SIGKILL`, coupure) peut laisser le rapport à `running` : seul
l'événement final et le code de sortie confirment la réussite.

## Protocole pour les agents

La sortie standard est du JSON Lines : **un objet JSON par ligne**, avec
`schemaVersion: 1`. Les diagnostics de Qt peuvent apparaître sur la sortie d'erreur.
Aucun texte d'interface traduit ne doit servir à déterminer la réussite.

Événements : `help`, `started`, `progress`, `completed` (une opération),
`inspection`, `project`, `result` (tâche terminée), `error`.
`progress.percent` concerne uniquement l'opération courante.
`result.report` contient le même objet que le rapport final sur disque.
`error.report.error` fournit un `code` stable et un `message` explicatif.

Codes de sortie : `0` réussite, `2` entrée ou recette invalide, `1` échec du
traitement ou de l'écriture, `130` annulation. `SIGINT` et `SIGTERM` demandent
une annulation coopérative : attendre la fin du processus pour libérer ses ressources.
Les codes d'erreur comprennent `invalid_input`, `import_failed`,
`operation_failed`, `write_failed`, `internal_error`, `cancelled`.

L'agent doit vérifier le code de sortie, consulter les rejets et les avertissements
du bilan, puis examiner l'aperçu. Une exécution réussie ne certifie ni que les
calibrations choisies conviennent à l'acquisition, ni que la finition correspond
au goût de l'utilisateur.

## Vérification développeur

Le test `agent_mode` est enregistré dans CTest si Python 3 est disponible. Il génère
des FITS d'étoiles, traite une recette réelle, réouvre les projets portables et
vérifie les erreurs, les fichiers sources inchangés et l'annulation. Il n'utilise
aucune dépendance Python externe. Les tests du moteur et des projets restent
applicables puisque ce mode réutilise les mêmes contrôleurs.
