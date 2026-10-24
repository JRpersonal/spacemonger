#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>

#include "HtmlReport.h"
#include "Folder.h"
#include "SystemPaths.h"

namespace {

// Smaller entries than this share of the scan are summed per folder.
const ui64 kDetailParts = 30000;

void JsonString(std::wstring &out, const wchar_t *text)
{
	out += L'"';
	for (const wchar_t *p = text != nullptr ? text : L""; *p != 0; ++p) {
		wchar_t c = *p;
		if (c == L'"') out += L"\\\"";
		else if (c == L'\\') out += L"\\\\";
		// '<' and '>' as escapes: no name can close the <script> it sits in.
		else if (c < 0x20 || c == L'<' || c == L'>' || c == L'&' || c == 0x2028 || c == 0x2029) {
			wchar_t buf[8];
			swprintf_s(buf, L"\\u%04x", (unsigned)c);
			out += buf;
		}
		else out += c;
	}
	out += L'"';
}

void JsonNumber(std::wstring &out, ui64 value)
{
	out += std::to_wstring(value);
}

// Local date of a FILETIME as "yyyy-mm-dd", or "" if unknown.
void JsonDate(std::wstring &out, ui64 filetime)
{
	SYSTEMTIME utc, st;
	FILETIME ft = { (DWORD)filetime, (DWORD)(filetime >> 32) };
	if (filetime == 0 || !::FileTimeToSystemTime(&ft, &utc)
		|| !::SystemTimeToTzSpecificLocalTime(NULL, &utc, &st)) { out += L"\"\""; return; }
	wchar_t buf[16];
	swprintf_s(buf, L"\"%04u-%02u-%02u\"", st.wYear, st.wMonth, st.wDay);
	out += buf;
}

bool IsPseudoEntry(const wchar_t *name)
{
	return name == nullptr || name[0] == L'<' || name[0] == L'|';
}

ui64 CountFiles(const CFolder *folder)
{
	ui64 files = 0;
	for (unsigned int i = 0; i < folder->cur; ++i) {
		if (IsPseudoEntry(folder->names[i])) continue;
		files += folder->children[i] != nullptr ? CountFiles(folder->children[i]) : 1;
	}
	return files;
}

// Writes the children of folder as [entry, ...], then ",files,restCount,restSize".
// A file is ["name",size,"date"], a folder
// ["name",size,"date",[children],files,restCount,restSize,system].
// Returns the number of files below folder.
ui64 WriteChildren(std::wstring &out, const CFolder *folder, ui64 minSize, bool topLevel, bool wholeVolume)
{
	ui64 files = 0, restCount = 0, restSize = 0;
	bool first = true;
	out += L'[';
	for (unsigned int i = 0; i < folder->cur; ++i) {
		const wchar_t *name = folder->names[i];
		if (IsPseudoEntry(name)) continue;
		const CFolder *child = folder->children[i];
		if (folder->sizes[i] < minSize) {
			++restCount;
			restSize += folder->sizes[i];
			// Files below a summed folder still count.
			files += child != nullptr ? CountFiles(child) : 1;
			continue;
		}
		if (!first) out += L',';
		first = false;
		out += L'[';
		JsonString(out, name);
		out += L',';
		JsonNumber(out, folder->sizes[i]);
		out += L',';
		JsonDate(out, folder->times[i]);
		if (child != nullptr) {
			out += L',';
			files += WriteChildren(out, child, minSize, false, false);
			out += L',';
			out += (topLevel && wholeVolume && SM_IsSystemRootEntry(name)) ? L'1' : L'0';
		}
		else {
			++files;
			if (topLevel && wholeVolume && SM_IsSystemRootEntry(name)) out += L",1";
		}
		out += L']';
	}
	out += L"],";
	JsonNumber(out, files);
	out += L',';
	JsonNumber(out, restCount);
	out += L',';
	JsonNumber(out, restSize);
	return files;
}

}

std::wstring SM_BuildReportJson(const CFolder *root, const SM_ReportVolume &volume,
	const SM_ReportOptions &options, const wchar_t *lang)
{
	std::wstring out;
	out.reserve(1 << 20);
	ui64 used = volume.totalSpace > volume.freeSpace ? volume.totalSpace - volume.freeSpace : 0;
	ui64 unreadable = volume.wholeVolume && used > volume.scannedBytes ? used - volume.scannedBytes : 0;

	out += L"{\"v\":1,\"lang\":";
	JsonString(out, lang);
	out += L",\"computer\":";
	JsonString(out, volume.computer.c_str());
	std::wstring path = volume.path;
	if (!path.empty() && path.back() != L'\\') path += L'\\';
	out += L",\"path\":";
	JsonString(out, path.c_str());
	out += L",\"whole\":";
	out += volume.wholeVolume ? L"true" : L"false";
	out += L",\"total\":";
	JsonNumber(out, volume.totalSpace);
	out += L",\"free\":";
	JsonNumber(out, volume.freeSpace);
	out += L",\"scanned\":";
	JsonNumber(out, volume.scannedBytes);
	out += L",\"unreadable\":";
	JsonNumber(out, unreadable);
	out += L",\"date\":";
	JsonDate(out, volume.scanTime);
	out += L",\"hideSystem\":";
	out += options.hideSystem ? L"true" : L"false";
	out += L",\"root\":[\"\",";
	JsonNumber(out, volume.scannedBytes);
	out += L",\"\",";
	if (root != nullptr) {
		ui64 minSize = std::max<ui64>(1, volume.scannedBytes / kDetailParts);
		WriteChildren(out, root, minSize, true, volume.wholeVolume);
	}
	else out += L"[],0,0,0";
	out += L",0]}";
	return out;
}

std::wstring SM_BuildHtmlReport(const std::wstring &pageTemplate, const std::wstring &json)
{
	const std::wstring marker = SM_HTML_DATA_MARKER;
	size_t at = pageTemplate.find(marker);
	if (at == std::wstring::npos) return std::wstring();
	std::wstring page;
	page.reserve(pageTemplate.size() + json.size());
	page.append(pageTemplate, 0, at);
	page += json;
	page.append(pageTemplate, at + marker.size(), std::wstring::npos);
	return page;
}
