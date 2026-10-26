#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <vector>
#include <wctype.h>

#include "ScanCache.h"
#include "Folder.h"

namespace {

const DWORD kMagic = 0x43534D53;		// "SMSC"
const DWORD kVersion = 1;
const DWORD kEndMarker = 0x444E4521;	// "!END"
const unsigned kMaxDepth = 300;
const size_t kMaxCachedRoots = 12;
const wchar_t kExtension[] = L".smcache";

enum { kEntryFolder = 1, kEntryActualDiffers = 2 };

class Writer {
public:
	explicit Writer(HANDLE file) : m_file(file), m_ok(true) { m_buffer.reserve(kChunk + 4096); }

	void Put(const void *data, size_t size)
	{
		if (!m_ok) return;
		const char *bytes = (const char *)data;
		m_buffer.insert(m_buffer.end(), bytes, bytes + size);
		if (m_buffer.size() >= kChunk) Flush();
	}
	void U8(unsigned char v) { Put(&v, sizeof(v)); }
	void U16(unsigned short v) { Put(&v, sizeof(v)); }
	void U32(DWORD v) { Put(&v, sizeof(v)); }
	void U64(ui64 v) { Put(&v, sizeof(v)); }
	void Flush()
	{
		if (!m_ok || m_buffer.empty()) return;
		DWORD written = 0;
		if (!::WriteFile(m_file, m_buffer.data(), (DWORD)m_buffer.size(), &written, NULL)
			|| written != m_buffer.size())
			m_ok = false;
		m_buffer.clear();
	}
	bool ok() const { return m_ok; }

private:
	static const size_t kChunk = 1 << 20;
	HANDLE m_file;
	std::vector<char> m_buffer;
	bool m_ok;
};

class Reader {
public:
	explicit Reader(HANDLE file) : m_file(file), m_buffer(1 << 20), m_pos(0), m_length(0), m_ok(true) {}

	bool Get(void *out, size_t size)
	{
		char *dst = (char *)out;
		while (m_ok && size > 0) {
			if (m_pos == m_length && !Fill()) return false;
			size_t n = (std::min)(size, m_length - m_pos);
			memcpy(dst, m_buffer.data() + m_pos, n);
			m_pos += n;
			dst += n;
			size -= n;
		}
		return m_ok;
	}
	bool U8(unsigned char &v) { return Get(&v, sizeof(v)); }
	bool U16(unsigned short &v) { return Get(&v, sizeof(v)); }
	bool U32(DWORD &v) { return Get(&v, sizeof(v)); }
	bool U64(ui64 &v) { return Get(&v, sizeof(v)); }

private:
	bool Fill()
	{
		DWORD read = 0;
		if (!::ReadFile(m_file, m_buffer.data(), (DWORD)m_buffer.size(), &read, NULL) || read == 0) {
			m_ok = false;
			return false;
		}
		m_pos = 0;
		m_length = read;
		return true;
	}

	HANDLE m_file;
	std::vector<char> m_buffer;
	size_t m_pos, m_length;
	bool m_ok;
};

// The synthetic entries FolderTree adds at the top of a scan: free space
// ("<<<<...") and unreadable space ("|..."). Neither is a real file name.
bool IsSyntheticName(const wchar_t *name)
{
	return name != nullptr && (name[0] == L'<' || name[0] == L'|');
}

bool Cancelled(const std::atomic<bool> *cancel)
{
	return cancel != nullptr && cancel->load();
}

bool WriteFolder(Writer &out, const CFolder *folder, bool isRoot, unsigned depth, const std::atomic<bool> *cancel)
{
	if (depth > kMaxDepth || Cancelled(cancel)) return false;
	DWORD count = 0;
	for (unsigned i = 0; i < folder->cur; ++i) {
		if (folder->names[i] != nullptr && !(isRoot && IsSyntheticName(folder->names[i]))) ++count;
	}
	out.U32(count);
	for (unsigned i = 0; i < folder->cur; ++i) {
		const wchar_t *name = folder->names[i];
		if (name == nullptr || (isRoot && IsSyntheticName(name))) continue;
		const size_t length = wcslen(name);
		if (length > 0xFFFF) return false;
		const CFolder *child = folder->children[i];
		unsigned char flags = 0;
		if (child != nullptr) flags |= kEntryFolder;
		else if (folder->actualsizes[i] != folder->sizes[i]) flags |= kEntryActualDiffers;
		out.U8(flags);
		out.U16((unsigned short)length);
		out.Put(name, length * sizeof(wchar_t));
		out.U64(folder->sizes[i]);
		if (flags & kEntryActualDiffers) out.U64(folder->actualsizes[i]);
		out.U64(folder->times[i]);
		if (child != nullptr && !WriteFolder(out, child, false, depth + 1, cancel)) return false;
		if (!out.ok()) return false;
	}
	return out.ok();
}

bool ReadFolder(Reader &in, CFolder *folder, CStringArena &arena, unsigned depth, const std::atomic<bool> *cancel)
{
	if (depth > kMaxDepth || Cancelled(cancel)) return false;
	DWORD count = 0;
	if (!in.U32(count) || count > SM_SCAN_CACHE_MAX_ENTRIES * 2) return false;
	std::vector<wchar_t> name;
	for (DWORD k = 0; k < count; ++k) {
		unsigned char flags = 0;
		unsigned short length = 0;
		ui64 size = 0, actual = 0, time = 0;
		if (!in.U8(flags) || !in.U16(length) || length == 0) return false;
		name.resize((size_t)length + 1);
		if (!in.Get(name.data(), (size_t)length * sizeof(wchar_t))) return false;
		name[length] = L'\0';
		if (!in.U64(size)) return false;
		actual = size;
		if ((flags & kEntryActualDiffers) && !in.U64(actual)) return false;
		if (!in.U64(time)) return false;

		if (flags & kEntryFolder) {
			CFolder *child = new CFolder;
			if (!ReadFolder(in, child, arena, depth + 1, cancel)
				|| !folder->AddFolderWithArena(arena, name.data(), length, child, time)) {
				delete child;
				return false;
			}
		}
		else if (!folder->AddFileWithArena(arena, name.data(), length, size, actual, time)) {
			return false;
		}
	}
	return true;
}

std::wstring CacheFolder()
{
	wchar_t local[MAX_PATH];
	DWORD n = ::ExpandEnvironmentStringsW(L"%LOCALAPPDATA%", local, MAX_PATH);
	if (n == 0 || n > MAX_PATH || wcschr(local, L'%') != nullptr) return std::wstring();
	std::wstring folder = std::wstring(local) + L"\\SpaceMonger";
	::CreateDirectoryW(folder.c_str(), NULL);
	folder += L"\\cache";
	if (!::CreateDirectoryW(folder.c_str(), NULL) && ::GetLastError() != ERROR_ALREADY_EXISTS)
		return std::wstring();
	return folder;
}

// Keeps the newest few cache files so the folder cannot grow without limit.
void PruneCacheFolder(const std::wstring &folder)
{
	struct Entry { std::wstring path; ui64 time; };
	std::vector<Entry> entries;
	WIN32_FIND_DATAW data;
	HANDLE find = ::FindFirstFileW((folder + L"\\*" + kExtension).c_str(), &data);
	if (find == INVALID_HANDLE_VALUE) return;
	do {
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		entries.push_back({ folder + L"\\" + data.cFileName,
			((ui64)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime });
	} while (::FindNextFileW(find, &data));
	::FindClose(find);
	if (entries.size() <= kMaxCachedRoots) return;
	std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.time > b.time; });
	for (size_t i = kMaxCachedRoots; i < entries.size(); ++i) ::DeleteFileW(entries[i].path.c_str());
}

}

