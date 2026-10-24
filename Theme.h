#ifndef THEME_H
#define THEME_H

// Light and dark appearance of the main window: toolbar, path bar, status
// strip and the map's background. The treemap boxes keep their colours in
// both; only what surrounds them changes. Dialogs stay in the Windows look
// (classic Win32 dialogs have no supported dark mode).

#include <windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

struct SM_Theme {
	bool dark;
	COLORREF face;		// toolbar, path bar, status strip
	COLORREF text;
	COLORREF grayText;
	COLORREF line;		// separators, outer frame
	COLORREF mapBack;	// behind and inside folders
	COLORREF freeFill;	// the free-space block
	COLORREF freeText;
	COLORREF editBack;
	HBRUSH faceBrush;
	HBRUSH mapBrush;
	HBRUSH editBrush;
};

// 0 = follow Windows (Settings > Personalisation > Colours > app mode),
// 1 = light, 2 = dark.
inline bool SM_WindowsAppsUseDark()
{
	DWORD value = 1, size = sizeof(value);
	if (::RegGetValueW(HKEY_CURRENT_USER,
			L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
			L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS)
		return false;
	return value == 0;
}

inline SM_Theme &SM_ThemeStorage()
{
	static SM_Theme theme = {};
	return theme;
}

inline void SM_ApplyThemeSetting(int setting)
{
	SM_Theme &t = SM_ThemeStorage();
	if (t.faceBrush != NULL) ::DeleteObject(t.faceBrush);
	if (t.mapBrush != NULL) ::DeleteObject(t.mapBrush);
	if (t.editBrush != NULL) ::DeleteObject(t.editBrush);
	t.dark = setting == 2 || (setting == 0 && SM_WindowsAppsUseDark());
	if (t.dark) {
		t.face = RGB(32, 32, 32);
		t.text = RGB(240, 240, 240);
		t.grayText = RGB(150, 150, 150);
		t.line = RGB(70, 70, 70);
		t.mapBack = RGB(45, 45, 45);
		t.freeFill = RGB(28, 28, 28);
		t.freeText = RGB(200, 200, 200);
		t.editBack = RGB(56, 56, 56);
	}
	else {
		t.face = ::GetSysColor(COLOR_3DFACE);
		t.text = ::GetSysColor(COLOR_BTNTEXT);
		t.grayText = ::GetSysColor(COLOR_GRAYTEXT);
		t.line = ::GetSysColor(COLOR_3DSHADOW);
		t.mapBack = ::GetSysColor(COLOR_3DFACE);
		t.freeFill = ::GetSysColor(COLOR_WINDOW);
		t.freeText = ::GetSysColor(COLOR_WINDOWTEXT);
		t.editBack = ::GetSysColor(COLOR_WINDOW);
	}
	t.faceBrush = ::CreateSolidBrush(t.face);
	t.mapBrush = ::CreateSolidBrush(t.mapBack);
	t.editBrush = ::CreateSolidBrush(t.editBack);
}

inline const SM_Theme &SM_CurrentTheme()
{
	SM_Theme &t = SM_ThemeStorage();
	if (t.faceBrush == NULL) SM_ApplyThemeSetting(0);
	return t;
}

// Icon font for toolbar and search glyphs: Segoe Fluent Icons (Windows 11),
// else Segoe MDL2 Assets (Windows 10); both share the code points we use.
inline const wchar_t *SM_IconFontFace()
{
	static const wchar_t *face = nullptr;
	if (face == nullptr) {
		LOGFONTW lf = {};
		lf.lfCharSet = DEFAULT_CHARSET;
		wcscpy_s(lf.lfFaceName, L"Segoe Fluent Icons");
		bool found = false;
		HDC dc = ::GetDC(NULL);
		::EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW *, const TEXTMETRICW *, DWORD, LPARAM param) -> int {
			*(bool *)param = true;
			return 0;
		}, (LPARAM)&found, 0);
		::ReleaseDC(NULL, dc);
		face = found ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
	}
	return face;
}

// Dark title bar (Windows 10 20H1 and later; ignored elsewhere).
inline void SM_ApplyThemeToFrame(HWND frame)
{
	BOOL dark = SM_CurrentTheme().dark ? TRUE : FALSE;
	const DWORD kUseImmersiveDarkMode = 20;	// DWMWA_USE_IMMERSIVE_DARK_MODE
	::DwmSetWindowAttribute(frame, kUseImmersiveDarkMode, &dark, sizeof(dark));
}

#endif
