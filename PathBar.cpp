#include "stdafx.h"
#include "spacemonger.h"
#include "PathBar.h"
#include "FolderView.h"
#include "FileTypes.h"
#include "TextFormat.h"
#include "Theme.h"
#include "Lang.h"
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace {

const UINT kSearchId = 1;
const UINT_PTR kSearchTimer = 1;
const UINT kSearchDelayMs = 180;	// search once typing pauses, not per key
const wchar_t kSeparator[] = L"  ›  ";	// single right-pointing angle quotation mark
const wchar_t kEllipsis[] = L"…";

CFolderView *View()
{
	return (CFolderView *)theApp.m_view;
}

int TextWidth(CDC &dc, const std::wstring &text)
{
	SIZE size = { 0, 0 };
	::GetTextExtentPoint32W(dc.m_hDC, text.c_str(), (int)text.size(), &size);
	return size.cx;
}

}

BEGIN_MESSAGE_MAP(CPathBar, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_WM_CTLCOLOR()
	ON_EN_CHANGE(kSearchId, OnSearchChanged)
	ON_WM_TIMER()
END_MESSAGE_MAP()

CPathBar::CPathBar()
	: m_font(NULL), m_dpi(96), m_hot(-1), m_tracking(false), m_searching(false), m_matches(0)
{
}

CPathBar::~CPathBar()
{
}

BOOL CPathBar::Create(CWnd *parent)
{
	CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL, IDC_ARROW));
	if (!CWnd::Create(cls, L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, CRect(0, 0, 1, 1), parent, 0x5101))
		return FALSE;
	if (!m_search.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
			CRect(0, 0, 1, 1), this, kSearchId))
		return FALSE;
	Relabel();
	ApplyTheme();
	return TRUE;
}

void CPathBar::Relabel(void)
{
	if (::IsWindow(m_search.m_hWnd))
		::SendMessageW(m_search.m_hWnd, EM_SETCUEBANNER, TRUE, (LPARAM)CurLang->search_cue);
	Invalidate();
}

void CPathBar::ApplyTheme(void)
{
	if (::IsWindow(m_search.m_hWnd)) {
		::SetWindowTheme(m_search.m_hWnd, SM_CurrentTheme().dark ? L"DarkMode_CFD" : NULL, NULL);
		m_search.Invalidate();
	}
	Invalidate();
}

void CPathBar::SetUiFont(HFONT font, UINT dpi)
{
	m_font = font;
	m_dpi = dpi != 0 ? dpi : 96;
	if (::IsWindow(m_search.m_hWnd)) m_search.SendMessage(WM_SETFONT, (WPARAM)font, TRUE);
	LayoutChildren();
	Invalidate();
}

int CPathBar::GetDesiredHeight(void) const
{
	TEXTMETRICW tm = {};
	HDC dc = ::GetDC(NULL);
	HGDIOBJ old = ::SelectObject(dc, m_font != NULL ? m_font : ::GetStockObject(DEFAULT_GUI_FONT));
	::GetTextMetricsW(dc, &tm);
	::SelectObject(dc, old);
	::ReleaseDC(NULL, dc);
	return tm.tmHeight + MulDiv(16, (int)m_dpi, 96);
}

void CPathBar::SetSegments(const std::vector<Segment> &segments)
{
	bool same = segments.size() == m_segments.size();
	for (size_t i = 0; same && i < segments.size(); ++i)
		same = segments[i].folder == m_segments[i].folder && segments[i].label == m_segments[i].label;
	if (same) return;
	m_segments = segments;
	m_hot = -1;
	Invalidate();
}

void CPathBar::SetMatchCount(bool searching, ui64 matches)
{
	m_searching = searching;
	m_matches = matches;
	Invalidate();
}

void CPathBar::FocusSearch(void)
{
	if (!::IsWindow(m_search.m_hWnd)) return;
	m_search.SetFocus();
	m_search.SetSel(0, -1);
}

void CPathBar::ClearSearch(void)
{
	if (::IsWindow(m_search.m_hWnd)) m_search.SetWindowText(L"");
}

