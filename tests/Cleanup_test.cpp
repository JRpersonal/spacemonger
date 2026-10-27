#include "../CleanupScanner.h"
#include "../Folder.h"
#include "../StringArena.h"

#include <stdio.h>
#include <string>
#include <vector>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
			fflush(stderr); \
			return 0; \
		} \
	} while (0)

static std::wstring MakeTempDir()
{
	wchar_t base[MAX_PATH];
	GetTempPathW(MAX_PATH, base);
	std::wstring dir = std::wstring(base) + L"SM_Cleanup_test_" + std::to_wstring(GetCurrentProcessId());
	CreateDirectoryW(dir.c_str(), NULL);
	return dir;
}

static void WriteFile(const std::wstring &path, DWORD bytes, bool old)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	std::vector<char> data(bytes, 'x');
	DWORD written = 0;
	if (bytes > 0) WriteFile(h, data.data(), bytes, &written, NULL);
	if (old) {
		// Three days ago: outside the 24 h "keep recent" window.
		FILETIME now;
		GetSystemTimeAsFileTime(&now);
		ULARGE_INTEGER t;
		t.LowPart = now.dwLowDateTime;
		t.HighPart = now.dwHighDateTime;
		t.QuadPart -= 3ull * 24 * 60 * 60 * 10000000ull;
		FILETIME ft;
		ft.dwLowDateTime = t.LowPart;
		ft.dwHighDateTime = t.HighPart;
		SetFileTime(h, NULL, NULL, &ft);
	}
	CloseHandle(h);
}

