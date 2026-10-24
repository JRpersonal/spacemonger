
#ifndef CMAINFRAME_H
#define CMAINFRAME_H

#ifdef bool
#pragma push_macro("bool")
#undef bool
#endif
#include "ScanStatusBar.h"
#include "PathBar.h"
#include <string>
#include <vector>
#ifdef bool
#pragma pop_macro("bool")
#endif
#include "ScanNotice.h"

#define WM_SCAN_NOTICE_CLICKED (WM_APP + 42)
#define WM_SCAN_DROPPED_PATH (WM_APP + 43)	// lParam: CString * (deleted by the handler)

class CScanNoticeButton : public CButton {
protected:
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	DECLARE_MESSAGE_MAP()
};

// The native Windows toolbar control (Common Controls v6), not MFC's
// CToolBar: CToolBar forces one width on all buttons, which truncates labels.
class CMainToolBar : public CToolBarCtrl {
public:
	CMainToolBar();
	virtual ~CMainToolBar();
	DECLARE_DYNCREATE(CMainToolBar)

	virtual int Create(CWnd *parent);

	enum {
		NoState = 0,
		ViewFree,
		ShowSystem,
	};

	virtual void EnableState(int state, BOOL enabled);
	void UpdateButtonsForView(class CFolderView *view);

	void Reload(void);
	void Rebuild(void);
	UINT GetDpi(void) const { return m_dpi; }
	int GetButtonHeight(void) const { return m_buttonHeight; }
	HFONT GetUiFont(void) const { return m_font; }
protected:
	UINT m_dpi;
	int m_buttonHeight;
	HFONT m_font;
	HIMAGELIST m_images;
	HIMAGELIST m_disabledImages;
public:

protected:
	//{{AFX_MSG(CMainToolBar)
	afx_msg void OnIgnoreUpdate(CCmdUI *ui);
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	//}}AFX_MSG
	afx_msg void OnCustomDraw(NMHDR *pNotify, LRESULT *pResult);
	DECLARE_MESSAGE_MAP()

};

class CMainFrame : public CFrameWnd {
public:
	CMainFrame();
	virtual ~CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)
	BOOL PreCreateWindow(CREATESTRUCT& cs);

	virtual void RecalcLayout(BOOL bNotify);

	// Scan progress in the window instead of a floating dialog. While a scan
	// runs the toolbar and the map ignore input; the frame stays enabled so
	// Cancel, moving and resizing keep working.
	void BeginScan(const CString &root);
	void UpdateScan(ui64 files, ui64 folders, ui64 bytes, ui64 usedspace,
		const std::wstring &currentPath, bool complete);
	void EndScan(void);
	BOOL IsScanCancelRequested(void) const { return m_scanStatus.IsCancelRequested(); }
	void RequestScanCancel(void) { m_scanStatus.RequestCancel(); }
	BOOL IsScanning(void) const { return m_scanning; }
	BOOL m_closeAfterScan;
	void SetScanNotice(ui64 skippedDirectories, DWORD firstError,
		const std::vector<std::wstring>& paths);
	void ClearScanNotice(void);
	// Language, DPI, theme (light/dark) or colour scheme changed.
	void ApplyAppearance(void);

protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy(void);
	afx_msg void OnSysCommand(UINT nid, LPARAM lparam);
	afx_msg void OnClose();
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnWindowPosChanged(WINDOWPOS *wp);
	afx_msg void OnShowWindow(BOOL bShow, UINT status);
	afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
	afx_msg void OnScanNoticeClicked();
	afx_msg LRESULT OnScanNoticeClickedMessage(WPARAM wParam, LPARAM lParam);
	afx_msg void OnScanNoticeUpdate(CCmdUI *ui);
	afx_msg void OnDropFiles(HDROP drop);
	afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
	afx_msg LRESULT OnScanDroppedPath(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	afx_msg void OnIgnoreUpdate(CCmdUI *ui);
	DECLARE_MESSAGE_MAP()

public:
	CMainToolBar m_toolbar;
	CPathBar m_pathBar;

protected:
	CScanNoticeButton m_scanNoticeButton;
	CScanStatusBar m_scanStatus;
	BOOL m_scanning;
	ScanNoticeState m_scanNotice;
	std::vector<std::wstring> m_skippedPaths;
};

#endif

