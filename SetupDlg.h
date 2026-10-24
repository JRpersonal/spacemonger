#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

class CSettingsDialog : public CDialog {
public:
	CSettingsDialog(CWnd *parent = NULL);
	DECLARE_DYNCREATE(CSettingsDialog)
	virtual ~CSettingsDialog();

protected:
	//{{AFX_MSG(CSettingsDialog)
	virtual void OnOK();
	virtual void OnCancel();
	virtual void OnLang();
	afx_msg void OnChange();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void UpdateDialogForNewLanguage(void);
	void ReadControls(void);

	virtual BOOL OnInitDialog();

protected:
	CCurrentSettings oldsettings;	// restored on Cancel
	Lang *oldlangptr;
};

#endif
