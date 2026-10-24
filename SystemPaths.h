#ifndef SYSTEMPATHS_H
#define SYSTEMPATHS_H

#include <wchar.h>

// Entries in a volume root that Windows owns: the user cannot meaningfully
// free space inside them by deleting files (Disk Cleanup and Settings are the
// right tools). The treemap shows them muted, and can hide them, so the eye
// goes to the folders where the user can actually act. Everything below one
// of these entries counts as system content too.
inline bool SM_IsSystemRootEntry(const wchar_t *name)
{
	static const wchar_t *const entries[] = {
		L"Windows", L"Program Files", L"Program Files (x86)", L"ProgramData",
		L"$Recycle.Bin", L"System Volume Information", L"Recovery", L"PerfLogs",
		L"$WinREAgent", L"$SysReset", L"$Windows.~BT", L"$Windows.~WS",
		L"Config.Msi", L"MSOCache", L"Boot", L"EFI",
		L"pagefile.sys", L"hiberfil.sys", L"swapfile.sys",
		L"DumpStack.log", L"DumpStack.log.tmp", L"bootmgr", L"BOOTNXT",
	};
	if (name == nullptr) return false;
	for (const wchar_t *entry : entries) {
		if (_wcsicmp(name, entry) == 0) return true;
	}
	return false;
}

#endif
