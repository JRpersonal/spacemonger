#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <vector>

#include "SpaceReport.h"
#include "Folder.h"
#include "SystemPaths.h"

namespace {

struct Entry {
	ui64 size;
	ui64 time;
	ui64 files;		// folders only
	std::wstring path;
	bool system;
};

// Keeps the n largest entries seen so far (min-heap on size). Of equally
// large entries the shorter path wins: a folder holding a single big subfolder
// is listed rather than the subfolder.
class TopList {
public:
	explicit TopList(size_t n) : m_n(n) {}
	bool Wants(ui64 size) const { return m_n > 0 && (m_heap.size() < m_n || size >= m_heap.front().size); }
	void Add(Entry &&entry)
	{
		if (!Wants(entry.size)) return;
		m_heap.push_back(std::move(entry));
		std::push_heap(m_heap.begin(), m_heap.end(), Smaller);
		if (m_heap.size() > m_n) {
			std::pop_heap(m_heap.begin(), m_heap.end(), Smaller);
			m_heap.pop_back();
		}
	}
	std::vector<Entry> Sorted()
	{
		std::vector<Entry> out = m_heap;
		std::sort(out.begin(), out.end(), Better);
		return out;
	}
private:
	static bool Better(const Entry &a, const Entry &b)
	{
		return a.size != b.size ? a.size > b.size : a.path.size() < b.path.size();
	}
	static bool Smaller(const Entry &a, const Entry &b) { return Better(a, b); }	// heap top = worst
	size_t m_n;
	std::vector<Entry> m_heap;
};

// Free space and the "not readable" estimate are pseudo entries of the tree.
bool IsPseudoEntry(const wchar_t *name)
{
	return name == nullptr || name[0] == L'<' || name[0] == L'|';
}

// Post-order walk; returns the number of files below folder.
ui64 Walk(const CFolder *folder, std::wstring &path, bool system, bool topLevel,
	const SM_ReportVolume &volume, const SM_ReportOptions &options, TopList &folders, TopList &files)
{
	ui64 count = 0;
	for (unsigned int i = 0; i < folder->cur; ++i) {
		const wchar_t *name = folder->names[i];
		if (IsPseudoEntry(name)) continue;
		bool entrySystem = system || (topLevel && volume.wholeVolume && SM_IsSystemRootEntry(name));
		if (entrySystem && options.hideSystem) continue;

		size_t mark = path.size();
		path += name;
		if (folder->children[i] != nullptr) {
			path += L'\\';
			ui64 below = Walk(folder->children[i], path, entrySystem, false, volume, options, folders, files);
			count += below;
			if (folders.Wants(folder->sizes[i]))
				folders.Add({ folder->sizes[i], folder->times[i], below, path, entrySystem });
		}
		else {
			++count;
			if (files.Wants(folder->sizes[i]))
				files.Add({ folder->sizes[i], folder->times[i], 0, path, entrySystem });
		}
		path.resize(mark);
	}
	return count;
}

class CsvWriter {
public:
	explicit CsvWriter(const SM_ReportOptions &options) : m_options(options) {}

	void Field(const std::wstring &value)
	{
		if (!m_lineStart) m_text += m_options.separator;
		m_lineStart = false;
		bool quote = value.find_first_of(std::wstring(L"\"\r\n") + m_options.separator) != std::wstring::npos;
		if (!quote) { m_text += value; return; }
		m_text += L'"';
		for (wchar_t c : value) {
			if (c == L'"') m_text += L'"';
			m_text += c;
		}
		m_text += L'"';
	}
	void Number(ui64 value) { Field(std::to_wstring(value)); }
	void Gigabytes(ui64 bytes)
	{
		// Two decimals, rounded; integer arithmetic avoids locale surprises.
		ui64 hundredths = bytes > (~0ull / 100) ? (bytes >> 30) * 100
			: (bytes * 100 + (1ull << 29)) >> 30;
		std::wstring s = std::to_wstring(hundredths / 100);
		s += m_options.decimal;
		ui64 frac = hundredths % 100;
		if (frac < 10) s += L'0';
		s += std::to_wstring(frac);
		Field(s);
	}
	void Percent(ui64 part, ui64 whole)
	{
		if (whole == 0) { Field(L""); return; }
		ui64 tenths = (ui64)((double)part * 1000.0 / (double)whole + 0.5);
		std::wstring s = std::to_wstring(tenths / 10);
		s += m_options.decimal;
		s += std::to_wstring(tenths % 10);
		Field(s);
	}
	void Date(ui64 filetime)
	{
		SYSTEMTIME utc, st;
		FILETIME ft = { (DWORD)filetime, (DWORD)(filetime >> 32) };
		if (filetime == 0 || !::FileTimeToSystemTime(&ft, &utc)
			|| !::SystemTimeToTzSpecificLocalTime(NULL, &utc, &st)) { Field(L""); return; }
		wchar_t buf[32];
		swprintf_s(buf, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
		Field(buf);
	}
	void EndLine() { m_text += L"\r\n"; m_lineStart = true; }
	const std::wstring &Text() const { return m_text; }

private:
	const SM_ReportOptions &m_options;
	std::wstring m_text;
	bool m_lineStart = true;
};

}

void SM_ReportLocaleSeparators(SM_ReportOptions &options)
{
	wchar_t list[8] = { 0 }, decimal[8] = { 0 };
	options.separator = L',';
	options.decimal = L'.';
	if (::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SLIST, list, 8) > 1 && list[1] == L'\0')
		options.separator = list[0];
	if (::GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SDECIMAL, decimal, 8) > 1 && decimal[1] == L'\0')
		options.decimal = decimal[0];
	if (options.separator == options.decimal) options.separator = L';';
}