void CPathBar::LayoutChildren(void)
{
	if (!::IsWindow(m_search.m_hWnd)) return;
	CRect rect;
	GetClientRect(&rect);
	int height = max(1, GetDesiredHeight() - Scale(8));
	int width = min(Scale(260), max(Scale(120), rect.Width() / 4));
	m_search.SetWindowPos(NULL, rect.right - width - Scale(8), (rect.Height() - height) / 2,
		width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CPathBar::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutChildren();
	Invalidate();
}

BOOL CPathBar::OnEraseBkgnd(CDC *pDC)
{
	return TRUE;
}

void CPathBar::OnPaint()
{
	CPaintDC paint(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty()) return;
	const SM_Theme &theme = SM_CurrentTheme();

	CDC dc;
	dc.CreateCompatibleDC(&paint);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&paint, rect.Width(), rect.Height());
	CBitmap *oldBitmap = dc.SelectObject(&bitmap);
	dc.FillSolidRect(&rect, theme.face);
	dc.FillSolidRect(0, rect.bottom - 1, rect.Width(), 1, theme.line);
	HGDIOBJ oldFont = ::SelectObject(dc.m_hDC, m_font != NULL ? m_font : ::GetStockObject(DEFAULT_GUI_FONT));
	dc.SetBkMode(TRANSPARENT);

	CRect searchRect(rect.right, 0, rect.right, 0);
	if (::IsWindow(m_search.m_hWnd)) {
		m_search.GetWindowRect(&searchRect);
		ScreenToClient(&searchRect);
	}
	int right = searchRect.left - Scale(10);
	TEXTMETRICW tm = {};
	::GetTextMetricsW(dc.m_hDC, &tm);
	int textTop = (rect.Height() - tm.tmHeight) / 2;

	// Match count left of the search box.
	if (m_searching) {
		CString count;
		count.Format(CurLang->search_result_format, SM_FormatCount(m_matches).c_str());
		int w = TextWidth(dc, (LPCWSTR)count);
		dc.SetTextColor(theme.grayText);
		::TextOutW(dc.m_hDC, right - w, textTop, count, count.GetLength());
		right -= w + Scale(16);
	}

	// Legend for "By file type", if it fits beside a short path.
	if (theApp.m_settings.color_scheme == 3) {
		int box = Scale(10), gap = Scale(5), spacing = Scale(12);
		int legendWidth = 0;
		for (int i = 0; i < SM_TYPE_COUNT; ++i)
			legendWidth += box + gap + TextWidth(dc, CurLang->filetype_labels[i]) + spacing;
		if (legendWidth < right - Scale(200)) {
			int x = right - legendWidth;
			for (int i = 0; i < SM_TYPE_COUNT; ++i) {
				COLORREF color, bright, dark;
				SM_FileTypeColors((SM_FileType)i, color, bright, dark);
				int top = (rect.Height() - box) / 2;
				dc.FillSolidRect(x, top, box, box, dark);
				dc.FillSolidRect(x + 1, top + 1, box - 2, box - 2, color);
				x += box + gap;
				dc.SetTextColor(theme.text);
				const wchar_t *label = CurLang->filetype_labels[i];
				::TextOutW(dc.m_hDC, x, textTop, label, (int)wcslen(label));
				x += TextWidth(dc, label) + spacing;
			}
			right -= legendWidth + Scale(8);
		}
	}

	// The path; when it is too long the first segment stays and the middle
	// collapses into "…".
	m_segmentRects.assign(m_segments.size(), CRect(0, 0, 0, 0));
	int left = Scale(10);
	int sepWidth = TextWidth(dc, kSeparator);
	std::vector<int> widths;
	int total = 0;
	for (const Segment &s : m_segments) {
		widths.push_back(TextWidth(dc, s.label) + Scale(8));
		total += widths.back();
	}
	total += sepWidth * (int)max((size_t)1, m_segments.size()) - sepWidth;
	size_t skipTo = 1;	// first segment shown after the root
	int ellipsisWidth = TextWidth(dc, kEllipsis) + sepWidth;
	while (m_segments.size() > 2 && skipTo < m_segments.size() - 1 && left + total > right) {
		total -= widths[skipTo] + sepWidth;
		if (skipTo == 1) total += ellipsisWidth;
		++skipTo;
	}
	int x = left;
	for (size_t i = 0; i < m_segments.size(); ++i) {
		if (i > 0 && i < skipTo && skipTo > 1) {
			if (i == 1) {
				dc.SetTextColor(theme.grayText);
				::TextOutW(dc.m_hDC, x, textTop, kSeparator, (int)wcslen(kSeparator));
				x += sepWidth;
				::TextOutW(dc.m_hDC, x, textTop, kEllipsis, 1);
				x += TextWidth(dc, kEllipsis);
			}
			continue;
		}
		if (i > 0) {
			dc.SetTextColor(theme.grayText);
			::TextOutW(dc.m_hDC, x, textTop, kSeparator, (int)wcslen(kSeparator));
			x += sepWidth;
		}
		CRect segmentRect(x, Scale(4), min(x + widths[i], right), rect.bottom - Scale(4));
		bool last = i + 1 == m_segments.size();
		if ((int)i == m_hot && !last) {
			CBrush hot(theme.dark ? RGB(70, 70, 70) : RGB(218, 218, 218));
			CPen none(PS_NULL, 0, RGB(0, 0, 0));
			CBrush *oldBrush = dc.SelectObject(&hot);
			CPen *oldPen = dc.SelectObject(&none);
			dc.RoundRect(&segmentRect, CPoint(Scale(6), Scale(6)));
			dc.SelectObject(oldPen);
			dc.SelectObject(oldBrush);
		}
		dc.SetTextColor(last ? theme.text : theme.grayText);
		CRect textRect(segmentRect.left + Scale(4), textTop, segmentRect.right, textTop + tm.tmHeight);
		::DrawTextW(dc.m_hDC, m_segments[i].label.c_str(), (int)m_segments[i].label.size(), &textRect,
			DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		m_segmentRects[i] = segmentRect;
		x += widths[i];
	}

	::SelectObject(dc.m_hDC, oldFont);
	paint.BitBlt(0, 0, rect.Width(), rect.Height(), &dc, 0, 0, SRCCOPY);
	dc.SelectObject(oldBitmap);
}

int CPathBar::HitSegment(CPoint point) const
{
	// The last segment is where you are; only the ones above it are links.
	for (size_t i = 0; i + 1 < m_segmentRects.size(); ++i)
		if (!m_segmentRects[i].IsRectEmpty() && m_segmentRects[i].PtInRect(point)) return (int)i;
	return -1;
}

void CPathBar::OnMouseMove(UINT flags, CPoint point)
{
	if (!m_tracking) {
		TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
		m_tracking = ::TrackMouseEvent(&tme) != FALSE;
	}
	int hot = HitSegment(point);
	if (hot != m_hot) {
		m_hot = hot;
		Invalidate();
	}
	CWnd::OnMouseMove(flags, point);
}

void CPathBar::OnMouseLeave()
{
	m_tracking = false;
	if (m_hot != -1) {
		m_hot = -1;
		Invalidate();
	}
}

BOOL CPathBar::OnSetCursor(CWnd *wnd, UINT hitTest, UINT message)
{
	if (wnd == this && hitTest == HTCLIENT) {
		CPoint point;
		::GetCursorPos(&point);
		ScreenToClient(&point);
		::SetCursor(::LoadCursor(NULL, HitSegment(point) >= 0 ? IDC_HAND : IDC_ARROW));
		return TRUE;
	}
	return CWnd::OnSetCursor(wnd, hitTest, message);
}

void CPathBar::OnLButtonUp(UINT flags, CPoint point)
{
	int hit = HitSegment(point);
	if (hit >= 0 && View() != NULL) View()->ZoomTo(m_segments[hit].folder);
	CWnd::OnLButtonUp(flags, point);
}

HBRUSH CPathBar::OnCtlColor(CDC *pDC, CWnd *wnd, UINT ctlColor)
{
	const SM_Theme &theme = SM_CurrentTheme();
	if (ctlColor == CTLCOLOR_EDIT && theme.dark) {
		pDC->SetTextColor(theme.text);
		pDC->SetBkColor(theme.editBack);
		return theme.editBrush;
	}
	return CWnd::OnCtlColor(pDC, wnd, ctlColor);
}

void CPathBar::OnSearchChanged()
{
	// Clearing the box ends the search at once; typing waits for a pause.
	if (m_search.GetWindowTextLength() == 0) {
		KillTimer(kSearchTimer);
		if (View() != NULL) View()->SetSearch(L"");
		return;
	}
	SetTimer(kSearchTimer, kSearchDelayMs, NULL);
}

void CPathBar::OnTimer(UINT_PTR id)
{
	if (id != kSearchTimer) {
		CWnd::OnTimer(id);
		return;
	}
	KillTimer(kSearchTimer);
	CString text;
	m_search.GetWindowText(text);
	text.Trim();
	if (View() != NULL) View()->SetSearch((LPCWSTR)text);
}

BOOL CPathBar::PreTranslateMessage(MSG *msg)
{
	if (msg->message == WM_KEYDOWN && msg->hwnd == m_search.m_hWnd) {
		if (msg->wParam == VK_ESCAPE) {
			ClearSearch();
			if (View() != NULL) View()->SetFocus();
			return TRUE;
		}
		if (msg->wParam == VK_RETURN) {
			OnTimer(kSearchTimer);	// search now
			if (View() != NULL) View()->SetFocus();
			return TRUE;
		}
	}
	return CWnd::PreTranslateMessage(msg);
}
