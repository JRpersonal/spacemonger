#ifndef HTMLREPORT_H
#define HTMLREPORT_H

// HTML space report: one self-contained page with the scan's treemap, drawn
// and laid out in the browser like the window does it (hover details, zoom,
// biggest folders and files, "Show in Explorer" through spacemonger: links).
// Written by "/html" for unattended rollouts and by Export in the window.

#include "SpaceReport.h"

// Placeholder in res/report.html that the data replaces.
#define SM_HTML_DATA_MARKER L"/*SPACEMONGER_DATA*/null"

// The scan as JSON for the page. Entries smaller than about 1/30000 of the
// scan are summed per folder ("N smaller entries"), which keeps a full drive
// at a few MB while every box the map can draw is still there.
// lang: "en", "de" or "fr", the page's language.
std::wstring SM_BuildReportJson(const CFolder *root, const SM_ReportVolume &volume,
	const SM_ReportOptions &options, const wchar_t *lang);

// The page: the template with the JSON in place of SM_HTML_DATA_MARKER.
// Empty if the marker is missing.
std::wstring SM_BuildHtmlReport(const std::wstring &pageTemplate, const std::wstring &json);

#endif