std::wstring SM_ScanCacheFile(const std::wstring &rootPath)
{
	std::wstring folder = CacheFolder();
	if (folder.empty() || rootPath.empty()) return std::wstring();
	// FNV-1a over the lower-case path, so "C:\" and "c:\" share one file.
	ui64 hash = 14695981039346656037ull;
	for (wchar_t c : rootPath) {
		hash ^= (ui64)towlower(c);
		hash *= 1099511628211ull;
	}
	wchar_t name[40];
	swprintf_s(name, L"\\%016llx%s", hash, kExtension);
	return folder + name;
}

bool SM_WriteScanCache(const std::wstring &file, const CFolder *root, const SM_ScanCacheInfo &info,
	const std::atomic<bool> *cancel)
{
	if (root == nullptr || file.empty() || info.numFiles + info.numFolders > SM_SCAN_CACHE_MAX_ENTRIES) return false;
	const std::wstring temp = file + L".tmp";
	HANDLE handle = ::CreateFileW(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (handle == INVALID_HANDLE_VALUE) return false;

	bool ok = false;
	{
		Writer out(handle);
		out.U32(kMagic);
		out.U32(kVersion);
		out.U64(info.scanTime);
		out.U64(info.numFiles);
		out.U64(info.numFolders);
		out.U64(info.filespace);
		out.U64(info.clustersize);
		out.U8(info.wholeVolume ? 1 : 0);
		out.U8(info.partial ? 1 : 0);
		out.U32((DWORD)info.path.size());
		out.Put(info.path.data(), info.path.size() * sizeof(wchar_t));
		ok = WriteFolder(out, root, true, 0, cancel);
		if (ok) out.U32(kEndMarker);
		out.Flush();
		ok = ok && out.ok();
	}
	::CloseHandle(handle);
	if (ok) ok = ::MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
	if (!ok) {
		::DeleteFileW(temp.c_str());
		return false;
	}
	std::wstring folder = file.substr(0, file.find_last_of(L'\\'));
	PruneCacheFolder(folder);
	return true;
}

CFolder *SM_ReadScanCache(const std::wstring &file, CStringArena &arena, SM_ScanCacheInfo &info,
	const std::atomic<bool> *cancel)
{
	HANDLE handle = ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (handle == INVALID_HANDLE_VALUE) return nullptr;

	CFolder *root = nullptr;
	{
		Reader in(handle);
		DWORD magic = 0, version = 0, pathLength = 0, endMarker = 0;
		unsigned char whole = 0, partial = 0;
		bool ok = in.U32(magic) && magic == kMagic && in.U32(version) && version == kVersion
			&& in.U64(info.scanTime) && in.U64(info.numFiles) && in.U64(info.numFolders)
			&& in.U64(info.filespace) && in.U64(info.clustersize)
			&& in.U8(whole) && in.U8(partial) && in.U32(pathLength) && pathLength > 0 && pathLength <= 32768;
		if (ok) {
			info.wholeVolume = whole != 0;
			info.partial = partial != 0;
			info.path.assign(pathLength, L'\0');
			ok = in.Get(&info.path[0], (size_t)pathLength * sizeof(wchar_t));
		}
		if (ok) {
			root = new CFolder;
			ok = ReadFolder(in, root, arena, 0, cancel) && in.U32(endMarker) && endMarker == kEndMarker;
		}
		if (!ok) {
			delete root;
			root = nullptr;
		}
	}
	::CloseHandle(handle);
	if (root != nullptr) root->Finalize();
	return root;
}
