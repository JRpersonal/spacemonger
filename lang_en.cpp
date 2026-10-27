
#include "stdafx.h"
#include "SpaceMonger.h"
#include "lang.h"

LangEntry Langs[] = {
	{ "us", L"English (US)", &LangUs },
	{ "uk", L"English (UK/Aust.)", &LangUk },
	{ "de", L"Deutsch", &LangDe },
	{ "fr", L"Français", &LangFr },
	{ NULL, NULL, NULL },
};

Lang *CurLang = &LangUs;

/////////////////////////////////////////////////////////////////////////////

static const wchar_t *densitynames[] = {
	L"Fewer, larger boxes", L"Normal", L"More detail", L"Maximum detail",
};

static const wchar_t *uscolornames[] = {
	L"Classic (by folder depth)", L"Muted", L"Windows colors", L"By file type",
};

static const wchar_t *enthemenames[] = {
	L"Automatic (Windows)", L"Light", L"Dark",
};

static const wchar_t *monthnames[] = {
	L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
	L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec",
};

static const wchar_t *attribnames[] = {
	L"Arch", L"Compress", L"Folder", L"Encrypt", L"Hidden", L"Offline",
	L"Read-Only", L"Reparse-Pt", L"Sparse", L"System", L"Temp",
};

static const wchar_t *entoolbarlabels[] = {
	L"Open", L"Rescan", L"Zoom Full", L"Zoom In", L"Zoom Out",
	L"Free Space", L"System", L"Delete", L"Clean Up", L"Report", L"Run as administrator", L"Settings", L"About",
};

static const wchar_t *enfiletypes[] = {
	L"Videos", L"Pictures", L"Music", L"Documents", L"Archives and images", L"Programs", L"Other",
};

static const wchar_t *encleanupkinds[] = {
	L"Temporary files (your account)", L"Temporary files (Windows)", L"Internet cache",
	L"Crash dumps", L"Windows error reports", L"DirectX shader cache",
	L"Microsoft Edge cache", L"Google Chrome cache", L"Firefox cache",
	L"npm cache (developer)", L"Go build cache (developer)", L"pip cache (developer)",
	L"Recycle Bin",
};

static const wchar_t *encleanuphints[] = {
	L"Video – move to an external drive or cloud storage",
	L"Disk image – usually not needed after installing",
	L"Virtual disk – compact it or remove unused virtual machines",
	L"Archive or backup – delete if already extracted or stored elsewhere",
	L"Old download – can be downloaded again",
	L"iPhone/iPad update – downloaded again when needed",
	L"",
};

Lang LangUs = {
	IDR_TOOLBAR_EN,
	L"About SpaceMonger",
	L"Free and open-source software under the MIT License.",
	L"This software is provided “as is”, without warranty of any kind. The authors"
		L" are not liable for any damage or data loss resulting from its use, including"
		L" the deletion of files. See the license for the full terms.",
	L"Disk space visualizer for Windows: see at a glance what fills your drive.",
	L"© 1998–2020 Sean Werkema\n© 2026 Johnny J. Andersen, Jens Roggenfelder",
	L"&Website",
	L"&Report a problem",
	L"&License",
	L"SpaceMonger could not open this URL:\n%s",
	L"OK", L"Cancel",
	L"bytes", L"Kb", L"Mb", L"Gb",
	L"Free space: %d.%d %%",
	L"Zoom &In", L"Zoom &Out", L"Zoom &Full", L"&Show in Explorer", L"&Delete",
	L"&Open Drive...", L"&Rescan Drive", L"&Show Free Space",
	L"%lu.%01lu%%", L"%lu.%01lu %s",
	L"Total", L"Free", L"Files Total:", L"Folders Total:", L"Files Found:", L"Folders Found:",
	'.', ',',
	densitynames, uscolornames, monthnames, attribnames,
	L"Deleting...",
	L"Select Drive to View",
	L"Scanning Disk...",
	L"SpaceMonger Settings",
	L"Map", L"Detail:", L"Colors:",
	L"Files", L"Folders",
	L"Behavior", L"Show details when hovering over a box", L"Highlight the box under the mouse",
	L"Disable the \"Delete\" command",
	L"Appearance", L"Language:", L"Automatic (Windows)", L"Theme:",
	enthemenames,
	L"Properties...",
	L"The scan could not be completed (Windows error %lu).",
	L"Scan results are incomplete: %llu directories could not be fully scanned (first Windows error %lu). Sizes may be understated.",
	L"Scan completed with %llu folders skipped. Sizes may be understated.",
	L"Skipped: %llu",
	L"Scan details",
	L"%llu folders were skipped; showing %llu paths.",
	L"Copy paths",
	L"Not readable (estimate)",
	L"Space Windows reports as used, but that SpaceMonger was not allowed to read:\nfolders without access rights (other users, System Volume Information),\nfile system data and shadow copies. Run SpaceMonger as administrator\nto see more of it. The size is an estimate.",
	L"%s  ·  %d.%d %% of the drive",
	L"%s files in %s folders",
	L"Modified %s",
	L"Managed by Windows. Free space here with Disk Cleanup or\nSettings > System > Storage rather than by deleting files.",
	L"Free Up Space",
	L"%s can be removed safely: temporary files, browser caches and the Recycle Bin.",
	L"Your biggest files – move them to an external drive or delete what you no longer need:",
	L"Item", L"Size", L"Location", L"Tip",
	L"Looking for files that can be removed safely…",
	L"&Clean up now",
	L"&Show in folder",
	L"Close",
	L"&Details",
	L"Done – %s freed.",
	L"Nothing to clean up right now.",
	L"Scan a drive first to see its biggest files.",
	L"&Move to Recycle Bin",
	L"%s moved to the Recycle Bin.",
	L"Measured %d of %d locations…",
	L"Cleaning: %s (%d of %d)",
	L"%s of %s cleaned",
	L"&Stop",
	L"Stopped – %s freed.",
	L"cleaning…",
	L"done",
	L"Showing the scan from %s while it is updated.",
	L"Finishing the scan…",
	L"Copy path and size",
	L"Keep the last scan and show it while rescanning",
	L"&Folder...",
	L"E&xport list...",
	L"Saved to %s",
	L"Search files and folders",
	L"%s matches",
	L"SpaceMonger could not be restarted as administrator.",
	enfiletypes,
	encleanupkinds,
	encleanuphints,
	entoolbarlabels,
};

