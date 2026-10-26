#ifndef CLEANUPDIALOG_H
#define CLEANUPDIALOG_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "CleanupScanner.h"

class CFolderTree;

// "Clean Up": empties well-known temporary folders and caches the user
// picks (with size and file count up front), and lists the largest personal
// files of the current scan as suggestions the user acts on themselves.
// Sizes are measured on a worker thread, so the dialog opens at once and
// each row fills in when its folder has been measured. Cleaning runs on a
// worker too: a progress bar and the current location show how far it is,
// and the button turns into Stop.
class CCleanupDialog : public CDialog {
public:
	CCleanupDialog(CFolderTree *tree, CWnd *parent = NULL);
	virtual ~CCleanupDialog();

	// True if anything was deleted, so the caller can rescan.
	BOOL FreedAnything(void) const { return m_freedAnything; }

protected:
	virtual BOOL OnInitDialog();
	afx_msg void OnDelete();
	afx_msg void OnOpenFolder();
	afx_msg void OnDetails();
	afx_msg void OnRecycleFiles();
	afx_msg void OnLargeChanged(NMHDR *pNotify, LRESULT *pResult);
	afx_msg void OnLargeInfoTip(NMHDR *pNotify, LRESULT *pResult);
	afx_msg void OnDestroy();
	afx_msg void OnTargetChanged(NMHDR *pNotify, LRESULT *pResult);
	afx_msg void OnLargeDblClk(NMHDR *pNotify, LRESULT *pResult);
	afx_msg LRESULT OnMeasured(WPARAM index, LPARAM generation);
	afx_msg LRESULT OnCleanFinished(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTimer(UINT_PTR id);
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()

	void FillTargets(void);
	void FillLargeFiles(void);
	void UpdateSummary(void);
	ui64 CheckedBytes(void);
	void StopMeasuring(void);
	void StopCleaning(void);
	void SetBusy(BOOL busy);
	void ShowCleanProgress(void);
	int RowOf(size_t index);

	CFolderTree *m_tree;
	CListCtrl m_targets;
	CListCtrl m_large;
	std::vector<SM_CleanupTarget> m_items;
	std::vector<SM_LargeFile> m_files;
	BOOL m_filling;
	BOOL m_freedAnything;
	BOOL m_detailsShown;

	std::thread m_measurer;
	std::atomic<bool> m_cancel;
	std::mutex m_lock;
	std::vector<SM_CleanupTarget> m_measured;	// written by the worker, guarded by m_lock
	size_t m_pending;
	LPARAM m_generation;	// drops results posted by a measurement that was restarted

	CProgressCtrl m_bar;
	std::thread m_cleaner;
	SM_CleanupProgress m_cleanProgress;
	BOOL m_cleaning;
	BOOL m_closeAfterClean;
	ui64 m_cleanTotal;						// measured bytes of the chosen locations
	std::vector<size_t> m_cleanChosen;		// indexes into m_items, in cleaning order
	std::atomic<size_t> m_cleanCurrent;		// position in m_cleanChosen being cleaned
	size_t m_cleanMarked;					// rows already shown as done
	CString m_lastStatus;					// avoids repainting identical text
};

#endif
