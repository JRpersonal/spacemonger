
#include "stdafx.h"
#include "spacemonger.h"
#include "Lang.h"
#include "SetupDlg.h"

// Settings kept to what people actually change: how detailed and how
// coloured the map is, how it behaves under the mouse, whether Delete is
// allowed, and language and theme. Everything else is a sensible fixed
// default (see CCurrentSettings::Load).

IMPLEMENT_DYNCREATE(CSettingsDialog, CDialog)

BEGIN_MESSAGE_MAP(CSettingsDialog, CDialog)
	//{{AFX_MSG_MAP(CSettingsDialog)
	ON_CBN_SELENDOK(IDC_LANG, OnLang)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace {

// Detail level list entries and the density value each one stands for.
const int kDensityForLevel[] = { -1, 0, 1, 2 };
const int kDetailLevels = sizeof(kDensityForLevel) / sizeof(kDensityForLevel[0]);

int LevelForDensity(int density)
{
	for (int i = 0; i < kDetailLevels; ++i)
		if (kDensityForLevel[i] == density) return i;
	return 1;
}

}

CSettingsDialog::CSettingsDialog(CWnd *parent)
	: CDialog(IDD_SETTINGS, parent)
{
}

CSettingsDialog::~CSettingsDialog()
{
}

void CSettingsDialog::OnCancel(void)
{
	memcpy(theApp.m_settings.lang, oldlang, sizeof(oldlang));
	CurLang = oldlangptr;
	CDialog::OnCancel();
}

void CSettingsDialog::OnOK(void)
{
	CComboBox *density = (CComboBox *)GetDlgItem(IDC_DENSITY);
	if (density != NULL && density->GetCurSel() >= 0 && density->GetCurSel() < kDetailLevels)
		theApp.m_settings.density = kDensityForLevel[density->GetCurSel()];
	CComboBox *colors = (CComboBox *)GetDlgItem(IDC_FILE_COLOR);
	if (colors != NULL && colors->GetCurSel() >= 0)
		theApp.m_settings.color_scheme = colors->GetCurSel();
	CComboBox *theme = (CComboBox *)GetDlgItem(IDC_THEME);
	if (theme != NULL && theme->GetCurSel() >= 0)
		theApp.m_settings.theme = theme->GetCurSel();

	theApp.m_settings.show_info_tips = IsDlgButtonChecked(IDC_SHOW_INFO_TIPS);
	theApp.m_settings.show_name_tips = theApp.m_settings.show_info_tips;
	theApp.m_settings.rollover_box = IsDlgButtonChecked(IDC_SHOW_ROLLOVER_BOX);
	theApp.m_settings.disable_delete = IsDlgButtonChecked(IDC_DISABLE_DELETE);

	CDialog::OnOK();
}

void CSettingsDialog::OnLang(void)
{
	CComboBox *lang = (CComboBox *)GetDlgItem(IDC_LANG);
	if (lang == NULL) return;
	int index = lang->GetCurSel();
	if (index < 0) return;
	// Entry 0 is "Automatic", then the languages in Langs[] order.
	const char *code = index == 0 ? "au" : Langs[index - 1].abbrev;
	if (code == NULL) return;
	theApp.m_settings.lang[0] = code[0];
	theApp.m_settings.lang[1] = code[1];
	theApp.m_settings.lang[2] = '\0';
	theApp.m_settings.lang[3] = '\0';
	CurLang = SM_ResolveLanguage(theApp.m_settings.lang);
	UpdateDialogForNewLanguage();
}

void CSettingsDialog::UpdateDialogForNewLanguage(void)
{
	SetWindowText(CurLang->settings);
	SetDlgItemText(IDOK, CurLang->ok);
	SetDlgItemText(IDCANCEL, CurLang->cancel);

	SetDlgItemText(IDC_STATIC_LAYOUT, CurLang->set_map);
	SetDlgItemText(IDC_STATIC_DENSITY, CurLang->set_detail);
	SetDlgItemText(IDC_STATIC_DISPLAYCOLORS, CurLang->set_colors);
	SetDlgItemText(IDC_STATIC_TOOLTIPS, CurLang->set_behavior);
	SetDlgItemText(IDC_SHOW_INFO_TIPS, CurLang->set_hover_details);
	SetDlgItemText(IDC_SHOW_ROLLOVER_BOX, CurLang->set_highlight);
	SetDlgItemText(IDC_DISABLE_DELETE, CurLang->disabledelete);
	SetDlgItemText(IDC_STATIC_MISCOPTIONS, CurLang->set_appearance);
	SetDlgItemText(IDC_STATIC_LANG, CurLang->set_language);
	SetDlgItemText(IDC_STATIC_THEME, CurLang->set_theme);

	CComboBox *density = (CComboBox *)GetDlgItem(IDC_DENSITY);
	if (density != NULL) {
		int current = density->GetCurSel();
		density->ResetContent();
		for (int i = 0; i < kDetailLevels; i++) density->AddString(CurLang->densitynames[i]);
		density->SetCurSel(current >= 0 ? current : LevelForDensity(theApp.m_settings.density));
	}
	CComboBox *colors = (CComboBox *)GetDlgItem(IDC_FILE_COLOR);
	if (colors != NULL) {
		int current = colors->GetCurSel();
		colors->ResetContent();
		for (int i = 0; i < 4; i++) colors->AddString(CurLang->colornames[i]);
		colors->SetCurSel(current >= 0 ? current : theApp.m_settings.color_scheme);
	}
	CComboBox *theme = (CComboBox *)GetDlgItem(IDC_THEME);
	if (theme != NULL) {
		int current = theme->GetCurSel();
		theme->ResetContent();
		for (int i = 0; i < 3; i++) theme->AddString(CurLang->themenames[i]);
		theme->SetCurSel(current >= 0 ? current : theApp.m_settings.theme);
	}
	CComboBox *lang = (CComboBox *)GetDlgItem(IDC_LANG);
	if (lang != NULL) {
		lang->ResetContent();
		lang->AddString(CurLang->set_language_auto);
		int selected = 0;
		for (int i = 0; Langs[i].name != NULL; i++) {
			lang->AddString(Langs[i].name);
			if (Langs[i].abbrev[0] == theApp.m_settings.lang[0]
				&& Langs[i].abbrev[1] == theApp.m_settings.lang[1])
				selected = i + 1;
		}
		lang->SetCurSel(selected);
	}
}

BOOL CSettingsDialog::OnInitDialog()
{
	memcpy(oldlang, theApp.m_settings.lang, sizeof(oldlang));
	oldlangptr = CurLang;

	CDialog::OnInitDialog();
	UpdateDialogForNewLanguage();
	CheckDlgButton(IDC_SHOW_INFO_TIPS, theApp.m_settings.show_info_tips);
	CheckDlgButton(IDC_SHOW_ROLLOVER_BOX, theApp.m_settings.rollover_box);
	CheckDlgButton(IDC_DISABLE_DELETE, theApp.m_settings.disable_delete);
	return TRUE;
}
