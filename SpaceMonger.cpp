
#include "StdAfx.h"
#include "SpaceMonger.h"
#include "Lang.h"
#include "MainFrm.h"
#include "FolderTree.h"
#include "FolderView.h"
#include "DriveDialog.h"
#include "SetupDlg.h"
#include "TipWnd.h"
#include "CleanupDialog.h"
#include "AboutDialog.h"
#include "CommandPolicy.h"
#include "PathUtil.h"
#include "SpaceReport.h"
#include "Theme.h"
#include <shobjidl.h>
#include <shlobj.h>

/////////////////////////////////////////////////////////////////////////////
// The primary CSpaceMonger object

CSpaceMonger theApp;

/////////////////////////////////////////////////////////////////////////////
// CApp

BEGIN_MESSAGE_MAP(CSpaceMonger, CWinApp)
	//{{AFX_MSG_MAP(CSpaceMonger)
	ON_COMMAND(ID_APP_ABOUT, OnAbout)
	ON_COMMAND(ID_APP_EXIT, OnAppExit)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_REFRESH, OnFileRefresh)
	ON_COMMAND(ID_FILE_RUN, OnFileRun)
	ON_COMMAND(ID_FILE_DELETE, OnFileDelete)
	ON_COMMAND(ID_FILE_PROPERTIES, OnFileProperties)
	ON_COMMAND(ID_VIEW_ZOOM_FULL, OnZoomFull)
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnZoomOut)


	ON_COMMAND(ID_VIEW_FREE, OnShowFree)
	ON_COMMAND(ID_VIEW_HIDE_SYSTEM, OnHideSystem)
	ON_COMMAND(ID_TOOLS_CLEANUP, OnCleanup)
	ON_COMMAND(ID_SETTINGS, OnSettings)
	ON_COMMAND(ID_FILE_RUNAS, OnRunAsAdmin)
	ON_COMMAND(ID_FILE_EXPORT, OnExport)
	//}}AFX_MSG_MAP
	ON_UPDATE_COMMAND_UI_RANGE(100, 41000, OnIgnoreUpdate)
END_MESSAGE_MAP()

CSpaceMonger::CSpaceMonger()
{
	m_document = NULL;
	m_mainframe = NULL;
	m_view = NULL;
	m_headless = false;
	m_exitCode = 0;
}

