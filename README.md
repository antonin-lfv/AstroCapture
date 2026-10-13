<h1 align="center">
  <br>
  🔭 AstroCapture
  <br>
</h1>

<h4 align="center">Préparez vos observations, capturez le ciel et révélez vos images.</h4>

<p align="center">
  Votre atelier d’astrophotographie sur Mac et Linux, en français et en anglais.
</p>

<p align="center">
  <a href="CHANGELOG.md"><img src="https://img.shields.io/badge/version-1.19.0-d4af37" alt="Version 1.19.0"></a>
  <a href="#installer-sur-mac"><img src="https://img.shields.io/badge/macOS-Apple_Silicon-7897c5" alt="macOS : Apple Silicon"></a>
  <a href="#installer-sur-linux"><img src="https://img.shields.io/badge/Linux-Ubuntu_%C2%B7_Mint-87b07c" alt="Linux : Ubuntu et Mint"></a>
  <img src="https://img.shields.io/badge/langues-FR_%C2%B7_EN-b69ad4" alt="Langues : français et anglais">
</p>

<p align="center">
  <a href="#ce-que-vous-pouvez-faire">Découvrir</a> •
  <a href="#installation">Installer</a> •
  <a href="#premiers-pas">Premiers pas</a> •
  <a href="#garder-son-travail-et-partager-une-image">Sauvegarder et exporter</a> •
  <a href="#aide-et-documentation">Aide</a>
</p>

<p align="center">
  <img src="logo.png" alt="Logo AstroCapture" width="280">
</p>

## Ce que vous pouvez faire

De la Lune aux galaxies, AstroCapture vous accompagne de la préparation d’une
observation jusqu’à l’image finale. Chaque page correspond à une étape :

| Page | Votre prochaine étape |
| --- | --- |
| 🌍 **Observation** | Choisissez un lieu et une heure, trouvez les objets visibles et consultez les prévisions de nuages. |
| 📷 **Capture** | Enregistrez les images d’une caméra connectée à votre télescope. |
| 🪐 **Traitement** | Combinez les meilleures images d’une vidéo de la Lune ou d’une planète, puis faites ressortir les détails. |
| ✨ **Ciel profond** | Alignez et combinez vos photos de nébuleuses ou de galaxies, puis ajustez les couleurs et le fond de ciel. |
| 🧩 **Mosaïque** | Raccordez plusieurs vues pour réunir la Lune ou le Soleil dans une seule image. |

Vous pouvez aussi partir de vidéos ou de photos déjà prises avec votre matériel
ou un télescope distant. **Pas besoin de caméra connectée pour préparer une
observation ou traiter vos images.**

## Installation

### Installer sur Mac

Le paquet actuel pour Mac est prévu pour les **Mac Apple Silicon**, équipés d’une puce de
la famille M.

