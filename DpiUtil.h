#ifndef DPIUTIL_H
#define DPIUTIL_H

// Per-monitor DPI helpers shared by the frame, toolbar, view and dialogs.
// The newer user32 entry points are looked up at run time so the binary
// still loads on Windows versions that lack them.

#include <windows.h>
#include <string.h>

inline UINT SM_GetWindowDpi(HWND hwnd)
{
	typedef UINT (WINAPI *GetDpiForWindowProc)(HWND);
	HMODULE hUser32 = ::GetModuleHandleA("user32.dll");
	GetDpiForWindowProc pGetDpiForWindow = hUser32 == NULL ? NULL
		: (GetDpiForWindowProc)::GetProcAddress(hUser32, "GetDpiForWindow");
	if (pGetDpiForWindow != NULL && hwnd != NULL) {
		UINT dpi = pGetDpiForWindow(hwnd);
		if (dpi != 0) return dpi;
	}
	UINT dpi = 96;
	HDC hdc = ::GetDC(hwnd);
	if (hdc != NULL) {
		dpi = (UINT)::GetDeviceCaps(hdc, LOGPIXELSY);
		::ReleaseDC(hwnd, hdc);
	}
	return dpi != 0 ? dpi : 96;
}

// Windows "Text size" (Settings > Accessibility), 100..225 %. Win32 apps
// apply it themselves; it is not part of the DPI.
inline int SM_GetTextScalePercent(void)
{
	DWORD value = 100, size = sizeof(value);
	if (::RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Accessibility",
			L"TextScaleFactor", RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS)
		return 100;
	if (value < 100) value = 100;
	if (value > 225) value = 225;
	return (int)value;
}

// Windows' message font (Segoe UI on Windows 10/11) for the given DPI. The
// system already folds the text size setting into it. Caller owns the font.
inline HFONT SM_CreateMessageFontForDpi(UINT dpi)
{
	typedef BOOL (WINAPI *SystemParametersInfoForDpiProc)(UINT, UINT, PVOID, UINT, UINT);
	NONCLIENTMETRICSW metrics;
	memset(&metrics, 0, sizeof(metrics));
	metrics.cbSize = sizeof(metrics);
	HMODULE hUser32 = ::GetModuleHandleA("user32.dll");
	SystemParametersInfoForDpiProc pForDpi = hUser32 == NULL ? NULL
		: (SystemParametersInfoForDpiProc)::GetProcAddress(hUser32, "SystemParametersInfoForDpi");
	if (pForDpi != NULL) {
		if (!pForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)) return NULL;
	} else {
		if (!::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) return NULL;
	}
	return ::CreateFontIndirectW(&metrics.lfMessageFont);
}

#endif