namespace {

// Command line:
//   SpaceMonger.exe [path] [/report <file or folder>] [/top N] [/hide-system]
// A path alone scans it in the window (drive, folder or \\PC01\c$). With
// /report nothing is shown: the scan is written as CSV and the program ends,
// for rollouts and admin scripts. A report folder gets <COMPUTER>_<date>.csv.
struct StartOptions {
	CString path;
	CString report;
	int top = 25;
	bool hideSystem = false;
	bool invalid = false;
};

CString CleanPathArgument(CString path)
{
	path.Trim();
	path.Trim(L'"');	// "C:\" arrives as C:" (the backslash escapes the quote)
	if (path.GetLength() == 2 && path[1] == L':') path += L'\\';
	return path;
}

StartOptions ParseStartOptions()
{
	StartOptions options;
	int argc = 0;
	LPWSTR *argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	if (argv == NULL) return options;
	CString path;
	for (int i = 1; i < argc; ++i) {
		CString arg = argv[i];
		if (arg.GetLength() < 2 || (arg[0] != L'/' && arg[0] != L'-')) {
			// Unquoted paths with spaces arrive in pieces; join them again.
			if (!path.IsEmpty()) path += L' ';
			path += arg;
			continue;
		}
		CString name = arg.Mid(1), value;
		int colon = name.Find(L':');
		bool hasValue = colon > 0;
		if (hasValue) { value = name.Mid(colon + 1); name = name.Left(colon); }
		name.MakeLower();
		auto takeValue = [&]() -> bool {
			if (!hasValue && i + 1 < argc) { value = argv[++i]; hasValue = true; }
			return hasValue && !value.IsEmpty();
		};
		if (name == L"report" || name == L"csv") {
			if (takeValue()) options.report = CleanPathArgument(value);
			else options.invalid = true;
		}
		else if (name == L"top") {
			options.top = takeValue() ? _wtoi(value) : 0;
			if (options.top <= 0) options.invalid = true;
		}
		else if (name == L"hide-system" || name == L"hidesystem") options.hideSystem = true;
		else options.invalid = true;	// includes /? and /help
	}
	::LocalFree(argv);
	options.path = CleanPathArgument(path);
	return options;
}

const wchar_t kUsage[] =
	L"SpaceMonger.exe [path] [/report <file or folder>] [/top N] [/hide-system]\r\n\r\n"
	L"  path           Drive, folder or share to scan, e.g. D:\\ or \\\\PC01\\c$\r\n"
	L"                 (default with /report: the Windows drive)\r\n"
	L"  /report        Scan without a window, write a CSV report and exit.\r\n"
	L"                 A folder gets <COMPUTER>_<yyyy-mm-dd>.csv.\r\n"
	L"  /top N         Folders and files to list (default 25)\r\n"
	L"  /hide-system   Leave out Windows, Program Files and other system content\r\n\r\n"
	L"Exit codes: 0 report written, 1 invalid arguments, 2 scan failed, 3 report not written.\r\n";

// Messages for the console a script started us from; a scheduled task has
// none, then they go nowhere and the exit code is all there is.
void ConsoleLine(const CString &text)
{
	static bool attached = ::AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
	if (!attached) return;
	HANDLE out = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (out == NULL || out == INVALID_HANDLE_VALUE) return;
	CString line = text + L"\r\n";
	DWORD written = 0;
	if (!::WriteConsoleW(out, (LPCWSTR)line, (DWORD)line.GetLength(), &written, NULL)) {
		// Redirected to a file: write UTF-8.
		CStringA utf8 = CW2A(line, CP_UTF8);
		::WriteFile(out, (LPCSTR)utf8, (DWORD)utf8.GetLength(), &written, NULL);
	}
}

SM_ReportVolume MakeReportVolume(const CFolderTree &tree)
{
	SM_ReportVolume volume;
	volume.path = (LPCWSTR)tree.m_path;
	if (!volume.path.empty() && volume.path.back() != L'\\') volume.path += L'\\';
	volume.computer = SM_ReportComputerForPath(volume.path);
	volume.wholeVolume = tree.wholeVolume;
	volume.totalSpace = tree.totalspace;
	volume.freeSpace = tree.freespace;
	volume.scannedBytes = tree.filespace;
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	volume.scanTime = ((ui64)now.dwHighDateTime << 32) | now.dwLowDateTime;
	return volume;
}

int RunHeadlessReport(const StartOptions &options)
{
	CString path = options.path;
	if (path.IsEmpty()) {
		wchar_t drive[MAX_PATH] = L"C:";
		::GetEnvironmentVariableW(L"SystemDrive", drive, MAX_PATH);
		path = CleanPathArgument(drive);
	}
	ConsoleLine(L"SpaceMonger: scanning " + path);

	CFolderTree tree;
	if (!tree.LoadTree(path, 0, NULL) || tree.GetRoot() == NULL) {
		ConsoleLine(L"SpaceMonger: the scan of " + path + L" failed.");
		return 2;
	}
	SM_ReportVolume volume = MakeReportVolume(tree);
	SM_ReportOptions reportOptions;
	SM_ReportLocaleSeparators(reportOptions);
	reportOptions.top = options.top;
	reportOptions.hideSystem = options.hideSystem;
	std::wstring text = SM_BuildSpaceReport(tree.GetRoot(), volume, reportOptions);
	std::wstring file = SM_ResolveReportPath((LPCWSTR)options.report, volume.computer);
	if (!SM_WriteUtf8File(file, text)) {
		CString message;
		message.Format(L"SpaceMonger: could not write %s (Windows error %lu).", file.c_str(), ::GetLastError());
		ConsoleLine(message);
		return 3;
	}
	ConsoleLine(CString(L"SpaceMonger: report written to ") + file.c_str());
	return 0;
}

}

CSpaceMonger::~CSpaceMonger()
{
}

