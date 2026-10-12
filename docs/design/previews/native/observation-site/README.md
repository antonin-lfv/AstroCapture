# Observation — version 1.18.6

Fenêtre native en français, à 1440 × 960, 1100 × 800 et 900 × 700.
Le lieu passe en premier dans les réglages et dans la page : horloge locale,
état solaire, prévision nuageuse et événements à venir. Les recommandations
précèdent la courbe de la cible ; trois sont proposées au départ.

- `observation-t68-night-*` : Utah le 13 décembre 2026 à 00 h 30 MST,
  soit 08 h 30 en France. M42 est en favori et passe en premier ; les hauteurs
  et l’obscurité sont calculées à cet instant, indépendamment de la cible active.
- `observation-t68-morning-*` : 10 h MST, état « Jour », aucune recommandation
  respectant l’obscurité et prochaine nuit astronomique le soir même. Le cache
  des événements solaires dépasse le midi qui termine la nuit affichée.
- `observation-t68-planning-*` : « Préparer la nuit » repart de la vraie horloge
  du Mac et vise la nuit sombre en cours ou la suivante. L’instant est vérifié
  contre l’événement calculé avant le changement de période. Les dates de ces
  captures suivent donc l’horloge du contrôle, pas le scénario de décembre.
- `observation-t68-now-900` : retour au suivi de l’heure réelle.
- `observation-site-clouds-*` : réponse Open-Meteo enregistrée pour Paris,
  affichée à son heure exacte dans le résumé. Le chargement réseau est désactivé
  dans ce scénario ; ce sont des données de contrôle, pas une nouvelle mesure.
- `observation-site-offline-900` : connexion interrompue, résultat conservé et
  message visible dans le résumé ; aucune donnée manquante n’est remplacée par 0 %.
- `observation-overview-clouds-*` : courbe et détails de la même prévision.
- `window-*`, `quit-*` et `restored-window` : parcours de traitement et reprise
  avec la copie de contrôle Orion HDR 1024 × 768, dérivée de l’image réelle.
  Les exports et la reprise entre processus conservent les pixels contrôlés.

T68 · Utah est un préréglage géographique (37,7378° N, 113,6975° O, 1570 m,
America/Denver). La [fiche géographique publiée](https://support.itelescope.net/support/solutions/articles/245471-telescope-t68)
sert uniquement à vérifier ces valeurs. Aucun compte, lien d’accès, limite
d’opérateur ou statut d’installation n’est utilisé par l’application. Un lieu
personnalisé fonctionne avec les mêmes calculs et prévisions ; les liens
personnels sur les lieux ne font pas partie de cette livraison.
