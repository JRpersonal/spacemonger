#ifndef CLEANUPSCANNER_H
#define CLEANUPSCANNER_H

// Clean-up suggestions, kept free of MFC so they can be unit tested:
//  * well-known temporary folders and caches that Windows or the owning app
//    recreates on demand (safe to empty), with size and file count;
//  * the largest personal files in a finished scan, outside Windows-owned
//    areas, with a hint about what kind of file it is.

#include <windows.h>
#include <string>
#include <vector>
#include <atomic>
#include "e.h"

struct CFolder;

enum SM_CleanupKind {
	SM_CLEAN_USER_TEMP,
	SM_CLEAN_WINDOWS_TEMP,
	SM_CLEAN_INTERNET_CACHE,
	SM_CLEAN_CRASH_DUMPS,
	SM_CLEAN_ERROR_REPORTS,
	SM_CLEAN_SHADER_CACHE,
	SM_CLEAN_EDGE_CACHE,
	SM_CLEAN_CHROME_CACHE,
	SM_CLEAN_FIREFOX_CACHE,
	SM_CLEAN_NPM_CACHE,
	SM_CLEAN_GO_CACHE,
	SM_CLEAN_PIP_CACHE,
	SM_CLEAN_RECYCLE_BIN,
	SM_CLEAN_KIND_COUNT
};

struct SM_CleanupTarget {
	SM_CleanupKind kind;
	std::vector<std::wstring> paths;	// folders whose contents are removed (not the folders)
	bool keepRecent;					// temp folders: keep files changed in the last 24 h
	bool defaultChecked;				// developer caches start unchecked
	ui64 bytes;
	ui64 files;
};

struct SM_CleanupResult {
	ui64 freedBytes;
	ui64 deletedFiles;
	ui64 skippedFiles;					// in use, access denied or too recent
};

// Known locations that exist on this machine, not yet measured.
std::vector<SM_CleanupTarget> SM_FindCleanupTargets(void);
// Sets bytes/files (what a clean-up would remove). Stops early when
// *cancel becomes true (the dialog closed meanwhile).
void SM_MeasureCleanupTarget(SM_CleanupTarget &target, const std::atomic<bool> *cancel = nullptr);
// Deletes the contents; never follows junctions or symlinks.
SM_CleanupResult SM_CleanTarget(const SM_CleanupTarget &target);

// Helpers exposed for tests: measure / clean one folder.
void SM_MeasureFolder(const std::wstring &path, bool keepRecent, ui64 &bytes, ui64 &files,
	const std::atomic<bool> *cancel = nullptr);
SM_CleanupResult SM_CleanFolder(const std::wstring &path, bool keepRecent);

enum SM_LargeFileKind {
	SM_LARGE_VIDEO,
	SM_LARGE_DISK_IMAGE,
	SM_LARGE_VIRTUAL_DISK,
	SM_LARGE_ARCHIVE,
	SM_LARGE_INSTALLER,
	SM_LARGE_PHONE_UPDATE,
	SM_LARGE_OTHER,
	SM_LARGE_KIND_COUNT
};

struct SM_LargeFile {
	std::wstring path;
	ui64 bytes;
	ui64 time;
	SM_LargeFileKind kind;
	bool inDownloads;
};

SM_LargeFileKind SM_ClassifyLargeFile(const std::wstring &name);

// The `max` largest files of at least `minBytes` below `root` (scanned from
// `rootPath`), skipping Windows-owned root entries and the given folders
// (e.g. the clean-up targets, which are offered separately).
std::vector<SM_LargeFile> SM_FindLargePersonalFiles(const CFolder *root,
	const std::wstring &rootPath, size_t max, ui64 minBytes,
	const std::vector<std::wstring> &excludedFolders);

#endif
