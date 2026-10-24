#include "stdafx.h"
#include "spacemonger.h"
#include "FolderView.h"
#include "AsyncScanEngine.h"
#include "MainFrm.h"
#include "CommandPolicy.h"
#include "TipWnd.h"
#include "PathUtil.h"
#include "Lang.h"
#include "UnicodeFontFallback.h"
#include "DpiUtil.h"
#include "TextFormat.h"
#include "FileTypes.h"
#include "Theme.h"
#include "PathBar.h"
#include <shlwapi.h>
#include <unordered_map>

IMPLEMENT_DYNCREATE(CFolderView, CFreeView)

static CString GetSizeString(ui64 size, ui64 totalspace, BOOL percent);

static COLORREF BoxColors[] = {
	PALETTERGB(0xFF, 0x7F, 0x7F),
	PALETTERGB(0xFF, 0xBF, 0x7F),
	PALETTERGB(0xFF, 0xFF, 0x00),
	PALETTERGB(0x7F, 0xFF, 0x7F),
	PALETTERGB(0x7F, 0xFF, 0xFF),
	PALETTERGB(0xBF, 0xBF, 0xFF),
	PALETTERGB(0xBF, 0xBF, 0xBF),
	PALETTERGB(0xFF, 0x7F, 0xFF),

	PALETTERGB(0xFF, 0xBF, 0xBF),
	PALETTERGB(0xFF, 0xDF, 0xBF),
	PALETTERGB(0xFF, 0xFF, 0xBF),
	PALETTERGB(0xBF, 0xFF, 0xBF),
	PALETTERGB(0xDF, 0xFF, 0xFF),
	PALETTERGB(0xDF, 0xDF, 0xFF),
	PALETTERGB(0xDF, 0xDF, 0xDF),
	PALETTERGB(0xFF, 0xBF, 0xFF),

	PALETTERGB(0xBF, 0x7F, 0x7F),
	PALETTERGB(0xBF, 0x9F, 0x5F),
	PALETTERGB(0xBF, 0xBF, 0x3F),
	PALETTERGB(0x7F, 0xBF, 0x7F),
	PALETTERGB(0x7F, 0xBF, 0xBF),
	PALETTERGB(0x9F, 0x9F, 0xFF),
	PALETTERGB(0x9F, 0x9F, 0x9F),
	PALETTERGB(0xBF, 0x7F, 0xBF),

	PALETTERGB(0x00, 0x00, 0x00),
	PALETTERGB(0xFF, 0xFF, 0xFF),
};

// These colors are used for specifically-chosen folder colors.
static COLORREF FixedColors[] = {
	PALETTERGB(0xFF, 0xFF, 0xFF),
	PALETTERGB(0xBF, 0xBF, 0xBF),
	PALETTERGB(0x7F, 0x7F, 0x7F),

	PALETTERGB(0xFF, 0x7F, 0x7F),
	PALETTERGB(0xFF, 0xBF, 0x7F),
	PALETTERGB(0xFF, 0xFF, 0x00),
	PALETTERGB(0x7F, 0xFF, 0x7F),
	PALETTERGB(0x7F, 0xFF, 0xFF),
	PALETTERGB(0xBF, 0xBF, 0xFF),
	PALETTERGB(0xFF, 0x7F, 0xFF),

	PALETTERGB(0xFF, 0xFF, 0xFF),
	PALETTERGB(0xFF, 0xFF, 0xFF),
	PALETTERGB(0xBF, 0xBF, 0xBF),

	PALETTERGB(0xFF, 0x9F, 0x9F),
	PALETTERGB(0xFF, 0xDF, 0xBF),
	PALETTERGB(0xFF, 0xFF, 0xBF),
	PALETTERGB(0xBF, 0xFF, 0xBF),
	PALETTERGB(0xDF, 0xFF, 0xFF),
	PALETTERGB(0xDF, 0xDF, 0xFF),
	PALETTERGB(0xFF, 0xBF, 0xFF),

	PALETTERGB(0xBF, 0xBF, 0xBF),
	PALETTERGB(0x7F, 0x7F, 0x7F),
	PALETTERGB(0x3F, 0x3F, 0x3F),

	PALETTERGB(0xBF, 0x7F, 0x7F),
	PALETTERGB(0xBF, 0x9F, 0x9F),
	PALETTERGB(0xBF, 0xBF, 0x3F),
	PALETTERGB(0x7F, 0xBF, 0x7F),
	PALETTERGB(0x7F, 0xBF, 0xBF),
	PALETTERGB(0x9F, 0x9F, 0xFF),
	PALETTERGB(0xBF, 0x7F, 0xBF),
};

CFolderView::CFolderView()
{
	rootfolder = NULL;
	selected = NULL;
	zoomlevel = 0;
	showfreespace = 1;
	lastcur = NULL;
	m_nameTipFontChoice = NULL;
	m_textHeight = 9;
	m_lineStep = 12;
	m_layoutDpi = 96;
	m_titlePadX = 2;
	m_animTimer = 0;
	m_animStart = 0;
	m_xfadeActive = false;
	m_xfadeStart = 0;
	m_xfadeMs = kCrossfadeMs;
}

CFolderView::~CFolderView()
{
	m_layoutNodes.clear();
	for (size_t i = 0; i < m_unicodeFonts.size(); ++i)
		if (m_unicodeFonts[i] != NULL) ::DeleteObject(m_unicodeFonts[i]);
}

BEGIN_MESSAGE_MAP(CFolderView, CFreeView)
	//{{AFX_MSG_MAP(CFolderView)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_MOUSEMOVE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
	ON_UPDATE_COMMAND_UI_RANGE(100, 41000, OnIgnoreUpdate)
END_MESSAGE_MAP()

void CFolderView::RecreateFonts(void)
{
	if (minifont.m_hObject != NULL) minifont.DeleteObject();
	for (size_t i = 0; i < m_unicodeFonts.size(); ++i)
		if (m_unicodeFonts[i] != NULL) ::DeleteObject(m_unicodeFonts[i]);
	m_unicodeFonts.clear();

	UINT dpi = SM_GetWindowDpi(m_hWnd);
	int textScale = SM_GetTextScalePercent();
	m_layoutDpi = MulDiv((int)dpi, textScale, 100);
	if (m_layoutDpi <= 96) {
		// 100 %: the classic metrics, pixel for pixel.
		m_textHeight = 9;
		m_lineStep = 12;
		m_titlePadX = 2;
	} else {
		// Scaled: a little larger than 9 px would give, with padding around
		// folder titles so they don't sit on the box edges.
		m_textHeight = MulDiv(10, m_layoutDpi, 96);
		m_lineStep = m_textHeight + MulDiv(6, m_layoutDpi, 96);
		m_titlePadX = MulDiv(4, m_layoutDpi, 96);
	}

	if (m_textHeight <= 9) {
		// 100 %: the classic bitmap face, pixel for pixel.
		minifont.CreateFont(-9, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
			OUT_RASTER_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			VARIABLE_PITCH|FF_SWISS, L"Small Fonts");
	} else {
		// Small Fonts is a bitmap font and only exists in tiny sizes; scaling
		// it up looks blocky, so larger sizes use the matching TrueType sans.
		minifont.CreateFont(-m_textHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			VARIABLE_PITCH|FF_SWISS, L"Microsoft Sans Serif");
	}

	// Keep the classic face as the first choice. Check Unicode-capable system
	// families against each name before using them as a fallback.
	m_unicodeFonts = SM_CreateUnicodeFallbackFonts(-m_textHeight);

	if (::IsWindow(m_nametipwnd.m_hWnd)) {
		m_nameTipFontChoice = NULL;
		SetNameTipFont((HFONT)minifont.GetSafeHandle());
	}
	// The info tip otherwise uses DEFAULT_GUI_FONT, which is never DPI-scaled.
	if (::IsWindow(m_infotipwnd.m_hWnd)) {
		HFONT tipFont = SM_CreateMessageFontForDpi(dpi);
		if (tipFont != NULL)
			::SendMessageW(m_infotipwnd.m_hWnd, WM_SETFONT, (WPARAM)tipFont, FALSE);
	}
}

TreemapConfig CFolderView::MakeLayoutConfig(void)
{
	TreemapConfig config;
	config.density = theApp.m_settings.density;
	config.bias = theApp.m_settings.bias;
	config.showFreeSpace = (showfreespace != 0);
	config.dpi = m_layoutDpi;
	config.titleHeight = m_lineStep;
	CFolderTree *tree = (CFolderTree *)GetDocument();
	if (tree != NULL && tree->wholeVolume) config.volumeRoot = tree->GetRoot();
	config.hideSystem = theApp.m_settings.hide_system != 0;
	return config;
}

