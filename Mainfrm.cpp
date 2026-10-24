
#include "StdAfx.h"
#include "SpaceMonger.h"
#include "FolderView.h"
#include "AboutDialog.h"
#include "ScanDetailsDialog.h"
#include "Theme.h"
#include "Lang.h"
#include "DpiUtil.h"
#include <string>

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SYSCOMMAND()
	ON_WM_ERASEBKGND()
	ON_WM_ACTIVATEAPP()
	ON_WM_CLOSE()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_SHOWWINDOW()
	ON_MESSAGE(0x02E0, OnDpiChanged)
	ON_MESSAGE(WM_SCAN_NOTICE_CLICKED, OnScanNoticeClickedMessage)
	ON_WM_DROPFILES()
	ON_WM_SETTINGCHANGE()
	ON_MESSAGE(WM_SCAN_DROPPED_PATH, OnScanDroppedPath)
	//}}AFX_MSG_MAP
	ON_UPDATE_COMMAND_UI(IDC_SCAN_NOTICE, OnScanNoticeUpdate)
	ON_UPDATE_COMMAND_UI_RANGE(100, 41000, OnIgnoreUpdate)
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CScanNoticeButton, CButton)
	ON_WM_LBUTTONUP()
	ON_WM_KEYUP()
END_MESSAGE_MAP()

void CScanNoticeButton::OnLButtonUp(UINT nFlags, CPoint point)
{
	CButton::OnLButtonUp(nFlags, point);
	CRect rect;
	GetClientRect(&rect);
	if (rect.PtInRect(point) && GetParent() != NULL)
		GetParent()->SendMessage(WM_SCAN_NOTICE_CLICKED, 0, 0);
}

void CScanNoticeButton::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CButton::OnKeyUp(nChar, nRepCnt, nFlags);
	if ((nChar == VK_SPACE || nChar == VK_RETURN) && GetParent() != NULL)
		GetParent()->SendMessage(WM_SCAN_NOTICE_CLICKED, 0, 0);
}

CMainFrame::CMainFrame()
{
	m_scanning = FALSE;
	m_closeAfterScan = FALSE;
}

CMainFrame::~CMainFrame()
{
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	CFrameWnd::PreCreateWindow(cs);

	cs.style &= ~FWS_ADDTOTITLE;
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	return(1);
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1) return(-1);

	SetWindowText(L"SpaceMonger");

	HINSTANCE hinst = AfxFindResourceHandle(MAKEINTRESOURCE(ID_SPACEMONGER), RT_ICON);
	HICON hicon = ::LoadIcon(hinst, MAKEINTRESOURCE(ID_SPACEMONGER));
	SetIcon(hicon, 1);
	//SetIcon(hicon, 0);

	if (!m_toolbar.Create(this)) return(-1);
	if (!m_scanNoticeButton.Create(L"", WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
		CRect(0, 0, 1, 1), this, IDC_SCAN_NOTICE)) return(-1);
	m_scanNoticeButton.ShowWindow(SW_HIDE);
	if (!m_scanStatus.Create(this)) return(-1);
	if (!m_pathBar.Create(this)) return(-1);
	m_pathBar.SetUiFont(m_toolbar.GetUiFont(), m_toolbar.GetDpi());
	SM_ApplyThemeToFrame(m_hWnd);

	CMenu *menu = GetSystemMenu(FALSE);
	menu->AppendMenu(MF_SEPARATOR);
	menu->AppendMenu(MF_STRING, ID_APP_ABOUT, CString(L"&")
		+ CurLang->about_spacemonger + L"...");

	WINDOWPLACEMENT w;
	w.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &w);
	theApp.m_settings.rect.left = w.rcNormalPosition.left;
	theApp.m_settings.rect.top = w.rcNormalPosition.top;
	theApp.m_settings.rect.right = w.rcNormalPosition.right;
	theApp.m_settings.rect.bottom = w.rcNormalPosition.bottom;
	theApp.m_settings.showcmd = w.showCmd;

	RecalcLayout(0);

	return(0);
}

