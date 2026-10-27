#include "stdafx.h"
#include "spacemonger.h"
#include "CleanupDialog.h"
#include "FolderTree.h"
#include "TextFormat.h"
#include "Lang.h"
#include <shlobj.h>
#include <algorithm>

namespace {

const UINT WM_CLEANUP_MEASURED = WM_APP + 0x51;
const UINT WM_CLEANUP_FINISHED = WM_APP + 0x52;
const UINT_PTR kCleanTimer = 1;
const int kProgressRange = 1000;
const ui64 kLargeFileMinimum = 200ull * 1024 * 1024;
const size_t kLargeFileCount = 25;
// Extra height (dialog units) the Details list takes when shown.
const int kDetailsHeight = 90;

std::wstring FileName(const std::wstring &path)
{
	size_t slash = path.find_last_of(L'\\');
	return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring FolderOf(const std::wstring &path)
{
	size_t slash = path.find_last_of(L'\\');
	return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

void ShowInExplorer(const std::wstring &path)
{
	PIDLIST_ABSOLUTE item = ILCreateFromPathW(path.c_str());
	if (item != NULL) {
		SHOpenFolderAndSelectItems(item, 0, NULL, 0);
		ILFree(item);
	}
}

}

BEGIN_MESSAGE_MAP(CCleanupDialog, CDialog)
	ON_BN_CLICKED(IDC_CLEANUP_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_CLEANUP_OPEN, OnOpenFolder)
	ON_BN_CLICKED(IDC_CLEANUP_DETAILS, OnDetails)
	ON_BN_CLICKED(IDC_CLEANUP_DELETE_FILES, OnRecycleFiles)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CLEANUP_LARGE, OnLargeChanged)
	ON_NOTIFY(LVN_GETINFOTIPW, IDC_CLEANUP_LARGE, OnLargeInfoTip)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_CLEANUP_LIST, OnTargetChanged)
	ON_NOTIFY(NM_DBLCLK, IDC_CLEANUP_LARGE, OnLargeDblClk)
	ON_MESSAGE(WM_CLEANUP_MEASURED, OnMeasured)
	ON_MESSAGE(WM_CLEANUP_FINISHED, OnCleanFinished)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

CCleanupDialog::CCleanupDialog(CFolderTree *tree, CWnd *parent)
	: CDialog(IDD_CLEANUP, parent)
{
	m_tree = tree;
	m_filling = FALSE;
	m_freedAnything = FALSE;
	m_detailsShown = FALSE;
	m_cancel = false;
	m_pending = 0;
	m_generation = 0;
	m_cleaning = FALSE;
	m_closeAfterClean = FALSE;
	m_cleanTotal = 0;
	m_cleanCurrent = 0;
	m_cleanMarked = 0;
}

CCleanupDialog::~CCleanupDialog()
{
	StopMeasuring();
	StopCleaning();
}

void CCleanupDialog::StopMeasuring(void)
{
	m_cancel = true;
	if (m_measurer.joinable()) m_measurer.join();
	m_cancel = false;
}

// Asks a running clean-up to stop and waits for the worker (it checks between
// files, so this takes at most one delete).
void CCleanupDialog::StopCleaning(void)
{
	m_cleanProgress.cancel.store(true);
	if (m_cleaner.joinable()) m_cleaner.join();
}

void CCleanupDialog::OnDestroy()
{
	StopMeasuring();
	StopCleaning();
	CDialog::OnDestroy();
}

// Closing while files are being removed stops the clean-up first and closes
// when the worker reports back, instead of pulling the dialog away from it.
void CCleanupDialog::OnCancel()
{
	if (m_cleaning) {
		m_closeAfterClean = TRUE;
		m_cleanProgress.cancel.store(true);
		GetDlgItem(IDC_CLEANUP_DELETE)->EnableWindow(FALSE);
		return;
	}
	CDialog::OnCancel();
}