void CFolderView::SetNameTipFont(HFONT font)
{
	if (!::IsWindow(m_nametipwnd.m_hWnd) || font == NULL || font == m_nameTipFontChoice)
		return;
	LOGFONTW logfont;
	if (::GetObjectW(font, sizeof(logfont), &logfont) != sizeof(logfont)) return;
	HFONT ownedFont = ::CreateFontIndirectW(&logfont);
	if (ownedFont == NULL) return;
	::SendMessageW(m_nametipwnd.m_hWnd, WM_SETFONT, (WPARAM)ownedFont, TRUE);
	m_nameTipFontChoice = font;
}

BOOL CFolderView::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
	if (message == 0x02E0 /* WM_DPICHANGED */) {
		RecreateFonts();
		OnSize(0, 0, 0);
		Invalidate();
		if (pResult != NULL) *pResult = 0;
		return TRUE;
	}
	if (message == WM_SETTINGCHANGE) {
		// Windows text size changed (or another metric): rebuild fonts and
		// the layout, whose title bars and minimum boxes depend on them.
		int oldHeight = m_textHeight;
		RecreateFonts();
		if (m_textHeight != oldHeight) {
			OnSize(0, 0, 0);
			Invalidate();
		}
	}

	if (::IsWindow(m_infotipwnd.m_hWnd)) {
		MSG msg;
		msg.message = message;
		msg.wParam = wParam;
		msg.lParam = lParam;
		msg.hwnd = m_hWnd;
		m_infotipwnd.ReflectMessage(&msg);
	}
	if (::IsWindow(m_nametipwnd.m_hWnd)) {
		MSG msg;
		msg.message = message;
		msg.wParam = wParam;
		msg.lParam = lParam;
		msg.hwnd = m_hWnd;
		m_nametipwnd.ReflectMessage(&msg);
	}
	return CFreeView::OnWndMsg(message, wParam, lParam, pResult);
}

int CFolderView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CFreeView::OnCreate(lpCreateStruct) == -1)
		return -1;
	if (!m_infotipwnd.CreateEz(this, 64, 64)) return -1;
	if (!m_nametipwnd.CreateEz(this, 64, 64)) return -1;
	
	int i;
	LOGPALETTE *logpalette = (LOGPALETTE *)malloc(sizeof(LOGPALETTE) + 26 * sizeof(PALETTEENTRY));
	logpalette->palVersion = 0x300;
	logpalette->palNumEntries = 26;
	for (i = 0; i < 26; i++) {
		logpalette->palPalEntry[i].peFlags = 0;
		logpalette->palPalEntry[i].peRed = GetRValue(BoxColors[i]);
		logpalette->palPalEntry[i].peGreen = GetGValue(BoxColors[i]);
		logpalette->palPalEntry[i].peBlue = GetBValue(BoxColors[i]);
	}
	m_palette.CreatePalette(logpalette);
	free(logpalette);

	black.CreateSolidBrush(RGB(0x00, 0x00, 0x00));
	white.CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

	RecreateFonts();

	m_infotipwnd.SetAutoShow(1);
	m_infotipwnd.SetAutoPos(1);
	m_infotipwnd.SetShowDelay(theApp.m_settings.infotip_delay);
	m_infotipwnd.EnableWindow(0);

	m_nametipwnd.SetAutoShow(1);
	m_nametipwnd.SetShowDelay(theApp.m_settings.nametip_delay);
	m_nametipwnd.SetVPadding(0);
	m_nametipwnd.SetHPadding(1);
	m_nametipwnd.EnableWindow(0);

	return 0;
}

void CFolderView::OnDestroy() 
{
	m_palette.DeleteObject();
	black.DeleteObject();
	white.DeleteObject();
	minifont.DeleteObject();

	m_infotipwnd.DestroyWindow();
	m_nametipwnd.DestroyWindow();
	if (m_tipIcon != NULL) { ::DestroyIcon(m_tipIcon); m_tipIcon = NULL; }

	CFreeView::OnDestroy();
}

const TreemapNode *CFolderView::GetDisplayFolderFromPoint(const CPoint &point)
{
	return TreemapEngine::HitTestItem(m_layoutNodes, point.x, point.y, m_lineStep);
}

// Opens the item's folder in Explorer with the item selected.
void CFolderView::ShowInExplorer(const TreemapNode *cur)
{
	if (cur == NULL || cur->source == NULL || cur->index == (ui32)-1) return;
	std::wstring path = BuildItemPathW(cur);
	PIDLIST_ABSOLUTE item = ILCreateFromPathW(path.c_str());
	if (item != NULL) {
		SHOpenFolderAndSelectItems(item, 0, NULL, 0);
		ILFree(item);
	}
}

// The folder box that directly contains a file box (NULL at the top level).
const TreemapNode *CFolderView::FolderOf(const TreemapNode *cur)
{
	if (cur == NULL || cur->source == NULL) return NULL;
	for (const auto &n : m_layoutNodes) {
		if (n.IsFolder() && n.source != NULL && n.index != (ui32)-1
			&& n.source->children[n.index] == cur->source) return &n;
	}
	return NULL;
}

// What Zoom In opens: the selected folder, or the folder of the selected file.
const TreemapNode *CFolderView::ZoomTarget(void)
{
	if (selected == NULL) return NULL;
	if (selected->IsFolder()) return selected;
	return FolderOf(selected);
}

// Wheel forward: one level into the folder under the pointer; back: one up.
BOOL CFolderView::OnMouseWheel(UINT flags, short delta, CPoint point)
{
	if (delta < 0) {
		ZoomOut();
		return TRUE;
	}
	ScreenToClient(&point);
	for (const auto &n : m_layoutNodes) {
		if (n.IsFolder() && n.depth == zoomlevel && point.x >= n.x && point.y >= n.y
			&& point.x < n.x + n.w && point.y < n.y + n.h) {
			ZoomIn(&n);
			break;
		}
	}
	return TRUE;
}

void CFolderView::OnKeyDown(UINT key, UINT repeat, UINT flags)
{
	CSpaceMonger *app = (CSpaceMonger *)AfxGetApp();
	switch (key) {
	case VK_RETURN: ZoomIn(ZoomTarget()); break;
	case VK_BACK: ZoomOut(); break;
	case VK_HOME: ZoomFull(); break;
	case VK_DELETE: app->OnFileDelete(); break;
	case VK_F5: app->OnFileRefresh(); break;
	case 'E': if (::GetKeyState(VK_CONTROL) < 0) app->OnExport(); break;
	case 'F':
		if (::GetKeyState(VK_CONTROL) < 0) {
			CMainFrame *frame = (CMainFrame *)GetTopLevelFrame();
			if (frame != NULL) frame->m_pathBar.FocusSearch();
		}
		break;
	case VK_ESCAPE:
		if (!m_search.empty()) {
			CMainFrame *frame = (CMainFrame *)GetTopLevelFrame();
			if (frame != NULL) frame->m_pathBar.ClearSearch();
		}
		break;
	default: CFreeView::OnKeyDown(key, repeat, flags); break;
	}
}

const TreemapNode *CFolderView::GetContainerDisplayFolderFromPoint(const CPoint &point)
{
	return TreemapEngine::HitTestContainer(m_layoutNodes, point.x, point.y);
}

void CFolderView::HighlightPathAtPoint(const CPoint &point)
{
	BOOL changed = FALSE;

	for (auto &cur : m_layoutNodes) {
		if (cur.name != NULL && !cur.IsSpecial()) {
			BOOL inside = point.x > cur.x && point.y > cur.y
				&& point.x < cur.x + cur.w && point.y < cur.y + cur.h;
			if (inside && !(cur.flags & TREEMAP_FLAG_HOVER)) {
				cur.flags |= TREEMAP_FLAG_HOVER;
				changed = TRUE;
			}
			else if (!inside && (cur.flags & TREEMAP_FLAG_HOVER)) {
				cur.flags &= ~TREEMAP_FLAG_HOVER;
				changed = TRUE;
			}
		}
	}

	// Nodes can't be redrawn in isolation: a folder's minimal draw clears its
	// interior, erasing every descendant already on screen. Repaint through
	// the double-buffered OnPaint instead. Hover only changes pixels when
	// rollover boxes are enabled, so skip the repaint otherwise.
	if (changed && theApp.m_settings.rollover_box) {
		InvalidateRect(NULL, FALSE);
		UpdateWindow();
	}
}