static bool Exists(const std::wstring &path)
{
	return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static int test_measure_and_clean_keep_recent()
{
	std::wstring dir = MakeTempDir();
	std::wstring sub = dir + L"\\sub";
	CreateDirectoryW(sub.c_str(), NULL);
	WriteFile(dir + L"\\old.tmp", 1000, true);
	WriteFile(sub + L"\\old2.tmp", 500, true);
	WriteFile(dir + L"\\new.tmp", 300, false);

	ui64 bytes = 0, files = 0;
	SM_MeasureFolder(dir, false, bytes, files);
	CHECK(bytes == 1800 && files == 3);
	bytes = files = 0;
	SM_MeasureFolder(dir, true, bytes, files);
	CHECK(bytes == 1500 && files == 2);

	SM_CleanupResult r = SM_CleanFolder(dir, true);
	CHECK(r.deletedFiles == 2 && r.freedBytes == 1500 && r.skippedFiles == 1);
	CHECK(!Exists(dir + L"\\old.tmp"));
	CHECK(!Exists(sub));				// emptied subfolder removed
	CHECK(Exists(dir + L"\\new.tmp"));	// recent file kept
	CHECK(Exists(dir));					// the target folder itself stays

	r = SM_CleanFolder(dir, false);
	CHECK(r.deletedFiles == 1 && !Exists(dir + L"\\new.tmp"));
	RemoveDirectoryW(dir.c_str());
	return 1;
}

static int test_in_use_file_is_skipped()
{
	std::wstring dir = MakeTempDir();
	std::wstring path = dir + L"\\locked.tmp";
	WriteFile(path, 100, true);
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	SM_CleanupResult r = SM_CleanFolder(dir, false);
	CHECK(r.deletedFiles == 0 && r.skippedFiles == 1);
	CloseHandle(h);
	r = SM_CleanFolder(dir, false);
	CHECK(r.deletedFiles == 1);
	RemoveDirectoryW(dir.c_str());
	return 1;
}

// The dialog reads these counters from another thread to show progress.
static int test_progress_counters_and_cancel()
{
	std::wstring dir = MakeTempDir();
	WriteFile(dir + L"\\a.tmp", 1000, true);
	WriteFile(dir + L"\\b.tmp", 500, true);
	WriteFile(dir + L"\\recent.tmp", 300, false);

	SM_CleanupTarget target;
	target.kind = SM_CLEAN_USER_TEMP;
	target.paths.push_back(dir);
	target.keepRecent = true;
	target.defaultChecked = true;
	target.bytes = target.files = 0;

	SM_CleanupProgress progress;
	SM_CleanupResult r = SM_CleanTarget(target, &progress);
	CHECK(r.deletedFiles == 2 && r.freedBytes == 1500);
	CHECK(progress.deletedFiles.load() == 2 && progress.freedBytes.load() == 1500);
	CHECK(progress.processedBytes.load() == 1500);	// the recent file was never part of the total
	CHECK(progress.skippedFiles.load() == 1);

	// A clean-up that was stopped before it started removes nothing.
	WriteFile(dir + L"\\c.tmp", 100, true);
	SM_CleanupProgress stopped;
	stopped.cancel.store(true);
	r = SM_CleanTarget(target, &stopped);
	CHECK(r.deletedFiles == 0 && Exists(dir + L"\\c.tmp"));

	SM_CleanFolder(dir, false);
	RemoveDirectoryW(dir.c_str());
	return 1;
}

static int test_classify()
{
	CHECK(SM_ClassifyLargeFile(L"holiday.MP4") == SM_LARGE_VIDEO);
	CHECK(SM_ClassifyLargeFile(L"docker_data.vhdx") == SM_LARGE_VIRTUAL_DISK);
	CHECK(SM_ClassifyLargeFile(L"Win11.iso") == SM_LARGE_DISK_IMAGE);
	CHECK(SM_ClassifyLargeFile(L"iPhone18,2_27.0_Restore.ipsw") == SM_LARGE_PHONE_UPDATE);
	CHECK(SM_ClassifyLargeFile(L"notes.txt") == SM_LARGE_OTHER);
	return 1;
}

static int test_large_personal_files()
{
	CStringArena arena;
	CFolder *windows = new CFolder;
	windows->AddFileWithArena(arena, L"huge.sys", 8, 9000, 9000, 0);
	CFolder *downloads = new CFolder;
	downloads->AddFileWithArena(arena, L"setup.bin", 9, 3000, 3000, 0);
	downloads->AddFileWithArena(arena, L"tiny.txt", 8, 10, 10, 0);
	CFolder *cache = new CFolder;
	cache->AddFileWithArena(arena, L"cache.bin", 9, 8000, 8000, 0);
	CFolder *user = new CFolder;
	user->AddFileWithArena(arena, L"movie.mkv", 9, 5000, 5000, 0);
	user->AddFolderWithArena(arena, L"Downloads", 9, downloads, 0);
	user->AddFolderWithArena(arena, L"Cache", 5, cache, 0);

	CFolder root;
	root.AddFolderWithArena(arena, L"Windows", 7, windows, 0);
	root.AddFolderWithArena(arena, L"me", 2, user, 0);
	root.AddFileWithArena(arena, L"<<<<", 4, 99999, 99999, 0);

	std::vector<std::wstring> excluded = { L"C:\\me\\Cache" };
	std::vector<SM_LargeFile> found = SM_FindLargePersonalFiles(&root, L"C:\\", 10, 1000, excluded);
	CHECK(found.size() == 2);								// not Windows, not the cache, not free space, not tiny
	CHECK(found[0].path == L"C:\\me\\movie.mkv" && found[0].kind == SM_LARGE_VIDEO);
	CHECK(found[1].path == L"C:\\me\\Downloads\\setup.bin");
	CHECK(found[1].inDownloads && found[1].kind == SM_LARGE_INSTALLER);

	found = SM_FindLargePersonalFiles(&root, L"C:\\", 1, 1000, excluded);
	CHECK(found.size() == 1 && found[0].bytes == 5000);		// keeps the largest
	return 1;
}

static int test_targets_exist()
{
	std::vector<SM_CleanupTarget> targets = SM_FindCleanupTargets();
	bool userTemp = false, bin = false;
	for (const auto &t : targets) {
		if (t.kind == SM_CLEAN_USER_TEMP) userTemp = !t.paths.empty();
		if (t.kind == SM_CLEAN_RECYCLE_BIN) bin = true;
	}
	CHECK(userTemp && bin);
	return 1;
}

int main()
{
	if (!test_measure_and_clean_keep_recent()) return 1;
	if (!test_in_use_file_is_skipped()) return 1;
	if (!test_progress_counters_and_cancel()) return 1;
	if (!test_classify()) return 1;
	if (!test_large_personal_files()) return 1;
	if (!test_targets_exist()) return 1;
	printf("Cleanup_test passed successfully.\n");
	return 0;
}
