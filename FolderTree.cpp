#include "stdafx.h"
#include "spacemonger.h"
#include "FolderTree.h"
#include "FolderView.h"
#include "MainFrm.h"
#include "AsyncScanEngine.h"
#include "PathUtil.h"
#include "ScanAccounting.h"
#include "Lang.h"
#include <algorithm>

CFolderTree::CFolderTree()
{
	root = cur = NULL;
	freespace = usedspace = totalspace = 0;
	clustersize = 1;
	numfiles = numfolders = filespace = 0;
	m_path = "";
	wholeVolume = false;
	scanTime = 0;

	m_engine = NULL;
	m_frame = NULL;
	m_includeSpace = FALSE;
	m_isVolumeRoot = false;
	m_stopping = m_finishing = false;
	m_finisherDone = false;
	m_lastUiTick = m_nextRenderTick = 0;
	m_lastRenderBytes = m_lastRenderEntries = (ui64)-1;
	m_cachePreview = false;
	m_cacheLoaderDone = false;
	m_cacheLoaderCancel = false;
	m_loadedRoot = NULL;
	memset(&m_loadedInfo, 0, sizeof(m_loadedInfo));
	m_cacheWriterCancel = false;
}

CFolderTree::~CFolderTree()
{
	if (m_engine != NULL) {
		// The program ends in the middle of a scan.
		if (!m_finishing) m_engine->Cancel();
		if (m_finisher.joinable()) m_finisher.join();
		delete m_engine;
		m_engine = NULL;
	}
	m_cacheLoaderCancel = true;
	if (m_cacheLoader.joinable()) m_cacheLoader.join();
	delete m_loadedRoot;
	StopCacheWriter();
	if (root != NULL) delete root;
}

void CFolderTree::StopCacheWriter(void)
{
	m_cacheWriterCancel = true;
	if (m_cacheWriter.joinable()) m_cacheWriter.join();
}

// Nothing usable came out of a scan.
void CFolderTree::ResetTree(void)
{
	if (root != NULL) { delete root; root = NULL; }
	nameArena.Reset();
	cur = NULL;
	freespace = usedspace = totalspace = 0;
	m_path = "";
	scanTime = 0;
}

bool CFolderTree::BeginLoad(const CString &path, BOOL includespace)
{
	if (path == "" || m_engine != NULL) return false;

	StopCacheWriter();
	m_cacheLoaderCancel = true;
	if (m_cacheLoader.joinable()) m_cacheLoader.join();
	delete m_loadedRoot;
	m_loadedRoot = NULL;
	m_loadedArena.Reset();

	m_frame = (CMainFrame *)theApp.m_mainframe;
	if (m_frame != NULL && !::IsWindow(m_frame->m_hWnd)) m_frame = NULL;
	if (m_frame != NULL) m_frame->ClearScanNotice();

	m_path = path;
	GetSpace(path);
	if (root != NULL) { delete root; root = NULL; }
	nameArena.Reset();
	cur = NULL;
	filespace = 0;
	scanTime = 0;
	m_cachePreview = false;

	std::wstring widePath(path.GetString());
	std::wstring absPath = PathUtil::GetAbsolutePath(widePath);
	absPath = PathUtil::EnsureTrailingBackslash(absPath);
	wchar_t volumePath[MAX_PATH] = { 0 };
	bool isVolumeRoot = GetVolumePathNameW(absPath.c_str(), volumePath, _countof(volumePath))
		&& _wcsicmp(absPath.c_str(), volumePath) == 0;
	wholeVolume = isVolumeRoot;
	std::wstring preparedPath = PathUtil::PrepareLongPath(absPath);
	preparedPath = PathUtil::EnsureTrailingBackslash(preparedPath);
	BOOL aligned = (clustersize != 0 && (clustersize & (clustersize - 1)) == 0);

	m_engine = new AsyncScanEngine;
	if (!m_engine->StartScan(preparedPath, clustersize - 1, aligned)) {
		delete m_engine;
		m_engine = NULL;
		m_frame = NULL;
		return false;
	}
	m_includeSpace = includespace;
	m_isVolumeRoot = isVolumeRoot;
	m_stopping = m_finishing = false;
	m_finisherDone = false;
	m_lastUiTick = m_nextRenderTick = 0;
	m_lastRenderBytes = m_lastRenderEntries = (ui64)-1;
	m_cacheKey = absPath;

	// Progress is shown in the main window's status strip; EndLoad ends it.
	if (m_frame != NULL) m_frame->BeginScan(path);

	// The window (not the headless modes) shows the last scan of this place
	// while the new one runs; it is read on a worker and appears when ready.
	if (m_frame != NULL && theApp.m_settings.scan_cache) {
		std::wstring file = SM_ScanCacheFile(m_cacheKey);
		if (!file.empty()) StartCacheLoad(file);
	}
	return true;
}

