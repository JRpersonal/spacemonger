#include "stdafx.h"
#include "spacemonger.h"
#include "FolderTree.h"
#include "FolderView.h"
#include "MainFrm.h"
#include "AsyncScanEngine.h"
#include "PathUtil.h"
#include "ScanAccounting.h"
#include "Lang.h"

CFolderTree::CFolderTree()
{
	root = cur = NULL;
	freespace = usedspace = totalspace = 0;
	m_path = "";
	wholeVolume = false;
}

CFolderTree::~CFolderTree()
{
	if (root != NULL) delete root;
}

BOOL CFolderTree::LoadTree(const CString &path, BOOL includespace, CWnd *modalwin)
{
	if (path == "") return 1;

	CMainFrame *frame = (CMainFrame *)theApp.m_mainframe;
	if (frame != NULL && !::IsWindow(frame->m_hWnd)) frame = NULL;
	if (frame != NULL) frame->ClearScanNotice();

	// Progress is shown in the main window's status strip; this ends it on
	// every return path.
	struct ScanUi {
		CMainFrame *frame;
		ScanUi(CMainFrame *f, const CString &root) : frame(f) { if (frame != NULL) frame->BeginScan(root); }
		~ScanUi() { if (frame != NULL && ::IsWindow(frame->m_hWnd)) frame->EndScan(); }
		bool cancelled() const { return frame != NULL && frame->IsScanCancelRequested(); }
		void cancel() { if (frame != NULL) frame->RequestScanCancel(); }
		void update(const ScanProgress &p, ui64 used) {
			if (frame != NULL)
				frame->UpdateScan(p.numFiles, p.numFolders, p.bytesScanned, used, p.currentPath, p.isComplete);
		}
	} scanUi(frame, path);

	m_path = path;
	GetSpace(path);
	if (root != NULL) { delete root; root = NULL; }
	nameArena.Reset();
	cur = NULL;
	filespace = 0;

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

	AsyncScanEngine engine;
	if (!engine.StartScan(preparedPath, clustersize - 1, aligned)) {
		return 0;
	}

	// Pumps pending messages; returns 0 if WM_QUIT arrived (scan is torn down).
	auto pumpMessages = [&]() -> BOOL {
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
				scanUi.cancel();
				continue;
			}
			if (msg.message == WM_QUIT) {
				engine.Cancel();
				engine.WaitForCompletion();
				::PostQuitMessage((int)msg.wParam);
				return 0;
			}
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		return 1;
	};

	DWORD last_ui_tick = 0;
	DWORD last_render_tick = 0;
	ui64 last_render_bytes = (ui64)-1;
	ui64 last_render_entries = (ui64)-1;
	while (engine.IsScanning()) {
		if (!pumpMessages()) return 0;

		if (scanUi.cancelled()) {
			engine.Cancel();
			break;
		}

		DWORD now = ::GetTickCount();
		if (now - last_ui_tick >= 50) {
			last_ui_tick = now;
			scanUi.update(engine.GetProgress(), usedspace);
		}

		if (now - last_render_tick >= 250) {
			last_render_tick = now;
			CFolderView *fv = (CFolderView *)theApp.m_view;
			if (fv != NULL && ::IsWindow(fv->m_hWnd)) {
				// Re-walking and re-laying-out the tree stalls the workers
				// (it holds the tree lock), so skip it when nothing changed.
				ScanProgress progress = engine.GetProgress();
				ui64 entries = progress.numFiles + progress.numFolders;
				if (progress.bytesScanned != last_render_bytes || entries != last_render_entries) {
					last_render_bytes = progress.bytesScanned;
					last_render_entries = entries;
					fv->UpdateLiveScanLayout(engine, totalspace, freespace, isVolumeRoot);
				}
			}
		}

		::Sleep(15);
	}

	// Keep pumping while the workers wind down so the UI stays responsive even
	// if one of them is stuck in a slow filesystem call.
	engine.RequestStop();
	while (!engine.IsFinished()) {
		if (!pumpMessages()) return 0;
		if (scanUi.cancelled()) engine.Cancel();
		::Sleep(10);
	}

	engine.WaitForCompletion();
	ScanProgress resultProgress = engine.GetProgress();

	if (scanUi.cancelled() || engine.IsCancelled() || resultProgress.isFailed) {
		// The view may still show the last live-scan frame; drop it so a
		// dead scan's half-built map doesn't linger on screen.
		CFolderView *fv = (CFolderView *)theApp.m_view;
		if (fv != NULL && ::IsWindow(fv->m_hWnd)) {
			fv->ClearLiveScanLayout();
		}
		if (root != NULL) { delete root; root = NULL; }
		nameArena.Reset();
		root = cur = NULL;
		freespace = usedspace = totalspace = 0;
		m_path = "";
		if (resultProgress.isFailed && !theApp.m_headless) {
			CString message;
			message.Format(CurLang->scan_failed_format, resultProgress.firstError);
			AfxMessageBox(message, MB_OK | MB_ICONEXCLAMATION);
		}
		return 0;
	}

	root = engine.DetachResult(nameArena);
	if (root == NULL) {
		nameArena.Reset();
		root = cur = NULL;
		freespace = usedspace = totalspace = 0;
		m_path = "";
		return 0;
	}

	cur = root;
	ScanProgress finalProgress = engine.GetProgress();
	numfiles = finalProgress.numFiles;
	numfolders = finalProgress.numFolders;
	filespace = finalProgress.bytesScanned;

	scanUi.update(finalProgress, usedspace);

	if (includespace && isVolumeRoot
		&& !root->AddFileWithArena(nameArena, L"<<<<<<<<<<<<<<<<<<<<", 1, freespace, freespace, 0)) {
		delete root;
		nameArena.Reset();
		root = cur = NULL;
		freespace = usedspace = totalspace = 0;
		m_path = "";
		return 0;
	}

	ui64 unaccountedSpace = ComputeUnaccountedSpace(usedspace, filespace);
	if (resultProgress.isPartial && isVolumeRoot && unaccountedSpace > 0) {
		std::wstring unavailableName = L"|";
		unavailableName += CurLang->scan_unavailable;
		if (!root->AddFileWithArena(nameArena, unavailableName.c_str(),
			(ui32)unavailableName.size(), unaccountedSpace, unaccountedSpace, 0)) {
			delete root;
			nameArena.Reset();
			root = cur = NULL;
			freespace = usedspace = totalspace = 0;
			m_path = "";
			return 0;
		}
	}

	root->Finalize();
	if (resultProgress.isPartial) {
		if (frame != NULL && ::IsWindow(frame->m_hWnd)) {
			frame->SetScanNotice(resultProgress.skippedDirectories,
				resultProgress.firstError, engine.GetSkippedDirectories());
		}
	}

	if (modalwin != NULL && ::IsWindow(modalwin->m_hWnd)) {
		modalwin->SetWindowPos(&CWnd::wndTop, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	}

	return 1;
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
