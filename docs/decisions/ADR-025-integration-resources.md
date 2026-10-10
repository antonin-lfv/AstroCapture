# ADR-025 — Ressources et fichiers de travail de l’empilement

Date : 10 octobre 2026. Lot 1.14.0.

## Périmètre

Les buffers de rejet précédents retenaient environ 65 536 échantillons RGB
**par pose**, auxquels s’ajoutaient les poids drizzle. Leur mémoire augmentait
avec le nombre de poses. Les projections et les masters robustes gardaient
également un descripteur ouvert par source, au risque d’atteindre la limite du
processus avant les 500 poses autorisées.

Le lot borne les buffers d’échantillons du rejet, ferme cette dépendance au
nombre de fichiers ouverts et ajoute un contrôle de ressources avant empilement.
Il ne relève pas le plafond de **32 MP** : projection, finition, images de résultat
et reprise utilisent encore des images complètes. Ce n’est pas une conversion
de toute la chaîne au traitement hors mémoire.

## Calcul et stockage

- Tuiles horizontales et verticales dimensionnées selon le nombre de poses et
  de canaux de poids. Le cumul des buffers Float32 lus reste **≤ 64 Mio**, y
  compris lorsqu’une ligne entière de 500 poses Bayer dépasse cette limite.
  Les petits vecteurs de valeurs/MAD, les métadonnées et les images complètes
  sont distincts de ce plafond. Le rejet, l’ordre d’accumulation et les poids
  ne changent pas.
- Une seule archive de travail par intégration ou construction de master
  robuste. Les projections sont référencées par offsets 64 bits. Pas de pile
  de descripteurs par pose ; les sommes et compteurs inutiles des masters sont
  libérés avant la passe de rejet robuste.
- Archive privée créée exclusivement : droits propriétaire sous POSIX,
  descripteur non hérité, fichier immédiatement détaché du répertoire tout en
  restant ouvert. Sous Windows, ouverture avec suppression à la fermeture et
  sans héritage. Fin normale, exception ou annulation libèrent le contenu ;
  la fermeture des descripteurs par le système libère aussi le contenu lorsque
  le processus s’arrête. Aucun fichier source ni autre contenu du dossier choisi
  n’est retiré. Windows/Linux restent à qualifier sur leurs propres machines.
- Espace vérifié avant création, avant chaque écriture de projection et tous
  les 16 Mio écrits, avec **256 Mio** de réserve. Les erreurs d’espace,
  écriture, lecture ou finalisation sont fatales au calcul entier ; elles ne
  deviennent jamais une simple exclusion de pose. Le résultat précédent reste
  disponible. La réserve est un contrôle, pas une réservation exclusive du disque.

## Estimation avant calcul

Dans Empilement, « Ressources avant l’empilement » affiche mémoire estimée,
budget, espace temporaire nécessaire et libre, ainsi que les raisons de refus.
L’aide décrit les unités et limites. « Dossier temporaire » peut sélectionner
un disque local accessible en écriture ; « Par défaut » rétablit le dossier
système. La préférence reste locale dans QSettings, hors des projets portables.
Une préférence vers un volume absent demande une correction explicite.

Les tailles sont estimées sur toutes les poses sélectionnées et la grille
**avant recadrage** :

| Mode | Archive temporaire, par pixel de sortie et par pose |
| --- | ---: |
| Empilement classique | 12 octets |
| Drizzle mono/RVB | 16 octets |
| Drizzle Bayer natif | 24 octets |

Les groupes étant séquentiels, la demande de disque est le maximum d’un groupe,
plus la réserve, et non la somme de leurs archives. La mémoire retient en revanche
les résultats des groupes déjà calculés. Le modèle compte une seule fois les
images partagées et aperçus existants de Ciel profond, ses caches, les buffers
natifs modélisés et une marge fixe de 256 Mio. Il compare les pics des étapes,
plutôt que d’additionner des allocations qui ne sont pas simultanées.

Budget : moitié de la RAM physique, au maximum 8 Gio ; repli à 2 Gio si la RAM
n’est pas connue. Il s’agit d’un **modèle conservateur**, pas d’une mesure RSS ni
d’une garantie de réussite sur une machine chargée. Les autres espaces de travail,
applications, allocations internes de bibliothèques et mémoire graphique ne sont
pas intégralement mesurés. Les erreurs d’allocation restent possibles.

Les résultats flottants prévus et références d’images sont également comparés
aux limites de reprise de 4 Gio / 64 références. Ce contrôle avant recadrage
est conservateur ; les contrôles de sauvegarde demeurent l’autorité finale,
notamment pour les aperçus, masques et autres actifs. Désactiver les cartes ou
répartir les groupes réduit le coût. Les anciens projets restent lisibles et
les options de traitement gardent leurs conventions.

La mesure du disque utilise le chemin effectif, comme le moteur : sur ce Mac,
un contrôle initial par table de montage classait à tort `/var` comme le volume
système en lecture seule, alors que son dossier temporaire est accessible.

## Fondements

- [Microsoft — ouverture de fichiers temporaires](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/open-wopen) : suppression à la dernière fermeture de descripteur et non-héritage.
- Les calculs scientifiques et conventions restent ceux des ADR-021 à ADR-024.

## Vérifications

Les comptes rendus du lot sont dans le [journal de validation](../design/VALIDATION.md).
Le [bilan V2](../releases/V2_READINESS.md) conserve le travail restant sur les
grandes images, mesures de ressources et essais d’endurance.
