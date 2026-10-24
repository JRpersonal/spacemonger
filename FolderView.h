#ifndef FOLDERVIEW_H
#define FOLDERVIEW_H

#include "FolderTree.h"
#include "TipWnd.h"
#include "TreemapLayout.h"
#include <vector>
#include <string>


typedef TreemapNode CDisplayFolder;

class AsyncScanEngine;

class CFolderView : public CFreeView {
public:
	CFolderView();
	DECLARE_DYNCREATE(CFolderView)
	virtual ~CFolderView();

	void UpdateLiveScanLayout(AsyncScanEngine& engine, ui64 totalspace, ui64 freespace, bool wholeVolume);
	void ClearLiveScanLayout(void);
	virtual void SetDocument(CFreeDoc *doc = NULL);
	virtual void SetPalette(void);

	void BuildTitleReverse(CFolder *folder, CString &string);
	const TreemapNode *GetDisplayFolderFromPoint(const CPoint &point);
	const TreemapNode *GetContainerDisplayFolderFromPoint(const CPoint &point);
	void HighlightPathAtPoint(const CPoint &point);

	void ZoomIn(const TreemapNode *folder);
	void ZoomOut(void);
	void ZoomFull(void);
	void ShowFreeSpace(BOOL show);

	inline bool IsAnythingOpen(void)
		{ return rootfolder != NULL; }
	inline bool IsAnythingSelected(void)
		{ return selected != NULL; }
	// Zoom In opens the selected folder or the folder of the selected file.
	const TreemapNode *ZoomTarget(void);
	const TreemapNode *FolderOf(const TreemapNode *cur);
	void ShowInExplorer(const TreemapNode *cur);
	inline bool CanZoomIn(void) { return ZoomTarget() != NULL; }
	inline bool IsSelectedAFolder(void)
		{ return selected != NULL && (selected->flags & 1) != 0; }
	inline bool IsZoomFull(void)
		{ return zoomlevel == 0; }

	void UpdateTitleBar(void);
	void RecreateFonts(void);

	void SelectFolder(const TreemapNode *cur);
	void BuildTitleReverseW(CFolder *folder, std::wstring& string);
	std::wstring BuildItemPathW(const TreemapNode *folder);
	std::wstring BuildContainerPathW(const TreemapNode *folder);

	//{{AFX_VIRTUAL(CFolderView)
protected:
	virtual void OnDraw(CDC *pDC);
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CFolderView)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT flags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnRButtonUp(UINT flags, CPoint point);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnPaint();
	//}}AFX_MSG
	afx_msg void OnIgnoreUpdate(CCmdUI *ui);
	BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT *pResult);
	afx_msg void OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized);
	DECLARE_MESSAGE_MAP()

public:
	virtual void OnUpdate(CFreeDoc *doc);

protected:
	void MinimalDrawDisplayFolder(CDC *pDC, const TreemapNode *cur, BOOL selected);
	void AnimateBox(const CRect &start, const CRect &end);
	void SetupInfoTip(const TreemapNode *cur);
	void SetupNameTip(const TreemapNode *cur);
	void SetNameTipFont(HFONT font);
	TreemapConfig MakeLayoutConfig(void);
	std::wstring FolderTitle(CDC *pDC, const TreemapNode *cur, int width);
	// Live frames and the switch to the finished map blend over the picture
	// on screen (ease-in-out) instead of replacing it.
	void StartCrossfade(int durationMs);
	afx_msg void OnTimer(UINT_PTR id);
	afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
	afx_msg void OnKeyDown(UINT key, UINT repeat, UINT flags);
	enum { kAnimationTimer = 7, kCrossfadeMs = 120, kFinalCrossfadeMs = 200 };
	bool m_xfadeActive;
	DWORD m_xfadeStart;
	int m_xfadeMs;
	CBitmap m_prevFrame;
	CSize m_prevFrameSize;

protected:
	CPalette m_palette;
	CBrush black, white;
	CFont minifont;
	// Treemap text metrics for the current DPI and Windows text size: the
	// classic 9 px Small Fonts at 100 %, a scaled TrueType face above that.
	// m_lineStep is the classic 12 px line pitch scaled the same way, and also
	// the folder title bar height including its top border.
	int m_textHeight;
	int m_lineStep;
	int m_layoutDpi;
	int m_titlePadX;	// left padding of folder titles
	std::vector<HFONT> m_unicodeFonts;
	CBitmap m_backBuffer;
	CSize m_backBufferSize;
	DWORD m_animStart;
	UINT_PTR m_animTimer;
	HFONT m_nameTipFontChoice;
	CFolder *rootfolder;
	std::vector<TreemapNode> m_layoutNodes;
	// Owns the name strings referenced by live-scan layout nodes, so the
	// nodes outlive the scan engine's arenas (e.g. on a cancelled scan).
	std::vector<std::wstring> m_liveNameStorage;
	CTipWnd m_infotipwnd;
	// Icon currently shown in the info tip; owned here (SHGFI_ICON hands ownership to the caller).
	HICON m_tipIcon = NULL;
	CTipWnd m_nametipwnd;
	const TreemapNode *lastcur;

public:
	int zoomlevel;
	const TreemapNode *selected;
	BOOL showfreespace;
};

#endif
