#ifndef SPACEREPORT_H
#define SPACEREPORT_H

// Space report: the biggest folders and files of a scan as CSV, for the
// "Export list" command and the headless "/report" mode used in company
// rollouts (one file per computer, collected on a share).

#ifdef bool
#pragma push_macro("bool")
#undef bool
#include <string>
#pragma pop_macro("bool")
#else
#include <string>
#endif

#include "e.h"

struct CFolder;

struct SM_ReportOptions {
	int top = 25;				// rows per section (folders, files)
	bool hideSystem = false;	// leave out Windows, Program Files, ...
	wchar_t separator = L',';	// CSV list separator
	wchar_t decimal = L'.';		// decimal point of the GB column
};

struct SM_ReportVolume {
	std::wstring computer;		// computer the scanned data lives on
	std::wstring path;			// scanned root, with trailing backslash
	bool wholeVolume = false;	// path is a volume root (drive or share)
	ui64 totalSpace = 0, freeSpace = 0, scannedBytes = 0;
	ui64 scanTime = 0;			// FILETIME (UTC) of the scan
};

// List and decimal separators of the user's locale, so a double-click opens
// the file in columns in Excel. Falls back to ',' and '.'.
void SM_ReportLocaleSeparators(SM_ReportOptions &options);

// The report as CSV text. Kinds: Summary rows (drive size, used, free, not
// readable), then the largest folders, then the largest files.
std::wstring SM_BuildSpaceReport(const CFolder *root, const SM_ReportVolume &volume,
	const SM_ReportOptions &options);

// Computer a path belongs to: the server of a UNC path (\\PC01\c$ -> PC01),
// otherwise this computer.
std::wstring SM_ReportComputerForPath(const std::wstring &path);

// "\\srv\reports\" or an existing folder -> "<folder>\<COMPUTER>_<yyyy-mm-dd>.csv"
// (or the given extension). Environment variables (%COMPUTERNAME%) are expanded.
std::wstring SM_ResolveReportPath(const std::wstring &target, const std::wstring &computer,
	const wchar_t *extension = L".csv");

// Writes UTF-8 with BOM (Excel then reads umlauts correctly).
bool SM_WriteUtf8File(const std::wstring &path, const std::wstring &text);

#endif