std::wstring SM_BuildSpaceReport(const CFolder *root, const SM_ReportVolume &volume,
	const SM_ReportOptions &options)
{
	TopList folders((size_t)std::max(0, options.top)), files((size_t)std::max(0, options.top));
	std::wstring path = volume.path;
	if (!path.empty() && path.back() != L'\\') path += L'\\';
	ui64 fileCount = root != nullptr ? Walk(root, path, false, true, volume, options, folders, files) : 0;

	ui64 used = volume.totalSpace > volume.freeSpace ? volume.totalSpace - volume.freeSpace : 0;
	// Shares relate to what is used on the drive, or to the scanned folder.
	ui64 whole = volume.wholeVolume ? used : volume.scannedBytes;

	CsvWriter csv(options);
	const wchar_t *header[] = { L"Computer", L"Kind", L"Path", L"Size (bytes)", L"Size (GB)",
		L"Share (%)", L"Files", L"Modified", L"System", L"Scanned" };
	for (const wchar_t *h : header) csv.Field(h);
	csv.EndLine();

	auto row = [&](const wchar_t *kind, const std::wstring &rowPath, ui64 size, bool share,
		const std::wstring &filesText, ui64 time, const wchar_t *system) {
		csv.Field(volume.computer);
		csv.Field(kind);
		csv.Field(rowPath);
		csv.Number(size);
		csv.Gigabytes(size);
		if (share) csv.Percent(size, whole); else csv.Field(L"");
		csv.Field(filesText);
		csv.Date(time);
		csv.Field(system);
		csv.Date(volume.scanTime);
		csv.EndLine();
	};

	if (volume.wholeVolume) {
		row(L"Drive size", volume.path, volume.totalSpace, false, L"", 0, L"");
		row(L"Used", volume.path, used, false, L"", 0, L"");
		row(L"Free", volume.path, volume.freeSpace, false, L"", 0, L"");
		if (used > volume.scannedBytes)
			row(L"Not readable", volume.path, used - volume.scannedBytes, true, L"", 0, L"");
	}
	else {
		row(L"Folder size", volume.path, volume.scannedBytes, false, std::to_wstring(fileCount), 0, L"");
	}
	for (const Entry &e : folders.Sorted())
		row(L"Folder", e.path, e.size, true, std::to_wstring(e.files), e.time, e.system ? L"yes" : L"no");
	for (const Entry &e : files.Sorted())
		row(L"File", e.path, e.size, true, L"", e.time, e.system ? L"yes" : L"no");
	return csv.Text();
}

std::wstring SM_ReportComputerForPath(const std::wstring &path)
{
	std::wstring p = path;
	if (p.rfind(L"\\\\?\\UNC\\", 0) == 0) p = L"\\\\" + p.substr(8);
	if (p.size() > 2 && p[0] == L'\\' && p[1] == L'\\' && p[2] != L'?' && p[2] != L'.') {
		size_t end = p.find(L'\\', 2);
		return p.substr(2, end == std::wstring::npos ? std::wstring::npos : end - 2);
	}
	wchar_t name[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
	DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
	if (::GetComputerNameW(name, &len)) return name;
	return L"";
}

std::wstring SM_ResolveReportPath(const std::wstring &target, const std::wstring &computer)
{
	std::wstring path = target;
	wchar_t expanded[32768];
	DWORD n = ::ExpandEnvironmentStringsW(target.c_str(), expanded, 32768);
	if (n > 0 && n <= 32768) path = expanded;

	DWORD attributes = ::GetFileAttributesW(path.c_str());
	bool folder = (!path.empty() && (path.back() == L'\\' || path.back() == L'/'))
		|| (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
	if (!folder) return path;

	if (path.back() != L'\\' && path.back() != L'/') path += L'\\';
	SYSTEMTIME st;
	::GetLocalTime(&st);
	wchar_t date[16];
	swprintf_s(date, L"%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
	std::wstring name = computer.empty() ? L"SpaceMonger" : computer;
	for (wchar_t &c : name)
		if (wcschr(L"\\/:*?\"<>|$", c) != nullptr) c = L'_';
	return path + name + L"_" + date + L".csv";
}

bool SM_WriteUtf8File(const std::wstring &path, const std::wstring &text)
{
	int bytes = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), NULL, 0, NULL, NULL);
	if (bytes < 0 || (bytes == 0 && !text.empty())) return false;
	std::string data("\xEF\xBB\xBF");
	size_t offset = data.size();
	data.resize(offset + (size_t)bytes);
	if (bytes > 0)
		::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &data[offset], bytes, NULL, NULL);

	// Write to a temp file and rename, so a collector never reads half a report.
	std::wstring temp = path + L".tmp";
	HANDLE h = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	BOOL ok = ::WriteFile(h, data.data(), (DWORD)data.size(), &written, NULL) && written == data.size();
	ok = ::CloseHandle(h) && ok;
	if (ok) ok = ::MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
	if (!ok) ::DeleteFileW(temp.c_str());
	return ok != FALSE;
}
