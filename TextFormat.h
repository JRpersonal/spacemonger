#ifndef TEXTFORMAT_H
#define TEXTFORMAT_H

// Locale-aware number and size formatting for UI text (status strip, tips,
// clean-up dialog). Sizes use Windows' own formatter, so they read like
// Explorer ("105 GB", "1,5 MB").

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string>

inline std::wstring SM_FormatCount(unsigned long long value)
{
	wchar_t digits[32];
	swprintf_s(digits, L"%llu", value);
	wchar_t decimal[8] = L".", thousand[8] = L",";
	::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal, 8);
	::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STHOUSAND, thousand, 8);
	NUMBERFMTW fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.Grouping = 3;
	fmt.lpDecimalSep = decimal;
	fmt.lpThousandSep = thousand;
	wchar_t formatted[64];
	if (::GetNumberFormatEx(LOCALE_NAME_USER_DEFAULT, 0, digits, &fmt, formatted, 64) > 0)
		return formatted;
	return digits;
}

inline std::wstring SM_FormatBytes(unsigned long long bytes)
{
	wchar_t buffer[64];
	if (SUCCEEDED(::StrFormatByteSizeEx(bytes, SFBS_FLAGS_ROUND_TO_NEAREST_DISPLAYED_DIGIT, buffer, 64)))
		return buffer;
	return SM_FormatCount(bytes);
}

#endif