BOOL CSpaceMonger::InitInstance()
{
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	int i;

	InitTipWnd(AfxGetInstanceHandle());

	// Load in the settings from the Registry.  If there were no settings,
	// we load defaults.  Once we've got them, save them back, which sounds
	// odd, but it will end up creating the necessary entries if they don't
	// exist.
	m_settings.Load();

	StartOptions start = ParseStartOptions();
	if (start.invalid || !start.report.IsEmpty()) {
		// Headless: returning FALSE ends the program with ExitInstance's code.
		m_headless = true;
		if (start.invalid) {
			ConsoleLine(kUsage);
			m_exitCode = 1;
		}
		else m_exitCode = RunHeadlessReport(start);
		return FALSE;
	}
	m_settings.Save();
	if (start.hideSystem) m_settings.hide_system = TRUE;
	SM_ApplyThemeSetting(m_settings.theme);

	int showcmd = m_settings.showcmd;
	RECT rect = m_settings.rect;
	if (!m_settings.save_pos) {
		showcmd = SW_SHOWNORMAL;
		rect.top = rect.left = CW_USEDEFAULT;
		rect.right = rect.bottom = 0;	// This is actually CW_USEDEFAULT because of a bug
										// in CFrameWnd::Create()
	}

	// Create the main window
	m_mainframe = new CMainFrame;
	m_mainframe->CFrameWnd::Create(NULL, L"SpaceMonger", WS_OVERLAPPEDWINDOW, rect);
	m_pMainWnd = m_mainframe;

	// Create the one and only document
	m_document = new CFolderTree;
	m_document->Create();

	// Create the main document view
	m_view = new CFolderView;
	m_view->Create(m_mainframe, 1);
	m_view->SetDocument(m_document);
	m_view->SetFocus();

	// Load in the animations
	static int anim_name[4] = {
		IDB_SCAN1, IDB_SCAN2, IDB_SCAN3, IDB_SCAN4,
	};
	for (i = 0; i < 4; i++)
		m_scan_animation[i] = (HBITMAP)LoadImage(m_hInstance,
			MAKEINTRESOURCE(anim_name[i]), IMAGE_BITMAP, 128, 48,
			LR_LOADTRANSPARENT|LR_LOADMAP3DCOLORS);

	m_mainframe->SetWindowText(L"SpaceMonger");
	m_mainframe->ShowWindow(showcmd);
	m_mainframe->UpdateWindow();
	m_mainframe->RecalcLayout();

	m_mainframe->DragAcceptFiles(TRUE);

	// "SpaceMonger.exe D:\" or "SpaceMonger.exe \\PC01\c$" scans that path right
	// away (shortcuts for a rollout, admin scripts, "Open with"). Otherwise
	// start with the drive picker: choosing a drive is what nearly every
	// session begins with.
	if (!start.path.IsEmpty()) {
		// LoadTree pumps messages itself, so this works before the message loop starts.
		ScanPath(start.path);
	}
	else m_mainframe->PostMessage(WM_COMMAND, ID_FILE_OPEN);

	return(TRUE);
}

int CSpaceMonger::ExitInstance()
{
	if (m_headless) {
		delete m_document;
		return m_exitCode;
	}
	m_settings.Save();

	// m_pMainWnd is auto-deleted by the framework
	delete m_view;
	delete m_document;

	return(0);
}

void CSpaceMonger::OnAppExit(void)
{
	CWinApp::OnAppExit();
}

void CSpaceMonger::OnFileOpen(void)
{
	CDriveDialog drvdialog;
	int drive = (int)drvdialog.DoModal();
	if (drive == -1) return;

	if (drive == CDriveDialog::kFolderChosen) {
		ScanPath(drvdialog.m_folder);
		return;
	}
	CString path;
	path.Format(L"%c:\\", (wchar_t)(drive + 'A'));
	ScanPath(path);
}

void CSpaceMonger::ScanPath(const CString &path)
{
	CMainFrame *frame = (CMainFrame *)m_mainframe;
	if (path.IsEmpty() || (frame != NULL && frame->IsScanning())) return;
	CFolderTree *ft = (CFolderTree *)m_document;
	ft->m_path = CleanPathArgument(path);
	// Called directly: a posted ID_FILE_REFRESH would be dropped, because
	// the command is disabled while nothing is open.
	OnFileRefresh();
}

// Exports the biggest folders and files of the scan as CSV (Excel), the same
// report the headless /report mode writes.
void CSpaceMonger::OnExport(void)
{
	CFolderTree *ft = (CFolderTree *)m_document;
	if (ft == NULL || ft->GetRoot() == NULL) return;
	SM_ReportVolume volume = MakeReportVolume(*ft);

	PWSTR documents = NULL;
	CString folder;
	if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents))) folder = documents;
	::CoTaskMemFree(documents);
	std::wstring suggested = SM_ResolveReportPath(folder.IsEmpty() ? L".\\" : (LPCWSTR)(folder + L"\\"), volume.computer);
	CString name = ::PathFindFileNameW(suggested.c_str());

	CFileDialog dialog(FALSE, L"csv", name, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
		L"CSV (*.csv)|*.csv||", m_mainframe);
	if (!folder.IsEmpty()) dialog.m_ofn.lpstrInitialDir = folder;
	if (dialog.DoModal() != IDOK) return;

	SM_ReportOptions options;
	SM_ReportLocaleSeparators(options);
	options.top = 100;
	options.hideSystem = m_settings.hide_system != 0;
	CString file = dialog.GetPathName();
	if (!SM_WriteUtf8File((LPCWSTR)file, SM_BuildSpaceReport(ft->GetRoot(), volume, options))) {
		CString message;
		message.Format(L"%s\n\nWindows error %lu", (LPCWSTR)file, ::GetLastError());
		m_mainframe->MessageBox(message, L"SpaceMonger", MB_ICONWARNING | MB_OK);
		return;
	}
	CString done;
	done.Format(CurLang->export_done_format, (LPCWSTR)file);
	m_mainframe->MessageBox(done, L"SpaceMonger", MB_ICONINFORMATION | MB_OK);
}