void CFolderView::OnLButtonDown(UINT flags, CPoint point)
{
	SetFocus();	// keyboard shortcuts work after clicking into the map
	const TreemapNode *cur = GetDisplayFolderFromPoint(point);
	SelectFolder(cur);
}

void CFolderView::OnLButtonDblClk(UINT flags, CPoint point)
{
	const TreemapNode *cur = GetDisplayFolderFromPoint(point);
	if (cur != NULL) {
		if (cur->flags & TREEMAP_FLAG_FOLDER)
			ZoomIn(cur);
		else
			ShowInExplorer(cur);	// where is it? Explorer, with the file selected
	}
}

void CFolderView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (nFlags != 0) return;

	const TreemapNode *cur = GetDisplayFolderFromPoint(point);
	if (cur == NULL) {
		// Special blocks are not selectable, but the unreadable-space block
		// gets an explanatory info tip.
		for (const auto &node : m_layoutNodes) {
			if ((node.flags & TREEMAP_FLAG_UNAVAILABLE) && point.x > node.x && point.y > node.y
				&& point.x < node.x + node.w && point.y < node.y + node.h) {
				cur = &node;
				break;
			}
		}
	}

	if (cur != lastcur) {
		HighlightPathAtPoint(point);
		if (theApp.m_settings.show_info_tips || (cur != NULL && (cur->flags & TREEMAP_FLAG_UNAVAILABLE)))
			SetupInfoTip(cur);
		if (theApp.m_settings.show_name_tips)
			SetupNameTip(cur);
		lastcur = cur;
	}
}

// Sizes read like Explorer's ("105 GB", "1,5 MB"), in the user's locale.
static void PrintFileSize(CString &string, ui64 size)
{
	string += SM_FormatBytes(size).c_str();
}

static void PrintDate(CString &string, ui64 time)
{
	if (time == 0) return;

	FILETIME ft;
	ft.dwLowDateTime = (DWORD)time;
	ft.dwHighDateTime = (DWORD)(time >> 32);

	FILETIME lft;
	FileTimeToLocalFileTime(&ft, &lft);

	SYSTEMTIME st;
	FileTimeToSystemTime(&lft, &st);

	wchar_t date[64], timebuf[64];
	GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, _countof(date));
	GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, NULL, timebuf, _countof(timebuf));

	string += date;
	string += L" ";
	string += timebuf;
}

// Files and folders below a folder, for the info tip.
static void CountTree(const CFolder *folder, ui64 &files, ui64 &folders)
{
	if (folder == NULL) return;
	for (unsigned int i = 0; i < folder->cur; ++i) {
		if (folder->children[i] != NULL) {
			++folders;
			CountTree(folder->children[i], files, folders);
		}
		else ++files;
	}
}
void CFolderView::SetupInfoTip(const TreemapNode *cur)
{
	m_infotipwnd.EnableWindow(0);

	if (cur != NULL && (cur->flags & TREEMAP_FLAG_UNAVAILABLE)
		&& cur->source != NULL && cur->index != (ui32)-1) {
		// Explain the estimate block instead of showing nothing for it.
		CFolderTree *tree = (CFolderTree *)GetDocument();
		CString text = CurLang->scan_unavailable_tip;
		text += L"\n\n";
		text += GetSizeString(cur->source->sizes[cur->index], tree != NULL ? tree->totalspace : 0, 0);
		m_infotipwnd.SetIcon(NULL, 0);
		m_infotipwnd.SetWindowText(text);
		m_infotipwnd.AutoSize();
		m_infotipwnd.EnableWindow(1);
		m_infotipwnd.RedrawWindow();
		return;
	}

	if (cur == NULL || cur->name == NULL || cur->IsSpecial())
		return;

	std::wstring widePath = BuildItemPathW(cur);
	std::wstring path = BuildContainerPathW(cur);
	CString string;
	CFolderTree *tree = (CFolderTree *)GetDocument();
	const int flags = theApp.m_settings.infotip_flags;

	WIN32_FILE_ATTRIBUTE_DATA info;
	memset(&info, 0, sizeof(info));
	GetFileAttributesExW(widePath.c_str(), GetFileExInfoStandard, &info);

	// Name first, then the full path of the item: where it really lives
	// matters more than any other detail when deciding what to do with it.
	string += cur->source->names[cur->index];
	string += L'\n';
	string += widePath.c_str();
	string += L'\n';

	// Size and its share of the drive: the number that matters here.
	if (flags & TIP_SIZE) {
		ui64 size = cur->source->actualsizes[cur->index];
		ui64 total = tree != NULL && tree->totalspace > 0 ? tree->totalspace : 1;
		int permille = (int)((size * (ui64)1000) / total);
		CString line;
		line.Format(CurLang->tip_drive_share_format, SM_FormatBytes(size).c_str(), permille / 10, permille % 10);
		string += line;
		string += L'\n';
	}

	// Folders: how much is inside.
	if ((cur->flags & TREEMAP_FLAG_FOLDER) && cur->source->children[cur->index] != NULL) {
		ui64 files = 0, folders = 0;
		CountTree(cur->source->children[cur->index], files, folders);
		CString line;
		line.Format(CurLang->tip_contents_format, SM_FormatCount(files).c_str(), SM_FormatCount(folders).c_str());
		string += line;
		string += L'\n';
	}

	if ((flags & TIP_DATE) && cur->source->times[cur->index] != 0) {
		CString date;
		PrintDate(date, cur->source->times[cur->index]);
		CString line;
		line.Format(CurLang->tip_modified_format, (LPCWSTR)date);
		string += line;
		string += L'\n';
	}

	if (flags & TIP_ATTRIB) {
		CString attributes;
		if (info.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)			attributes += L" ", attributes += CurLang->attribnames[0];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED)		attributes += L" ", attributes += CurLang->attribnames[1];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)		attributes += L" ", attributes += CurLang->attribnames[2];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED)		attributes += L" ", attributes += CurLang->attribnames[3];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)			attributes += L" ", attributes += CurLang->attribnames[4];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE)			attributes += L" ", attributes += CurLang->attribnames[5];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY)		attributes += L" ", attributes += CurLang->attribnames[6];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)	attributes += L" ", attributes += CurLang->attribnames[7];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE)		attributes += L" ", attributes += CurLang->attribnames[8];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)			attributes += L" ", attributes += CurLang->attribnames[9];
		if (info.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY)		attributes += L" ", attributes += CurLang->attribnames[10];
		attributes.Trim();
		if (!attributes.IsEmpty()) {
			string += attributes;
			string += L'\n';
		}
	}

	if (cur->flags & TREEMAP_FLAG_SYSTEM) {
		string += L'\n';
		string += CurLang->tip_system_note;
	}
	string.TrimRight(L'\n');
	if (theApp.m_settings.infotip_flags & TIP_ICON) {
		SHFILEINFOW fileinfo;
		memset(&fileinfo, 0, sizeof(SHFILEINFOW));
		SHGetFileInfoW(widePath.c_str(), 0,
			&fileinfo, sizeof(SHFILEINFOW), SHGFI_DISPLAYNAME|SHGFI_ICON);
		m_infotipwnd.SetIcon(fileinfo.hIcon, 1);
		if (m_tipIcon != NULL) ::DestroyIcon(m_tipIcon);
		m_tipIcon = fileinfo.hIcon;
		m_infotipwnd.SetIconPos(TW_LEFT);
	}

	m_infotipwnd.SetWindowText(string);
	m_infotipwnd.AutoSize();
	m_infotipwnd.EnableWindow(1);
	m_infotipwnd.RedrawWindow();
}

