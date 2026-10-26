
#include "stdafx.h"
#include "SpaceMonger.h"
#include "lang.h"

static const wchar_t *densitynames[] = {
	L"Moins de cases, plus grandes", L"Normal", L"Plus de détails", L"Détails maximum",
};

static const wchar_t *uscolornames[] = {
	L"Classique (par profondeur)", L"Atténué", L"Couleurs Windows", L"Par type de fichier",
};

static const wchar_t *frthemenames[] = {
	L"Automatique (Windows)", L"Clair", L"Sombre",
};
static const wchar_t *monthnames[] = {
	L"Jan", L"Fev", L"Mars", L"Avril", L"Mai", L"Juin",
	L"Juil", L"Août", L"Sep", L"Oct", L"Nov", L"Dec",
};

static const wchar_t *attribnames[] = {
	L"Arch", L"Compress", L"Dossier", L"Encrypt", L"Caché", L"Pas-au-ligne",
	L"Lire-Seulement", L"Reparse-Pt", L"Clairsemé", L"Système", L"Temp",
};

static const wchar_t *frtoolbarlabels[] = {
	L"Ouvrir", L"Rouvrir", L"Voir tout", L"Zoom +", L"Zoom -",
	L"Espace libre", L"Système", L"Effacer", L"Nettoyer", L"Rapport", L"Lancer en administrateur", L"Réglages", L"À propos",
};

static const wchar_t *frfiletypes[] = {
	L"Vidéos", L"Images", L"Musique", L"Documents", L"Archives et images disque", L"Programmes", L"Autres",
};

static const wchar_t *frcleanupkinds[] = {
	L"Fichiers temporaires (votre compte)", L"Fichiers temporaires (Windows)", L"Cache Internet",
	L"Vidages sur incident", L"Rapports d'erreurs Windows", L"Cache des shaders DirectX",
	L"Cache de Microsoft Edge", L"Cache de Google Chrome", L"Cache de Firefox",
	L"Cache npm (développeur)", L"Cache de compilation Go (développeur)", L"Cache pip (développeur)",
	L"Corbeille",
};

static const wchar_t *frcleanuphints[] = {
	L"Vidéo – à déplacer sur un disque externe ou dans le cloud",
	L"Image disque – généralement inutile après l'installation",
	L"Disque virtuel – à compacter, ou supprimez les machines virtuelles inutilisées",
	L"Archive ou sauvegarde – à supprimer si déjà extraite ou stockée ailleurs",
	L"Ancien téléchargement – peut être téléchargé à nouveau",
	L"Mise à jour iPhone/iPad – téléchargée à nouveau si nécessaire",
	L"",
};

Lang LangFr = {
	IDR_TOOLBAR_FR,
	L"À propos de SpaceMonger",
	L"Logiciel libre et open source, sous licence MIT.",
	L"Ce logiciel est fourni « tel quel », sans garantie d'aucune sorte. Les auteurs"
		L" ne sont pas responsables des dommages ou pertes de données résultant de son"
		L" utilisation, y compris la suppression de fichiers. Voir la licence pour les conditions.",
	L"Visualiseur d'espace disque pour Windows : voyez d'un coup d'œil ce qui remplit votre disque.",
	L"© 1998–2020 Sean Werkema\n© 2026 Johnny J. Andersen, Jens Roggenfelder",
	L"Site &Web",
	L"Signaler un &problème",
	L"&Licence",
	L"SpaceMonger n'a pas pu ouvrir cette adresse :\n%s",
	L"D'accord", L"Annul",
	L"octets", L"Ko", L"Mo", L"Go",
	L"Espace libre : %d,%d %%",
	L"&Vrombir", L"&Démarrer", L"Voir &Tout", L"Afficher dans l'&Explorateur", L"&Effacer",
	L"&Ouvrir un Disque...", L"&Rouvrir", L"&Montrer l'Espace Libre",
	L"%lu,%01lu%%", L"%lu,%01lu %s",
	L"Total", L"Libre", L"Fichiers Totals:", L"Dossiers Totals:", L"Fichiers Trouvés:", L"Dossiers Trouvés:",
	',', '.',
	densitynames, uscolornames, monthnames, attribnames,
	L"Effacer...",
	L"Choisir un Disque",
	L"Scruter le Disque...",
	L"Configuration de SpaceMonger",
	L"Carte", L"Détail :", L"Couleurs :",
	L"Fichiers", L"Dossiers",
	L"Comportement", L"Afficher les détails au survol d'une case", L"Mettre en évidence la case sous la souris",
	L"Désactiver la commande « Effacer »",
	L"Apparence", L"Langue :", L"Automatique (Windows)", L"Thème :",
	frthemenames,
	L"Properties...",
	L"L'analyse n'a pas pu aboutir (erreur Windows %lu).",
	L"Analyse incomplète : %llu dossiers n'ont pas pu être entièrement analysés (première erreur Windows %lu). Les tailles peuvent être sous-estimées.",
	L"Analyse terminée avec %llu dossiers ignorés. Les tailles peuvent être sous-estimées.",
	L"Ignorés : %llu",
	L"Détails de l'analyse",
	L"%llu dossiers ont été ignorés ; %llu chemins sont affichés.",
	L"Copier les chemins",
	L"Illisible (estimation)",
	L"Espace que Windows indique comme utilisé, mais que SpaceMonger n'a pas pu lire :\ndossiers sans droits d'accès (autres utilisateurs, System Volume Information),\ndonnées du système de fichiers et clichés instantanés. Lancez SpaceMonger\nen tant qu'administrateur pour en voir davantage. La taille est une estimation.",
	L"%s  ·  %d,%d %% du disque",
	L"%s fichiers dans %s dossiers",
	L"Modifié le %s",
	L"Géré par Windows. Libérez de l'espace ici avec le Nettoyage de disque ou\nParamètres > Système > Stockage plutôt qu'en supprimant des fichiers.",
	L"Libérer de l'espace",
	L"%s peuvent être supprimés sans risque : fichiers temporaires, caches des navigateurs et Corbeille.",
	L"Vos plus gros fichiers – déplacez-les sur un disque externe ou supprimez ceux dont vous n'avez plus besoin :",
	L"Élément", L"Taille", L"Emplacement", L"Conseil",
	L"Recherche des fichiers supprimables sans risque…",
	L"&Nettoyer maintenant",
	L"&Afficher dans le dossier",
	L"Fermer",
	L"&Détails",
	L"Terminé – %s libérés.",
	L"Rien à nettoyer pour le moment.",
	L"Analysez d'abord un disque pour voir ses plus gros fichiers.",
	L"&Mettre à la Corbeille",
	L"%s mis à la Corbeille.",
	L"%d sur %d emplacements mesurés…",
	L"Nettoyage : %s (%d sur %d)",
	L"%s sur %s nettoyés",
	L"&Arrêter",
	L"Arrêté – %s libérés.",
	L"nettoyage…",
	L"terminé",
	L"Affiche l'analyse du %s pendant sa mise à jour.",
	L"Finalisation de l'analyse…",
	L"Copier le chemin et la taille",
	L"Garder la dernière analyse et l'afficher pendant une nouvelle analyse",
	L"&Dossier...",
	L"E&xporter la liste...",
	L"Enregistré dans %s",
	L"Rechercher des fichiers et dossiers",
	L"%s résultats",
	L"SpaceMonger n'a pas pu être relancé en tant qu'administrateur.",
	frfiletypes,
	frcleanupkinds,
	frcleanuphints,
	frtoolbarlabels,
};