void CMainFrame::OnDestroy(void)
{
	WINDOWPLACEMENT w;
	w.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &w);
	theApp.m_settings.rect.left = w.rcNormalPosition.left;
	theApp.m_settings.rect.top = w.rcNormalPosition.top;
	theApp.m_settings.rect.right = w.rcNormalPosition.right;
	theApp.m_settings.rect.bottom = w.rcNormalPosition.bottom;
	theApp.m_settings.showcmd = w.showCmd;
}

void CMainFrame::OnActivateApp(BOOL bActive, DWORD dwThreadID)
{
	if (bActive) {
		CSpaceMonger *monger = (CSpaceMonger *)AfxGetApp();
		if (monger != NULL && monger->m_view != NULL) {
			CFolderView *view = (CFolderView *)monger->m_view;
			view->OnUpdate(monger->m_document);
		}
	}
	else {
		CSpaceMonger *monger = (CSpaceMonger *)AfxGetApp();
		if (monger != NULL && monger->m_view != NULL) {
			CFolderView *view = (CFolderView *)monger->m_view;
			view->SelectFolder(NULL);
		}
	}
}

void CMainFrame::RecalcLayout(BOOL bNotify)
{
	CRect rectBorder;
	int numwnds, wndnum, width;
	CWnd *wnd, *first, *last;

	if (m_bInRecalcLayout || !IsWindow(m_hWnd)
		|| (wnd = GetWindow(GW_CHILD)) == NULL) return;

	m_bInRecalcLayout = TRUE;

	first = wnd->GetWindow(GW_HWNDFIRST);
	last = wnd->GetWindow(GW_HWNDLAST);

	GetClientRect(&rectBorder);
	rectBorder.OffsetRect(-rectBorder.left, -rectBorder.top);
	width = rectBorder.right - rectBorder.left;

	// Position the toolbar and the scan notice; both follow the toolbar's
	// DPI-dependent button height.
	int dpi = (int)m_toolbar.GetDpi();
	int buttonHeight = m_toolbar.GetButtonHeight();
	int pad = MulDiv(4, dpi, 96);
	int barHeight = buttonHeight + 2 * pad;
	m_toolbar.SetWindowPos(NULL, 0, 0, width, barHeight,
		SWP_NOZORDER|SWP_NOACTIVATE);
	if (::IsWindow(m_scanNoticeButton.m_hWnd) && m_scanNotice.IsVisible()) {
		if (m_toolbar.GetUiFont() != NULL)
			m_scanNoticeButton.SendMessage(WM_SETFONT, (WPARAM)m_toolbar.GetUiFont(), FALSE);
		int noticeWidth = min(MulDiv(160, dpi, 96), max(MulDiv(120, dpi, 96), width / 5));
		m_scanNoticeButton.SetWindowPos(&CWnd::wndTop,
			width - noticeWidth - pad, pad,
			noticeWidth, buttonHeight,
			SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}
	rectBorder.top += barHeight;
	if (::IsWindow(m_pathBar.m_hWnd)) {
		int pathHeight = m_pathBar.GetDesiredHeight();
		m_pathBar.SetWindowPos(NULL, 0, rectBorder.top, width, pathHeight, SWP_NOZORDER | SWP_NOACTIVATE);
		rectBorder.top += pathHeight;
	}
	int statusHeight = m_scanStatus.GetDesiredHeight();
	if (statusHeight > 0) {
		m_scanStatus.SetWindowPos(NULL, 0, rectBorder.bottom - statusHeight, width, statusHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		rectBorder.bottom -= statusHeight;
	}

	// First count how many windows need to be rearranged
	numwnds = 0;
	if (first != NULL) {
		wnd = first;
		while (1) {
			if (wnd->m_hWnd != m_toolbar.m_hWnd && wnd->m_hWnd != m_scanNoticeButton.m_hWnd
				&& wnd->m_hWnd != m_scanStatus.m_hWnd && wnd->m_hWnd != m_pathBar.m_hWnd)
				numwnds++;
			if (wnd == last) break;
			wnd = wnd->GetWindow(GW_HWNDNEXT);
		}
	}

	// Now lay them all out in a horizontal band (cheap & easy)
	if (first != NULL && numwnds > 0) {
		wndnum = 0, wnd = first;
		while (1) {
			if (wnd->m_hWnd != m_toolbar.m_hWnd && wnd->m_hWnd != m_scanNoticeButton.m_hWnd
				&& wnd->m_hWnd != m_scanStatus.m_hWnd && wnd->m_hWnd != m_pathBar.m_hWnd) {
				wnd->SetWindowPos(NULL, rectBorder.left + (width * wndnum) / numwnds, rectBorder.top,
					(width * (wndnum+1)) / numwnds - (width * wndnum) / numwnds,
					rectBorder.bottom - rectBorder.top,
					SWP_NOZORDER);
				if (++wndnum == numwnds) break;
			}
			wnd = wnd->GetWindow(GW_HWNDNEXT);
		}
	}

	m_bInRecalcLayout = FALSE;
}

void CMainFrame::SetScanNotice(ui64 skippedDirectories, DWORD firstError,
	const std::vector<std::wstring>& paths)
{
	m_scanNotice.Set(skippedDirectories, firstError);
	m_skippedPaths = paths;

	CString text;
	text.Format(CurLang->scan_partial_button_format, skippedDirectories);
	m_scanNoticeButton.SetWindowText(text);
	// No button for it: the "Not readable" block on the map explains the gap,
	// and "Run as administrator" on the toolbar is what fills it.
}

void CMainFrame::ClearScanNotice(void)
{
	m_scanNotice.Clear();
	m_skippedPaths.clear();
	if (::IsWindow(m_scanNoticeButton.m_hWnd)) {
		m_scanNoticeButton.EnableWindow(FALSE);
		m_scanNoticeButton.ShowWindow(SW_HIDE);
		RecalcLayout(0);
	}
}

void CMainFrame::OnScanNoticeClicked()
{
	if (!m_scanNotice.IsVisible()) return;
	CScanDetailsDialog dialog(m_scanNotice.skippedDirectories, m_skippedPaths, this);
	dialog.DoModal();
}

LRESULT CMainFrame::OnScanNoticeClickedMessage(WPARAM wParam, LPARAM lParam)
{
	OnScanNoticeClicked();
	return 0;
}

// A drive, folder or file dropped from Explorer is scanned (a file: its
// folder). The scan starts after the drop has completed, so Explorer is not
// kept waiting for it.
void CMainFrame::OnDropFiles(HDROP drop)
{
	UINT length = ::DragQueryFileW(drop, 0, NULL, 0);
	CString path;
	if (length > 0) {
		::DragQueryFileW(drop, 0, path.GetBuffer(length + 1), length + 1);
		path.ReleaseBuffer();
	}
	::DragFinish(drop);
	if (path.IsEmpty() || m_scanning) return;

	DWORD attributes = ::GetFileAttributesW(path);
	if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
		int slash = path.ReverseFind(L'\\');
		if (slash > 0) path = path.Left(slash + 1);
	}
	SetForegroundWindow();
	PostMessage(WM_SCAN_DROPPED_PATH, 0, (LPARAM)new CString(path));
}

LRESULT CMainFrame::OnScanDroppedPath(WPARAM wParam, LPARAM lParam)
{
	CString *path = (CString *)lParam;
	if (path != NULL) {
		theApp.ScanPath(*path);
		delete path;
	}
	return 0;
}

void CMainFrame::OnScanNoticeUpdate(CCmdUI *ui)
{
	ui->Enable(m_scanNotice.IsVisible());
}

void CMainFrame::OnSysCommand(UINT nid, LPARAM lparam)
{
	if (nid == ID_APP_ABOUT) {
		CAboutDialog about;
		about.DoModal();
	}
	else CFrameWnd::OnSysCommand(nid, lparam);
}

BOOL CMainFrame::OnEraseBkgnd(CDC* pDC)
{
#if 0
	CRect rect;
	GetClientRect(&rect);
	FillBox(pDC->m_hDC, ColorFlat, 0, 0,
		rect.right - rect.left, rect.bottom - rect.top);
#endif
	return(-1);
}

void CMainFrame::OnClose()
{
	// Closing mid-scan would destroy the window under the running scan loop;
	// cancel instead and close once the scan has unwound.
	if (m_scanning) {
		m_closeAfterScan = TRUE;
		m_scanStatus.RequestCancel();
		return;
	}
	CFrameWnd::OnClose();
}

void CMainFrame::BeginScan(const CString &root)
{
	m_scanning = TRUE;
	m_toolbar.EnableWindow(FALSE);
	if (theApp.m_view != NULL) theApp.m_view->EnableWindow(FALSE);
	m_scanStatus.Begin(root, m_toolbar.GetUiFont(), m_toolbar.GetDpi());
	RecalcLayout(0);
	m_scanStatus.UpdateWindow();
}

void CMainFrame::UpdateScan(ui64 files, ui64 folders, ui64 bytes, ui64 usedspace,
	const std::wstring &currentPath, bool complete)
{
	m_scanStatus.Update(files, folders, bytes, usedspace, currentPath, complete);
}

void CMainFrame::EndScan(void)
{
	m_scanStatus.End();
	m_toolbar.EnableWindow(TRUE);
	if (theApp.m_view != NULL) theApp.m_view->EnableWindow(TRUE);
	m_scanning = FALSE;
	RecalcLayout(0);
	if (m_closeAfterScan) PostMessage(WM_CLOSE);
}

void CMainFrame::OnIgnoreUpdate(CCmdUI *ui)
{
	GeneralIgnoreUpdate(ui);
}

void CMainFrame::OnWindowPosChanged(WINDOWPOS *wp)
{
	CFrameWnd::OnWindowPosChanged(wp);

	WINDOWPLACEMENT w;
	w.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &w);
	theApp.m_settings.rect.left = w.rcNormalPosition.left;
	theApp.m_settings.rect.top = w.rcNormalPosition.top;
	theApp.m_settings.rect.right = w.rcNormalPosition.right;
	theApp.m_settings.rect.bottom = w.rcNormalPosition.bottom;
	theApp.m_settings.showcmd = w.showCmd;
}