void CFolderTree::StartCacheLoad(const std::wstring &file)
{
	m_cacheLoaderDone = false;
	m_cacheLoaderCancel = false;
	m_cacheLoader = std::thread([this, file]() {
		m_loadedRoot = SM_ReadScanCache(file, m_loadedArena, m_loadedInfo, &m_cacheLoaderCancel);
		m_cacheLoaderDone = true;
	});
}

// The cached scan becomes the map on screen, with its date, until the scan
// that is running replaces it. Only taken if it is really this place.
void CFolderTree::AdoptCachePreview(void)
{
	m_cacheLoader.join();
	CFolder *loaded = m_loadedRoot;
	m_loadedRoot = NULL;
	CFolderView *fv = (CFolderView *)theApp.m_view;
	const bool usable = loaded != NULL && fv != NULL && ::IsWindow(fv->m_hWnd)
		&& _wcsicmp(m_loadedInfo.path.c_str(), m_cacheKey.c_str()) == 0
		&& m_loadedInfo.wholeVolume == m_isVolumeRoot;
	if (!usable) {
		delete loaded;
		m_loadedArena.Reset();
		return;
	}

	root = loaded;
	nameArena.Merge(m_loadedArena);
	cur = root;
	numfiles = m_loadedInfo.numFiles;
	numfolders = m_loadedInfo.numFolders;
	filespace = m_loadedInfo.filespace;
	scanTime = m_loadedInfo.scanTime;
	if (!AddSyntheticEntries(m_includeSpace != FALSE, m_loadedInfo.partial)) {
		fv->SetDocument(NULL);
		delete root;
		root = cur = NULL;
		nameArena.Reset();
		scanTime = 0;
		return;
	}
	m_cachePreview = true;
	fv->ClearLiveScanLayout();
	fv->SetDocument(this);
	UpdateAllViews();
	if (m_frame != NULL) m_frame->SetScanPreview(scanTime);
}

// Free space and, for a partial whole-volume scan, the part of the used space
// that could not be read, as blocks at the top of the map.
bool CFolderTree::AddSyntheticEntries(bool includespace, bool partial)
{
	if (includespace && m_isVolumeRoot
		&& !root->AddFileWithArena(nameArena, L"<<<<<<<<<<<<<<<<<<<<", 1, freespace, freespace, 0))
		return false;

	ui64 unaccountedSpace = ComputeUnaccountedSpace(usedspace, filespace);
	if (partial && m_isVolumeRoot && unaccountedSpace > 0) {
		std::wstring unavailableName = L"|";
		unavailableName += CurLang->scan_unavailable;
		if (!root->AddFileWithArena(nameArena, unavailableName.c_str(),
			(ui32)unavailableName.size(), unaccountedSpace, unaccountedSpace, 0))
			return false;
	}
	root->Finalize();
	return true;
}