// Restarts elevated on the same path, so folders of other users and System
// Volume Information become readable. The current instance closes only once
// the elevated one has actually started (the UAC prompt may be declined).
void CSpaceMonger::OnRunAsAdmin(void)
{
	wchar_t exe[MAX_PATH];
	if (::GetModuleFileNameW(NULL, exe, MAX_PATH) == 0) return;
	CString args;
	CFolderTree *ft = (CFolderTree *)m_document;
	if (ft != NULL && !ft->m_path.IsEmpty()) {
		CString path = ft->m_path;
		// A trailing backslash would escape the closing quote ("C:\"); the
		// command-line handling turns "C:" back into "C:\".
		path.TrimRight(L'\\');
		args = L"\"" + path + L"\"";
	}
	SHELLEXECUTEINFOW info;
	memset(&info, 0, sizeof(info));
	info.cbSize = sizeof(info);
	info.hwnd = m_mainframe->m_hWnd;
	info.lpVerb = L"runas";
	info.lpFile = exe;
	info.lpParameters = args.IsEmpty() ? NULL : (LPCWSTR)args;
	info.nShow = SW_SHOWNORMAL;
	if (::ShellExecuteExW(&info)) {
		m_mainframe->PostMessage(WM_CLOSE);
	}
	else if (::GetLastError() != ERROR_CANCELLED) {
		m_mainframe->MessageBox(CurLang->restart_admin_failed, L"SpaceMonger", MB_ICONWARNING | MB_OK);
	}
}

void CSpaceMonger::OnFileRefresh(void)
{
	CFolderTree *ft = (CFolderTree *)m_document;

	// The frame stays enabled during the scan so its Cancel button works;
	// LoadTree locks the toolbar and the map through CMainFrame::BeginScan.
	m_view->SetDocument(NULL);
	m_view->UpdateWindow();

	((CFolderView *)m_view)->zoomlevel = 0;
	ft->LoadTree(ft->m_path, 1, m_mainframe);
	m_view->SetDocument(m_document);
	ft->UpdateAllViews();
}

void CSpaceMonger::OnFileRun(void)
{
	CFolderView *fv = (CFolderView *)m_view;
	CFolderTree *ft = (CFolderTree *)m_document;

	if (fv == NULL || ft == NULL || fv->selected == NULL) return;

	// Opening and running happen in Explorer; SpaceMonger shows where it is.
	fv->ShowInExplorer(fv->selected);
}

static HRESULT DeleteShellItem(const std::wstring& path, HWND owner)
{
	HRESULT initResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	char uninitialize = initResult == S_OK || initResult == S_FALSE;
	IShellItem *item = NULL;
	IFileOperation *operation = NULL;
	HRESULT result;

	if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE)
		return initResult;

	result = SHCreateItemFromParsingName(path.c_str(), NULL, IID_PPV_ARGS(&item));
	if (FAILED(result)) {
		std::wstring preparedPath = PathUtil::PrepareLongPath(path);
		if (preparedPath != path)
			result = SHCreateItemFromParsingName(preparedPath.c_str(), NULL, IID_PPV_ARGS(&item));
	}
	if (SUCCEEDED(result))
		result = CoCreateInstance(CLSID_FileOperation, NULL, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&operation));
	if (SUCCEEDED(result))
		result = operation->SetOwnerWindow(owner);
	if (SUCCEEDED(result))
		result = operation->SetOperationFlags(
			FOF_SILENT | FOF_NOCONFIRMATION | FOF_ALLOWUNDO | FOFX_RECYCLEONDELETE);
	if (SUCCEEDED(result))
		result = operation->DeleteItem(item, NULL);
	if (SUCCEEDED(result))
		result = operation->PerformOperations();
	if (SUCCEEDED(result)) {
		BOOL aborted = FALSE;
		result = operation->GetAnyOperationsAborted(&aborted);
		if (SUCCEEDED(result) && aborted)
			result = E_ABORT;
	}

	if (operation != NULL)
		operation->Release();
	if (item != NULL)
		item->Release();
	if (uninitialize)
		CoUninitialize();
	return result;
}

void CSpaceMonger::OnFileDelete(void)
{
	CFolderView *fv = (CFolderView *)m_view;
	CFolderTree *ft = (CFolderTree *)m_document;

	if (fv == NULL || ft == NULL ||
		!SM_CanDeleteSelection(m_settings.disable_delete, fv->selected != NULL)) return;

	std::wstring title = fv->BuildItemPathW(fv->selected);
	BOOL isfolder;
	if (fv->selected->source->children[fv->selected->index] != NULL)
		isfolder = 1;
	else isfolder = 0;

	if (FAILED(DeleteShellItem(title, m_mainframe->m_hWnd))) {
		AfxMessageBox(L"Windows failed to delete file.");
		return;
	}

	if (isfolder) {
		delete fv->selected->source->children[fv->selected->index];
		fv->selected->source->children[fv->selected->index] = NULL;
	}
	ui64 subsize = fv->selected->source->sizes[fv->selected->index];
	CFolder *folder = fv->selected->source;
	if (isfolder) folder->size_children -= subsize;
	else folder->size_self -= subsize;
	folder = folder->parent;
	while (folder != NULL) {
		folder->size_children -= subsize;
		folder = folder->parent;
	}
	fv->selected->source->sizes[fv->selected->index] = 0;

	if (m_settings.auto_rescan)
		OnFileRefresh();
	else
		fv->OnUpdate(m_document);
}

