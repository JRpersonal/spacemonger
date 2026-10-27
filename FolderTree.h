#ifndef FOLDERTREE_H
#define FOLDERTREE_H

#include "Folder.h"

#ifdef _AFX

#include <afxwin.h>

#ifdef bool
#pragma push_macro("bool")
#undef bool
#include <thread>
#include <atomic>
#pragma pop_macro("bool")
#else
#include <thread>
#include <atomic>
#endif

#include "ScanCache.h"

class AsyncScanEngine;
class CMainFrame;

class CFolderTree : public CFreeDoc {
public:
	CFolderTree();
	virtual ~CFolderTree();

	// Scans in the background so the window stays usable: BeginLoad starts the
	// scan, PollLoad (called from a timer, every few dozen milliseconds) shows
	// the progress and returns true while the scan is still running, and
	// FinishLoad takes over the result once PollLoad returned false.
	bool BeginLoad(const CString &path, BOOL includespace);
	bool PollLoad(void);
	BOOL FinishLoad(void);
	bool IsLoading(void) const { return m_engine != NULL; }
	// The previous scan of this place is on screen while the new one runs.
	bool IsShowingCache(void) const { return m_cachePreview; }

	// Blocking scan for the headless modes: BeginLoad, PollLoad until done, FinishLoad.
	BOOL LoadTree(const CString &path, BOOL includespace, CWnd *modalwin);
	void GetSpace(const CString &path);
	CFolder *GetRoot(void);

	// The tree must not change or disappear while the cache file is written
	// from it; anything that edits or replaces it calls this first (the
	// write is dropped, the next scan writes it again).
	void StopCacheWriter(void);

	CFolder *SetCur(const CString &path);
	CFolder *GetCur(void);
	CFolder *Down(unsigned int index);
	CFolder *Up(void);

protected:
	CFolder *root, *cur;

private:
	void EndLoad(void);
	bool AddSyntheticEntries(bool includespace, bool partial);
	void StartCacheLoad(const std::wstring &file);
	void AdoptCachePreview(void);
	void StartCacheWrite(bool partial);
	void ResetTree(void);

	// State of a running background scan.
	AsyncScanEngine *m_engine;
	CMainFrame *m_frame;
	BOOL m_includeSpace;
	bool m_isVolumeRoot;
	bool m_stopping;			// the scan is over; workers are winding down
	bool m_finishing;			// the helper thread is closing the scan
	std::thread m_finisher;
	std::atomic<bool> m_finisherDone;
	DWORD m_lastUiTick;
	DWORD m_nextRenderTick;
	ui64 m_lastRenderBytes, m_lastRenderEntries;

	// The previous scan of the same place, loaded on a worker while the scan runs.
	bool m_cachePreview;		// the tree shown is the cached one
	std::wstring m_cacheKey;	// the scan root, normalised: what the cache file is named after
	std::thread m_cacheLoader;
	std::atomic<bool> m_cacheLoaderDone;
	std::atomic<bool> m_cacheLoaderCancel;
	CFolder *m_loadedRoot;
	CStringArena m_loadedArena;
	SM_ScanCacheInfo m_loadedInfo;

	// Writes the cache of a finished scan.
	std::thread m_cacheWriter;
	std::atomic<bool> m_cacheWriterCancel;

public:
	CStringArena nameArena;
	CString m_path;
	ui64 freespace, usedspace, totalspace, clustersize;
	ui64 numfiles, numfolders;
	ui64 filespace;
	// The scan root is a volume root: the map shows free/unavailable space
	// with the same fixed split as the live scan.
	bool wholeVolume;
	// FILETIME of the scan the map shows (a cached one: when it was made).
	ui64 scanTime;
};

#endif // _AFX

#endif // FOLDERTREE_H