BOOL CCleanupDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetWindowText(CurLang->cleanup_title);
	SetDlgItemText(IDC_CLEANUP_TEMP_HEADER, CurLang->cleanup_measuring);
	SetDlgItemText(IDC_CLEANUP_LARGE_HEADER, CurLang->cleanup_large_header);
	SetDlgItemText(IDC_CLEANUP_DELETE, CurLang->cleanup_delete);
	SetDlgItemText(IDC_CLEANUP_DETAILS, CurLang->cleanup_details);
	SetDlgItemText(IDC_CLEANUP_OPEN, CurLang->cleanup_open);
	SetDlgItemText(IDCANCEL, CurLang->cleanup_close);
	SetDlgItemText(IDC_CLEANUP_TOTAL, L"");
	SetDlgItemText(IDC_CLEANUP_DELETE_FILES, CurLang->cleanup_recycle);
	SetDlgItemText(IDC_CLEANUP_MOVED, L"");
	GetDlgItem(IDC_CLEANUP_DELETE_FILES)->EnableWindow(FALSE);

	m_bar.SubclassDlgItem(IDC_CLEANUP_PROGRESS, this);
	m_bar.SetRange32(0, kProgressRange);
	m_bar.ShowWindow(SW_HIDE);

	m_targets.SubclassDlgItem(IDC_CLEANUP_LIST, this);
	m_large.SubclassDlgItem(IDC_CLEANUP_LARGE, this);
	m_targets.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	// Check boxes: pick several big files and move them away in one go.
	m_large.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	// Keep the path tip up long enough to read a long path.
	CToolTipCtrl *tips = m_large.GetToolTips();
	if (tips != NULL) tips->SetDelayTime(TTDT_AUTOPOP, 20000);

	// Column widths as shares of the list width, so they follow the DPI.
	CRect rect;
	m_targets.GetClientRect(&rect);
	int w = rect.Width() - ::GetSystemMetrics(SM_CXVSCROLL);
	m_targets.InsertColumn(0, CurLang->cleanup_col_item, LVCFMT_LEFT, w * 38 / 100);
	m_targets.InsertColumn(1, CurLang->cleanup_col_size, LVCFMT_RIGHT, w * 14 / 100);
	m_targets.InsertColumn(2, CurLang->cleanup_col_location, LVCFMT_LEFT, w * 48 / 100);
	m_large.GetClientRect(&rect);
	w = rect.Width() - ::GetSystemMetrics(SM_CXVSCROLL);
	m_large.InsertColumn(0, CurLang->cleanup_col_item, LVCFMT_LEFT, w * 36 / 100);
	m_large.InsertColumn(1, CurLang->cleanup_col_size, LVCFMT_RIGHT, w * 12 / 100);
	m_large.InsertColumn(2, CurLang->cleanup_col_hint, LVCFMT_LEFT, w * 52 / 100);

	// Details start hidden: the one-button path is what most people need.
	m_targets.ShowWindow(SW_HIDE);

	FillTargets();
	FillLargeFiles();
	GetDlgItem(IDC_CLEANUP_DELETE)->SetFocus();
	return FALSE;
}

int CCleanupDialog::RowOf(size_t index)
{
	for (int i = 0; i < m_targets.GetItemCount(); ++i)
		if (m_targets.GetItemData(i) == (DWORD_PTR)index) return i;
	return -1;
}

void CCleanupDialog::FillTargets(void)
{
	StopMeasuring();
	m_filling = TRUE;
	m_targets.DeleteAllItems();
	m_items = SM_FindCleanupTargets();
	for (size_t i = 0; i < m_items.size(); ++i) {
		SM_CleanupTarget &target = m_items[i];
		int index = m_targets.InsertItem((int)i, CurLang->cleanup_kind_labels[target.kind]);
		m_targets.SetItemData(index, (DWORD_PTR)i);
		m_targets.SetItemText(index, 1, L"…");	// measuring
		CString location;
		for (size_t p = 0; p < target.paths.size(); ++p) {
			if (p > 0) location += L";  ";
			location += target.paths[p].c_str();
		}
		m_targets.SetItemText(index, 2, location);
		m_targets.SetCheck(index, target.defaultChecked);
	}
	m_filling = FALSE;

	// Measure on a worker; each finished row is posted back to the dialog.
	m_measured = m_items;
	m_pending = m_items.size();
	m_bar.SetPos(0);
	m_bar.ShowWindow(SW_SHOW);
	HWND self = m_hWnd;
	LPARAM generation = ++m_generation;
	m_measurer = std::thread([this, self, generation]() {
		for (size_t i = 0; i < m_measured.size() && !m_cancel; ++i) {
			SM_CleanupTarget target;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				target = m_measured[i];
			}
			SM_MeasureCleanupTarget(target, &m_cancel);
			if (m_cancel) break;
			{
				std::lock_guard<std::mutex> guard(m_lock);
				m_measured[i] = target;
			}
			::PostMessageW(self, WM_CLEANUP_MEASURED, (WPARAM)i, generation);
		}
	});
	UpdateSummary();
}

