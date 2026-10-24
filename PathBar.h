#ifndef PATHBAR_H
#define PATHBAR_H

#include <string>
#include <vector>

struct CFolder;

// Strip between the toolbar and the map: where you are (clickable path,
// C:\ › Users › Anna), the colour legend when the map is coloured by file
// type, and the search box (Ctrl+F) with its match count.
class CPathBar : public CWnd {
public:
	CPathBar();
	virtual ~CPathBar();

	BOOL Create(CWnd *parent);
	void SetUiFont(HFONT font, UINT dpi);
	int GetDesiredHeight(void) const;

	// Path segments from the scanned root to the zoomed folder.
	struct Segment {
		std::wstring label;
		CFolder *folder;
	};
	void SetSegments(const std::vector<Segment> &segments);
	void SetMatchCount(bool searching, ui64 matches);
	void FocusSearch(void);
	void ClearSearch(void);
	void ApplyTheme(void);
	void Relabel(void);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT flags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonUp(UINT flags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd *wnd, UINT hitTest, UINT message);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *wnd, UINT ctlColor);
	afx_msg void OnSearchChanged();
	afx_msg void OnSearchFocusChanged();
	afx_msg void OnTimer(UINT_PTR id);
	virtual BOOL PreTranslateMessage(MSG *msg);
	DECLARE_MESSAGE_MAP()

	void LayoutChildren(void);
	int HitSegment(CPoint point) const;
	int Scale(int value) const { return MulDiv(value, (int)m_dpi, 96); }

	CEdit m_search;			// borderless, inside m_searchBox
	CRect m_searchBox;		// the drawn field: magnifier, edit, frame
	HFONT m_font;
	HFONT m_iconFont;
	UINT m_dpi;
	std::vector<Segment> m_segments;
	std::vector<CRect> m_segmentRects;	// filled while painting
	int m_hot;
	bool m_tracking;
	bool m_searching;
	ui64 m_matches;
};

#endif
