#ifndef FOLDERTREE_H
#define FOLDERTREE_H

#include "Folder.h"

#ifdef _AFX

#include <afxwin.h>

class CFolderTree : public CFreeDoc {
public:
	CFolderTree();
	virtual ~CFolderTree();

	BOOL LoadTree(const CString &path, BOOL includespace, CWnd *modalwin);
	void GetSpace(const CString &path);
	CFolder *GetRoot(void);

	CFolder *SetCur(const CString &path);
	CFolder *GetCur(void);
	CFolder *Down(unsigned int index);
	CFolder *Up(void);

protected:
	CFolder *root, *cur;

public:
	CStringArena nameArena;
	CString m_path;
	ui64 freespace, usedspace, totalspace, clustersize;
	ui64 numfiles, numfolders;
	ui64 filespace;
	// The scan root is a volume root: the map shows free/unavailable space
	// with the same fixed split as the live scan.
	bool wholeVolume;
};

#endif // _AFX

#endif // FOLDERTREE_H
