#include "stdafx.h"
#include "spacemonger.h"
#include "ScanStatusBar.h"
#include "Theme.h"
#include "PathUtil.h"
#include "Lang.h"
#include "TextFormat.h"
#include <cmath>

namespace {

const UINT kCancelId = 0x7F10;
const int kProgressRange = 1000;

CString FormatCount(ui64 value) { return CString(SM_FormatCount(value).c_str()); }

// "Scanning Disk..." without the dots, as a label in front of the path.
CString ScanningLabel(void)
{
	CString label = CurLang->scanning;
	label.TrimRight(L'.');
	return label;
}

CString FormatBytes(ui64 bytes) { return CString(SM_FormatBytes(bytes).c_str()); }

}

BEGIN_MESSAGE_MAP(CScanStatusBar, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_BN_CLICKED(kCancelId, OnCancelClicked)
END_MESSAGE_MAP()

CScanStatusBar::CScanStatusBar()
{
	m_font = NULL;
	m_dpi = 96;
	m_cancelRequested = FALSE;
}

CScanStatusBar::~CScanStatusBar()
{
}

BOOL CScanStatusBar::Create(CWnd *parent)
{
	CString cls = AfxRegisterWndClass(0, ::LoadCursor(NULL, IDC_ARROW), NULL, NULL);
	if (!CWnd::Create(cls, L"", WS_CHILD | WS_CLIPCHILDREN, CRect(0, 0, 0, 0), parent, 0x7F0F))
		return FALSE;
	if (!m_progress.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), this, 0x7F11))
		return FALSE;
	m_progress.SetRange32(0, kProgressRange);
	if (!m_cancel.Create(L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		CRect(0, 0, 0, 0), this, kCancelId))
		return FALSE;
	return TRUE;
}

int CScanStatusBar::TextHeight(void) const
{
	int height = MulDiv(16, (int)m_dpi, 96);
	HDC dc = ::GetDC(m_hWnd);
	if (dc != NULL) {
		HGDIOBJ old = ::SelectObject(dc, m_font != NULL ? m_font : ::GetStockObject(DEFAULT_GUI_FONT));
		TEXTMETRICW tm;
		if (::GetTextMetricsW(dc, &tm)) height = tm.tmHeight;
		::SelectObject(dc, old);
		::ReleaseDC(m_hWnd, dc);
	}
	return height;
}

int CScanStatusBar::GetDesiredHeight(void) const
{
	if (!::IsWindow(m_hWnd) || !IsWindowVisible()) return 0;
	int pad = MulDiv(8, (int)m_dpi, 96);
	int gap = MulDiv(6, (int)m_dpi, 96);
	int bar = MulDiv(4, (int)m_dpi, 96);
	return pad + TextHeight() + gap + bar + gap + TextHeight() + pad;
}

void CScanStatusBar::Begin(const CString &root, HFONT font, UINT dpi)
{
	m_font = font;
	m_dpi = dpi != 0 ? dpi : 96;
	m_cancelRequested = FALSE;
	m_root = root;
	m_title.Format(L"%s  %s", (LPCWSTR)ScanningLabel(), (LPCWSTR)root);
	m_detail.Empty();
	m_note.Empty();
	m_path.clear();
	if (m_font != NULL) m_cancel.SendMessage(WM_SETFONT, (WPARAM)m_font, FALSE);
	CString cancel = CurLang->cancel;
	m_cancel.SetWindowText(cancel);
	m_cancel.EnableWindow(TRUE);
	m_progress.SetPos(0);
	ShowWindow(SW_SHOW);
	LayoutChildren();
	Invalidate(FALSE);
}

void CScanStatusBar::Update(ui64 files, ui64 folders, ui64 bytes, ui64 usedspace,
	const std::wstring &currentPath, bool complete)
{
	if (!::IsWindow(m_hWnd)) return;

	int pos = 0;
	if (complete) {
		pos = kProgressRange;
	} else if (usedspace > 0 && bytes > 0) {
		double ratio = (double)bytes / (double)usedspace;
		if (ratio > 1.0) ratio = 1.0;
		// ratio^1.5: large files found early must not fill the bar before
		// the directory walk is anywhere near done.
		pos = (int)(pow(ratio, 1.5) * (kProgressRange * 0.98));
	}
	m_progress.SetPos(pos);

	m_title.Format(L"%s  %s  \x2013  %d %%", (LPCWSTR)ScanningLabel(), (LPCWSTR)m_root,
		(pos * 100) / kProgressRange);
	m_detail = FormatCount(files) + L" " + CurLang->files + L"  \x00B7  "
		+ FormatCount(folders) + L" " + CurLang->folders + L"  \x00B7  " + FormatBytes(bytes);
	m_path = PathUtil::RemoveLongPathPrefix(currentPath);
	if (m_cancelRequested) m_cancel.EnableWindow(FALSE);

	// Only the text area; the progress bar and button paint themselves.
	CRect rect;
	GetClientRect(&rect);
	InvalidateRect(&rect, FALSE);
}