void CSpaceMonger::OnZoomIn(void)
{
	((CFolderView *)m_view)->ZoomIn(((CFolderView *)m_view)->ZoomTarget());
}

void CSpaceMonger::OnZoomOut(void)
{
	((CFolderView *)m_view)->ZoomOut();
}

void CSpaceMonger::OnZoomFull(void)
{
	((CFolderView *)m_view)->ZoomFull();
}

void CSpaceMonger::OnShowFree(void)
{
	CFolderView *fv = (CFolderView *)m_view;
	CMainFrame *mf = (CMainFrame *)m_mainframe;
	if (fv == NULL || mf == NULL) return;

	BOOL showfreespace = !fv->showfreespace;
	mf->m_toolbar.EnableState(CMainToolBar::ViewFree, showfreespace);
	fv->ShowFreeSpace(showfreespace);
}

void CSpaceMonger::OnHideSystem(void)
{
	CFolderView *fv = (CFolderView *)m_view;
	CMainFrame *mf = (CMainFrame *)m_mainframe;
	if (fv == NULL || mf == NULL) return;

	m_settings.hide_system = !m_settings.hide_system;
	m_settings.Save();
	mf->m_toolbar.EnableState(CMainToolBar::ShowSystem, !m_settings.hide_system);
	fv->OnUpdate(m_document);
}

void CSpaceMonger::OnCleanup(void)
{
	CFolderTree *ft = (CFolderTree *)m_document;
	CCleanupDialog dialog(ft, m_mainframe);
	dialog.DoModal();
	// Show the space that was just freed.
	if (dialog.FreedAnything() && ft != NULL && ft->GetRoot() != NULL)
		OnFileRefresh();
}

void CSpaceMonger::OnSettings(void)
{
	// The dialog applies every change at once and undoes them on Cancel.
	CSettingsDialog settingsdialog;
	if (settingsdialog.DoModal() == IDOK)
		m_settings.Save();
}

void CSpaceMonger::ApplySettings(void)
{
	if (m_view != NULL && m_document != NULL) {
		m_view->OnUpdate(m_document);
		((CFolderView *)m_view)->UpdateTitleBar();
	}
	if (m_mainframe != NULL)
		((CMainFrame *)m_mainframe)->ApplyAppearance();
}

void CSpaceMonger::OnFileProperties(void)
{
	CFolderView *fv = (CFolderView *)m_view;
	CFolderTree *ft = (CFolderTree *)m_document;

	if (fv == NULL || ft == NULL || fv->selected == NULL) return;

	std::wstring path = fv->BuildItemPathW(fv->selected);

	SHELLEXECUTEINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.hwnd = fv->GetSafeHwnd();
	si.nShow = SW_SHOW;
	si.fMask  = SEE_MASK_INVOKEIDLIST;
	si.lpVerb = L"properties";
	si.lpFile = path.c_str();
	if (!ShellExecuteExW(&si))
		AfxMessageBox(L"Windows failed to show file properties.");
}

void CSpaceMonger::OnAbout()
{
	CAboutDialog about;
	about.DoModal();
}

void CSpaceMonger::OnIgnoreUpdate(CCmdUI *ui)
{
	GeneralIgnoreUpdate(ui);
}

void GeneralIgnoreUpdate(CCmdUI *ui)
{
	CFolderView *fv = (CFolderView *)theApp.m_view;
	const TreemapNode *cur = fv->selected;
	int zoom = fv->zoomlevel;

	if (ui->m_nID == ID_FILE_DELETE) {
		if (!SM_CanDeleteSelection(theApp.m_settings.disable_delete, cur != NULL))
			ui->Enable(0);
	}
	else if (ui->m_nID == ID_FILE_RUN) {
		if (cur == NULL) ui->Enable(0);
	}
	else if (ui->m_nID == ID_FILE_PROPERTIES) {
		if (cur == NULL) ui->Enable(0);
	}
	else if (ui->m_nID == ID_FILE_REFRESH) {
		CFolderTree *ft = (CFolderTree *)theApp.m_document;
		if (ft == NULL || ft->GetRoot() == NULL) ui->Enable(0);
	}
	else if (ui->m_nID == ID_VIEW_FREE) {
		CFolderTree *ft = (CFolderTree *)theApp.m_document;
		ui->SetCheck(fv->showfreespace);
		if (ft == NULL || ft->GetRoot() == NULL) ui->Enable(0);
	}
	else if (ui->m_nID == ID_VIEW_ZOOM_IN) {
		if (!fv->CanZoomIn()) ui->Enable(0);
	}
	else if (ui->m_nID == ID_VIEW_ZOOM_OUT || ui->m_nID == ID_VIEW_ZOOM_FULL) {
		if (zoom == 0) ui->Enable(0);
	}
}