LRESULT CCleanupDialog::OnMeasured(WPARAM index, LPARAM generation)
{
	if (generation != m_generation || index >= m_items.size()) return 0;
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_items[index].bytes = m_measured[index].bytes;
		m_items[index].files = m_measured[index].files;
	}
	if (m_pending > 0) --m_pending;
	if (!m_cleaning) {
		size_t measured = m_items.size() - m_pending;
		m_bar.SetPos((int)(measured * kProgressRange / m_items.size()));
		if (m_pending == 0) m_bar.ShowWindow(SW_HIDE);
	}
	int row = RowOf(index);
	if (row >= 0) {
		const SM_CleanupTarget &target = m_items[index];
		m_filling = TRUE;
		m_targets.SetItemText(row, 1, SM_FormatBytes(target.bytes).c_str());
		if (target.bytes == 0) m_targets.SetCheck(row, FALSE);
		m_filling = FALSE;
	}
	UpdateSummary();
	return 0;
}

ui64 CCleanupDialog::CheckedBytes(void)
{
	ui64 total = 0;
	for (int i = 0; i < m_targets.GetItemCount(); ++i) {
		if (m_targets.GetCheck(i)) total += m_items[m_targets.GetItemData(i)].bytes;
	}
	return total;
}

// One sentence on top says what one click will free.
void CCleanupDialog::UpdateSummary(void)
{
	if (m_cleaning) return;	// the clean-up shows its own status
	ui64 total = CheckedBytes();
	CString text;
	if (m_pending > 0)
		text.Format(CurLang->cleanup_measuring_format, (int)(m_items.size() - m_pending), (int)m_items.size());
	else if (total == 0) text = CurLang->cleanup_nothing;
	else text.Format(CurLang->cleanup_summary_format, SM_FormatBytes(total).c_str());
	SetDlgItemText(IDC_CLEANUP_TEMP_HEADER, text);
	BOOL ready = total > 0 && m_pending == 0;
	CWnd *button = GetDlgItem(IDC_CLEANUP_DELETE);
	BOOL wasEnabled = button->IsWindowEnabled();
	button->EnableWindow(ready);
	// Once measuring is done, the one button that matters gets the focus.
	if (ready && !wasEnabled) {
		SetDefID(IDC_CLEANUP_DELETE);
		button->SetFocus();
	}
}

void CCleanupDialog::OnTargetChanged(NMHDR *pNotify, LRESULT *pResult)
{
	NMLISTVIEW *change = (NMLISTVIEW *)pNotify;
	// The check box lives in the state image bits.
	if (!m_filling && (change->uChanged & LVIF_STATE)
		&& ((change->uNewState ^ change->uOldState) & LVIS_STATEIMAGEMASK))
		UpdateSummary();
	*pResult = 0;
}