void CFolderView::SetupNameTip(const TreemapNode *cur)
{
	m_nametipwnd.EnableWindow(0);

	if (cur == NULL || cur->name == NULL || cur->IsSpecial()) return;

	CDC *pDC = GetDC();
	int len = (int)wcslen(cur->name);
	HFONT nameFont = SM_SelectFontForText(pDC->GetSafeHdc(),
		(HFONT)minifont.GetSafeHandle(), m_unicodeFonts, cur->name, len);
	pDC->SelectObject(nameFont);
	SetNameTipFont(nameFont);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextAlign(TA_LEFT | TA_TOP | TA_NOUPDATECP);
	pDC->SelectPalette(&m_palette, 0);

	int tx, ty, failed = 0;
	CSize size;
	::GetTextExtentPoint32W(pDC->GetSafeHdc(), cur->name, len, &size);
	int x = cur->x, y = cur->y, w = cur->w + 1, h = cur->h + 1;
	if (size.cx > w - 2) tx = x + 2;
	else if (cur->flags & 1) tx = x + 2, failed++;
	else tx = x + (w - size.cx) / 2, failed++;
	if (size.cy > h - 2) ty = y + 1;
	else if (cur->flags & 1) ty = y + 1, failed++;
	else ty = y + (h - size.cy) / 2, failed++;
	if (failed == 2) {
		ReleaseDC(pDC);
		return;
	}

	if (!(cur->flags & 1) && h >= 3 * m_lineStep && w >= 4 * m_lineStep)
		ty -= m_lineStep;

	if (selected == cur) {
		m_nametipwnd.SetBgColor(RGB(0,0,0));
		m_nametipwnd.SetTextColor(RGB(255,255,255));
	}
	else if (cur->flags & 4) {
		m_nametipwnd.SetBgColor(BoxColors[(cur->depth & 7) + 8]);
		m_nametipwnd.SetTextColor(RGB(0,0,0));
	}
	else {
		m_nametipwnd.SetBgColor(BoxColors[cur->depth & 7]);
		m_nametipwnd.SetTextColor(RGB(0,0,0));
	}
	::SetWindowTextW(m_nametipwnd.m_hWnd, cur->name);
	m_nametipwnd.AutoSize();
	m_nametipwnd.MoveWindow(this, tx-2, ty-1);
	m_nametipwnd.PushOnScreen();

	ReleaseDC(pDC);
	m_nametipwnd.EnableWindow(1);
	m_nametipwnd.RedrawWindow();
}

static void AddMenuEntry(HMENU menu, UINT cmd = 0, const wchar_t *string = NULL,
	BOOL checked = 0, BOOL disabled = 0, BOOL bold = 0)
{
	MENUITEMINFO mii;
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_CHECKMARKS|MIIM_ID|MIIM_STATE|MIIM_TYPE;
	mii.fType = ((string != NULL) ? MFT_STRING : MFT_SEPARATOR);
	mii.fState = 0;
	if (bold) mii.fState |= MFS_DEFAULT;
	if (checked) mii.fState |= MFS_CHECKED;
	if (disabled) mii.fState |= MFS_GRAYED|MFS_DISABLED;
	mii.wID = cmd;
	mii.hSubMenu = NULL;
	mii.hbmpChecked = NULL;
	mii.hbmpUnchecked = NULL;
	mii.dwTypeData = (wchar_t *)string;
	if (string != NULL) mii.cch = (UINT)wcslen(string);
	else mii.cch = 0;

	::InsertMenuItem(menu, GetMenuItemCount(menu), TRUE, &mii);
}

void CFolderView::OnRButtonUp(UINT flags, CPoint point)
{
	const TreemapNode *cur = GetDisplayFolderFromPoint(point);
	SelectFolder(cur);
	ClientToScreen(&point);

	bool showinfo = m_infotipwnd.IsWindowEnabled();
	bool showname = m_nametipwnd.IsWindowEnabled();
	m_infotipwnd.EnableWindow(0);
	m_nametipwnd.EnableWindow(0);

	CSpaceMonger *app = (CSpaceMonger *)AfxGetApp();
	HMENU menu = ::CreatePopupMenu();
	::AddMenuEntry(menu, ID_VIEW_ZOOM_IN, CurLang->zoomin, 0, cur == NULL, (cur != NULL) && (cur->flags & 1) != 0);
	::AddMenuEntry(menu, ID_VIEW_ZOOM_OUT, CurLang->zoomout, 0, cur == NULL, 0);
	::AddMenuEntry(menu, ID_VIEW_ZOOM_FULL, CurLang->zoomfull, 0, cur == NULL, 0);
	::AddMenuEntry(menu);
	::AddMenuEntry(menu, ID_FILE_RUN, CurLang->run, 0, cur == NULL, (cur != NULL) && (cur->flags & 1) == 0);	// Show in Explorer
	::AddMenuEntry(menu, ID_FILE_DELETE, CurLang->del, 0,
		!SM_CanDeleteSelection(app->m_settings.disable_delete, cur != NULL), 0);
	::AddMenuEntry(menu);
	::AddMenuEntry(menu, ID_FILE_PROPERTIES, CurLang->properties, 0, cur == NULL, 0);
	::AddMenuEntry(menu);
	::AddMenuEntry(menu, ID_FILE_EXPORT, CurLang->export_list, 0, !IsAnythingOpen(), 0);
	::TrackPopupMenuEx(menu, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
		point.x, point.y, theApp.m_mainframe->m_hWnd, NULL);
	::DestroyMenu(menu);

	if (showname) m_nametipwnd.EnableWindow(1);
	if (showinfo) m_infotipwnd.EnableWindow(1);
}

void CFolderView::SelectFolder(const TreemapNode *cur)
{
	if (cur == selected) return;

	selected = cur;

	// See HighlightPathAtPoint: nodes can't be redrawn in isolation.
	InvalidateRect(NULL, FALSE);
	UpdateWindow();
	UpdateTitleBar();
}

static CString GetSizeString(ui64 size, ui64 totalspace, BOOL percent)
{
	CString result;

	if (totalspace == 0) totalspace = (ui64)(-1);

	if (percent) {
		size = (size * (ui64)1000) / totalspace;
		result.Format(CurLang->percent_format, (ui32)size / 10, (ui32)size % 10);
	}
	else {
		result = SM_FormatBytes(size).c_str();
	}
	return result;
}

void CFolderView::UpdateTitleBar(void)
{
	CMainFrame *mainfrm = (CMainFrame *)GetTopLevelFrame();
	if (mainfrm == NULL) return;

	mainfrm->m_toolbar.UpdateButtonsForView(this);

	CFolderTree *ft = (CFolderTree *)GetDocument();
	if (ft == NULL || rootfolder == NULL) {
		mainfrm->SetWindowText(L"SpaceMonger");
		return;
	}

	UpdatePathBar();

	CString title = L"";
	ui64 size;

	// A folder scan's path has no trailing backslash (a drive's has).
	CString base = ft->m_path;
	if (!base.IsEmpty() && base.Right(1) != L"\\") base += L"\\";

	if (selected != NULL && selected->name != NULL) {
		title += base;
		BuildTitleReverse(selected->source, title);
		title += selected->source->names[selected->index];
		size = selected->source->sizes[selected->index];
		title += L"  -  " + GetSizeString(size, ft->totalspace, 1)
			+ L"  -  " + GetSizeString(size, ft->totalspace, 0) + L"  -  ";
	}
	else if (rootfolder != NULL) {
		title += base;
		BuildTitleReverse(rootfolder, title);
		if (rootfolder->parent == NULL) size = ft->totalspace; // Kludge
		else size = rootfolder->SizeTotal();
		title += L"  -  " + GetSizeString(size, ft->totalspace, 0)
			+ L" " + CurLang->total + L"  -  " + GetSizeString(ft->freespace, ft->freespace, 0)
			+ L" " + CurLang->free + L"  -  ";
	}

	title += L"SpaceMonger";

	mainfrm->SetWindowText(title);
}	

// Path bar segments: the scanned root (C:\, D:\Fotos, \\PC01\c$), then each
// folder down to the one the map shows.
void CFolderView::UpdatePathBar(void)
{
	CMainFrame *frame = (CMainFrame *)GetTopLevelFrame();
	if (frame == NULL || !::IsWindow(frame->m_pathBar.m_hWnd)) return;
	std::vector<CPathBar::Segment> segments;
	CFolderTree *tree = (CFolderTree *)GetDocument();
	if (tree != NULL && rootfolder != NULL && !tree->m_path.IsEmpty()) {
		std::vector<CFolder *> chain;
		for (CFolder *f = rootfolder; f != NULL; f = f->parent) chain.push_back(f);
		std::wstring rootLabel = PathUtil::RemoveLongPathPrefix((LPCWSTR)tree->m_path);
		if (rootLabel.size() > 3 && rootLabel.back() == L'\\') rootLabel.pop_back();
		segments.push_back({ rootLabel, chain.back() });
		for (size_t i = chain.size() - 1; i-- > 0;) {
			CFolder *f = chain[i];
			segments.push_back({ f->parent->names[f->parentindex], f });
		}
	}
	frame->m_pathBar.SetSegments(segments);
}

void CFolderView::ZoomTo(CFolder *folder)
{
	if (folder == NULL || folder == rootfolder) return;
	rootfolder = folder;
	zoomlevel = 0;
	for (CFolder *parent = folder->parent; parent != NULL; parent = parent->parent) zoomlevel++;
	OnUpdate(GetDocument());
	UpdateTitleBar();
}

