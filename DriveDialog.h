
#ifndef DRIVEDIALOG_H
#define DRIVEDIALOG_H

struct CDriveInfo {
	CDriveInfo();
	~CDriveInfo();
	void Free(void);
	// iconSize: pixel size of the list's image list (32 at 96 DPI).
	void LoadDriveInfo(char letter, int iconSize = 32);

	char letter;
	BOOL avail;
	HICON icon;
	wchar_t *longname;
};

class CDriveDialog : public CDialog {
public:
	CDriveDialog(CWnd* pParent = NULL);

	// DoModal result when a folder was picked instead of a drive (m_folder).
	enum { kFolderChosen = 100 };
	CString m_folder;

	//{{AFX_VIRTUAL(CDriveDialog)
	virtual void OnOK();
	virtual void OnCancel();
	//}}AFX_VIRTUAL

protected:
	//{{AFX_MSG(CDriveDialog)
	afx_msg BOOL OnInitDialog();
	afx_msg void OnDriveDblClk(NMHDR *pNotifyStruct, LRESULT *result);
	afx_msg void OnOpenFolder();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	CDriveInfo *driveinfo;
};

#endif