// The button says what it does; no extra confirmation. Only safe items are
// preselected, and recent or in-use files are left alone anyway.
void CCleanupDialog::OnDelete()
{
	// While cleaning, the same button stops it.
	if (m_cleaning) {
		m_cleanProgress.cancel.store(true);
		GetDlgItem(IDC_CLEANUP_DELETE)->EnableWindow(FALSE);
		return;
	}

	std::vector<size_t> chosen;
	for (int i = 0; i < m_targets.GetItemCount(); ++i) {
		if (m_targets.GetCheck(i)) chosen.push_back(m_targets.GetItemData(i));
	}
	if (chosen.empty()) return;
	StopMeasuring();

	std::vector<SM_CleanupTarget> targets;
	m_cleanTotal = 0;
	for (size_t index : chosen) {
		targets.push_back(m_items[index]);
		m_cleanTotal += m_items[index].bytes;
	}
	m_cleanChosen = chosen;
	m_cleanProgress.processedBytes.store(0);
	m_cleanProgress.freedBytes.store(0);
	m_cleanProgress.deletedFiles.store(0);
	m_cleanProgress.skippedFiles.store(0);
	m_cleanProgress.cancel.store(false);
	m_cleanCurrent.store(0);
	m_cleanMarked = 0;
	m_closeAfterClean = FALSE;
	m_lastStatus.Empty();
	m_cleaning = TRUE;
	SetBusy(TRUE);
	m_bar.SetPos(0);
	m_bar.ShowWindow(SW_SHOW);
	SetTimer(kCleanTimer, 150, NULL);

	HWND self = m_hWnd;
	m_cleaner = std::thread([this, self, targets]() {
		for (size_t k = 0; k < targets.size() && !m_cleanProgress.cancel.load(); ++k) {
			m_cleanCurrent.store(k);
			SM_CleanTarget(targets[k], &m_cleanProgress);
		}
		::PostMessageW(self, WM_CLEANUP_FINISHED, 0, 0);
	});
	ShowCleanProgress();
}

// The dialog stays usable while the worker runs, but nothing that would change
// the lists under it: only Stop (the delete button) and Close work.
void CCleanupDialog::SetBusy(BOOL busy)
{
	m_targets.EnableWindow(!busy);
	m_large.EnableWindow(!busy);
	GetDlgItem(IDC_CLEANUP_DETAILS)->EnableWindow(!busy);
	GetDlgItem(IDC_CLEANUP_OPEN)->EnableWindow(!busy && !m_files.empty());
	SetDlgItemText(IDC_CLEANUP_DELETE, busy ? CurLang->cleanup_stop : CurLang->cleanup_delete);
	GetDlgItem(IDC_CLEANUP_DELETE)->EnableWindow(busy);
	if (busy) {
		GetDlgItem(IDC_CLEANUP_DELETE_FILES)->EnableWindow(FALSE);
	} else {
		LRESULT unused = 0;
		OnLargeChanged(NULL, &unused);
	}
}

void CCleanupDialog::ShowCleanProgress(void)
{
	if (m_cleanChosen.empty()) return;
	ui64 processed = m_cleanProgress.processedBytes.load();
	ui64 shown = (std::min)(processed, m_cleanTotal);
	m_bar.SetPos(m_cleanTotal > 0 ? (int)(shown * kProgressRange / m_cleanTotal) : 0);

	size_t current = (std::min)(m_cleanCurrent.load(), m_cleanChosen.size() - 1);
	// Finished locations say so; the one being emptied is marked.
	m_filling = TRUE;
	for (; m_cleanMarked < current; ++m_cleanMarked) {
		int row = RowOf(m_cleanChosen[m_cleanMarked]);
		if (row >= 0) m_targets.SetItemText(row, 1, CurLang->cleanup_row_done);
	}
	int row = RowOf(m_cleanChosen[current]);
	if (row >= 0) m_targets.SetItemText(row, 1, CurLang->cleanup_row_cleaning);
	m_filling = FALSE;

	CString status, line;
	status.Format(CurLang->cleanup_running_format,
		CurLang->cleanup_kind_labels[m_items[m_cleanChosen[current]].kind],
		(int)current + 1, (int)m_cleanChosen.size());
	line.Format(CurLang->cleanup_progress_format, SM_FormatBytes(shown).c_str(),
		SM_FormatBytes(m_cleanTotal).c_str());
	status += L"\r\n" + line;
	if (status != m_lastStatus) {
		m_lastStatus = status;
		SetDlgItemText(IDC_CLEANUP_TOTAL, status);
	}
}

