#ifndef TREEMAPLAYOUT_H
#define TREEMAPLAYOUT_H

#include <vector>
#include <string>
#include "e.h"
#include "Folder.h"

enum TreemapNodeFlags : ui32 {
	TREEMAP_FLAG_NONE     = 0,
	TREEMAP_FLAG_FOLDER   = (1 << 0), // 1: Node represents a folder
	TREEMAP_FLAG_SPECIAL  = (1 << 1), // 2: Special block (e.g. Free Space)
	TREEMAP_FLAG_HOVER    = (1 << 2), // 4: Mouse hover highlight
	TREEMAP_FLAG_UNAVAILABLE = (1 << 3), // 8: Estimated unaccounted space
	TREEMAP_FLAG_PENDING  = (1 << 4), // 16: Live scan: used space not scanned yet
	TREEMAP_FLAG_SYSTEM   = (1 << 5), // 32: Inside a Windows-owned root entry (SystemPaths.h)
	TREEMAP_FLAG_SCANNING = (1 << 6), // 64: Live scan: folder still being read, contents not shown yet
};

struct TreemapNode {
	const wchar_t *name;
	si32 depth;
	ui32 flags;
	CFolder *source;
	ui32 index;
	ui32 sample;          // placeholder block (index -1): its largest entry, else -1
	si16 x, y, w, h;

	TreemapNode()
		: name(nullptr), depth(0), flags(TREEMAP_FLAG_NONE),
		  source(nullptr), index((ui32)-1), sample((ui32)-1),
		  x(0), y(0), w(0), h(0) {}

	inline bool IsFolder() const { return (flags & TREEMAP_FLAG_FOLDER) != 0; }
	inline bool IsSpecial() const { return (flags & TREEMAP_FLAG_SPECIAL) != 0; }
	inline bool IsHovered() const { return (flags & TREEMAP_FLAG_HOVER) != 0; }
};

struct TreemapConfig {
	int density;          // [-3..3], defaults to 0
	int bias;             // Aspect ratio preference [-8..8], defaults to 0
	bool showFreeSpace;   // Whether to allocate space for <Free Space>
	int dpi;              // Target DPI, defaults to 96
	int titleHeight;      // Folder title bar incl. top border; 12 for the classic 9 px font
	const CFolder *volumeRoot; // Scanned volume root, for system-entry detection (or null)
	bool hideSystem;      // Leave system root entries out of the layout
	bool liveMode;        // Live scan: show folder contents only once complete

	TreemapConfig()
		: density(0), bias(0), showFreeSpace(true), dpi(96), titleHeight(12),
		  volumeRoot(nullptr), hideSystem(false), liveMode(false) {}
};

// Inner frame of a folder box: 3 px sides and bottom, a title bar on top.
static const int TREEMAP_FOLDER_BORDER = 3;

class TreemapEngine {
public:
	// Computes layout for the given folder tree into a flat vector of TreemapNode
	static void ComputeLayout(
		int x, int y, int w, int h,
		CFolder *root,
		int startDepth,
		const TreemapConfig &config,
		std::vector<TreemapNode> &outNodes
	);

	// Live-scan layout for a scan whose final size is known up front (a whole
	// volume). The used and free areas are fixed from the first frame; the
	// scanned part of the used area grows while a pending block for the rest
	// shrinks, so boxes grow in place instead of the whole map rescaling every
	// frame. root's sizes must already hold the live (partial) totals.
	static void ComputeLiveLayout(
		int x, int y, int w, int h,
		CFolder *root,
		ui64 usedBytes,
		ui64 freeBytes,
		const TreemapConfig &config,
		std::vector<TreemapNode> &outNodes
	);

	// The finished map of a whole volume, laid out like the last live frame:
	// root's "<..." entry becomes the free block, its "|..." entry (space the
	// scan could not read) takes the place of the pending block.
	static void ComputeVolumeLayout(
		int x, int y, int w, int h,
		CFolder *root,
		const TreemapConfig &config,
		std::vector<TreemapNode> &outNodes
	);

	// Calculates DPI-scaled minimum box dimensions based on density setting [-3..3]
	static void GetMinDimensions(int density, int dpi, int &outHMin, int &outVMin);

	// Finds the innermost item (file or folder header) at point (px, py)
	static const TreemapNode* HitTestItem(
		const std::vector<TreemapNode> &nodes,
		int px, int py,
		int titleHeight = 12
	);

	// Finds the tightest container folder enclosing point (px, py)
	static const TreemapNode* HitTestContainer(
		const std::vector<TreemapNode> &nodes,
		int px, int py
	);
};

#endif // TREEMAPLAYOUT_H