void CMainFrame::OnShowWindow(BOOL bShow, UINT status)
{
	CFrameWnd::OnShowWindow(bShow, status);

	WINDOWPLACEMENT w;
	w.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &w);
	theApp.m_settings.rect.left = w.rcNormalPosition.left;
	theApp.m_settings.rect.top = w.rcNormalPosition.top;
	theApp.m_settings.rect.right = w.rcNormalPosition.right;
	theApp.m_settings.rect.bottom = w.rcNormalPosition.bottom;
	theApp.m_settings.showcmd = w.showCmd;
}

LRESULT CMainFrame::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
	RECT* prc = (RECT*)lParam;
	if (prc != NULL) {
		SetWindowPos(NULL, prc->left, prc->top, prc->right - prc->left, prc->bottom - prc->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	ApplyAppearance();
	if (theApp.m_view != NULL) {
		((CFolderView*)theApp.m_view)->RecreateFonts();
		theApp.m_view->Invalidate();
	}
	return 0;
}

void CMainFrame::ApplyAppearance(void)
{
	SM_ApplyThemeSetting(theApp.m_settings.theme);
	SM_ApplyThemeToFrame(m_hWnd);
	if (::IsWindow(m_toolbar.m_hWnd)) m_toolbar.Reload();
	if (::IsWindow(m_pathBar.m_hWnd)) {
		m_pathBar.SetUiFont(m_toolbar.GetUiFont(), m_toolbar.GetDpi());
		m_pathBar.Relabel();
		m_pathBar.ApplyTheme();
	}
	if (::IsWindow(m_scanStatus.m_hWnd)) m_scanStatus.Invalidate();
	RecalcLayout(0);
	if (theApp.m_view != NULL) theApp.m_view->Invalidate(FALSE);
	// The caption repaints only when it changes; nudge it for the dark title bar.
	SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// Windows switched between light and dark app mode.
void CMainFrame::OnSettingChange(UINT flags, LPCTSTR section)
{
	CFrameWnd::OnSettingChange(flags, section);
	if (section != NULL && _wcsicmp(section, L"ImmersiveColorSet") == 0 && theApp.m_settings.theme == 0)
		ApplyAppearance();
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CMainToolBar, CToolBarCtrl)

BEGIN_MESSAGE_MAP(CMainToolBar, CToolBarCtrl)
	//{{AFX_MSG_MAP(CMainToolBar)
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_UPDATE_COMMAND_UI_RANGE(100, 41000, OnIgnoreUpdate)
END_MESSAGE_MAP()

namespace {

// Toolbar layout; the label of each button is CurLang->toolbarlabels[n] and
// its icon kToolbarGlyphs[n], in the order of the non-separator entries.
const UINT kToolbarCommands[] = {
	ID_FILE_OPEN, ID_FILE_REFRESH, ID_SEPARATOR,
	ID_VIEW_ZOOM_FULL, ID_VIEW_ZOOM_IN, ID_VIEW_ZOOM_OUT, ID_SEPARATOR,
	ID_VIEW_FREE, ID_VIEW_HIDE_SYSTEM, ID_SEPARATOR,
	ID_FILE_DELETE, ID_SEPARATOR,
	ID_TOOLS_CLEANUP, ID_FILE_RUNAS, ID_SEPARATOR,
	ID_SETTINGS, ID_SEPARATOR,
	ID_APP_ABOUT,
};

// Code points shared by Segoe Fluent Icons (Windows 11) and Segoe MDL2
// Assets (Windows 10): FolderOpen, Refresh, FullScreen, ZoomIn, ZoomOut,
// PieSingle, System, Delete, Broom, Shield, Setting, Info.
const wchar_t kToolbarGlyphs[] = {
	0xE838, 0xE72C, 0xE740, 0xE8A3, 0xE71F, 0xEB05, 0xE770, 0xE74D, 0xEA99, 0xEA18, 0xE713, 0xE946,
};
const int kToolbarImageCount = sizeof(kToolbarGlyphs) / sizeof(kToolbarGlyphs[0]);

bool FontFaceInstalled(HDC dc, const wchar_t *face)
{
	LOGFONTW lf;
	memset(&lf, 0, sizeof(lf));
	lf.lfCharSet = DEFAULT_CHARSET;
	wcscpy_s(lf.lfFaceName, face);
	bool found = false;
	::EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM param) -> int {
		*(bool *)param = true;
		return 0;
	}, (LPARAM)&found, 0);
	return found;
}

// Renders the icon glyphs into a 32-bit image list. Glyphs are drawn black on
// white with grayscale anti-aliasing; the gray level becomes the alpha of a
// solid colour, so the icons blend cleanly onto any toolbar background.
HIMAGELIST CreateGlyphImageList(int size, COLORREF color)
{
	HIMAGELIST list = ImageList_Create(size, size, ILC_COLOR32, kToolbarImageCount, 0);
	if (list == NULL) return NULL;

	HDC screen = ::GetDC(NULL);
	HDC dc = ::CreateCompatibleDC(screen);
	const wchar_t *face = FontFaceInstalled(dc, L"Segoe Fluent Icons")
		? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
	HFONT font = ::CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH, face);

	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = size * kToolbarImageCount;
	bi.bmiHeader.biHeight = -size;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	void *bits = NULL;
	HBITMAP bmp = ::CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (dc != NULL && font != NULL && bmp != NULL) {
		HGDIOBJ oldBmp = ::SelectObject(dc, bmp);
		HGDIOBJ oldFont = ::SelectObject(dc, font);
		RECT all = { 0, 0, size * kToolbarImageCount, size };
		::FillRect(dc, &all, (HBRUSH)::GetStockObject(WHITE_BRUSH));
		::SetTextColor(dc, RGB(0, 0, 0));
		::SetBkMode(dc, TRANSPARENT);
		for (int i = 0; i < kToolbarImageCount; ++i) {
			RECT cell = { i * size, 0, (i + 1) * size, size };
			::DrawTextW(dc, &kToolbarGlyphs[i], 1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		::GdiFlush();
		BYTE r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
		DWORD *px = (DWORD *)bits;
		for (int i = 0; i < size * size * kToolbarImageCount; ++i) {
			BYTE alpha = (BYTE)(255 - (px[i] & 0xFF));
			// Premultiplied BGRA.
			px[i] = ((DWORD)alpha << 24) | ((DWORD)(r * alpha / 255) << 16)
				| ((DWORD)(g * alpha / 255) << 8) | (DWORD)(b * alpha / 255);
		}
		::SelectObject(dc, oldFont);
		::SelectObject(dc, oldBmp);
		ImageList_Add(list, bmp, NULL);
	}
	if (bmp != NULL) ::DeleteObject(bmp);
	if (font != NULL) ::DeleteObject(font);
	if (dc != NULL) ::DeleteDC(dc);
	::ReleaseDC(NULL, screen);
	return list;
}

}

CMainToolBar::CMainToolBar()
{
	m_dpi = 96;
	m_buttonHeight = 22;
	m_font = NULL;
	m_images = NULL;
	m_disabledImages = NULL;
}

CMainToolBar::~CMainToolBar()
{
	if (m_images != NULL) ImageList_Destroy(m_images);
	if (m_disabledImages != NULL) ImageList_Destroy(m_disabledImages);
	if (m_font != NULL) ::DeleteObject(m_font);
}

int CMainToolBar::Create(CWnd *parent)
{
	// The frame positions the toolbar itself (CCS_NORESIZE/NOPARENTALIGN).
	// TBSTYLE_CUSTOMERASE lets OnCustomDraw paint the strip's background.
	if (!CToolBarCtrl::Create(WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS
			| TBSTYLE_CUSTOMERASE | CCS_NODIVIDER | CCS_NORESIZE | CCS_NOPARENTALIGN,
			CRect(0, 0, 0, 0), parent, AFX_IDW_TOOLBAR)) return 0;
	SetButtonStructSize(sizeof(TBBUTTON));
	// Labels are wide strings; without this the control reads them as ANSI
	// and shows only the first character.
	::SendMessageW(m_hWnd, CCM_SETUNICODEFORMAT, TRUE, 0);
	// Buttons without BTNS_SHOWTEXT show only their icon; the label becomes
	// their tooltip. Keeps the bar within narrow windows.
	SetExtendedStyle(TBSTYLE_EX_MIXEDBUTTONS);
	Rebuild();

	EnableState(ViewFree, 1);
	EnableState(ShowSystem, !theApp.m_settings.hide_system);

	return 1;
}

// Applies the current DPI, language and system colours: message font,
// Fluent icons at the matching size, then (re)creates the buttons. The
// control measures BTNS_AUTOSIZE buttons only when they are added, so a
// label change means adding the buttons again.
void CMainToolBar::Rebuild(void)
{
	m_dpi = SM_GetWindowDpi(m_hWnd);

	HFONT font = SM_CreateMessageFontForDpi(m_dpi);
	if (font != NULL) {
		::SendMessageW(m_hWnd, WM_SETFONT, (WPARAM)font, FALSE);
		if (m_font != NULL) ::DeleteObject(m_font);
		m_font = font;
	}

	int iconSize = MulDiv(16, (int)m_dpi, 96);
	const SM_Theme &theme = SM_CurrentTheme();
	HIMAGELIST images = CreateGlyphImageList(iconSize, theme.text);
	HIMAGELIST disabled = CreateGlyphImageList(iconSize, theme.dark ? RGB(110, 110, 110) : theme.grayText);
	::SendMessageW(m_hWnd, TB_SETIMAGELIST, 0, (LPARAM)images);
	::SendMessageW(m_hWnd, TB_SETDISABLEDIMAGELIST, 0, (LPARAM)disabled);
	if (m_images != NULL) ImageList_Destroy(m_images);
	if (m_disabledImages != NULL) ImageList_Destroy(m_disabledImages);
	m_images = images;
	m_disabledImages = disabled;
	SetBitmapSize(CSize(iconSize, iconSize));
	::SendMessageW(m_hWnd, TB_SETLISTGAP, (WPARAM)MulDiv(6, (int)m_dpi, 96), 0);
	::SendMessageW(m_hWnd, TB_SETPADDING, 0,
		MAKELPARAM(MulDiv(12, (int)m_dpi, 96), MulDiv(10, (int)m_dpi, 96)));

	// Labels go into the control's own string pool as one double-NUL-
	// terminated block; buttons then refer to them by index.
	std::wstring pool;
	for (int i = 0; i < kToolbarImageCount; ++i) {
		pool += CurLang->toolbarlabels[i];
		pool.push_back(L'\0');
	}
	pool.push_back(L'\0');
	INT_PTR firstString = ::SendMessageW(m_hWnd, TB_ADDSTRINGW, 0, (LPARAM)pool.c_str());

	// Keep enabled/checked states across the rebuild.
	int count = sizeof(kToolbarCommands) / sizeof(kToolbarCommands[0]);
	TBBUTTON buttons[sizeof(kToolbarCommands) / sizeof(kToolbarCommands[0])];
	int label = 0;
	for (int i = 0; i < count; ++i) {
		memset(&buttons[i], 0, sizeof(buttons[i]));
		if (kToolbarCommands[i] == ID_SEPARATOR) {
			buttons[i].fsStyle = BTNS_SEP;
			continue;
		}
		int id = (int)kToolbarCommands[i];
		BYTE state = TBSTATE_ENABLED;
		if (GetButtonCount() > 0 && CommandToIndex(id) >= 0)
			state = (BYTE)GetState(id);
		buttons[i].idCommand = id;
		buttons[i].iBitmap = label;
		buttons[i].fsState = state;
		bool iconOnly = id == ID_VIEW_ZOOM_FULL || id == ID_VIEW_ZOOM_IN || id == ID_VIEW_ZOOM_OUT
			|| id == ID_SETTINGS || id == ID_APP_ABOUT;
		buttons[i].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | (iconOnly ? 0 : BTNS_SHOWTEXT)
			| ((id == ID_VIEW_FREE || id == ID_VIEW_HIDE_SYSTEM) ? BTNS_CHECK : 0);
		buttons[i].iString = firstString >= 0 ? firstString + label : -1;
		++label;
	}
	SetRedraw(FALSE);
	while (GetButtonCount() > 0) DeleteButton(0);
	::SendMessageW(m_hWnd, TB_ADDBUTTONSW, (WPARAM)count, (LPARAM)buttons);
	AutoSize();
	SetRedraw(TRUE);
	Invalidate();

	CSize size = GetButtonSize();
	m_buttonHeight = size.cy > 0 ? size.cy : iconSize + MulDiv(10, (int)m_dpi, 96);
}

// The frame never erases its background and a flat toolbar would show the
// parent's, so paint the strip in the dialog face colour ourselves.
void CMainToolBar::OnCustomDraw(NMHDR *pNotify, LRESULT *pResult)
{
	NMTBCUSTOMDRAW *cd = (NMTBCUSTOMDRAW *)pNotify;
	*pResult = CDRF_DODEFAULT;
	const SM_Theme &theme = SM_CurrentTheme();
	if (cd->nmcd.dwDrawStage == CDDS_PREERASE) {
		RECT rect;
		::GetClientRect(m_hWnd, &rect);
		::FillRect(cd->nmcd.hdc, &rect, theme.faceBrush);
		*pResult = CDRF_SKIPDEFAULT;
	}
	else if (cd->nmcd.dwDrawStage == CDDS_PREPAINT && theme.dark) {
		*pResult = CDRF_NOTIFYITEMDRAW;
	}
	else if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && theme.dark) {
		// Light labels; the light theme plate behind hot and checked buttons
		// is replaced by a dark-grey one drawn here.
		cd->clrText = (cd->nmcd.uItemState & CDIS_DISABLED) ? RGB(110, 110, 110) : theme.text;
		COLORREF plate = theme.face;
		if (cd->nmcd.uItemState & (CDIS_CHECKED | CDIS_SELECTED)) plate = RGB(68, 68, 68);
		else if (cd->nmcd.uItemState & CDIS_HOT) plate = RGB(52, 52, 52);
		if (plate != theme.face) {
			HBRUSH brush = ::CreateSolidBrush(plate);
			HPEN pen = ::CreatePen(PS_SOLID, 1, (cd->nmcd.uItemState & CDIS_CHECKED) ? RGB(96, 96, 96) : plate);
			HGDIOBJ oldBrush = ::SelectObject(cd->nmcd.hdc, brush);
			HGDIOBJ oldPen = ::SelectObject(cd->nmcd.hdc, pen);
			int round = MulDiv(4, (int)m_dpi, 96);
			::RoundRect(cd->nmcd.hdc, cd->nmcd.rc.left, cd->nmcd.rc.top, cd->nmcd.rc.right, cd->nmcd.rc.bottom, round, round);
			::SelectObject(cd->nmcd.hdc, oldPen);
			::SelectObject(cd->nmcd.hdc, oldBrush);
			::DeleteObject(pen);
			::DeleteObject(brush);
		}
		*pResult = TBCDRF_USECDCOLORS | TBCDRF_NOBACKGROUND | TBCDRF_NOEDGES | TBCDRF_NOOFFSET;
	}
}