/////////////////////////////////////////////////////////////////////////////
//  Application Settings
//

void CCurrentSettings::Reset(void)
{
	density = 0;
	file_color = 0;
	folder_color = 0;
	auto_rescan = 0;
	animated_zoom = 1;
	disable_delete = 0;
	bias = 0;
	show_name_tips = 1;
	nametip_delay = 125;
	show_info_tips = 1;
	rollover_box = 0;
	infotip_flags = TIP_DATE|TIP_SIZE|TIP_ICON;
	infotip_delay = 250;
	rect.left = rect.right = rect.top = rect.bottom = CW_USEDEFAULT;
	showcmd = SW_SHOWNORMAL;
	save_pos = 0;
	lang[0] = 'a';	// "au": follow the Windows display language
	lang[1] = 'u';
	lang[2] = '\0';
	lang[3] = '\0';
	hide_system = 0;
	color_scheme = 0;
	theme = 0;
}

void CCurrentSettings::Load(void)
{
	HKEY key;
	Reset();
	if (RegOpenKey(HKEY_CURRENT_USER, L"Software\\65\\SpaceMonger\\1.3", &key) != ERROR_SUCCESS)
		return;

	DWORD size = sizeof(density);
	DWORD type;
	RegQueryValueEx(key, L"density",				NULL, &type, (unsigned char *)&density, &size);
	size = sizeof(file_color);
	RegQueryValueEx(key, L"file_color",			NULL, &type, (unsigned char *)&file_color, &size);
	size = sizeof(folder_color);
	RegQueryValueEx(key, L"folder_color",		NULL, &type, (unsigned char *)&folder_color, &size);
	size = sizeof(auto_rescan);
	RegQueryValueEx(key, L"auto_rescan",			NULL, &type, (unsigned char *)&auto_rescan, &size);
	size = sizeof(animated_zoom);
	RegQueryValueEx(key, L"animated_zoom",		NULL, &type, (unsigned char *)&animated_zoom, &size);
	size = sizeof(disable_delete);
	RegQueryValueEx(key, L"disable_delete",		NULL, &type, (unsigned char *)&disable_delete, &size);
	size = sizeof(save_pos);
	RegQueryValueEx(key, L"save_pos",			NULL, &type, (unsigned char *)&save_pos, &size);
	size = sizeof(bias);
	RegQueryValueEx(key, L"bias",				NULL, &type, (unsigned char *)&bias, &size);
	size = sizeof(show_name_tips);
	RegQueryValueEx(key, L"show_name_tips",		NULL, &type, (unsigned char *)&show_name_tips, &size);
	size = sizeof(rollover_box);
	RegQueryValueEx(key, L"show_rollover_box",	NULL, &type, (unsigned char *)&rollover_box, &size);
	size = sizeof(hide_system);
	RegQueryValueEx(key, L"hide_system_folders",	NULL, &type, (unsigned char *)&hide_system, &size);
	size = sizeof(show_info_tips);
	RegQueryValueEx(key, L"show_info_tips",		NULL, &type, (unsigned char *)&show_info_tips, &size);
	size = sizeof(nametip_delay);
	RegQueryValueEx(key, L"nametip_delay",		NULL, &type, (unsigned char *)&nametip_delay, &size);
	size = sizeof(infotip_flags);
	RegQueryValueEx(key, L"infotip_flags",		NULL, &type, (unsigned char *)&infotip_flags, &size);
	size = sizeof(infotip_delay);
	RegQueryValueEx(key, L"infotip_delay",		NULL, &type, (unsigned char *)&infotip_delay, &size);
	size = sizeof(rect.left);
	RegQueryValueEx(key, L"window_rect.left",	NULL, &type, (unsigned char *)&rect.left, &size);
	size = sizeof(rect.top);
	RegQueryValueEx(key, L"window_rect.top",		NULL, &type, (unsigned char *)&rect.top, &size);
	size = sizeof(rect.right);
	RegQueryValueEx(key, L"window_rect.right",	NULL, &type, (unsigned char *)&rect.right, &size);
	size = sizeof(rect.bottom);
	RegQueryValueEx(key, L"window_rect.bottom",	NULL, &type, (unsigned char *)&rect.bottom, &size);
	size = sizeof(showcmd);
	RegQueryValueEx(key, L"window_showcmd",		NULL, &type, (unsigned char *)&showcmd, &size);
	size = sizeof(lang);
	RegQueryValueExA(key, "lang",				NULL, &type, (unsigned char *)&lang, &size);
	size = sizeof(color_scheme);
	RegQueryValueEx(key, L"color_scheme",		NULL, &type, (unsigned char *)&color_scheme, &size);
	size = sizeof(theme);
	RegQueryValueEx(key, L"theme",				NULL, &type, (unsigned char *)&theme, &size);
	// Settings before 1.4.6 stored "us" without anyone choosing it; from
	// then on the language follows Windows unless picked in Settings.
	DWORD settingsVersion = 0;
	size = sizeof(settingsVersion);
	if (RegQueryValueEx(key, L"settings_version", NULL, &type, (unsigned char *)&settingsVersion, &size) != ERROR_SUCCESS
		|| settingsVersion < 2) {
		lang[0] = 'a'; lang[1] = 'u';
		if (folder_color == 1) color_scheme = 2;	// was "Windows Colors"
	}
	RegCloseKey(key);

	if (density < -3) density = -3;
	if (density > 3) density = 3;

	if (file_color < 0 || file_color > 12) file_color = 0;
	if (folder_color < 0 || folder_color > 12) folder_color = 0;

	auto_rescan = !!auto_rescan;
	animated_zoom = !!animated_zoom;
	disable_delete = !!disable_delete;
	save_pos = !!save_pos;

	rollover_box = !!rollover_box;
	hide_system = !!hide_system;

	show_name_tips = !!show_name_tips;
	if (nametip_delay < 0) nametip_delay = 0;
	if (nametip_delay > 99999) nametip_delay = 99999;

	show_info_tips = !!show_info_tips;
	infotip_flags &= TIP_PATH|TIP_NAME|TIP_ICON|TIP_DATE|TIP_SIZE|TIP_ATTRIB;
	if (infotip_delay < 0) infotip_delay = 0;
	if (infotip_delay > 99999) infotip_delay = 99999;

	// Options that are no longer settings: always on / fixed.
	bias = 0;
	auto_rescan = 1;
	animated_zoom = 1;
	save_pos = 1;
	show_name_tips = show_info_tips;
	nametip_delay = 125;
	infotip_delay = 400;
	infotip_flags = TIP_PATH|TIP_NAME|TIP_ICON|TIP_DATE|TIP_SIZE;
	if (density < -1) density = -1;
	if (density > 2) density = 2;
	if (color_scheme < 0 || color_scheme > 3) color_scheme = 0;
	if (theme < 0 || theme > 2) theme = 0;

	// Ensure that the window's provided rectangle has at least a little of
	// its area visible to the end-user.
	int scrnwidth = GetSystemMetrics(SM_CXSCREEN), scrnheight = GetSystemMetrics(SM_CYSCREEN);
	if (rect.top > rect.bottom) {
		int temp = rect.top;
		rect.top = rect.bottom;
		rect.bottom = temp;
	}
	if (rect.left > rect.right) {
		int temp = rect.left;
		rect.left = rect.right;
		rect.right = temp;
	}
	if (rect.right - rect.left > scrnwidth) rect.right = rect.left + scrnwidth;
	if (rect.bottom - rect.top > scrnheight) rect.bottom = rect.top + scrnheight;
	if (rect.top > scrnheight-8)
		rect.bottom -= (rect.top - (scrnheight-8)), rect.top = scrnheight-8;
	if (rect.left > scrnwidth-8)
		rect.right -= (rect.left - (scrnwidth-8)), rect.left = scrnwidth-8;
	if (rect.right < 8)
		rect.left += (8-rect.right), rect.right = 8;
	if (rect.bottom < 8)
		rect.top += (8-rect.bottom), rect.bottom = 8;

	// Ensure that the window's placement is comprehensible.
	if (showcmd != SW_SHOWMINIMIZED && showcmd != SW_SHOWMAXIMIZED
		&& showcmd != SW_SHOWNORMAL)
		showcmd = SW_SHOWNORMAL;

	lang[0] = (char)tolower((unsigned char)lang[0]);
	lang[1] = (char)tolower((unsigned char)lang[1]);
	lang[2] = '\0';
	lang[3] = '\0';
	CurLang = SM_ResolveLanguage(lang);
}

