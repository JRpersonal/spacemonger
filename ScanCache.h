#ifndef SCANCACHE_H
#define SCANCACHE_H

// The last finished scan of a folder or drive, kept on disk so the next scan
// of the same place can show it at once while the real scan runs and replaces
// it. The cache never stands in for a scan: sizes inside a folder change
// without the folder's own time stamp changing, so nothing is skipped or
// trusted, only shown early (with its date). Kept free of MFC for unit tests.

#ifdef bool
#pragma push_macro("bool")
#undef bool
#include <string>
#include <atomic>
#pragma pop_macro("bool")
#else
#include <string>
#include <atomic>
#endif

#include "e.h"
#include "StringArena.h"

struct CFolder;

struct SM_ScanCacheInfo {
	std::wstring path;		// the scan root, as scanned
	ui64 scanTime;			// FILETIME of the scan
	ui64 numFiles;
	ui64 numFolders;
	ui64 filespace;			// bytes of all files
	ui64 clustersize;
	bool wholeVolume;
	bool partial;			// some folders could not be read
};

// Scans larger than this are not cached: the file would be huge and loading
// it would take longer than it saves.
const ui64 SM_SCAN_CACHE_MAX_ENTRIES = 3000000;

// %LOCALAPPDATA%\SpaceMonger\cache\<hash of path>.smcache; empty if the
// location is unavailable.
std::wstring SM_ScanCacheFile(const std::wstring &rootPath);

// Writes root (without the synthetic free-space and unreadable-space entries
// at its top) atomically. False if it failed or *cancel became true. Old cache
// files beyond a handful are removed.
bool SM_WriteScanCache(const std::wstring &file, const CFolder *root, const SM_ScanCacheInfo &info,
	const std::atomic<bool> *cancel = nullptr);

// Reads a cache file into a new tree whose names live in arena. NULL for a
// missing, damaged or foreign file, or when *cancel became true.
CFolder *SM_ReadScanCache(const std::wstring &file, CStringArena &arena, SM_ScanCacheInfo &info,
	const std::atomic<bool> *cancel = nullptr);

#endif
