#include "../ScanCache.h"
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

static std::wstring TempFile(const wchar_t *name)
{
	wchar_t base[MAX_PATH];
	GetTempPathW(MAX_PATH, base);
	return std::wstring(base) + L"SM_ScanCache_test_" + std::to_wstring(GetCurrentProcessId()) + name;
}

// \Users (alice: report.docx, video.mkv; bob: empty) + notes.txt + synthetic entries.
static CFolder *MakeTree(CStringArena &arena)
{
	CFolder *alice = new CFolder;
	alice->AddFileWithArena(arena, L"report.docx", 11, 4096, 3000, 111);
	alice->AddFileWithArena(arena, L"video.mkv", 9, 900000, 900000, 222);
	CFolder *bob = new CFolder;
	CFolder *users = new CFolder;
	users->AddFolderWithArena(arena, L"alice", 5, alice, 333);
	users->AddFolderWithArena(arena, L"bob", 3, bob, 444);
	CFolder *root = new CFolder;
	root->AddFolderWithArena(arena, L"Users", 5, users, 555);
	root->AddFileWithArena(arena, L"notes.txt", 9, 1024, 1024, 666);
	root->AddFileWithArena(arena, L"<", 1, 777777, 777777, 0);			// free space
	root->AddFileWithArena(arena, L"|Not readable", 13, 888888, 888888, 0);	// unreadable space
	root->Finalize();
	return root;
}

static SM_ScanCacheInfo MakeInfo()
{
	SM_ScanCacheInfo info;
	info.path = L"C:\\";
	info.scanTime = 132000000000000000ull;
	info.numFiles = 3;
	info.numFolders = 3;
	info.filespace = 905120;
	info.clustersize = 4096;
	info.wholeVolume = true;
	info.partial = true;
	return info;
}

static int FindEntry(const CFolder *folder, const wchar_t *name)
{
	for (unsigned i = 0; i < folder->cur; ++i)
		if (wcscmp(folder->names[i], name) == 0) return (int)i;
	return -1;
}

static int test_round_trip()
{
	CStringArena arena;
	CFolder *tree = MakeTree(arena);
	std::wstring file = TempFile(L"_round.smcache");
	CHECK(SM_WriteScanCache(file, tree, MakeInfo()));

	CStringArena loadedArena;
	SM_ScanCacheInfo info;
	CFolder *loaded = SM_ReadScanCache(file, loadedArena, info);
	CHECK(loaded != NULL);
	CHECK(info.path == L"C:\\" && info.scanTime == 132000000000000000ull);
	CHECK(info.numFiles == 3 && info.numFolders == 3 && info.filespace == 905120 && info.clustersize == 4096);
	CHECK(info.wholeVolume && info.partial);

	// The synthetic entries are not stored; everything real is.
	CHECK(loaded->cur == 2);
	CHECK(FindEntry(loaded, L"<") < 0 && FindEntry(loaded, L"|Not readable") < 0);
	int users = FindEntry(loaded, L"Users");
	CHECK(users >= 0 && loaded->children[users] != NULL);
	CHECK(loaded->times[users] == 555);
	CHECK(loaded->sizes[users] == 904096 && loaded->actualsizes[users] == 904096);
	int notes = FindEntry(loaded, L"notes.txt");
	CHECK(notes >= 0 && loaded->children[notes] == NULL && loaded->sizes[notes] == 1024 && loaded->times[notes] == 666);

	CFolder *usersFolder = loaded->children[users];
	int alice = FindEntry(usersFolder, L"alice");
	int bob = FindEntry(usersFolder, L"bob");
	CHECK(alice >= 0 && bob >= 0 && usersFolder->children[bob] != NULL && usersFolder->children[bob]->cur == 0);
	CFolder *aliceFolder = usersFolder->children[alice];
	int report = FindEntry(aliceFolder, L"report.docx");
	CHECK(report >= 0);
	CHECK(aliceFolder->sizes[report] == 4096 && aliceFolder->actualsizes[report] == 3000);	// both sizes survive
	CHECK(aliceFolder->times[report] == 111);
	CHECK(loaded->SizeTotal() == 905120);

	delete loaded;
	delete tree;
	DeleteFileW(file.c_str());
	return 1;
}

static int test_damaged_files_are_rejected()
{
	CStringArena arena;
	CFolder *tree = MakeTree(arena);
	std::wstring file = TempFile(L"_damaged.smcache");
	CHECK(SM_WriteScanCache(file, tree, MakeInfo()));

	// Cut short: the end marker is missing.
	HANDLE h = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h != INVALID_HANDLE_VALUE);
	DWORD size = GetFileSize(h, NULL);
	SetFilePointer(h, (LONG)(size - 12), NULL, FILE_BEGIN);
	SetEndOfFile(h);
	CloseHandle(h);
	CStringArena loadedArena;
	SM_ScanCacheInfo info;
	CHECK(SM_ReadScanCache(file, loadedArena, info) == NULL);

	// Not one of ours.
	h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written = 0;
	WriteFile(h, "this is not a cache file at all, just text", 42, &written, NULL);
	CloseHandle(h);
	CHECK(SM_ReadScanCache(file, loadedArena, info) == NULL);

	// Missing.
	DeleteFileW(file.c_str());
	CHECK(SM_ReadScanCache(file, loadedArena, info) == NULL);
	delete tree;
	return 1;
}

static int test_cancel_and_limits()
{
	CStringArena arena;
	CFolder *tree = MakeTree(arena);
	std::wstring file = TempFile(L"_cancel.smcache");

	std::atomic<bool> cancel(true);
	CHECK(!SM_WriteScanCache(file, tree, MakeInfo(), &cancel));
	CHECK(GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES);	// no half-written file left
	CHECK(GetFileAttributesW((file + L".tmp").c_str()) == INVALID_FILE_ATTRIBUTES);

	// Huge scans are not cached.
	SM_ScanCacheInfo big = MakeInfo();
	big.numFiles = SM_SCAN_CACHE_MAX_ENTRIES;
	big.numFolders = 1;
	CHECK(!SM_WriteScanCache(file, tree, big));

	// A write replaces the previous file.
	CHECK(SM_WriteScanCache(file, tree, MakeInfo()));
	SM_ScanCacheInfo second = MakeInfo();
	second.scanTime = 5;
	CHECK(SM_WriteScanCache(file, tree, second));
	CStringArena loadedArena;
	SM_ScanCacheInfo info;
	CFolder *loaded = SM_ReadScanCache(file, loadedArena, info);
	CHECK(loaded != NULL && info.scanTime == 5);
	delete loaded;
	delete tree;
	DeleteFileW(file.c_str());
	return 1;
}

static int test_file_name_follows_path_not_case()
{
	std::wstring a = SM_ScanCacheFile(L"C:\\Users\\Me\\");
	std::wstring b = SM_ScanCacheFile(L"c:\\users\\me\\");
	std::wstring c = SM_ScanCacheFile(L"D:\\");
	CHECK(!a.empty() && a == b && a != c);
	CHECK(a.size() > 8 && a.compare(a.size() - 8, 8, L".smcache") == 0);
	return 1;
}

int main()
{
	if (!test_round_trip()) return 1;
	if (!test_damaged_files_are_rejected()) return 1;
	if (!test_cancel_and_limits()) return 1;
	if (!test_file_name_follows_path_not_case()) return 1;
	printf("ScanCache_test passed successfully.\n");
	return 0;
}