namespace {

bool IsPseudoName(const wchar_t *name)
{
	return name == NULL || name[0] == L'<' || name[0] == L'|';
}

// Case folding for the whole BMP, built once: the search runs over every
// name of a scan (millions on a system drive), and StrStrIW per name is
// too slow for that.
const wchar_t *FoldTable()
{
	static wchar_t *table = []() {
		wchar_t *t = new wchar_t[65536];
		for (int c = 0; c < 65536; ++c) t[c] = (wchar_t)c;
		t[0xD800] = 0xD800;
		::CharLowerBuffW(t + 1, 0xD7FF);	// surrogates stay as they are
		::CharLowerBuffW(t + 0xE000, 0x2000);
		return t;
	}();
	return table;
}

// needle is already folded.
bool ContainsFolded(const wchar_t *haystack, const std::wstring &needle, const wchar_t *fold)
{
	if (needle.empty()) return true;
	const wchar_t first = needle[0];
	const size_t n = needle.size();
	for (const wchar_t *p = haystack; *p != 0; ++p) {
		if (fold[*p] != first) continue;
		size_t i = 1;
		while (i < n && p[i] != 0 && fold[p[i]] == needle[i]) ++i;
		if (i == n) return true;
	}
	return false;
}

ui64 CountSearchMatches(const CFolder *folder, const std::wstring &needle, const wchar_t *fold)
{
	ui64 count = 0;
	for (unsigned int i = 0; i < folder->cur; ++i) {
		if (IsPseudoName(folder->names[i])) continue;
		if (ContainsFolded(folder->names[i], needle, fold)) ++count;
		if (folder->children[i] != NULL) count += CountSearchMatches(folder->children[i], needle, fold);
	}
	return count;
}

}

bool CFolderView::SearchMatches(const TreemapNode *cur) const
{
	const wchar_t *name = (cur->source != NULL && cur->index != (ui32)-1)
		? cur->source->names[cur->index] : cur->name;
	return !IsPseudoName(name) && ContainsFolded(name, m_searchFolded, FoldTable());
}

void CFolderView::SetSearch(const std::wstring &text)
{
	if (text == m_search) return;
	m_search = text;
	const wchar_t *fold = FoldTable();
	m_searchFolded = text;
	for (wchar_t &c : m_searchFolded) c = fold[c];
	ui64 matches = 0;
	CFolderTree *tree = (CFolderTree *)GetDocument();
	if (!m_search.empty() && tree != NULL && tree->GetRoot() != NULL) {
		CWaitCursor wait;
		matches = CountSearchMatches(tree->GetRoot(), m_searchFolded, fold);
	}
	CMainFrame *frame = (CMainFrame *)GetTopLevelFrame();
	if (frame != NULL) frame->m_pathBar.SetMatchCount(!m_search.empty(), matches);
	InvalidateRect(NULL, FALSE);
}

void CFolderView::BuildTitleReverse(CFolder *folder, CString &string)
{
	if (folder->parent != NULL)
		BuildTitleReverse(folder->parent, string);
	if (folder->parent == NULL) return;

	string += folder->parent->names[folder->parentindex];
	string += L"\\";
}

void CFolderView::BuildTitleReverseW(CFolder *folder, std::wstring& string)
{
	if (folder->parent != NULL)
		BuildTitleReverseW(folder->parent, string);
	if (folder->parent == NULL) return;

	PathUtil::AppendComponent(string, folder->parent->names[folder->parentindex]);
	if (!string.empty() && string.back() != L'\\')
		string += L'\\';
}

std::wstring CFolderView::BuildContainerPathW(const TreemapNode *folder)
{
	CFolderTree *tree = (CFolderTree *)GetDocument();
	std::wstring relativePath;

	if (folder != NULL)
		BuildTitleReverseW(folder->source, relativePath);

	return PathUtil::BuildWidePath(tree == NULL ? L"" : tree->m_path, relativePath, NULL);
}

std::wstring CFolderView::BuildItemPathW(const TreemapNode *folder)
{
	CFolderTree *tree = (CFolderTree *)GetDocument();
	std::wstring relativePath;
	const wchar_t *leafName = NULL;

	if (folder != NULL) {
		BuildTitleReverseW(folder->source, relativePath);
		if (folder->source != NULL && folder->index != (ui32)-1)
			leafName = folder->source->names[folder->index];
	}

	return PathUtil::BuildWidePath(tree == NULL ? L"" : tree->m_path, relativePath, leafName);
}

// A folder's title: its name, and its size in brackets when that still fits
// ("Users (248 GB)"). Files and boxes without size data show the bare name.
std::wstring CFolderView::FolderTitle(CDC *pDC, const TreemapNode *cur, int width)
{
	std::wstring name = cur->name != NULL ? cur->name : L"";
	if (!cur->IsFolder() || cur->source == NULL || cur->index == (ui32)-1) return name;
	std::wstring withSize = name + L"  (" + SM_FormatBytes(cur->source->sizes[cur->index]) + L")";
	SIZE size;
	if (::GetTextExtentPoint32W(pDC->GetSafeHdc(), withSize.c_str(), (int)withSize.size(), &size)
		&& size.cx <= width)
		return withSize;
	return name;
}

void CFolderView::OnDraw(CDC *pDC)
{
	pDC->SelectObject(&minifont);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextAlign(TA_LEFT | TA_TOP | TA_NOUPDATECP);
	pDC->SelectPalette(&m_palette, 0);
	pDC->RealizePalette();

	const SM_Theme &theme = SM_CurrentTheme();
	DrawBox(pDC->m_hDC, theme.mapBrush, 0, 0, m_width, m_height);
	FillBox(pDC->m_hDC, theme.mapBrush, 1, 1, m_width-2, m_height-2);

	for (const auto &cur : m_layoutNodes)
		MinimalDrawDisplayFolder(pDC, &cur, selected == &cur);

	// The selection outline goes on top of everything (children would
	// otherwise cover a folder's frame): a thick black frame, as in the
	// classic look, with a white inner edge so it also stands out against
	// dark neighbours.
	if (selected != NULL) {
		int t = max(3, MulDiv(3, (int)SM_GetWindowDpi(m_hWnd), 96));
		int x = selected->x, y = selected->y, w = selected->w + 1, h = selected->h + 1;
		if (w > 2 * t + 2 && h > 2 * t + 2) {
			HBRUSH frame = (HBRUSH)::GetStockObject(BLACK_BRUSH);
			HBRUSH edge = (HBRUSH)::GetStockObject(WHITE_BRUSH);
			FillBox(pDC->m_hDC, frame, x, y, w, t);
			FillBox(pDC->m_hDC, frame, x, y + h - t, w, t);
			FillBox(pDC->m_hDC, frame, x, y, t, h);
			FillBox(pDC->m_hDC, frame, x + w - t, y, t, h);
			DrawBox(pDC->m_hDC, edge, x + t, y + t, w - 2 * t, h - 2 * t);

			// The frame covers the folder title; write it again, white on black.
			if (selected->IsFolder() && selected->name != NULL) {
				int len = (int)wcslen(selected->name);
				HFONT font = SM_SelectFontForText(pDC->GetSafeHdc(),
					(HFONT)minifont.GetSafeHandle(), m_unicodeFonts, selected->name, len);
				HGDIOBJ oldFont = ::SelectObject(pDC->GetSafeHdc(), font);
				std::wstring label = FolderTitle(pDC, selected, w - 2 * max(m_titlePadX, t + 2));
				SIZE size;
				::GetTextExtentPoint32W(pDC->GetSafeHdc(), selected->name, len, &size);
				int bar = max(m_lineStep, t + size.cy + 2);
				FillBox(pDC->m_hDC, frame, x, y, w, min(h, bar));
				CRgn clip;
				clip.CreateRectRgn(x + t, y, x + w - t, y + min(h, bar));
				pDC->SelectClipRgn(&clip);
				COLORREF oldColor = pDC->SetTextColor(RGB(255, 255, 255));
				int tx = x + max(m_titlePadX, t + 2);
				int ty = y + max(0, (min(h, bar) - size.cy) / 2);
				::TextOutW(pDC->GetSafeHdc(), tx, ty, label.c_str(), (int)label.size());
				pDC->SetTextColor(oldColor);
				pDC->SelectClipRgn(NULL);
				::SelectObject(pDC->GetSafeHdc(), oldFont);
			}
		}
	}
}

// The Muted scheme: the classic palette, halfway to white.
static COLORREF SoftColor(COLORREF c)
{
	return RGB((GetRValue(c) + 255) / 2, (GetGValue(c) + 255) / 2, (GetBValue(c) + 255) / 2);
}