void CScanStatusBar::SetNote(const CString &note)
{
	m_note = note;
	if (::IsWindow(m_hWnd)) Invalidate(FALSE);
}

void CScanStatusBar::End(void)
{
	if (!::IsWindow(m_hWnd)) return;
	ShowWindow(SW_HIDE);
}

void CScanStatusBar::OnCancelClicked()
{
	m_cancelRequested = TRUE;
	m_cancel.EnableWindow(FALSE);
}

void CScanStatusBar::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutChildren();
}

void CScanStatusBar::LayoutChildren(void)
{
	if (!::IsWindow(m_progress.m_hWnd)) return;
	CRect rect;
	GetClientRect(&rect);
	int pad = MulDiv(12, (int)m_dpi, 96);
	int gap = MulDiv(6, (int)m_dpi, 96);
	int text = TextHeight();
	int bar = MulDiv(4, (int)m_dpi, 96);
	int buttonW = MulDiv(96, (int)m_dpi, 96);
	int buttonH = text + MulDiv(12, (int)m_dpi, 96);

	m_cancel.MoveWindow(rect.right - pad - buttonW, (rect.Height() - buttonH) / 2, buttonW, buttonH);
	int barTop = MulDiv(8, (int)m_dpi, 96) + text + gap;
	m_progress.MoveWindow(pad, barTop, max(0, rect.right - 3 * pad - buttonW), bar);
}

BOOL CScanStatusBar::OnEraseBkgnd(CDC *pDC)
{
	return TRUE;
}

void CScanStatusBar::OnPaint()
{
	CPaintDC paint(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty()) return;

	// Double-buffered so the per-tick text update never flickers.
	CDC mem;
	mem.CreateCompatibleDC(&paint);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&paint, rect.Width(), rect.Height());
	CBitmap *oldBitmap = mem.SelectObject(&bitmap);

	const SM_Theme &theme = SM_CurrentTheme();
	mem.FillSolidRect(&rect, theme.face);
	mem.FillSolidRect(0, 0, rect.Width(), 1, theme.line);

	HGDIOBJ oldFont = ::SelectObject(mem.m_hDC, m_font != NULL ? m_font : ::GetStockObject(DEFAULT_GUI_FONT));
	mem.SetBkMode(TRANSPARENT);

	int pad = MulDiv(12, (int)m_dpi, 96);
	int top = MulDiv(8, (int)m_dpi, 96);
	int gap = MulDiv(6, (int)m_dpi, 96);
	int text = TextHeight();
	int bar = MulDiv(4, (int)m_dpi, 96);
	int buttonW = MulDiv(96, (int)m_dpi, 96);
	int right = rect.right - 3 * pad - buttonW + pad;

	// Line 1: what is scanned and how far, counts and size on the right.
	CRect titleRect(pad, top, right, top + text);
	mem.SetTextColor(theme.text);
	mem.DrawText(m_title, &titleRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	mem.SetTextColor(theme.grayText);
	mem.DrawText(m_detail, &titleRect, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);

	// Line 2 (below the bar): the folder being read.
	int pathTop = top + text + gap + bar + gap;
	CRect pathRect(pad, pathTop, right, pathTop + text);
	if (!m_note.IsEmpty()) {
		// The note keeps its place; the current folder takes what is left.
		CSize noteSize = mem.GetTextExtent(m_note + L"  \x00B7  ");
		CRect noteRect(pad, pathTop, min(right, pad + noteSize.cx), pathTop + text);
		mem.DrawText(m_note + L"  \x00B7  ", &noteRect, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		pathRect.left = noteRect.right;
	}
	mem.DrawText(m_path.c_str(), (int)m_path.size(), &pathRect,
		DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS);

	::SelectObject(mem.m_hDC, oldFont);
	paint.BitBlt(0, 0, rect.Width(), rect.Height(), &mem, 0, 0, SRCCOPY);
	mem.SelectObject(oldBitmap);
}
