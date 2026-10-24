#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wctype.h>

#include "ReportLink.h"

namespace {

const wchar_t kScheme[] = L"spacemonger:";
const wchar_t kClassKey[] = L"Software\\Classes\\spacemonger";

int HexValue(wchar_t c)
{
	if (c >= L'0' && c <= L'9') return c - L'0';
	if (c >= L'a' && c <= L'f') return c - L'a' + 10;
	if (c >= L'A' && c <= L'F') return c - L'A' + 10;
	return -1;
}

// Percent-decoded UTF-8; false on a broken escape or invalid UTF-8.
bool DecodeComponent(const std::wstring &text, std::wstring &out)
{
	std::string bytes;
	for (size_t i = 0; i < text.size(); ++i) {
		wchar_t c = text[i];
		if (c == L'%') {
			if (i + 2 >= text.size()) return false;
			int hi = HexValue(text[i + 1]), lo = HexValue(text[i + 2]);
			if (hi < 0 || lo < 0) return false;
			bytes += (char)(hi * 16 + lo);
			i += 2;
		}
		else if (c < 0x80) bytes += (char)c;
		else return false;	// links arrive encoded
	}
	out.clear();
	if (bytes.empty()) return true;
	int n = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)bytes.size(), NULL, 0);
	if (n <= 0) return false;
	out.resize((size_t)n);
	::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), (int)bytes.size(), &out[0], n);
	return true;
}

}

bool SM_IsPlainLocalPath(const std::wstring &path)
{
	if (path.size() < 3 || path.size() > 32000) return false;
	if (!iswalpha(path[0]) || path[0] > 0x7F || path[1] != L':' || path[2] != L'\\') return false;
	for (wchar_t c : path) {
		if (c < 0x20 || wcschr(L"/\"*?<>|", c) != nullptr) return false;
	}
	if (path.find(L':', 2) != std::wstring::npos) return false;	// alternate data streams
	// No "." or ".." segments and no empty ones (C:\\a); a trailing
	// backslash is fine.
	size_t start = 3;
	while (start < path.size()) {
		size_t end = path.find(L'\\', start);
		if (end == std::wstring::npos) end = path.size();
		std::wstring part = path.substr(start, end - start);
		if (part.empty() || part == L"." || part == L"..") return false;
		start = end + 1;
	}
	return true;
}

SM_LinkAction SM_ParseReportLink(const std::wstring &url, std::wstring &path)
{
	path.clear();
	const size_t schemeLength = sizeof(kScheme) / sizeof(kScheme[0]) - 1;
	if (url.size() <= schemeLength || _wcsnicmp(url.c_str(), kScheme, schemeLength) != 0) return SM_LINK_NONE;
	std::wstring rest = url.substr(schemeLength);
	if (rest.rfind(L"//", 0) == 0) rest = rest.substr(2);
	size_t question = rest.find(L'?');
	if (question == std::wstring::npos) return SM_LINK_NONE;
	std::wstring verb = rest.substr(0, question);
	if (!verb.empty() && verb.back() == L'/') verb.pop_back();
	SM_LinkAction action = SM_LINK_NONE;
	if (_wcsicmp(verb.c_str(), L"show") == 0) action = SM_LINK_SHOW;
	else if (_wcsicmp(verb.c_str(), L"scan") == 0) action = SM_LINK_SCAN;
	else return SM_LINK_NONE;

	std::wstring decoded;
	if (!DecodeComponent(rest.substr(question + 1), decoded) || !SM_IsPlainLocalPath(decoded)) return SM_LINK_NONE;
	path = decoded;
	return action;
}

bool SM_RegisterReportLinks(const std::wstring &exe)
{
	std::wstring command = L"\"" + exe + L"\" /link \"%1\"";
	std::wstring icon = L"\"" + exe + L"\",0";
	auto set = [](const wchar_t *subkey, const wchar_t *name, const std::wstring &value) {
		std::wstring key = kClassKey;
		if (subkey != nullptr) key = key + L"\\" + subkey;
		return ::RegSetKeyValueW(HKEY_CURRENT_USER, key.c_str(), name, REG_SZ, value.c_str(),
			(DWORD)((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
	};
	return set(nullptr, nullptr, L"URL:SpaceMonger")
		&& set(nullptr, L"URL Protocol", L"")
		&& set(L"DefaultIcon", nullptr, icon)
		&& set(L"shell\\open\\command", nullptr, command);
}

bool SM_UnregisterReportLinks()
{
	LSTATUS status = ::RegDeleteTreeW(HKEY_CURRENT_USER, kClassKey);
	return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}