// System content (SystemPaths.h) is drawn washed out, so the eye goes to the
// folders where the user can actually free space.
static COLORREF MuteColor(COLORREF c)
{
	const bool dark = SM_CurrentTheme().dark;
	const int keep = dark ? 22 : 30, base = dark ? 50 : 228;
	return RGB((GetRValue(c) * keep + base * (100 - keep)) / 100,
		(GetGValue(c) * keep + base * (100 - keep)) / 100,
		(GetBValue(c) * keep + base * (100 - keep)) / 100);
}

// A placeholder block whose largest entry is a folder (too small to draw).
static bool SampleIsFolder(const TreemapNode *cur)
{
	return cur->index == (ui32)-1 && cur->sample != (ui32)-1 && cur->source != NULL
		&& cur->source->children[cur->sample] != NULL;
}

void CFolderView::MinimalDrawDisplayFolder(CDC *pDC, const TreemapNode *cur, BOOL sel)
{
	int x, y, w, h;

	x = cur->x, y = cur->y, w = cur->w + 1, h = cur->h + 1;

	const SM_Theme &theme = SM_CurrentTheme();
	// While searching, everything that does not match is washed out (system
	// content that matches is shown normally); otherwise system content is.
	const bool muted = cur->depth != -1 && (!m_search.empty()
		? !SearchMatches(cur) : (cur->flags & TREEMAP_FLAG_SYSTEM) != 0);
	const COLORREF mutedText = theme.dark ? RGB(165, 165, 165) : RGB(120, 120, 120);
	if (cur->depth != -1) {
		if (muted) {
			CBrush outline;
			outline.CreateSolidBrush(theme.dark ? RGB(90, 90, 90) : RGB(170, 170, 170));
			DrawBox(pDC->m_hDC, outline, x, y, w, h);
		}
		else DrawBox(pDC->m_hDC, black, x, y, w, h);
	}

	if (w > 2 && h > 2) {
		COLORREF color, bright, dark;
		CBrush brush, bbrush, dbrush;
		const int scheme = theApp.m_settings.color_scheme;

		if (sel && !(cur->flags & 2))
			color = bright = dark = RGB(0, 0, 0);
		else if (cur->depth != -1) {
			if (scheme == 2) {
				// Windows colours: plain dialog-face boxes.
				color = GetSysColor(COLOR_3DFACE);
				bright = GetSysColor(COLOR_3DHILIGHT);
				dark = GetSysColor(COLOR_3DSHADOW);
			}
			else if (scheme == 3 && !(cur->flags & 1) && !SampleIsFolder(cur)) {
				// By file type: files coloured by what they are; a block of
				// files too small to draw takes the type of its largest one.
				const ui32 entry = cur->index != (ui32)-1 ? cur->index : cur->sample;
				const wchar_t *name = (cur->source != NULL && entry != (ui32)-1)
					? cur->source->names[entry] : cur->name;
				SM_FileTypeColors(SM_ClassifyFileType(name), color, bright, dark);
			}
			else if (scheme == 3) {
				// ...with folders kept neutral, so the types stand out.
				int shade = (theme.dark ? 160 : 236) - (cur->depth % 4) * 8;
				color = RGB(shade, shade, shade);
				bright = RGB(250, 250, 250);
				dark = RGB(shade - 40, shade - 40, shade - 40);
			}
			else {
				color = BoxColors[cur->depth & 7];
				bright = BoxColors[(cur->depth & 7) + 8];
				dark = BoxColors[(cur->depth & 7) + 16];
				if (scheme == 1) {
					// Muted: the classic palette, softened.
					color = SoftColor(color);
					bright = SoftColor(bright);
					dark = SoftColor(dark);
				}
			}
			if (theApp.m_settings.rollover_box) {
				if (cur->flags & 4) dark = color, color = bright, bright = RGB(255,255,255);
				else bright = color, color = dark;
			}
		}
		else if (cur->flags & TREEMAP_FLAG_UNAVAILABLE)
			color = bright = dark = theme.dark ? RGB(75, 75, 75) : RGB(200, 200, 200);
		else if (cur->flags & TREEMAP_FLAG_PENDING)
			color = bright = dark = theme.mapBack;
		else color = bright = dark = theme.freeFill;	// free space: empty

		if (muted && !sel) {
			color = MuteColor(color);
			bright = MuteColor(bright);
			dark = MuteColor(dark);
		}

		brush.CreateSolidBrush(color);
		bbrush.CreateSolidBrush(bright);
		dbrush.CreateSolidBrush(dark);

		DrawDualBox(pDC->m_hDC, bbrush, dbrush, x + 1, y + 1, w - 2, h - 2);
		if (cur->flags & 1) {
			const int border = TREEMAP_FOLDER_BORDER;
			DrawBox(pDC->m_hDC, brush, x + 2, y + 2, w - 4, h - 4);
			FillBox(pDC->m_hDC, brush, x + border, y + border, w - 2 * border, m_lineStep - border);
			if (h > m_lineStep + border && w > 2 * border) {
				FillBox(pDC->m_hDC, theme.mapBrush, x + border, y + m_lineStep,
					w - 2 * border, h - m_lineStep - border);
				if (cur->flags & TREEMAP_FLAG_SCANNING) {
					// Still being read: a light hatch until its contents arrive.
					CBrush hatch;
					hatch.CreateHatchBrush(HS_BDIAGONAL, theme.dark ? RGB(80, 80, 80) : RGB(205, 205, 205));
					COLORREF oldBk = pDC->SetBkColor(theme.mapBack);
					int oldMode = pDC->SetBkMode(OPAQUE);
					FillBox(pDC->m_hDC, hatch, x + border, y + m_lineStep,
						w - 2 * border, h - m_lineStep - border);
					pDC->SetBkMode(oldMode);
					pDC->SetBkColor(oldBk);
				}
			}
		}
		else FillBox(pDC->m_hDC, brush, x + 2, y + 2, w - 4, h - 4);

		if (cur->depth == -1) {
			// Free, unreadable and pending blocks get a visible edge; the
			// unreadable one is hatched so it never reads as free space.
			CBrush edge;
			edge.CreateSolidBrush(theme.line);
			DrawBox(pDC->m_hDC, edge, x, y, w, h);
			if ((cur->flags & TREEMAP_FLAG_UNAVAILABLE) && w > 4 && h > 4) {
				CBrush hatch;
				hatch.CreateHatchBrush(HS_BDIAGONAL, RGB(150, 150, 150));
				COLORREF oldBk = pDC->SetBkColor(color);
				int oldMode = pDC->SetBkMode(OPAQUE);
				FillBox(pDC->m_hDC, hatch, x + 1, y + 1, w - 2, h - 2);
				pDC->SetBkMode(oldMode);
				pDC->SetBkColor(oldBk);
			}
		}
	}

	if (cur->flags & TREEMAP_FLAG_PENDING) {
		// Live scan: used space that has not been scanned yet. Hatch it so it
		// reads as "still to come" next to the solid free-space block.
		if (w > 4 && h > 4) {
			CBrush hatch;
			hatch.CreateHatchBrush(HS_BDIAGONAL, theme.line);
			COLORREF oldBk = pDC->SetBkColor(theme.mapBack);
			int oldMode = pDC->SetBkMode(OPAQUE);
			FillBox(pDC->m_hDC, hatch, x + 2, y + 2, w - 4, h - 4);
			pDC->SetBkMode(oldMode);
			pDC->SetBkColor(oldBk);
		}
		CSize labelSize = pDC->GetTextExtent(CurLang->scanning);
		if (labelSize.cx <= w - 4 && labelSize.cy <= h - 4) {

			int savedClip = pDC->SaveDC();
			pDC->IntersectClipRect(x, y, x + w, y + h);	// nests inside a caller's clip
			pDC->SetTextColor(theme.freeText);
			pDC->TextOut(x + (w - labelSize.cx) / 2, y + (h - labelSize.cy) / 2, CurLang->scanning);
			pDC->SetTextColor(RGB(0, 0, 0));
			pDC->RestoreDC(savedClip);
		}
		return;
	}

	if (cur->name != NULL) {

		int savedClip = pDC->SaveDC();
		pDC->IntersectClipRect(x, y, x + w, y + h);	// nests inside a caller's clip

		int len = (int)wcslen(cur->name);
		CSize size;
		int tx, ty;

		HFONT nameFont = SM_SelectFontForText(pDC->GetSafeHdc(),
			(HFONT)minifont.GetSafeHandle(), m_unicodeFonts, cur->name, len);
		HGDIOBJ oldNameFont = ::SelectObject(pDC->GetSafeHdc(), nameFont);
		::GetTextExtentPoint32W(pDC->GetSafeHdc(), cur->name, len, &size);
		::SelectObject(pDC->GetSafeHdc(), oldNameFont);
		if (size.cx > w - 2 || (cur->flags & 1)) tx = x + m_titlePadX;
		else tx = x + (w - size.cx) / 2;
		// Folder titles: vertically centred in the title bar below the frame.
		if ((cur->flags & 1) && m_textHeight > 9)
			ty = y + TREEMAP_FOLDER_BORDER + max(0, (m_lineStep - TREEMAP_FOLDER_BORDER - size.cy) / 2);
		else if (cur->flags & 1) ty = y + 1;
		else if (size.cy > h - 2) ty = y + 1;
		else ty = y + (h - size.cy) / 2;

		if ((cur->flags & TREEMAP_FLAG_SPECIAL) && !(cur->flags & TREEMAP_FLAG_UNAVAILABLE)) {
			// There's only one free-space block, so we can afford to
			// be a little less efficient with it.
			CFolderTree *ft = (CFolderTree *)GetDocument();
			if (ft != NULL) {
				CString string;
				ui64 ts = ft->totalspace;
				if (ts <= 1) ts = 1;
				si32 freepercent = (si32)(ft->freespace * (ui64)1000 / ts);
				string.Format(CurLang->freespace_format, freepercent / 10, freepercent % 10);
				// Two centred lines: the share free, and how much that is of the
				// drive. File and folder counts tell nobody anything here.
				const int step = m_lineStep;
				CString lines[2] = {
					string,
					CString(SM_FormatBytes(ft->freespace).c_str()) + L" " + CurLang->free + L" / "
						+ SM_FormatBytes(ft->totalspace).c_str() + L" " + CurLang->total,
				};
				pDC->SetTextColor(theme.freeText);
				for (int line = 0; line < 2; ++line) {
					size = pDC->GetTextExtent(lines[line]);
					tx = size.cx > w - 2 ? x + 2 : x + (w - size.cx) / 2;
					pDC->TextOut(tx, ty - step + line * step, lines[line]);
				}
				pDC->SetTextColor(RGB(0, 0, 0));
			}
		}
		else {
			if (sel) pDC->SetTextColor(RGB(0xFF,0xFF,0xFF));
			else if (muted) pDC->SetTextColor(mutedText);
			if (cur->flags & TREEMAP_FLAG_UNAVAILABLE) {
				// The block is an estimate, so show its label and size without
				// treating it as a real file path. Word-wrapped and centred:
				// the block is often narrow.
				CString text = cur->name + 1;
				CFolderTree *ft = (CFolderTree *)GetDocument();
				if (ft != NULL && cur->source != NULL && cur->index != (ui32)-1)
					text += L"\n" + GetSizeString(cur->source->sizes[cur->index], ft->totalspace, 0);
				CRect box(x + 4, y + 4, x + w - 4, y + h - 4);
				CRect measure = box;
				pDC->DrawText(text, &measure, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
				int offset = max(0, (box.Height() - measure.Height()) / 2);
				box.top += offset;
				pDC->SetTextColor(theme.dark ? RGB(210, 210, 210) : RGB(64, 64, 64));
				pDC->DrawText(text, &box, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
				pDC->SetTextColor(RGB(0, 0, 0));
				pDC->RestoreDC(savedClip);
				return;
			}
			if (!(cur->flags & 1) && h >= 3 * m_lineStep && w >= 4 * m_lineStep
				&& cur->source != NULL && cur->index != (ui32)-1) {
				// Enough room (probably) for the date and file size
				CString string;
				CSize size;
				string.Empty();
				PrintFileSize(string, cur->source->actualsizes[cur->index]);
				size = pDC->GetTextExtent(string);
				if (size.cx > w-2)
					pDC->TextOut(x+2, ty+1, string);
				else pDC->TextOut(x+(w-size.cx)/2, ty+1, string);
				string.Empty();
				PrintDate(string, cur->source->times[cur->index]);
				size = pDC->GetTextExtent(string);
				if (size.cx > w-2)
					pDC->TextOut(x+2, ty + m_lineStep - 1, string);
				else pDC->TextOut(x+(w-size.cx)/2, ty + m_lineStep - 1, string);
				ty -= m_lineStep;
			}
			oldNameFont = ::SelectObject(pDC->GetSafeHdc(), nameFont);
			std::wstring label = FolderTitle(pDC, cur, w - 2 * m_titlePadX);
			::TextOutW(pDC->GetSafeHdc(), tx, ty, label.c_str(), (int)label.size());
			::SelectObject(pDC->GetSafeHdc(), oldNameFont);
			if (sel || muted) pDC->SetTextColor(RGB(0,0,0));
		}
		pDC->RestoreDC(savedClip);
	}
}

BOOL CFolderView::OnEraseBkgnd(CDC* pDC)
{
	// Suppress background erasing to prevent flicker (we paint entire client area)
	return TRUE;
}

void CFolderView::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	int w = rect.Width();
	int h = rect.Height();
	if (w <= 0 || h <= 0) return;

	// The back buffer is kept between paints: at 4K, allocating a new one
	// for every animation frame costs more than drawing the map.
	if (m_backBuffer.m_hObject == NULL || m_backBufferSize.cx != w || m_backBufferSize.cy != h) {
		if (m_backBuffer.m_hObject != NULL) m_backBuffer.DeleteObject();
		m_backBuffer.CreateCompatibleBitmap(&dc, w, h);
		m_backBufferSize = CSize(w, h);
	}
	CDC memDC;
	memDC.CreateCompatibleDC(&dc);
	CBitmap *oldBitmap = memDC.SelectObject(&m_backBuffer);

	OnDraw(&memDC);

	if (m_xfadeActive && m_prevFrame.m_hObject != NULL && m_prevFrameSize == CSize(w, h)) {
		// Cross-fade: the previous picture on top, fading out (ease-in-out).
		double t = (double)(::GetTickCount() - m_xfadeStart) / m_xfadeMs;
		if (t < 1.0) {
			double e = 1.0 - (1.0 - t) * (1.0 - t);	// ease-out: change shows at once, settles softly
			CDC prev;
			prev.CreateCompatibleDC(&dc);
			CBitmap *oldPrev = prev.SelectObject(&m_prevFrame);
			BLENDFUNCTION blend = { AC_SRC_OVER, 0, (BYTE)(255.0 * (1.0 - e)), 0 };
			::AlphaBlend(memDC.m_hDC, 0, 0, w, h, prev.m_hDC, 0, 0, w, h, blend);
			prev.SelectObject(oldPrev);
		}
	}

	dc.BitBlt(0, 0, w, h, &memDC, 0, 0, SRCCOPY);
	memDC.SelectObject(oldBitmap);
}

void CFolderView::UpdateLiveScanLayout(AsyncScanEngine& engine, ui64 totalspace, ui64 freespace, bool wholeVolume)
{
	CRect rect;
	GetClientRect(&rect);
	m_width = rect.right - rect.left;
	m_height = rect.bottom - rect.top;

	if (m_width <= 0 || m_height <= 0) return;

	TreemapConfig config = MakeLayoutConfig();

	selected = NULL;
	lastcur = NULL;

	StartCrossfade(kCrossfadeMs);
	// Free space belongs to a drive; a folder scan shows only the folder.
	if (!wholeVolume) freespace = 0;
	engine.GenerateLiveLayout(m_width - 1, m_height - 1, totalspace, freespace, config,
		m_layoutNodes, m_liveNameStorage, wholeVolume);

	InvalidateRect(NULL, FALSE);
	UpdateWindow();
}
// Live frames, and the switch to the finished map, cross-fade briefly: just
// long enough to soften the change, and shorter than the live frame interval
// so a blend always finishes before the next one starts. Blends start from a
// clean, unblended picture; starting from a half-blended one stacks ghost
// images and looks blurred.
void CFolderView::StartCrossfade(int durationMs)
{
	if (m_backBuffer.m_hObject == NULL || !::IsWindow(m_hWnd)) return;	// nothing shown yet
	CClientDC screen(this);
	if (m_xfadeActive) {
		// The back buffer holds a blended picture; redraw the current one clean.
		CDC clean;
		clean.CreateCompatibleDC(&screen);
		CBitmap *oldClean = clean.SelectObject(&m_backBuffer);
		OnDraw(&clean);
		clean.SelectObject(oldClean);
	}
	if (m_prevFrame.m_hObject == NULL || m_prevFrameSize != m_backBufferSize) {
		if (m_prevFrame.m_hObject != NULL) m_prevFrame.DeleteObject();
		m_prevFrame.CreateCompatibleBitmap(&screen, m_backBufferSize.cx, m_backBufferSize.cy);
		m_prevFrameSize = m_backBufferSize;
	}
	CDC src, dst;
	src.CreateCompatibleDC(&screen);
	dst.CreateCompatibleDC(&screen);
	CBitmap *oldSrc = src.SelectObject(&m_backBuffer);
	CBitmap *oldDst = dst.SelectObject(&m_prevFrame);
	dst.BitBlt(0, 0, m_backBufferSize.cx, m_backBufferSize.cy, &src, 0, 0, SRCCOPY);
	src.SelectObject(oldSrc);
	dst.SelectObject(oldDst);
	m_xfadeActive = true;
	m_xfadeStart = ::GetTickCount();
	m_xfadeMs = durationMs;
	if (m_animTimer == 0) m_animTimer = SetTimer(kAnimationTimer, 15, NULL);
}

void CFolderView::OnTimer(UINT_PTR id)
{
	if (id != kAnimationTimer) {
		CFreeView::OnTimer(id);
		return;
	}
	if (!m_xfadeActive || ::GetTickCount() - m_xfadeStart >= (DWORD)m_xfadeMs) {
		m_xfadeActive = false;
		KillTimer(m_animTimer);
		m_animTimer = 0;
	}
	InvalidateRect(NULL, FALSE);
	UpdateWindow();
}
void CFolderView::ClearLiveScanLayout(void)
{
	m_layoutNodes.clear();
	m_liveNameStorage.clear();
	selected = NULL;
	lastcur = NULL;

	if (::IsWindow(m_hWnd)) {
		InvalidateRect(NULL, FALSE);
		UpdateWindow();
	}
}

void CFolderView::SetDocument(CFreeDoc *doc)
{
	// From the last live frame to the finished map (or back to empty): blend.
	StartCrossfade(kFinalCrossfadeMs);
	CFreeView::SetDocument(doc);

	if (doc != NULL)
		rootfolder = ((CFolderTree *)doc)->GetRoot();
	else rootfolder = NULL;
	zoomlevel = 0;

	// A running search counts the matches of the new scan.
	std::wstring search;
	search.swap(m_search);
	SetSearch(search);

	UpdateTitleBar();
	OnSize(0, m_width, m_height);
	// Through OnPaint's back buffer; drawing into the window DC flickers.
	InvalidateRect(NULL, FALSE);
	UpdateWindow();
}

void CFolderView::OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized)
{
	if (nState == WA_ACTIVE || nState == WA_CLICKACTIVE)
		SetPalette();
}