// "au" (or anything unknown) follows the Windows display language:
// German, French, British/Australian English, otherwise US English.
Lang *SM_ResolveLanguage(const char *code)
{
	char wanted[3] = { code[0], code[1], '\0' };
	if (wanted[0] == 'a' && wanted[1] == 'u') {
		LANGID ui = ::GetUserDefaultUILanguage();
		switch (PRIMARYLANGID(ui)) {
		case LANG_GERMAN: wanted[0] = 'd'; wanted[1] = 'e'; break;
		case LANG_FRENCH: wanted[0] = 'f'; wanted[1] = 'r'; break;
		case LANG_ENGLISH:
			if (SUBLANGID(ui) == SUBLANG_ENGLISH_UK || SUBLANGID(ui) == SUBLANG_ENGLISH_AUS
				|| SUBLANGID(ui) == SUBLANG_ENGLISH_NZ || SUBLANGID(ui) == SUBLANG_ENGLISH_EIRE) {
				wanted[0] = 'u'; wanted[1] = 'k';
			} else {
				wanted[0] = 'u'; wanted[1] = 's';
			}
			break;
		default: wanted[0] = 'u'; wanted[1] = 's'; break;
		}
	}
	for (int i = 0; Langs[i].name != NULL; i++) {
		if (wanted[0] == Langs[i].abbrev[0] && wanted[1] == Langs[i].abbrev[1])
			return Langs[i].text;
	}
	return Langs[0].text;
}