static const wchar_t *ukcolornames[] = {
	L"Classic (by folder depth)", L"Muted", L"Windows colours", L"By file type",
};

Lang LangUk = {
	IDR_TOOLBAR_EN,
	L"About SpaceMonger",
	L"Free and open-source software under the MIT Licence.",
	L"This software is provided “as is”, without warranty of any kind. The authors"
		L" are not liable for any damage or data loss resulting from its use, including"
		L" the deletion of files. See the licence for the full terms.",
	L"Disk space visualiser for Windows: see at a glance what fills your drive.",
	L"© 1998–2020 Sean Werkema\n© 2026 Johnny J. Andersen, Jens Roggenfelder",
	L"&Website",
	L"&Report a problem",
	L"&Licence",
	L"SpaceMonger could not open this URL:\n%s",
	L"OK", L"Cancel",
	L"bytes", L"Kb", L"Mb", L"Gb",
	L"Free space: %d.%d %%",
	L"Zoom &In", L"Zoom &Out", L"Zoom &Full", L"&Show in Explorer", L"&Delete",
	L"&Open Drive...", L"&Rescan Drive", L"&Show Free Space",
	L"%lu.%01lu%%", L"%lu.%01lu %s",
	L"Total", L"Free", L"Files Total:", L"Folders Total:", L"Files Found:", L"Folders Found:",
	'.', ',',
	densitynames, ukcolornames, monthnames, attribnames,
	L"Deleting...",
	L"Select Drive to View",
	L"Scanning Disk...",
	L"SpaceMonger Settings",
	L"Map", L"Detail:", L"Colors:",
	L"Files", L"Folders",
	L"Behavior", L"Show details when hovering over a box", L"Highlight the box under the mouse",
	L"Disable the \"Delete\" command",
	L"Appearance", L"Language:", L"Automatic (Windows)", L"Theme:",
	enthemenames,
	L"Properties...",
	L"The scan could not be completed (Windows error %lu).",
	L"Scan results are incomplete: %llu directories could not be fully scanned (first Windows error %lu). Sizes may be understated.",
	L"Scan completed with %llu folders skipped. Sizes may be understated.",
	L"Skipped: %llu",
	L"Scan details",
	L"%llu folders were skipped; showing %llu paths.",
	L"Copy paths",
	L"Not readable (estimate)",
	L"Space Windows reports as used, but that SpaceMonger was not allowed to read:\nfolders without access rights (other users, System Volume Information),\nfile system data and shadow copies. Run SpaceMonger as administrator\nto see more of it. The size is an estimate.",
	L"%s  ·  %d.%d %% of the drive",
	L"%s files in %s folders",
	L"Modified %s",
	L"Managed by Windows. Free space here with Disk Cleanup or\nSettings > System > Storage rather than by deleting files.",
	L"Free Up Space",
	L"%s can be removed safely: temporary files, browser caches and the Recycle Bin.",
	L"Your biggest files – move them to an external drive or delete what you no longer need:",
	L"Item", L"Size", L"Location", L"Tip",
	L"Looking for files that can be removed safely…",
	L"&Clean up now",
	L"&Show in folder",
	L"Close",
	L"&Details",
	L"Done – %s freed.",
	L"Nothing to clean up right now.",
	L"Scan a drive first to see its biggest files.",
	L"&Move to Recycle Bin",
	L"%s moved to the Recycle Bin.",
	L"Measured %d of %d locations…",
	L"Cleaning: %s (%d of %d)",
	L"%s of %s cleaned",
	L"&Stop",
	L"Stopped – %s freed.",
	L"cleaning…",
	L"done",
	L"Showing the scan from %s while it is updated.",
	L"Finishing the scan…",
	L"Copy path and size",
	L"Keep the last scan and show it while rescanning",
	L"&Folder...",
	L"E&xport list...",
	L"Saved to %s",
	L"Search files and folders",
	L"%s matches",
	L"SpaceMonger could not be restarted as administrator.",
	enfiletypes,
	encleanupkinds,
	encleanuphints,
	entoolbarlabels,
};

