
#include "stdafx.h"
#include "SpaceMonger.h"
#include "lang.h"

static const wchar_t *densitynames[] = {
	L"Weniger, größere Kästchen", L"Normal", L"Mehr Details", L"Maximale Details",
};

static const wchar_t *colornames[] = {
	L"Klassisch (nach Ordnertiefe)", L"Dezent", L"Windows-Farben", L"Nach Dateityp",
};

static const wchar_t *themenames[] = {
	L"Automatisch (Windows)", L"Hell", L"Dunkel",
};

static const wchar_t *monthnames[] = {
	L"Jan", L"Feb", L"Mär", L"Apr", L"Mai", L"Jun",
	L"Jul", L"Aug", L"Sep", L"Okt", L"Nov", L"Dez",
};

static const wchar_t *attribnames[] = {
	L"Archiv", L"Komprimiert", L"Ordner", L"Verschlüsselt", L"Versteckt", L"Offline",
	L"Schreibgeschützt", L"Analysepunkt", L"Sparse", L"System", L"Temporär",
};

static const wchar_t *toolbarlabels[] = {
	L"Öffnen", L"Neu scannen", L"Ganz heraus", L"Hinein", L"Heraus",
	L"Freier Platz", L"System", L"Löschen", L"Aufräumen", L"Bericht", L"Als Administrator starten", L"Einstellungen", L"Info",
};

static const wchar_t *filetypes[] = {
	L"Videos", L"Bilder", L"Musik", L"Dokumente", L"Archive und Images", L"Programme", L"Sonstiges",
};

static const wchar_t *cleanupkinds[] = {
	L"Temporäre Dateien (Ihr Konto)", L"Temporäre Dateien (Windows)", L"Internet-Cache",
	L"Absturzabbilder", L"Windows-Fehlerberichte", L"DirectX-Shader-Cache",
	L"Microsoft Edge-Cache", L"Google Chrome-Cache", L"Firefox-Cache",
	L"npm-Cache (Entwickler)", L"Go-Build-Cache (Entwickler)", L"pip-Cache (Entwickler)",
	L"Papierkorb",
};

static const wchar_t *cleanuphints[] = {
	L"Video – auf eine externe Festplatte oder in die Cloud verschieben",
	L"Disk-Image – nach der Installation meist nicht mehr nötig",
	L"Virtuelle Festplatte – verkleinern oder ungenutzte virtuelle Maschinen entfernen",
	L"Archiv oder Sicherung – löschen, wenn bereits entpackt oder anderswo gesichert",
	L"Alter Download – lässt sich erneut herunterladen",
	L"iPhone/iPad-Update – wird bei Bedarf neu geladen",
	L"",
};

Lang LangDe = {
	IDR_TOOLBAR_EN,
	L"Über SpaceMonger",
	L"Freie Open-Source-Software unter der MIT-Lizenz.",
	L"Diese Software wird „wie besehen“ bereitgestellt, ohne jegliche Gewährleistung. Die Autoren"
		L" haften nicht für Schäden oder Datenverlust durch ihre Verwendung, einschließlich des"
		L" Löschens von Dateien. Die vollständigen Bedingungen stehen in der Lizenz.",
	L"Speicherplatz-Visualisierung für Windows: Auf einen Blick sehen, was das Laufwerk füllt.",
	L"© 1998–2020 Sean Werkema\n© 2026 Johnny J. Andersen, Jens Roggenfelder",
	L"&Website",
	L"&Problem melden",
	L"&Lizenz",
	L"SpaceMonger konnte diese Adresse nicht öffnen:\n%s",
	L"OK", L"Abbrechen",
	L"Bytes", L"KB", L"MB", L"GB",
	L"Freier Platz: %d,%d %%",
	L"&Hinein zoomen", L"&Heraus zoomen", L"&Ganz heraus", L"Im &Explorer zeigen", L"&Löschen",
	L"Laufwerk ö&ffnen...", L"&Neu scannen", L"&Freien Platz zeigen",
	L"%lu,%01lu %%", L"%lu,%01lu %s",
	L"Gesamt", L"Frei", L"Dateien gesamt:", L"Ordner gesamt:", L"Dateien gefunden:", L"Ordner gefunden:",
	',', '.',
	densitynames, colornames, monthnames, attribnames,
	L"Wird gelöscht...",
	L"Laufwerk auswählen",
	L"Laufwerk wird gescannt...",
	L"SpaceMonger-Einstellungen",
	L"Karte", L"Detailgrad:", L"Farben:",
	L"Dateien", L"Ordner",
	L"Verhalten", L"Details beim Darüberfahren anzeigen", L"Kästchen unter der Maus hervorheben",
	L"Befehl „Löschen“ deaktivieren",
	L"Darstellung", L"Sprache:", L"Automatisch (Windows)", L"Design:",
	themenames,
	L"Eigenschaften...",
	L"Der Scan konnte nicht abgeschlossen werden (Windows-Fehler %lu).",
	L"Das Ergebnis ist unvollständig: %llu Ordner konnten nicht vollständig gelesen werden (erster Windows-Fehler %lu). Größen können zu niedrig sein.",
	L"Scan abgeschlossen, %llu Ordner übersprungen. Größen können zu niedrig sein.",
	L"Übersprungen: %llu",
	L"Scan-Details",
	L"%llu Ordner wurden übersprungen; %llu Pfade werden angezeigt.",
	L"Pfade kopieren",
	L"Nicht lesbar (geschätzt)",
	L"Speicher, den Windows als belegt meldet, den SpaceMonger aber nicht lesen durfte:\nOrdner ohne Zugriffsrechte (andere Benutzer, System Volume Information),\nDateisystemdaten und Schattenkopien. Als Administrator gestartet sieht\nSpaceMonger mehr davon. Die Größe ist geschätzt.",
	L"%s  ·  %d,%d %% des Laufwerks",
	L"%s Dateien in %s Ordnern",
	L"Geändert %s",
	L"Von Windows verwaltet. Hier schaffen Sie Platz mit der Datenträgerbereinigung oder\nunter Einstellungen > System > Speicher, nicht durch Löschen von Dateien.",
	L"Speicherplatz freigeben",
	L"%s lassen sich gefahrlos entfernen: temporäre Dateien, Browser-Caches und der Papierkorb.",
	L"Ihre größten Dateien – auf eine externe Festplatte verschieben oder löschen, was Sie nicht mehr brauchen:",
	L"Element", L"Größe", L"Ort", L"Tipp",
	L"Suche nach Dateien, die gefahrlos entfernt werden können…",
	L"&Jetzt aufräumen",
	L"Im &Ordner zeigen",
	L"Schließen",
	L"&Details",
	L"Fertig – %s freigegeben.",
	L"Im Moment gibt es nichts aufzuräumen.",
	L"Scannen Sie zuerst ein Laufwerk, um seine größten Dateien zu sehen.",
	L"In den &Papierkorb",
	L"%s in den Papierkorb verschoben.",
	L"&Ordner...",
	L"Liste e&xportieren...",
	L"Gespeichert unter %s",
	L"Dateien und Ordner suchen",
	L"%s Treffer",
	L"SpaceMonger konnte nicht als Administrator neu gestartet werden.",
	filetypes,
	cleanupkinds,
	cleanuphints,
	toolbarlabels,
};