void CFolderView::SetPalette(void)
{
	CDC *dc = GetDC();
	dc->SelectPalette(&m_palette, 0);
	dc->RealizePalette();
	ReleaseDC(dc);
}

void CFolderView::OnUpdate(CFreeDoc *doc)
{
	UpdateTitleBar();

	OnSize(0, m_width, m_height);

	InvalidateRect(NULL, FALSE);
	UpdateWindow();

	m_infotipwnd.SetShowDelay(theApp.m_settings.infotip_delay);
	m_nametipwnd.SetShowDelay(theApp.m_settings.nametip_delay);
}

void CFolderView::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;
	GetClientRect(&rect);
	m_width = cx = rect.right - rect.left;
	m_height = cy = rect.bottom - rect.top;

	m_layoutNodes.clear();
	m_liveNameStorage.clear();
	selected = NULL;
	lastcur = NULL;

	if (rootfolder == NULL) {
		if (GetDocument() == NULL) return;
		rootfolder = ((CFolderTree *)GetDocument())->GetRoot();
		zoomlevel = 0;
		if (rootfolder == NULL) return;
	}

	TreemapConfig config = MakeLayoutConfig();

	CFolderTree *tree = (CFolderTree *)GetDocument();
	if (zoomlevel == 0 && tree != NULL && tree->wholeVolume && rootfolder == tree->GetRoot())
		TreemapEngine::ComputeVolumeLayout(0, 0, cx - 1, cy - 1, rootfolder, config, m_layoutNodes);
	else
		TreemapEngine::ComputeLayout(0, 0, cx - 1, cy - 1, rootfolder, zoomlevel, config, m_layoutNodes);
}