void CCleanupDialog::OnTimer(UINT_PTR id)
{
	if (id == kCleanTimer && m_cleaning) ShowCleanProgress();
	else CDialog::OnTimer(id);
}

LRESULT CCleanupDialog::OnCleanFinished(WPARAM, LPARAM)
{
	if (m_cleaner.joinable()) m_cleaner.join();
	KillTimer(kCleanTimer);
	m_cleaning = FALSE;
	m_bar.ShowWindow(SW_HIDE);

	const bool stopped = m_cleanProgress.cancel.load();
	const ui64 freed = m_cleanProgress.freedBytes.load();
	if (m_cleanProgress.deletedFiles.load() > 0) m_freedAnything = TRUE;
	if (m_closeAfterClean) {
		CDialog::OnCancel();
		return 0;
	}
	SetBusy(FALSE);

	CString done;
	done.Format(stopped ? CurLang->cleanup_stopped_format : CurLang->cleanup_done_format,
		SM_FormatBytes(freed).c_str());
	SetDlgItemText(IDC_CLEANUP_TOTAL, done);
	FillTargets();
	return 0;
}

void CCleanupDialog::OnDetails()
{
	m_detailsShown = !m_detailsShown;
	m_targets.ShowWindow(m_detailsShown ? SW_SHOW : SW_HIDE);

	// Push the lower half down (or back up) by the list's height.
	CRect units(0, 0, 0, kDetailsHeight);
	MapDialogRect(&units);
	int dy = m_detailsShown ? units.bottom : -units.bottom;
	static const int below[] = { IDC_CLEANUP_SEP, IDC_CLEANUP_LARGE_HEADER, IDC_CLEANUP_LARGE,
		IDC_CLEANUP_OPEN, IDC_CLEANUP_DELETE_FILES, IDC_CLEANUP_MOVED, IDCANCEL };
	for (int id : below) {
		CWnd *control = GetDlgItem(id);
		if (control == NULL) continue;
		CRect r;
		control->GetWindowRect(&r);
		ScreenToClient(&r);
		control->SetWindowPos(NULL, r.left, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	CRect window;
	GetWindowRect(&window);
	SetWindowPos(NULL, 0, 0, window.Width(), window.Height() + dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CCleanupDialog::FillLargeFiles(void)
{
	m_large.DeleteAllItems();
	m_files.clear();
	CFolder *root = m_tree != NULL ? m_tree->GetRoot() : NULL;
	if (root == NULL) {
		int index = m_large.InsertItem(0, CurLang->cleanup_no_scan);
		m_large.SetItemData(index, (DWORD_PTR)-1);
		GetDlgItem(IDC_CLEANUP_OPEN)->EnableWindow(FALSE);
		return;
	}
	// The clean-up targets are offered above; don't list their files twice.
	std::vector<std::wstring> excluded;
	for (const auto &target : m_items)
		for (const auto &path : target.paths) excluded.push_back(path);
	// Files that apps manage themselves (thumbnail cache, Outlook's offline
	// mailbox): deleting them by hand frees nothing for long or breaks the app.
	static const wchar_t *const managed[] = {
		L"%LOCALAPPDATA%\\Microsoft\\Windows\\Explorer",
		L"%LOCALAPPDATA%\\Microsoft\\Outlook",
	};
	for (const wchar_t *folder : managed) {
		wchar_t expanded[MAX_PATH];
		if (::ExpandEnvironmentStringsW(folder, expanded, MAX_PATH) > 0) excluded.push_back(expanded);
	}
	std::wstring rootPath = (LPCWSTR)m_tree->m_path;
	m_files = SM_FindLargePersonalFiles(root, rootPath, kLargeFileCount, kLargeFileMinimum, excluded);
	// Files already moved away during this dialog are still in the scan data.
	m_files.erase(std::remove_if(m_files.begin(), m_files.end(), [](const SM_LargeFile &f) {
		return ::GetFileAttributesW(f.path.c_str()) == INVALID_FILE_ATTRIBUTES;
	}), m_files.end());
	for (size_t i = 0; i < m_files.size(); ++i) {
		const SM_LargeFile &file = m_files[i];
		int index = m_large.InsertItem((int)i, FileName(file.path).c_str());
		m_large.SetItemData(index, (DWORD_PTR)i);
		m_large.SetCheck(index, FALSE);	// personal files: never preselected
		m_large.SetItemText(index, 1, SM_FormatBytes(file.bytes).c_str());
		CString hint = CurLang->cleanup_hint_labels[file.kind];
		if (hint.IsEmpty()) hint = FolderOf(file.path).c_str();
		m_large.SetItemText(index, 2, hint);
	}
	if (!m_files.empty()) m_large.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	GetDlgItem(IDC_CLEANUP_OPEN)->EnableWindow(!m_files.empty());
}

// Hovering a file shows where it is: the full path, so nobody has to guess
// which "video.mp4" a row means before moving it to the Recycle Bin.
void CCleanupDialog::OnLargeInfoTip(NMHDR *pNotify, LRESULT *pResult)
{
	NMLVGETINFOTIPW *tip = (NMLVGETINFOTIPW *)pNotify;
	*pResult = 0;
	if (tip->iItem < 0 || tip->pszText == NULL || tip->cchTextMax <= 0) return;
	size_t index = (size_t)m_large.GetItemData(tip->iItem);
	if (index >= m_files.size()) return;
	wcsncpy_s(tip->pszText, tip->cchTextMax, m_files[index].path.c_str(), _TRUNCATE);
}

void CCleanupDialog::OnLargeChanged(NMHDR *pNotify, LRESULT *pResult)
{
	BOOL any = FALSE;
	for (int i = 0; i < m_large.GetItemCount() && !any; ++i) {
		if (m_large.GetCheck(i) && (size_t)m_large.GetItemData(i) < m_files.size()) any = TRUE;
	}
	GetDlgItem(IDC_CLEANUP_DELETE_FILES)->EnableWindow(any && !m_cleaning);
	*pResult = 0;
}

// Moves the checked files to the Recycle Bin in one operation: personal
// files stay recoverable, and the Recycle Bin is part of "Clean up now", so
// the space they take shows up there right away. Windows itself asks before
// deleting a file that is too big for the Recycle Bin.
void CCleanupDialog::OnRecycleFiles()
{
	std::wstring list;
	std::vector<size_t> chosen;
	for (int i = 0; i < m_large.GetItemCount(); ++i) {
		size_t index = (size_t)m_large.GetItemData(i);
		if (!m_large.GetCheck(i) || index >= m_files.size()) continue;
		list += m_files[index].path;
		list.push_back(L'\0');
		chosen.push_back(index);
	}
	if (chosen.empty()) return;
	list.push_back(L'\0');

	SHFILEOPSTRUCTW op;
	memset(&op, 0, sizeof(op));
	op.hwnd = m_hWnd;
	op.wFunc = FO_DELETE;
	op.pFrom = list.c_str();
	op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING;
	{
		CWaitCursor wait;
		::SHFileOperationW(&op);
	}

	ui64 moved = 0;
	for (size_t index : chosen) {
		if (::GetFileAttributesW(m_files[index].path.c_str()) == INVALID_FILE_ATTRIBUTES)
			moved += m_files[index].bytes;
	}
	if (moved > 0) {
		m_freedAnything = TRUE;
		CString text;
		text.Format(CurLang->cleanup_recycled_format, SM_FormatBytes(moved).c_str());
		SetDlgItemText(IDC_CLEANUP_MOVED, text);
	}
	FillLargeFiles();
	GetDlgItem(IDC_CLEANUP_DELETE_FILES)->EnableWindow(FALSE);
	FillTargets();	// the Recycle Bin above now holds them
}

void CCleanupDialog::OnOpenFolder()
{
	int selected = m_large.GetNextItem(-1, LVNI_SELECTED);
	if (selected < 0 || (size_t)m_large.GetItemData(selected) >= m_files.size()) return;
	ShowInExplorer(m_files[m_large.GetItemData(selected)].path);
}

void CCleanupDialog::OnLargeDblClk(NMHDR *pNotify, LRESULT *pResult)
{
	OnOpenFolder();
	*pResult = 0;
}
