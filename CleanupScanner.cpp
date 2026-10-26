#include "CleanupScanner.h"
#include "Folder.h"
#include "PathUtil.h"
#include "SystemPaths.h"
#include <shellapi.h>
#include <algorithm>
#include <wctype.h>

namespace {

std::wstring ExpandEnv(const wchar_t *text)
{
	wchar_t buffer[MAX_PATH * 2];
	DWORD n = ::ExpandEnvironmentStringsW(text, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
	if (n == 0 || n > sizeof(buffer) / sizeof(buffer[0])) return std::wstring();
	return buffer;
}

bool IsDirectory(const std::wstring &path)
{
	DWORD attributes = ::GetFileAttributesW(PathUtil::PrepareLongPath(path).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)
		&& !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

std::wstring Join(const std::wstring &a, const wchar_t *b)
{
	std::wstring result = a;
	if (!result.empty() && result.back() != L'\\') result += L'\\';
	result += b;
	return result;
}

// Browser profile folders: "Default", "Profile 1", ... below a User Data folder.
std::vector<std::wstring> SubFolders(const std::wstring &parent)
{
	std::vector<std::wstring> result;
	WIN32_FIND_DATAW data;
	HANDLE find = ::FindFirstFileExW(PathUtil::PrepareLongPath(Join(parent, L"*")).c_str(),
		FindExInfoBasic, &data, FindExSearchNameMatch, NULL, 0);
	if (find == INVALID_HANDLE_VALUE) return result;
	do {
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
		if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
		result.push_back(Join(parent, data.cFileName));
	} while (::FindNextFileW(find, &data));
	::FindClose(find);
	return result;
}

void AddTarget(std::vector<SM_CleanupTarget> &targets, SM_CleanupKind kind,
	std::vector<std::wstring> paths, bool keepRecent, bool defaultChecked)
{
	std::vector<std::wstring> existing;
	for (const auto &p : paths) {
		if (!p.empty() && IsDirectory(p)) existing.push_back(p);
	}
	if (existing.empty()) return;
	SM_CleanupTarget target;
	target.kind = kind;
	target.paths = existing;
	target.keepRecent = keepRecent;
	target.defaultChecked = defaultChecked;
	target.bytes = target.files = 0;
	targets.push_back(target);
}

std::vector<std::wstring> BrowserCaches(const std::wstring &userData)
{
	std::vector<std::wstring> paths;
	if (!IsDirectory(userData)) return paths;
	for (const auto &profile : SubFolders(userData)) {
		paths.push_back(Join(profile, L"Cache\\Cache_Data"));
		paths.push_back(Join(profile, L"Code Cache"));
		paths.push_back(Join(profile, L"GPUCache"));
	}
	return paths;
}

const ui64 kRecentWindow = 24ull * 60 * 60 * 10000000ull;	// 24 h in 100 ns units

bool IsRecent(const FILETIME &time)
{
	FILETIME now;
	::GetSystemTimeAsFileTime(&now);
	ui64 t = ((ui64)time.dwHighDateTime << 32) | time.dwLowDateTime;
	ui64 n = ((ui64)now.dwHighDateTime << 32) | now.dwLowDateTime;
	return t + kRecentWindow > n;
}

template <typename Visit>
void WalkFolder(const std::wstring &path, Visit &visit, bool removeEmptyFolders,
	const std::atomic<bool> *cancel = nullptr)
{
	if (cancel != nullptr && cancel->load()) return;
	WIN32_FIND_DATAW data;
	HANDLE find = ::FindFirstFileExW(PathUtil::PrepareLongPath(Join(path, L"*")).c_str(),
		FindExInfoBasic, &data, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (find == INVALID_HANDLE_VALUE) return;
	do {
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
		std::wstring child = Join(path, data.cFileName);
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Never follow junctions or symlinks out of the clean-up area.
			if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
			WalkFolder(child, visit, removeEmptyFolders, cancel);
			if (removeEmptyFolders) ::RemoveDirectoryW(PathUtil::PrepareLongPath(child).c_str());
		} else {
			visit(child, data);
		}
	} while ((cancel == nullptr || !cancel->load()) && ::FindNextFileW(find, &data));
	::FindClose(find);
}

ui64 FileSize(const WIN32_FIND_DATAW &data)
{
	return ((ui64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

bool EndsWithNoCase(const std::wstring &text, const wchar_t *suffix)
{
	size_t n = wcslen(suffix);
	return text.size() >= n && _wcsicmp(text.c_str() + text.size() - n, suffix) == 0;
}

std::wstring Lower(std::wstring text)
{
	for (auto &c : text) c = (wchar_t)towlower(c);
	return text;
}

}

std::vector<SM_CleanupTarget> SM_FindCleanupTargets(void)
{
	std::vector<SM_CleanupTarget> targets;
	std::wstring local = ExpandEnv(L"%LOCALAPPDATA%");

	AddTarget(targets, SM_CLEAN_USER_TEMP, { ExpandEnv(L"%TEMP%") }, true, true);
	AddTarget(targets, SM_CLEAN_WINDOWS_TEMP, { ExpandEnv(L"%SystemRoot%\\Temp") }, true, true);
	AddTarget(targets, SM_CLEAN_INTERNET_CACHE, { Join(local, L"Microsoft\\Windows\\INetCache") }, false, true);
	AddTarget(targets, SM_CLEAN_CRASH_DUMPS, { Join(local, L"CrashDumps") }, false, true);
	AddTarget(targets, SM_CLEAN_ERROR_REPORTS, {
		Join(local, L"Microsoft\\Windows\\WER\\ReportArchive"),
		Join(local, L"Microsoft\\Windows\\WER\\ReportQueue"),
		ExpandEnv(L"%ProgramData%\\Microsoft\\Windows\\WER\\ReportArchive"),
		ExpandEnv(L"%ProgramData%\\Microsoft\\Windows\\WER\\ReportQueue") }, false, true);
	AddTarget(targets, SM_CLEAN_SHADER_CACHE, { Join(local, L"D3DSCache") }, false, true);
	AddTarget(targets, SM_CLEAN_EDGE_CACHE, BrowserCaches(Join(local, L"Microsoft\\Edge\\User Data")), false, true);
	AddTarget(targets, SM_CLEAN_CHROME_CACHE, BrowserCaches(Join(local, L"Google\\Chrome\\User Data")), false, true);
	{
		std::vector<std::wstring> firefox;
		std::wstring profiles = Join(local, L"Mozilla\\Firefox\\Profiles");
		if (IsDirectory(profiles)) {
			for (const auto &profile : SubFolders(profiles)) firefox.push_back(Join(profile, L"cache2"));
		}
		AddTarget(targets, SM_CLEAN_FIREFOX_CACHE, firefox, false, true);
	}
	// Developer caches are safe to drop but cost a re-download; opt in.
	AddTarget(targets, SM_CLEAN_NPM_CACHE, { Join(local, L"npm-cache") }, false, false);
	AddTarget(targets, SM_CLEAN_GO_CACHE, { Join(local, L"go-build") }, false, false);
	AddTarget(targets, SM_CLEAN_PIP_CACHE, { Join(local, L"pip\\Cache") }, false, false);

	SM_CleanupTarget bin;
	bin.kind = SM_CLEAN_RECYCLE_BIN;
	bin.keepRecent = false;
	bin.defaultChecked = true;
	bin.bytes = bin.files = 0;
	targets.push_back(bin);
	return targets;
}

void SM_MeasureFolder(const std::wstring &path, bool keepRecent, ui64 &bytes, ui64 &files,
	const std::atomic<bool> *cancel)
{
	auto visit = [&](const std::wstring &, const WIN32_FIND_DATAW &data) {
		if (keepRecent && IsRecent(data.ftLastWriteTime)) return;
		bytes += FileSize(data);
		++files;
	};
	WalkFolder(path, visit, false, cancel);
}

void SM_MeasureCleanupTarget(SM_CleanupTarget &target, const std::atomic<bool> *cancel)
{
	target.bytes = target.files = 0;
	if (target.kind == SM_CLEAN_RECYCLE_BIN) {
		SHQUERYRBINFO info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		if (SUCCEEDED(::SHQueryRecycleBinW(NULL, &info))) {
			target.bytes = (ui64)info.i64Size;
			target.files = (ui64)info.i64NumItems;
		}
		return;
	}
	for (const auto &path : target.paths)
		SM_MeasureFolder(path, target.keepRecent, target.bytes, target.files, cancel);
}

SM_CleanupResult SM_CleanFolder(const std::wstring &path, bool keepRecent, SM_CleanupProgress *progress)
{
	SM_CleanupResult result = { 0, 0, 0 };
	auto skipped = [&](const WIN32_FIND_DATAW &data) {
		++result.skippedFiles;
		if (progress != nullptr) {
			progress->skippedFiles.fetch_add(1);
			progress->processedBytes.fetch_add(FileSize(data));
		}
	};
	auto visit = [&](const std::wstring &file, const WIN32_FIND_DATAW &data) {
		if (keepRecent && IsRecent(data.ftLastWriteTime)) {
			// Recent files were not counted by the measurement either.
			++result.skippedFiles;
			if (progress != nullptr) progress->skippedFiles.fetch_add(1);
			return;
		}
		std::wstring prepared = PathUtil::PrepareLongPath(file);
		if (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
			::SetFileAttributesW(prepared.c_str(), data.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
		if (::DeleteFileW(prepared.c_str())) {
			result.freedBytes += FileSize(data);
			++result.deletedFiles;
			if (progress != nullptr) {
				progress->freedBytes.fetch_add(FileSize(data));
				progress->deletedFiles.fetch_add(1);
				progress->processedBytes.fetch_add(FileSize(data));
			}
		} else {
			skipped(data);	// in use or access denied: leave it
		}
	};
	WalkFolder(path, visit, true, progress != nullptr ? &progress->cancel : nullptr);
	return result;
}

SM_CleanupResult SM_CleanTarget(const SM_CleanupTarget &target, SM_CleanupProgress *progress)
{
	SM_CleanupResult total = { 0, 0, 0 };
	if (target.kind == SM_CLEAN_RECYCLE_BIN) {
		SHQUERYRBINFO info;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		::SHQueryRecycleBinW(NULL, &info);
		if (SUCCEEDED(::SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND))) {
			total.freedBytes = (ui64)info.i64Size;
			total.deletedFiles = (ui64)info.i64NumItems;
			if (progress != nullptr) {
				progress->freedBytes.fetch_add(total.freedBytes);
				progress->deletedFiles.fetch_add(total.deletedFiles);
				progress->processedBytes.fetch_add(total.freedBytes);
			}
		}
		return total;
	}
	for (const auto &path : target.paths) {
		if (progress != nullptr && progress->cancel.load()) break;
		SM_CleanupResult r = SM_CleanFolder(path, target.keepRecent, progress);
		total.freedBytes += r.freedBytes;
		total.deletedFiles += r.deletedFiles;
		total.skippedFiles += r.skippedFiles;
	}
	return total;
}

SM_LargeFileKind SM_ClassifyLargeFile(const std::wstring &name)
{
	static const struct { const wchar_t *ext; SM_LargeFileKind kind; } table[] = {
		{ L".mp4", SM_LARGE_VIDEO }, { L".mkv", SM_LARGE_VIDEO }, { L".mov", SM_LARGE_VIDEO },
		{ L".avi", SM_LARGE_VIDEO }, { L".wmv", SM_LARGE_VIDEO }, { L".m4v", SM_LARGE_VIDEO },
		{ L".mts", SM_LARGE_VIDEO }, { L".webm", SM_LARGE_VIDEO },
		{ L".iso", SM_LARGE_DISK_IMAGE }, { L".img", SM_LARGE_DISK_IMAGE }, { L".dmg", SM_LARGE_DISK_IMAGE },
		{ L".vhdx", SM_LARGE_VIRTUAL_DISK }, { L".vhd", SM_LARGE_VIRTUAL_DISK },
		{ L".vmdk", SM_LARGE_VIRTUAL_DISK }, { L".vdi", SM_LARGE_VIRTUAL_DISK }, { L".qcow2", SM_LARGE_VIRTUAL_DISK },
		{ L".zip", SM_LARGE_ARCHIVE }, { L".7z", SM_LARGE_ARCHIVE }, { L".rar", SM_LARGE_ARCHIVE },
		{ L".tar", SM_LARGE_ARCHIVE }, { L".gz", SM_LARGE_ARCHIVE }, { L".bak", SM_LARGE_ARCHIVE },
		{ L".exe", SM_LARGE_INSTALLER }, { L".msi", SM_LARGE_INSTALLER }, { L".msix", SM_LARGE_INSTALLER },
		{ L".ipsw", SM_LARGE_PHONE_UPDATE },
	};
	for (const auto &entry : table) {
		if (EndsWithNoCase(name, entry.ext)) return entry.kind;
	}
	return SM_LARGE_OTHER;
}

namespace {

struct LargeSearch {
	size_t max;
	ui64 minBytes;
	std::vector<std::wstring> excluded;	// lower case, trailing backslash
	std::vector<SM_LargeFile> found;

	bool Excluded(const std::wstring &folderPath) const
	{
		std::wstring lower = Lower(folderPath);
		if (lower.empty() || lower.back() != L'\\') lower += L'\\';
		for (const auto &prefix : excluded) {
			if (lower.compare(0, prefix.size(), prefix) == 0) return true;
		}
		return false;
	}

	void Offer(SM_LargeFile &&file)
	{
		auto smaller = [](const SM_LargeFile &a, const SM_LargeFile &b) { return a.bytes > b.bytes; };
		if (found.size() < max) {
			found.push_back(std::move(file));
			std::push_heap(found.begin(), found.end(), smaller);
		} else if (!found.empty() && file.bytes > found.front().bytes) {
			std::pop_heap(found.begin(), found.end(), smaller);
			found.back() = std::move(file);
			std::push_heap(found.begin(), found.end(), smaller);
		}
	}

	void Walk(const CFolder *folder, const std::wstring &path, bool isRoot)
	{
		if (folder == nullptr || Excluded(path)) return;
		for (unsigned int i = 0; i < folder->cur; ++i) {
			const wchar_t *name = folder->names[i];
			if (name == nullptr) continue;
			if (name[0] == L'<' || name[0] == L'|') continue;	// free / unreadable space
			if (isRoot && SM_IsSystemRootEntry(name)) continue;
			std::wstring child = Join(path, name);
			if (folder->children[i] != nullptr) {
				Walk(folder->children[i], child, false);
			} else if (folder->sizes[i] >= minBytes) {
				SM_LargeFile file;
				file.path = child;
				file.bytes = folder->sizes[i];
				file.time = folder->times[i];
				file.kind = SM_ClassifyLargeFile(name);
				file.inDownloads = Lower(child).find(L"\\downloads\\") != std::wstring::npos;
				if (file.inDownloads && (file.kind == SM_LARGE_OTHER || file.kind == SM_LARGE_ARCHIVE))
					file.kind = SM_LARGE_INSTALLER;	// "old download" hint
				Offer(std::move(file));
			}
		}
	}
};

}

std::vector<SM_LargeFile> SM_FindLargePersonalFiles(const CFolder *root,
	const std::wstring &rootPath, size_t max, ui64 minBytes,
	const std::vector<std::wstring> &excludedFolders)
{
	LargeSearch search;
	search.max = max;
	search.minBytes = minBytes;
	for (const auto &folder : excludedFolders) {
		std::wstring lower = Lower(folder);
		if (lower.empty()) continue;
		if (lower.back() != L'\\') lower += L'\\';
		search.excluded.push_back(lower);
	}
	if (max > 0) search.Walk(root, rootPath, true);
	std::sort(search.found.begin(), search.found.end(),
		[](const SM_LargeFile &a, const SM_LargeFile &b) { return a.bytes > b.bytes; });
	return search.found;
}