1. Ouvrez la page des [Releases GitHub](https://github.com/antonin-lfv/AstroCapture/releases/latest).
   Dans la section **Assets**, téléchargez le fichier `AstroCapture-<version>.dmg`.
2. Ouvrez le fichier téléchargé.
3. Glissez **AstroCapture** dans le dossier **Applications**.
4. Lancez l’application depuis ce dossier.

Pour une mise à jour, quittez AstroCapture, puis remplacez l’application de la
même façon. Vos préférences, préréglages et historique sont conservés.

Le paquet n’est pas encore certifié par Apple : macOS peut demander une
autorisation d’ouverture.

### Installer sur Linux

La version Linux est une **AppImage pour PC 64 bits (x86-64)**, compatible avec
**Ubuntu 22.04 ou plus récent** et **Linux Mint 21 et 22**. Qt et les bibliothèques
de traitement sont inclus : pas besoin de les installer séparément.

1. Ouvrez la page des [Releases GitHub](https://github.com/antonin-lfv/AstroCapture/releases/latest).
   Dans la section **Assets**, téléchargez le fichier
   `AstroCapture-<version>-Linux-x86_64.zip`, puis décompressez-le.
2. Dans les propriétés du fichier `.AppImage`, autorisez son exécution comme programme.
3. Double-cliquez dessus pour lancer AstroCapture.

Pour une mise à jour, quittez l’application et remplacez l’AppImage ; vos projets
et préférences restent conservés.

Les pilotes de capture ZWO ASI, NexImage/TIS et UVC sont inclus. **La capture avec
une vraie caméra reste à valider sur Linux.**

## Premiers pas

L’**Accueil** permet de retrouver vos projets et vidéos récents. Pour commencer,
ouvrez la page correspondant à votre objectif et suivez les étapes ci-dessous.
Vous pouvez aussi ouvrir une image PNG ou TIFF directement dans **Traitement**.
Vos images exportées se retrouvent dans **Historique**.

### 🌍 Préparer une observation

Dans **Observation**, choisissez votre lieu, une date et un objet. Le premier
lancement utilise Paris ; vous pouvez sélectionner une ville, saisir des
coordonnées ou cliquer sur la carte. Enregistrez vos lieux et objets habituels
en favoris pour les retrouver rapidement.

![Observation : préparer M42 depuis l’Utah, avec les heures sur place et chez soi](assets/screenshots/readme-observation.png)

La page vous montre :

- où il fait jour ou nuit, ainsi que les horaires du Soleil ;
- les objets proposés à l’heure choisie ou pendant la nuit ;
- la direction où regarder et la hauteur de l’objet dans le ciel ;
- les prévisions de couverture nuageuse pour ce lieu.

Les cartes et graphiques suivent la même heure. **Sur place** indique l’heure au
lieu d’observation ; **Chez moi** indique à quelle heure cet instant correspond
sur votre ordinateur. C’est utile pour préparer une prise de vue depuis un télescope
situé dans un autre pays. **Quand lancer l’enregistrement ?** propose un créneau,
affiché à l’heure de votre ordinateur pour votre lancement manuel.

Les nuages sont une prévision météo. La visibilité calculée ne tient pas compte
d’un arbre, d’un bâtiment ou du relief devant votre télescope.

### ✨ Traiter une nébuleuse ou une galaxie

Dans **Ciel profond**, le parcours suit trois étapes : **Images → Empilement →
Finition**. Empiler consiste à combiner plusieurs photos du même objet pour
atténuer le bruit, ce grain qui masque les détails faibles.

1. **Ajoutez vos photos.** Vérifiez leur type dans la liste. Si vous utilisez des
   poses brutes, ajoutez aussi vos images de calibration : darks, flats et bias
   selon votre acquisition. Elles servent à corriger les défauts du capteur et
   les différences d’éclairage. Si vos poses sont déjà corrigées, cochez
   **Poses déjà calibrées**.
2. **Créez l’image empilée.** AstroCapture aligne les étoiles et combine les
   prises sélectionnées. Le bilan indique combien de poses ont été conservées.
3. **Ajustez le résultat dans Finition.** Révélez les détails avec l’étirement,
   puis réglez le fond de ciel, les couleurs et le bruit. Comparez le résultat
   avant et après vos réglages.
4. **Sauvegardez le projet et exportez votre image.** Vous pourrez reprendre
   les réglages plus tard et ajouter une légende à l’export.

![Ciel profond : finition de M42 à partir de dix poses de 30 secondes prises avec le T68](assets/screenshots/readme-deep-sky.png)

Une image astronomique peut paraître très sombre avant l’étirement : c’est
normal. L’étirement rend les détails faibles visibles.

Les formats acceptés comprennent **FITS, RAW à matrice Bayer, PNG et TIFF**.
Les outils avancés, comme la composition de plusieurs filtres, les masques et
les réglages séparés des étoiles et du fond, restent disponibles dans Finition.

### 🪐 Traiter la Lune ou une planète

Dans **Traitement**, ouvrez votre vidéo **SER**, un format courant en
astrophotographie planétaire.

1. Cliquez sur **Analyser la vidéo** pour classer les images par netteté.
2. Choisissez les images à conserver, puis **Aligner et empiler**.
3. Renforcez progressivement les détails, ajustez les couleurs et comparez
   l’avant et l’après.
4. Exportez le résultat en PNG, TIFF ou JPEG.

![Traitement : Saturne dans le fichier jupyter.ser, avec 2 246 images conservées sur 8 984](assets/screenshots/readme-processing.png)

### 🧩 Assembler une mosaïque

Pour assembler plusieurs portions de la Lune, traitez d’abord chaque vidéo,
puis ouvrez les images obtenues dans **Mosaïque** et cliquez sur
**Assembler automatiquement**. Les vues doivent partager une zone commune
pour pouvoir être raccordées.

![Mosaïque : la Lune assemblée à partir des 70 vues du 30 mai 2023](assets/screenshots/readme-mosaic.png)

## Garder son travail et partager une image

Les fichiers d’origine restent intacts. Les réglages et les exports travaillent
sur le résultat du traitement.

| À conserver | À quoi cela sert |
| --- | --- |
| **Projet portable `.astroproject`** dans Ciel profond | Reprendre le traitement avec ses sources, résultats, réglages et légende réunis dans un fichier. Il peut être volumineux. |
| **PNG ou TIFF 16 bits** | Garder une image finale de bonne qualité pour la retoucher ou l’imprimer. |
| **JPEG** | Partager facilement une version plus légère. |
| **FITS linéaire** dans Ciel profond | Conserver l’empilement avant les réglages de finition, pour repartir de cette base. |

À l’export, vous pouvez ajouter un titre et une légende en **blanc ou en doré** :
nom de l’objet, matériel, date ou temps de pose.

La sauvegarde automatique permet de reprendre la dernière session depuis
l’Accueil. Enregistrez aussi un projet pour conserver un traitement de ciel
profond que vous souhaitez retrouver ou déplacer.

## Aide et documentation

L’onglet **Guide** explique les étapes dans l’application, même sans connexion.
Les aides **?** près des réglages décrivent leur effet et donnent des repères
pour commencer. Vous pouvez annuler un réglage avec **⌘ Z sur Mac** ou **Ctrl+Z sur Linux**.

Les images et les traitements restent sur votre ordinateur. Internet sert aux
prévisions météo et à certaines recherches de villes ou d’objets. Les cartes
jour/nuit, les calculs de visibilité, le catalogue inclus et les favoris
fonctionnent hors connexion ; les problèmes de connexion sont signalés.

La compatibilité d’une caméra dépend de son modèle et de son pilote. La
Celestron NexImage 5 est le matériel de référence testé sur Mac. Vous pouvez
essayer la capture avec la caméra simulée proposée dans l’application.

Pour approfondir :

- [Évolutions du logiciel](CHANGELOG.md).
- [Développement et contribution](CONTRIBUTING.md).
