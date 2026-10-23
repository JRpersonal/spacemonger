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
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void UpdateDialogForNewLanguage(void);

	virtual BOOL OnInitDialog();

protected:
	char oldlang[4];
	Lang *oldlangptr;
};

#endif
