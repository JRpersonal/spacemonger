#ifndef SCANSTATUSBAR_H
#define SCANSTATUSBAR_H

#include <string>

// Progress strip at the bottom of the main window while a scan runs: the
// scanned root and percentage, file/folder counts and size, the current
// folder, a native (Windows-themed) progress bar and a Cancel button. It
// replaces the old floating scan dialog, which covered the live treemap and
// flickered with every frame.
class CScanStatusBar : public CWnd {
public:
	CScanStatusBar();
	virtual ~CScanStatusBar();

	BOOL Create(CWnd *parent);
	void Begin(const CString &root, HFONT font, UINT dpi);
	void Update(ui64 files, ui64 folders, ui64 bytes, ui64 usedspace,
		const std::wstring &currentPath, bool complete);
	void End(void);
	// A short note in front of the folder line ("Showing the scan from ...").
	void SetNote(const CString &note);
	// Height for the current font and DPI; 0 while hidden.
	int GetDesiredHeight(void) const;
	BOOL IsCancelRequested(void) const { return m_cancelRequested; }
	void RequestCancel(void) { m_cancelRequested = TRUE; }

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnCancelClicked();
	DECLARE_MESSAGE_MAP()

	void LayoutChildren(void);
	int TextHeight(void) const;

	CProgressCtrl m_progress;
	CButton m_cancel;
	HFONT m_font;
	UINT m_dpi;
	BOOL m_cancelRequested;
	CString m_root;
	CString m_title;
	CString m_detail;
	CString m_note;
	std::wstring m_path;
};

#endif