bool CFolderTree::PollLoad(void)
{
	if (m_engine == NULL) return false;
	CMainFrame *frame = m_frame;
	const bool cancelRequested = frame != NULL && frame->IsScanCancelRequested();

	if (!m_stopping) {
		if (cancelRequested) m_engine->Cancel();

		if (!m_cachePreview && m_cacheLoader.joinable() && m_cacheLoaderDone && m_engine->IsScanning())
			AdoptCachePreview();

		if (m_engine->IsScanning()) {
			DWORD now = ::GetTickCount();
			if (frame != NULL && now - m_lastUiTick >= 50) {
				m_lastUiTick = now;
				ScanProgress p = m_engine->GetProgress();
				frame->UpdateScan(p.numFiles, p.numFolders, p.bytesScanned, usedspace, p.currentPath, p.isComplete);
			}

			CFolderView *fv = (CFolderView *)theApp.m_view;
			if (!m_cachePreview && fv != NULL && (int)(now - m_nextRenderTick) >= 0 && ::IsWindow(fv->m_hWnd)) {
				m_nextRenderTick = now + 250;
				// Re-walking and re-laying-out the tree stalls the workers
				// (it holds the tree lock), so skip it when nothing changed.
				ScanProgress progress = m_engine->GetProgress();
				ui64 entries = progress.numFiles + progress.numFolders;
				if (progress.bytesScanned != m_lastRenderBytes || entries != m_lastRenderEntries) {
					m_lastRenderBytes = progress.bytesScanned;
					m_lastRenderEntries = entries;
					fv->UpdateLiveScanLayout(*m_engine, totalspace, freespace, m_isVolumeRoot);
					// A big tree takes longer to lay out; wait proportionally
					// so the map never costs more than about a ninth of the time.
					DWORD took = ::GetTickCount() - now;
					m_nextRenderTick = now + (std::max)((DWORD)250, took * 8);
				}
			}
			return true;
		}
		// The scan has drained, failed or been cancelled: let the workers go.
		m_engine->RequestStop();
		m_stopping = true;
	}

	// Keep the window alive while the workers wind down, even if one of them
	// is stuck in a slow filesystem call.
	if (!m_finishing) {
		if (cancelRequested) m_engine->Cancel();
		if (!m_engine->IsFinished()) return true;

		// Joining the workers, merging their string pools and sorting the folders
		// can take a moment on a big drive; do that off the window's thread.
		m_finishing = true;
		m_finisherDone = false;
		if (frame != NULL) {
			ScanProgress p = m_engine->GetProgress();
			frame->UpdateScan(p.numFiles, p.numFolders, p.bytesScanned, usedspace, CurLang->scan_finalizing, false);
		}
		AsyncScanEngine *engine = m_engine;
		m_finisher = std::thread([this, engine]() {
			engine->WaitForCompletion();
			m_finisherDone = true;
		});
		return true;
	}
	if (!m_finisherDone) return true;
	m_finisher.join();
	return false;
}

// Ends the progress strip and releases the engine; runs on every way out of
// FinishLoad.
void CFolderTree::EndLoad(void)
{
	if (m_finisher.joinable()) m_finisher.join();
	delete m_engine;
	m_engine = NULL;
	m_cacheLoaderCancel = true;
	if (m_cacheLoader.joinable()) m_cacheLoader.join();
	delete m_loadedRoot;
	m_loadedRoot = NULL;
	m_loadedArena.Reset();
	m_cachePreview = false;
	CMainFrame *frame = m_frame;
	m_frame = NULL;
	if (frame != NULL && ::IsWindow(frame->m_hWnd)) frame->EndScan();
}

BOOL CFolderTree::FinishLoad(void)
{
	if (m_engine == NULL) return 0;
	struct EndGuard {
		CFolderTree *tree;
		~EndGuard() { tree->EndLoad(); }
	} guard = { this };

	CMainFrame *frame = m_frame;
	CFolderView *fv = (CFolderView *)theApp.m_view;
	if (fv != NULL && !::IsWindow(fv->m_hWnd)) fv = NULL;
	ScanProgress resultProgress = m_engine->GetProgress();

	// The view may still show the last live frame or the cached map, which
	// points into the tree about to go; let go of both.
	auto releaseView = [&]() {
		if (fv == NULL) return;
		if (m_cachePreview) fv->SetDocument(NULL);
		fv->ClearLiveScanLayout();
	};

	if ((frame != NULL && frame->IsScanCancelRequested()) || m_engine->IsCancelled() || resultProgress.isFailed) {
		releaseView();
		ResetTree();
		if (resultProgress.isFailed && !theApp.m_headless) {
			CString message;
			message.Format(CurLang->scan_failed_format, resultProgress.firstError);
			AfxMessageBox(message, MB_OK | MB_ICONEXCLAMATION);
		}
		return 0;
	}

	releaseView();
	if (root != NULL) { delete root; root = NULL; }	// the cached map
	nameArena.Reset();
	root = m_engine->DetachResult(nameArena);
	if (root == NULL) {
		ResetTree();
		return 0;
	}

	cur = root;
	ScanProgress finalProgress = m_engine->GetProgress();
	numfiles = finalProgress.numFiles;
	numfolders = finalProgress.numFolders;
	filespace = finalProgress.bytesScanned;
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	scanTime = ((ui64)now.dwHighDateTime << 32) | now.dwLowDateTime;

	if (frame != NULL) frame->UpdateScan(finalProgress.numFiles, finalProgress.numFolders,
		finalProgress.bytesScanned, usedspace, finalProgress.currentPath, finalProgress.isComplete);

	if (!AddSyntheticEntries(m_includeSpace != FALSE, resultProgress.isPartial)) {
		ResetTree();
		return 0;
	}
	if (resultProgress.isPartial && frame != NULL && ::IsWindow(frame->m_hWnd)) {
		frame->SetScanNotice(resultProgress.skippedDirectories,
			resultProgress.firstError, m_engine->GetSkippedDirectories());
	}
	if (frame != NULL) StartCacheWrite(resultProgress.isPartial);
	return 1;
}

