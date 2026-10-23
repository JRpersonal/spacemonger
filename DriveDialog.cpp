#include "stdafx.h"
#include "spacemonger.h"
#include "DriveDialog.h"
#include "Lang.h"
#include "DpiUtil.h"
#include <commoncontrols.h>
#include <shobjidl.h>

namespace {

int GetDriveIndex(CListCtrl *list, int item)
{
	const DWORD_PTR driveIndex = list->GetItemData(item);
	if (driveIndex > static_cast<DWORD_PTR>('Z' - 'A')) {
		ASSERT(FALSE);
		return -1;
	}
	return static_cast<int>(driveIndex);
}

}

CDriveDialog::CDriveDialog(CWnd* pParent)
	: CDialog(IDD_OPEN_DIALOG, pParent)
{
}

BEGIN_MESSAGE_MAP(CDriveDialog, CDialog)
	//{{AFX_MSG_MAP(CDriveDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_DRIVE_LIST, OnDriveDblClk)
	ON_BN_CLICKED(IDC_OPEN_FOLDER, OnOpenFolder)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CDriveDialog::OnDriveDblClk(NMHDR *pNotifyStruct, LRESULT *result)
{
	CListCtrl *lc = (CListCtrl *)GetDlgItem(IDC_DRIVE_LIST);
	if (lc == NULL) return;
	if (lc->GetSelectedCount() != 1) return;

	int i, max = lc->GetItemCount();
	for (i = 0; i < max; i++) {
		if (lc->GetItemState(i, LVIS_SELECTED)) {
			EndDialog(GetDriveIndex(lc, i));
		}
	}
}

void CDriveDialog::OnOK()
{
	CListCtrl *lc = (CListCtrl *)GetDlgItem(IDC_DRIVE_LIST);
	if (lc == NULL) { EndDialog(-1); return; }
	if (lc->GetSelectedCount() != 1) return;

	int i, max = lc->GetItemCount();
	for (i = 0; i < max; i++) {
		if (lc->GetItemState(i, LVIS_SELECTED)) {
			EndDialog(GetDriveIndex(lc, i));
		}
	}
}

// Any folder, including network shares: typing \\PC01\c$ into the folder
// box of the picker works too.
void CDriveDialog::OnOpenFolder()
{
	HRESULT init = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	IFileOpenDialog *picker = NULL;
	if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker)))) {
		DWORD options = 0;
		picker->GetOptions(&options);
		picker->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
		IShellItem *item = NULL;
		PWSTR path = NULL;
		if (SUCCEEDED(picker->Show(m_hWnd)) && SUCCEEDED(picker->GetResult(&item))) {
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != NULL) {
				m_folder = path;
				::CoTaskMemFree(path);
			}
			item->Release();
		}
		picker->Release();
	}
	if (SUCCEEDED(init)) ::CoUninitialize();
	if (!m_folder.IsEmpty()) EndDialog(kFolderChosen);
}

void CDriveDialog::OnCancel()
{
	EndDialog(-1);
}

BOOL CDriveDialog::OnInitDialog()
{
	CWaitCursor wait;

	CDialog::OnInitDialog();

	CDriveInfo driveinfo[26];
	HIMAGELIST imagelist;
	int i, index;
	LVITEMW item = {};

	// Drive icons at the dialog's DPI (96 px at 300 %), not a fixed 32 px.
	const int iconSize = MulDiv(32, (int)SM_GetWindowDpi(m_hWnd), 96);
	imagelist = ImageList_Create(iconSize, iconSize, ILC_COLOR32 | ILC_MASK, 0, 26);

	CListCtrl *lc = (CListCtrl *)GetDlgItem(IDC_DRIVE_LIST);
	::SendMessage(lc->m_hWnd, LVM_SETIMAGELIST, (WPARAM)LVSIL_NORMAL, (LPARAM)(HIMAGELIST)imagelist);

	SetWindowText(CurLang->selectdrive);
	SetDlgItemText(IDOK, CurLang->ok);
	SetDlgItemText(IDCANCEL, CurLang->cancel);
	SetDlgItemText(IDC_OPEN_FOLDER, CurLang->open_folder);

	lc->InsertColumn(0, L"Name");
	if (lc != NULL) {
		lc->SetItemCount(26);
		index = 0;
		for (i = 'A'; i <= 'Z'; i++) {
			driveinfo[i-'A'].LoadDriveInfo((char)i, iconSize);
			if (driveinfo[i-'A'].avail) {
				ImageList_AddIcon(imagelist, driveinfo[i-'A'].icon);
				item.mask = LVIF_STATE|LVIF_TEXT|LVIF_IMAGE|LVIF_PARAM;
				item.iItem = index;
				item.iSubItem = 0;
				item.state = 0;
				item.stateMask = LVIS_SELECTED;
				item.pszText = driveinfo[i-'A'].longname;
				item.cchTextMax = (int)wcslen(driveinfo[i-'A'].longname);
				item.iImage = index;
				item.lParam = i - 'A';
				::SendMessageW(lc->m_hWnd, LVM_INSERTITEMW, (WPARAM)0, (LPARAM)(&item));
				index++;
			}
			driveinfo[i-'A'].Free();
		}
		// Preselect the first drive (normally C:) so OK or Enter starts the scan.
		if (index > 0) {
			lc->SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			lc->SetFocus();
			return FALSE;	// focus set explicitly
		}
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

CDriveInfo::CDriveInfo()
	: letter(0), avail(FALSE), icon(NULL), longname(NULL)
{
}

CDriveInfo::~CDriveInfo()
{
	Free();
}

void CDriveInfo::Free(void)
{
	if (longname != NULL) {
		delete[] longname;
		longname = NULL;
	}
	if (icon != NULL) {
		DestroyIcon(icon);
		icon = NULL;
	}
}

void CDriveInfo::LoadDriveInfo(char let, int iconSize)
{
	Free();

	wchar_t name[8];
	UINT type;

	if (let >= 'a' && let <= 'z') let -= 32;
	name[0] = (wchar_t)let;
	name[1] = L':';
	name[2] = L'\\';
	name[3] = L'\0';

	type = GetDriveTypeW(name);

	if (type != 0 && type != 1) {
		SHFILEINFOW fileinfo;
		memset(&fileinfo, 0, sizeof(SHFILEINFO));

		SHGetFileInfoW(name, 0, &fileinfo, sizeof(SHFILEINFOW),
			SHGFI_DISPLAYNAME|SHGFI_ICON|SHGFI_SYSICONINDEX);

		avail = 1;
		letter = let;
		icon = fileinfo.hIcon;
		// SHGFI_ICON is always 32 px; above that take the shell's 48 px or
		// 256 px rendition and let the image list scale it down.
		if (iconSize > 32) {
			IImageList *shellImages = NULL;
			int which = iconSize > 48 ? SHIL_JUMBO : SHIL_EXTRALARGE;
			if (SUCCEEDED(SHGetImageList(which, IID_PPV_ARGS(&shellImages))) && shellImages != NULL) {
				HICON large = NULL;
				if (SUCCEEDED(shellImages->GetIcon(fileinfo.iIcon, ILD_TRANSPARENT, &large)) && large != NULL) {
					if (icon != NULL) DestroyIcon(icon);
					icon = large;
				}
				shellImages->Release();
			}
		}
		longname = new wchar_t[wcslen(fileinfo.szDisplayName)+1];
		wcscpy_s(longname, wcslen(fileinfo.szDisplayName)+1, fileinfo.szDisplayName);
	}
	else {
		avail = 0;
		letter = let;
		icon = NULL;
		longname = NULL;
	}
}
