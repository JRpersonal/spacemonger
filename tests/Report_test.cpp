#include "../SpaceReport.h"
#include "../Folder.h"
#include "../StringArena.h"

#include <windows.h>
#include <stdio.h>
#include <string>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "CHECK failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
			fflush(stderr); \
			return 0; \
		} \
	} while (0)

static bool Contains(const std::wstring &text, const wchar_t *part)
{
	return text.find(part) != std::wstring::npos;
}

static size_t Position(const std::wstring &text, const wchar_t *part)
{
	return text.find(part);
}

static int TestReportListsBiggestFirst()
{
	CStringArena arena;
	CFolder root;
	CFolder *windows = new CFolder;
	windows->AddFileWithArena(arena, L"huge.sys", 8, 9000, 9000, 0);
	CFolder *downloads = new CFolder;
	downloads->AddFileWithArena(arena, L"setup.bin", 9, 3000, 3000, 0);
	downloads->AddFileWithArena(arena, L"a,b \"c\".txt", 11, 10, 10, 0);
	CFolder *user = new CFolder;
	user->AddFolderWithArena(arena, L"Downloads", 9, downloads, 0);
	root.AddFolderWithArena(arena, L"Windows", 7, windows, 0);
	root.AddFolderWithArena(arena, L"me", 2, user, 0);
	root.AddFileWithArena(arena, L"<<<<<<<<<<<<<<<<<<<<", 20, 50000, 50000, 0);	// free space
	root.AddFileWithArena(arena, L"|Not readable", 13, 400, 400, 0);
	root.Finalize();

	SM_ReportVolume volume;
	volume.computer = L"PC01";
	volume.path = L"C:\\";
	volume.wholeVolume = true;
	volume.totalSpace = 70000;
	volume.freeSpace = 50000;
	volume.scannedBytes = 12010;

	SM_ReportOptions options;
	options.top = 10;
	std::wstring csv = SM_BuildSpaceReport(&root, volume, options);

	CHECK(Contains(csv, L"Computer,Kind,Path,"));
	CHECK(Contains(csv, L"PC01,Free,C:\\,50000,"));
	CHECK(Contains(csv, L"PC01,Not readable,C:\\,7990,"));
	CHECK(Contains(csv, L"PC01,Folder,C:\\Windows\\,9000,"));
	CHECK(Contains(csv, L"PC01,Folder,C:\\me\\Downloads\\,3010,"));
	CHECK(Contains(csv, L"PC01,File,C:\\me\\Downloads\\setup.bin,3000,"));
	// Separators and quotes in names are escaped.
	CHECK(Contains(csv, L"\"C:\\me\\Downloads\\a,b \"\"c\"\".txt\""));
	// Pseudo entries are never listed as files.
	CHECK(!Contains(csv, L"<<<<"));
	CHECK(!Contains(csv, L"|Not readable"));
	// Biggest folder first; Windows is system content.
	CHECK(Position(csv, L"C:\\Windows\\,") < Position(csv, L"C:\\me\\,"));
	CHECK(Contains(csv, L"C:\\Windows\\huge.sys,9000,0.00,"));

	options.hideSystem = true;
	options.top = 1;
	options.separator = L';';
	options.decimal = L',';
	csv = SM_BuildSpaceReport(&root, volume, options);
	CHECK(!Contains(csv, L"Windows"));
	CHECK(Contains(csv, L"PC01;Folder;C:\\me\\;3010;"));
	CHECK(!Contains(csv, L"C:\\me\\Downloads\\;"));	// top 1 only
	return 1;
}

static int TestComputerAndReportPath()
{
	CHECK(SM_ReportComputerForPath(L"\\\\PC01\\c$\\") == L"PC01");
	CHECK(SM_ReportComputerForPath(L"\\\\?\\UNC\\PC02\\d$\\x") == L"PC02");
	CHECK(!SM_ReportComputerForPath(L"C:\\").empty());

	wchar_t temp[MAX_PATH];
	GetTempPathW(MAX_PATH, temp);
	std::wstring resolved = SM_ResolveReportPath(temp, L"PC$01");
	CHECK(resolved.rfind(temp, 0) == 0);
	CHECK(Contains(resolved, L"PC_01_"));
	CHECK(resolved.size() > 4 && resolved.substr(resolved.size() - 4) == L".csv");
	CHECK(SM_ResolveReportPath(L"C:\\x\\report.csv", L"PC01") == L"C:\\x\\report.csv");

	std::wstring file = std::wstring(temp) + L"SM_Report_test.csv";
	CHECK(SM_WriteUtf8File(file, L"Größe\r\n"));
	HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	CHECK(h != INVALID_HANDLE_VALUE);
	char buf[16] = { 0 };
	DWORD read = 0;
	ReadFile(h, buf, sizeof(buf), &read, NULL);
	CloseHandle(h);
	DeleteFileW(file.c_str());
	CHECK(read == 3 + 7 + 2);	// BOM + "Größe" in UTF-8 + CRLF
	CHECK((unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF);
	return 1;
}

int main()
{
	if (!TestReportListsBiggestFirst()) return 1;
	if (!TestComputerAndReportPath()) return 1;
	printf("Report_test passed\n");
	return 0;
}