// Saves the finished scan for the next time, off the window's thread. The
// tree stays untouched until StopCacheWriter.
void CFolderTree::StartCacheWrite(bool partial)
{
	if (!theApp.m_settings.scan_cache || root == NULL || numfiles + numfolders > SM_SCAN_CACHE_MAX_ENTRIES) return;
	std::wstring file = SM_ScanCacheFile(m_cacheKey);
	if (file.empty()) return;
	SM_ScanCacheInfo info;
	info.path = m_cacheKey;
	info.scanTime = scanTime;
	info.numFiles = numfiles;
	info.numFolders = numfolders;
	info.filespace = filespace;
	info.clustersize = clustersize;
	info.wholeVolume = m_isVolumeRoot;
	info.partial = partial;
	m_cacheWriterCancel = false;
	const CFolder *tree = root;
	m_cacheWriter = std::thread([this, file, info, tree]() {
		SM_WriteScanCache(file, tree, info, &m_cacheWriterCancel);
	});
}

BOOL CFolderTree::LoadTree(const CString &path, BOOL includespace, CWnd *modalwin)
{
	if (path == "") return 1;
	if (!BeginLoad(path, includespace)) return 0;
	while (PollLoad()) ::Sleep(15);
	return FinishLoad();
}

void CFolderTree::GetSpace(const CString &pathIn)
{
	// The disk functions want the volume root (C:\ or \\PC01\c$\), not a
	// folder somewhere on it.
	wchar_t volumeRoot[MAX_PATH] = { 0 };
	CString path = pathIn;
	if (path.Right(1) != L"\\") path += L"\\";
	if (::GetVolumePathNameW(path, volumeRoot, MAX_PATH)) path = volumeRoot;

	DWORD SecPerClus = 0, BytesPerSec = 0, ClusPerDisk = 0, FreeClus = 0;
	ui64 oddfree = 0, total = 0, totalfree = 0;
	BOOL gotclusters;

	typedef BOOL (WINAPI *GetFreeDiskSpaceExFunc)(LPCTSTR pathname,
		ui64 *oddfree, ui64 *total, ui64 *totalfree);

	// First, compute the cluster size (which will be needed later)
	gotclusters = GetDiskFreeSpace(path, &SecPerClus, &BytesPerSec, &FreeClus, &ClusPerDisk);
	clustersize = gotclusters ? (ui64)BytesPerSec * (ui64)SecPerClus : 1;

	// Next, load in Kernel32 and use GetDiskFreeSpaceEx to find out
	// the size of the disk. If GetDiskFreeSpaceEx doesn't exist, then
	// fall back on the values from GetDiskFreeSpace.
	HINSTANCE hLibrary = LoadLibraryW(L"KERNEL32.DLL");
	GetFreeDiskSpaceExFunc getfreediskspaceex =
		hLibrary == NULL ? NULL : (GetFreeDiskSpaceExFunc)GetProcAddress(hLibrary, "GetDiskFreeSpaceExW");
	if (getfreediskspaceex != NULL && getfreediskspaceex(path, &oddfree, &total, &totalfree)) {
		freespace = totalfree;
		totalspace = total;
	}
	else if (gotclusters) {
		freespace = clustersize * (ui64)FreeClus;
		totalspace = clustersize * (ui64)ClusPerDisk;
	}
	else {
		freespace = totalspace = 0;
	}
	if (hLibrary != NULL) FreeLibrary(hLibrary);

	usedspace = totalspace > freespace ? totalspace - freespace : 0;
}

CFolder *CFolderTree::GetRoot(void)
{
	return(root);
}

CFolder *CFolderTree::SetCur(const CString &path)
{
	return(cur);
}

CFolder *CFolderTree::GetCur(void)
{
	return(cur);
}

CFolder *CFolderTree::Down(unsigned int index)
{
	if (index < cur->cur) {
		CFolder *newfolder = cur->children[index];
		if (newfolder != NULL) {
			cur = newfolder;
			return(cur);
		}
	}
	return(NULL);
}

CFolder *CFolderTree::Up(void)
{
	if (cur != root) {
		cur = cur->parent;
		return(cur);
	}
	return(NULL);
}

//////////////////////////////////////////////////////////////////////////////