void CCurrentSettings::Save(void)
{
	HKEY key;
	if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\65\\SpaceMonger\\1.3", &key) != ERROR_SUCCESS)
		return;
	DWORD size;
	size = sizeof(density);
	RegSetValueEx(key, L"density",			NULL, REG_DWORD, (unsigned char *)&density, size);
	size = sizeof(file_color);
	RegSetValueEx(key, L"file_color",		NULL, REG_DWORD, (unsigned char *)&file_color, size);
	size = sizeof(folder_color);
	RegSetValueEx(key, L"folder_color",		NULL, REG_DWORD, (unsigned char *)&folder_color, size);
	size = sizeof(auto_rescan);
	RegSetValueEx(key, L"auto_rescan",		NULL, REG_DWORD, (unsigned char *)&auto_rescan, size);
	size = sizeof(animated_zoom);
	RegSetValueEx(key, L"animated_zoom",		NULL, REG_DWORD, (unsigned char *)&animated_zoom, size);
	size = sizeof(disable_delete);
	RegSetValueEx(key, L"disable_delete",	NULL, REG_DWORD, (unsigned char *)&disable_delete, size);
	size = sizeof(save_pos);
	RegSetValueEx(key, L"save_pos",			NULL, REG_DWORD, (unsigned char *)&save_pos, size);
	size = sizeof(bias);
	RegSetValueEx(key, L"bias",				NULL, REG_DWORD, (unsigned char *)&bias, size);
	size = sizeof(show_name_tips);
	RegSetValueEx(key, L"show_name_tips",	NULL, REG_DWORD, (unsigned char *)&show_name_tips, size);
	size = sizeof(rollover_box);
	RegSetValueEx(key, L"show_rollover_box",	NULL, REG_DWORD, (unsigned char *)&rollover_box, size);
	size = sizeof(hide_system);
	RegSetValueEx(key, L"hide_system_folders",	NULL, REG_DWORD, (unsigned char *)&hide_system, size);
	size = sizeof(show_info_tips);
	RegSetValueEx(key, L"show_info_tips",	NULL, REG_DWORD, (unsigned char *)&show_info_tips, size);
	size = sizeof(nametip_delay);
	RegSetValueEx(key, L"nametip_delay",		NULL, REG_DWORD, (unsigned char *)&nametip_delay, size);
	size = sizeof(infotip_flags);
	RegSetValueEx(key, L"infotip_flags",		NULL, REG_DWORD, (unsigned char *)&infotip_flags, size);
	size = sizeof(infotip_delay);
	RegSetValueEx(key, L"infotip_delay",		NULL, REG_DWORD, (unsigned char *)&infotip_delay, size);
	size = sizeof(rect.left);
	RegSetValueEx(key, L"window_rect.left",	NULL, REG_DWORD, (unsigned char *)&rect.left, size);
	size = sizeof(rect.top);
	RegSetValueEx(key, L"window_rect.top",	NULL, REG_DWORD, (unsigned char *)&rect.top, size);
	size = sizeof(rect.right);
	RegSetValueEx(key, L"window_rect.right",	NULL, REG_DWORD, (unsigned char *)&rect.right, size);
	size = sizeof(rect.bottom);
	RegSetValueEx(key, L"window_rect.bottom",NULL, REG_DWORD, (unsigned char *)&rect.bottom, size);
	size = sizeof(showcmd);
	RegSetValueEx(key, L"window_showcmd",	NULL, REG_DWORD, (unsigned char *)&showcmd, size);
	size = sizeof(lang);
	RegSetValueExA(key, "lang",				NULL, REG_SZ, (unsigned char *)&lang, 3);
	size = sizeof(color_scheme);
	RegSetValueEx(key, L"color_scheme",		NULL, REG_DWORD, (unsigned char *)&color_scheme, size);
	size = sizeof(theme);
	RegSetValueEx(key, L"theme",				NULL, REG_DWORD, (unsigned char *)&theme, size);
	DWORD settingsVersion = 2;
	RegSetValueEx(key, L"settings_version",	NULL, REG_DWORD, (unsigned char *)&settingsVersion, sizeof(settingsVersion));
	RegCloseKey(key);
}