BOOL CMainToolBar::OnEraseBkgnd(CDC *pDC)
{
	CRect rect;
	GetClientRect(&rect);
	pDC->FillSolidRect(&rect, SM_CurrentTheme().face);
	return TRUE;
}
void CMainToolBar::EnableState(int state, BOOL enabled)
{
	switch (state) {
	case CMainToolBar::ViewFree:
		CheckButton(ID_VIEW_FREE, enabled);
		break;
	case CMainToolBar::ShowSystem:
		// Pressed = shown, like Free Space; the setting stores the opposite.
		CheckButton(ID_VIEW_HIDE_SYSTEM, enabled);
		break;
	default:
		break;
	}
}
static inline void SetButtonEnabledIfChanged(CToolBarCtrl &ctrl, int id, BOOL enable)
{
	if ((ctrl.IsButtonEnabled(id) != 0) != (enable != 0)) {
		ctrl.EnableButton(id, enable);
	}
}

void CMainToolBar::UpdateButtonsForView(CFolderView *view)
{
	CSpaceMonger *app = (CSpaceMonger *)AfxGetApp();
	CToolBarCtrl &ctrl = *this;
	SetButtonEnabledIfChanged(ctrl, ID_FILE_DELETE, !app->m_settings.disable_delete
		&& view->IsAnythingOpen() && view->IsAnythingSelected());
	SetButtonEnabledIfChanged(ctrl, ID_FILE_OPEN, 1);
	SetButtonEnabledIfChanged(ctrl, ID_FILE_REFRESH, view->IsAnythingOpen());
	SetButtonEnabledIfChanged(ctrl, ID_VIEW_ZOOM_FULL, view->IsAnythingOpen() && !view->IsZoomFull());
	SetButtonEnabledIfChanged(ctrl, ID_VIEW_ZOOM_OUT, view->IsAnythingOpen() && !view->IsZoomFull());
	SetButtonEnabledIfChanged(ctrl, ID_VIEW_ZOOM_IN, view->IsAnythingOpen() && view->CanZoomIn());
	SetButtonEnabledIfChanged(ctrl, ID_VIEW_FREE, view->IsAnythingOpen());
	SetButtonEnabledIfChanged(ctrl, ID_VIEW_HIDE_SYSTEM, view->IsAnythingOpen());
	SetButtonEnabledIfChanged(ctrl, ID_TOOLS_CLEANUP, 1);
	SetButtonEnabledIfChanged(ctrl, ID_FILE_RUNAS, !::IsUserAnAdmin());
	SetButtonEnabledIfChanged(ctrl, ID_SETTINGS, 1);
	SetButtonEnabledIfChanged(ctrl, ID_APP_ABOUT, 1);
}

void CMainToolBar::OnIgnoreUpdate(CCmdUI *ui)
{
	GeneralIgnoreUpdate(ui);
}

void CMainToolBar::Reload(void)
{
	Rebuild();
	RedrawWindow();
}
