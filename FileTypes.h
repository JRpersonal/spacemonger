#ifndef FILETYPES_H
#define FILETYPES_H

// File categories for the "By file type" colour scheme and its legend.
// Grouped the way a non-technical user thinks about their files.

#include <windows.h>
#include <wchar.h>

enum SM_FileType {
	SM_TYPE_VIDEO,
	SM_TYPE_IMAGE,
	SM_TYPE_AUDIO,
	SM_TYPE_DOCUMENT,
	SM_TYPE_ARCHIVE,		// archives, disk images, virtual disks, backups
	SM_TYPE_PROGRAM,		// programs, libraries, installers
	SM_TYPE_OTHER,
	SM_TYPE_COUNT
};

inline SM_FileType SM_ClassifyFileType(const wchar_t *name)
{
	static const struct { const wchar_t *ext; SM_FileType type; } table[] = {
		{ L"mp4", SM_TYPE_VIDEO }, { L"mkv", SM_TYPE_VIDEO }, { L"mov", SM_TYPE_VIDEO },
		{ L"avi", SM_TYPE_VIDEO }, { L"wmv", SM_TYPE_VIDEO }, { L"m4v", SM_TYPE_VIDEO },
		{ L"mts", SM_TYPE_VIDEO }, { L"webm", SM_TYPE_VIDEO }, { L"mpg", SM_TYPE_VIDEO },
		{ L"jpg", SM_TYPE_IMAGE }, { L"jpeg", SM_TYPE_IMAGE }, { L"png", SM_TYPE_IMAGE },
		{ L"heic", SM_TYPE_IMAGE }, { L"gif", SM_TYPE_IMAGE }, { L"bmp", SM_TYPE_IMAGE },
		{ L"tif", SM_TYPE_IMAGE }, { L"tiff", SM_TYPE_IMAGE }, { L"raw", SM_TYPE_IMAGE },
		{ L"cr2", SM_TYPE_IMAGE }, { L"nef", SM_TYPE_IMAGE }, { L"arw", SM_TYPE_IMAGE },
		{ L"dng", SM_TYPE_IMAGE }, { L"psd", SM_TYPE_IMAGE }, { L"webp", SM_TYPE_IMAGE },
		{ L"mp3", SM_TYPE_AUDIO }, { L"wav", SM_TYPE_AUDIO }, { L"flac", SM_TYPE_AUDIO },
		{ L"m4a", SM_TYPE_AUDIO }, { L"aac", SM_TYPE_AUDIO }, { L"ogg", SM_TYPE_AUDIO },
		{ L"wma", SM_TYPE_AUDIO },
		{ L"pdf", SM_TYPE_DOCUMENT }, { L"doc", SM_TYPE_DOCUMENT }, { L"docx", SM_TYPE_DOCUMENT },
		{ L"xls", SM_TYPE_DOCUMENT }, { L"xlsx", SM_TYPE_DOCUMENT }, { L"ppt", SM_TYPE_DOCUMENT },
		{ L"pptx", SM_TYPE_DOCUMENT }, { L"txt", SM_TYPE_DOCUMENT }, { L"odt", SM_TYPE_DOCUMENT },
		{ L"pst", SM_TYPE_DOCUMENT }, { L"ost", SM_TYPE_DOCUMENT }, { L"csv", SM_TYPE_DOCUMENT },
		{ L"zip", SM_TYPE_ARCHIVE }, { L"7z", SM_TYPE_ARCHIVE }, { L"rar", SM_TYPE_ARCHIVE },
		{ L"tar", SM_TYPE_ARCHIVE }, { L"gz", SM_TYPE_ARCHIVE }, { L"iso", SM_TYPE_ARCHIVE },
		{ L"img", SM_TYPE_ARCHIVE }, { L"vhd", SM_TYPE_ARCHIVE }, { L"vhdx", SM_TYPE_ARCHIVE },
		{ L"vmdk", SM_TYPE_ARCHIVE }, { L"bak", SM_TYPE_ARCHIVE }, { L"ipsw", SM_TYPE_ARCHIVE },
		{ L"cab", SM_TYPE_ARCHIVE }, { L"dmg", SM_TYPE_ARCHIVE },
		{ L"exe", SM_TYPE_PROGRAM }, { L"dll", SM_TYPE_PROGRAM }, { L"msi", SM_TYPE_PROGRAM },
		{ L"msix", SM_TYPE_PROGRAM }, { L"sys", SM_TYPE_PROGRAM }, { L"appx", SM_TYPE_PROGRAM },
		{ L"jar", SM_TYPE_PROGRAM }, { L"node", SM_TYPE_PROGRAM },
	};
	if (name == NULL) return SM_TYPE_OTHER;
	const wchar_t *dot = wcsrchr(name, L'.');
	if (dot == NULL || dot[1] == L'\0') return SM_TYPE_OTHER;
	for (const auto &entry : table) {
		if (_wcsicmp(dot + 1, entry.ext) == 0) return entry.type;
	}
	return SM_TYPE_OTHER;
}

// Distinct, not too saturated colours (fill, light edge, dark edge).
inline void SM_FileTypeColors(SM_FileType type, COLORREF &color, COLORREF &bright, COLORREF &dark)
{
	static const COLORREF fills[SM_TYPE_COUNT] = {
		RGB(186, 140, 230),	// video: violet
		RGB(122, 200, 120),	// image: green
		RGB(245, 176, 90),	// audio: orange
		RGB(110, 168, 235),	// document: blue
		RGB(214, 188, 120),	// archive / disk image: sand
		RGB(232, 118, 118),	// program: red
		RGB(205, 205, 205),	// other: grey
	};
	COLORREF c = fills[type < SM_TYPE_COUNT ? type : SM_TYPE_OTHER];
	color = c;
	bright = RGB(min(255, GetRValue(c) + 40), min(255, GetGValue(c) + 40), min(255, GetBValue(c) + 40));
	dark = RGB(GetRValue(c) * 3 / 4, GetGValue(c) * 3 / 4, GetBValue(c) * 3 / 4);
}

#endif
