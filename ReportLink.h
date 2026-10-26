#ifndef REPORTLINK_H
#define REPORTLINK_H

// spacemonger: links from the HTML report back to Windows:
//   spacemonger:show?<path>  selects the file or folder in Explorer
//   spacemonger:scan?<path>  opens SpaceMonger on that folder
// <path> is percent-encoded UTF-8. Any web page can open such a link, so
// only plain local paths (C:\...) are accepted: no shares, no device paths,
// no "..". Nothing a link does deletes or changes a file.

#ifdef bool
#pragma push_macro("bool")
#undef bool
#include <string>
#pragma pop_macro("bool")
#else
#include <string>
#endif

enum SM_LinkAction { SM_LINK_NONE, SM_LINK_SHOW, SM_LINK_SCAN };

// Action and path of a link; SM_LINK_NONE for anything else.
SM_LinkAction SM_ParseReportLink(const std::wstring &url, std::wstring &path);

// True for "X:\" and "X:\folder\file" without "..", wildcards, quotes or
// control characters.
bool SM_IsPlainLocalPath(const std::wstring &path);

// Registers or removes the spacemonger: link handler for the current user
// (HKCU\Software\Classes\spacemonger), pointing at exe.
bool SM_RegisterReportLinks(const std::wstring &exe);
bool SM_UnregisterReportLinks();

// True when the handler already points at exe, so a report made in the window
// can register it on demand (and again after the program was moved).
bool SM_ReportLinksRegistered(const std::wstring &exe);

#endif
