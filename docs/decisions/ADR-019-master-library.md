# Bibliothèque de masters

Création depuis les sources de calibration sélectionnées, ou import explicite
d'un master FITS. Un master conserve caméra, binning, gain, offset, température,
pose, filtre, nombre de sources et statistiques de qualité. Les combinaisons
incompatibles sont refusées. Les métadonnées absentes exigent une vérification
explicite lors de la sélection ; aucune extrapolation de durée des darks.

La création fait une moyenne en double précision avec rejet médiane/MAD à 3σ
à partir de trois sources. Les fichiers temporaires permettent un calcul par
blocs sans conserver toutes les poses en mémoire. Moins de trois sources ne
permettent pas de rejet robuste ; une source seule ne réduit pas le bruit.

Un master dark contient le bias. Les flats sont corrigés par dark-flat ou bias
avant normalisation individuelle et moyenne. La provenance FITS AC_MASTR / AC_FLAT
permet de reconnaître un flat déjà corrigé sans refaire la soustraction.
Un flat externe doit être confirmé comme corrigé. Un master ne peut pas être
mélangé avec ses sources ou un autre master lors de l'intégration.

FITS Float32, fichiers privés identifiés par UUID, SHA256 vérifié avant usage,
manifest JSON atomique sous verrou. Retirer une entrée laisse le fichier FITS
sur disque, car des projets peuvent le référencer. Les projets portables
emportent les masters comme leurs autres sources.

Tests : rejet d'un impact cosmique, conservation des métadonnées et de la
précision, incompatibilités gain/température, revue obligatoire des informations
manquantes, application dark/flat sans double soustraction, empreinte altérée,
annulation, persistance de session et retrait non destructif.