void CFolderView::ZoomIn(const TreemapNode *folder)
{
	CFolderTree *doc = (CFolderTree *)GetDocument();
	CFolder *oldroot = rootfolder;
	if (folder == NULL) return;

	if (folder != NULL && folder->source != NULL && folder->index != (ui32)-1 && folder->source->children[folder->index] != NULL)
		rootfolder = folder->source->children[folder->index];

	if (rootfolder == oldroot) return;

	CRect start(folder->x, folder->y,
		folder->x+folder->w, folder->y+folder->h);
	CRect end;
	GetClientRect(&end);
	end.OffsetRect(-end.left, -end.top);
	if (theApp.m_settings.animated_zoom) AnimateBox(start, end);

	CFolder *parent = rootfolder->parent;
	zoomlevel = 0;
	while (parent != NULL) zoomlevel++, parent = parent->parent;
	OnUpdate(doc);
	UpdateTitleBar();
}

void CFolderView::ZoomOut(void)
{
	CFolderTree *doc = (CFolderTree *)GetDocument();
	CFolder *oldroot = rootfolder;

	if (rootfolder != NULL && rootfolder->parent != NULL)
		rootfolder = rootfolder->parent;

	if (rootfolder == oldroot) return;

	CRect start;
	GetClientRect(&start);
	start.OffsetRect(-start.left, -start.top);
	CRect end;
	end.left = end.right = start.right / 2;
	end.top = end.bottom = start.bottom / 2;
	if (theApp.m_settings.animated_zoom) AnimateBox(start, end);

	zoomlevel--;
	OnUpdate(doc);
	UpdateTitleBar();
}

void CFolderView::ZoomFull(void)
{
	CFolderTree *doc = (CFolderTree *)GetDocument();
	CFolder *oldroot = rootfolder;

	if (doc != NULL)
		rootfolder = ((CFolderTree *)doc)->GetRoot();
	else rootfolder = NULL;

	if (rootfolder == oldroot) return;

	CRect start;
	GetClientRect(&start);
	start.OffsetRect(-start.left, -start.top);
	CRect end;
	end.left = end.right = start.right / 2;
	end.top = end.bottom = start.bottom / 2;
	if (theApp.m_settings.animated_zoom) AnimateBox(start, end);

	zoomlevel = 0;
	OnUpdate(doc);
	UpdateTitleBar();
}

void CFolderView::ShowFreeSpace(BOOL shown)
{
	if (showfreespace == shown) return;
	showfreespace = shown;
	OnUpdate(GetDocument());
}

static void ComputeNewRect(CRect &result, const CRect &start, const CRect &end, ui32 max, ui32 step)
{
	if (max <= 0) max = 1;
	result.left = ((start.left * (max - step)) + (end.left * step)) / max;
	result.top = ((start.top * (max - step)) + (end.top * step)) / max;
	result.right = ((start.right * (max - step)) + (end.right * step)) / max;
	result.bottom = ((start.bottom * (max - step)) + (end.bottom * step)) / max;
}

static void MinimalRect(CDC *dc, const CRect &rect)
{
	dc->MoveTo(rect.left, rect.top);
	dc->LineTo(rect.right, rect.top);
	dc->LineTo(rect.right, rect.bottom);
	dc->LineTo(rect.left, rect.bottom);
	dc->LineTo(rect.left, rect.top);
}

void CFolderView::AnimateBox(const CRect &start, const CRect &end)
{
	// Total time of animation: 400 milliseconds + render time, or
	// about a half a second.

	ui32 step;
	CRect rect = start;
	CDC *dc = GetDC();
	dc->SetROP2(R2_NOT);
	for (step = 0; step < 8; step++) {
		ComputeNewRect(rect, start, end, 8, step);
		MinimalRect(dc, rect);
		Sleep(25);
	}
	for (step = 0; step < 8; step++) {
		ComputeNewRect(rect, start, end, 8, step);
		MinimalRect(dc, rect);
		Sleep(25);
	}
	dc->SetROP2(R2_COPYPEN);
	ReleaseDC(dc);
}

void CFolderView::OnIgnoreUpdate(CCmdUI *ui)
{
	GeneralIgnoreUpdate(ui);
}
