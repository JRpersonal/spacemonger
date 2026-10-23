#include "TreemapLayout.h"
#include "SystemPaths.h"
#include <windows.h>
#include <algorithm>

void TreemapEngine::GetMinDimensions(int density, int dpi, int &outHMin, int &outVMin)
{
	static const int minsizes[][2] = {
		{ 96, 64, },
		{ 64, 48, },
		{ 48, 32, },
		{ 32, 24, },
		{ 24, 16, },
		{ 16, 12, },
		{ 8,  6,  },
	};

	int d = density + 3;
	if (d < 0) d = 0;
	if (d > 6) d = 6;
	if (dpi <= 0) dpi = 96;

	outHMin = MulDiv(minsizes[d][0], dpi, 96);
	outVMin = MulDiv(minsizes[d][1], dpi, 96);
}

// True if (folder, index) lies in a system entry of the volume root: walk up
// to the entry directly below the root and check its name.
static bool IsSystemEntry(const CFolder *folder, ui32 index, const CFolder *volumeRoot)
{
	if (volumeRoot == nullptr || folder == nullptr) return false;
	const CFolder *f = folder;
	ui32 i = index;
	while (f != nullptr && f != volumeRoot) {
		i = f->parentindex;
		f = f->parent;
	}
	if (f != volumeRoot || i == (ui32)-1 || i >= f->cur) return false;
	return SM_IsSystemRootEntry(f->names[i]);
}

static void AddNode(
	std::vector<TreemapNode> &nodes,
	const TreemapConfig &config,
	CFolder *source, ui32 index,
	si32 depth, si16 x, si16 y, si16 w, si16 h, ui32 flags)
{
	if (IsSystemEntry(source, index, config.volumeRoot)) flags |= TREEMAP_FLAG_SYSTEM;
	TreemapNode node;
	wchar_t c = 0;

	if (source != nullptr && index != (ui32)-1) {
		node.name = source->names[index];
		if (node.name != nullptr) c = node.name[0];
	} else {
		node.name = nullptr;
	}

	if (c == L'*' || c == L'<' || c == L'>' || c == L'?' || c == L'|') {
		node.depth = -1;
		flags |= TREEMAP_FLAG_SPECIAL;
		if (c == L'|') flags |= TREEMAP_FLAG_UNAVAILABLE;
	} else {
		node.depth = depth;
	}

	node.flags = flags;
	node.source = source;
	node.index = index;
	node.x = x;
	node.y = y;
	node.w = w;
	node.h = h;

	nodes.push_back(node);
}

static void ComputeLayoutInternal(
	int x, int y, int w, int h,
	CFolder *folder,
	int depth,
	const TreemapConfig &config,
	int hmin, int vmin,
	std::vector<TreemapNode> &outNodes,
	bool skipVolumeSpecials = false);

// Live scans fill the map folder by folder: every subfolder appears as one
// group once all of it has been read, also inside folders that are still
// being scanned. Loose files of an unfinished folder wait until that folder
// is done, so nothing animates file by file. (The scan root is exempt: its
// files are few and large.)
static bool ShowContents(const CFolder *child, const TreemapConfig &config)
{
	return child != nullptr;
}

static bool HideLooseEntries(const CFolder *folder, const TreemapConfig &config)
{
	return config.liveMode && folder != nullptr && !folder->liveComplete && folder->parent != nullptr;
}

static ui32 FolderFlags(const CFolder *child, const TreemapConfig &config)
{
	if (child == nullptr) return TREEMAP_FLAG_NONE;
	return TREEMAP_FLAG_FOLDER | (config.liveMode && !child->liveComplete ? TREEMAP_FLAG_SCANNING : TREEMAP_FLAG_NONE);
}

static void SizeFolders(
	int x, int y, int w, int h,
	CFolder *folder,
	int *index, int *scratch, int numindices, int depth,
	const TreemapConfig &config,
	int hmin, int vmin,
	std::vector<TreemapNode> &outNodes)
{
	if (folder == nullptr || numindices <= 0) return;

	int numlist1 = 0, numlist2 = 0, largest;
	int list2_back = numindices;
	si64 list1sum = 0, list2sum = 0, bignum;
	int x1, y1, w1, h1;
	int x2, y2, w2, h2;
	int split;

	// Split the lists evenly. We assume the sizes are sorted
	// in descending order. Overall, this is a greedy algorithm,
	// so it should produce fairly good results.
	for (largest = 0; largest < numindices; largest++) {
		bignum = (si64)folder->sizes[index[largest]];
		if (folder->names[index[largest]] != nullptr
			&& folder->names[index[largest]][0] == L'<'
			&& !config.showFreeSpace) {
			bignum = 0;
		}
		if (bignum != 0) {
			if (list1sum <= list2sum) {
				scratch[numlist1++] = index[largest];
				list1sum += bignum;
			} else {
				scratch[--list2_back] = index[largest];
				list2sum += bignum;
				numlist2++;
			}
		}
	}

	// Don't bother if the files have no space
	if (list1sum + list2sum <= 0) {
		return;
	}

	// Copy lists back into the index array
	for (int i = 0; i < numlist1; i++) {
		index[i] = scratch[i];
	}
	for (int i = 0; i < numlist2; i++) {
		index[numlist1 + i] = scratch[numindices - 1 - i];
	}

	// Calculate aspect ratio with bias to determine split axis
	int wbias = 8, hbias = 8;
	if (config.bias > 0) {
		wbias = config.bias + 8;
	} else if (config.bias < 0) {
		hbias = -config.bias + 8;
	}

	if (((w * wbias) / 8) > ((h * hbias) / 8)) {
		split = (int)(((si64)w * list1sum) / (list1sum + list2sum));
		x1 = x; y1 = y; w1 = split; h1 = h;
		x2 = x + split; y2 = y; w2 = w - split; h2 = h;
	} else {
		split = (int)(((si64)h * list1sum) / (list1sum + list2sum));
		x1 = x; y1 = y; w1 = w; h1 = split;
		x2 = x; y2 = y + split; w2 = w; h2 = h - split;
	}

	// Subdivide list1
	if (numlist1 > 1 && w1 > hmin && h1 > vmin) {
		SizeFolders(x1, y1, w1, h1, folder, index, scratch, numlist1, depth, config, hmin, vmin, outNodes);
	} else if (numlist1 > 0) {
		if (folder->children[index[0]] == nullptr && HideLooseEntries(folder, config)) {
			// Loose file of an unfinished folder: keep its space, show it later.
		} else if (w1 > hmin && h1 > vmin) {
			AddNode(outNodes, config, folder, index[0],
				depth, (si16)x1, (si16)y1, (si16)w1, (si16)h1,
				FolderFlags(folder->children[index[0]], config));
			if (ShowContents(folder->children[index[0]], config)) {
				ComputeLayoutInternal(x1 + TREEMAP_FOLDER_BORDER, y1 + config.titleHeight,
					w1 - 2 * TREEMAP_FOLDER_BORDER, h1 - config.titleHeight - TREEMAP_FOLDER_BORDER,
					folder->children[index[0]], depth + 1, config, hmin, vmin, outNodes);
			}
		} else if (!HideLooseEntries(folder, config)) {
			// Sub-threshold region: emit a depth-colored placeholder block so
			// dense folders keep the classic "filled" texture.
			AddNode(outNodes, config, folder, (ui32)-1, depth, (si16)x1, (si16)y1, (si16)w1, (si16)h1, TREEMAP_FLAG_NONE);
		}
	}

	// Subdivide list2
	if (numlist2 > 1 && w2 > hmin && h2 > vmin) {
		SizeFolders(x2, y2, w2, h2, folder, index + numlist1, scratch, numlist2, depth, config, hmin, vmin, outNodes);
	} else if (numlist2 > 0) {
		if (folder->children[index[numlist1]] == nullptr && HideLooseEntries(folder, config)) {
			// Loose file of an unfinished folder: keep its space, show it later.
		} else if (w2 > hmin && h2 > vmin) {
			AddNode(outNodes, config, folder, index[numlist1],
				depth, (si16)x2, (si16)y2, (si16)w2, (si16)h2,
				FolderFlags(folder->children[index[numlist1]], config));
			if (ShowContents(folder->children[index[numlist1]], config)) {
				ComputeLayoutInternal(x2 + TREEMAP_FOLDER_BORDER, y2 + config.titleHeight,
					w2 - 2 * TREEMAP_FOLDER_BORDER, h2 - config.titleHeight - TREEMAP_FOLDER_BORDER,
					folder->children[index[numlist1]], depth + 1, config, hmin, vmin, outNodes);
			}
		} else if (!HideLooseEntries(folder, config)) {
			AddNode(outNodes, config, folder, (ui32)-1, depth, (si16)x2, (si16)y2, (si16)w2, (si16)h2, TREEMAP_FLAG_NONE);
		}
	}
}

static void ComputeLayoutInternal(
	int x, int y, int w, int h,
	CFolder *folder,
	int depth,
	const TreemapConfig &config,
	int hmin, int vmin,
	std::vector<TreemapNode> &outNodes,
	bool skipVolumeSpecials)
{
	if (folder == nullptr || folder->cur == 0 || w <= 0 || h <= 0) return;

	std::vector<int> indices;
	indices.reserve(folder->cur);
	for (unsigned int i = 0; i < folder->cur; i++) {
		// A volume layout places free and unavailable space itself.
		if (skipVolumeSpecials && folder->names[i] != nullptr
			&& (folder->names[i][0] == L'<' || folder->names[i][0] == L'|')) continue;
		if (config.hideSystem && folder == config.volumeRoot && SM_IsSystemRootEntry(folder->names[i])) continue;
		indices.push_back((int)i);
	}
	if (indices.empty()) return;

	// The greedy split in SizeFolders assumes sizes in descending order.
	// Finalized trees are already sorted, but live scan trees are in
	// insertion order; stable_sort keeps finalized layouts unchanged.
	std::stable_sort(indices.begin(), indices.end(), [folder](int a, int b) {
		return folder->sizes[a] > folder->sizes[b];
	});

	std::vector<int> scratch(indices.size());
	SizeFolders(x, y, w, h, folder, indices.data(), scratch.data(), (int)indices.size(), depth, config, hmin, vmin, outNodes);
}

void TreemapEngine::ComputeLayout(
	int x, int y, int w, int h,
	CFolder *root,
	int startDepth,
	const TreemapConfig &config,
	std::vector<TreemapNode> &outNodes)
{
	outNodes.clear();
	if (root == nullptr || root->cur == 0 || w <= 0 || h <= 0) return;

	int hmin = 0, vmin = 0;
	GetMinDimensions(config.density, config.dpi, hmin, vmin);

	ComputeLayoutInternal(x, y, w, h, root, startDepth, config, hmin, vmin, outNodes);
}

// Splits a rectangle along its longer side; the first part gets num/den of it.
static void SplitRect(int x, int y, int w, int h, ui64 num, ui64 den,
	int &x1, int &y1, int &w1, int &h1, int &x2, int &y2, int &w2, int &h2)
{
	if (den == 0) num = 0, den = 1;
	if (num > den) num = den;
	if (w > h) {
		int split = (int)((double)w * (double)num / (double)den);
		x1 = x; y1 = y; w1 = split; h1 = h;
		x2 = x + split; y2 = y; w2 = w - split; h2 = h;
	} else {
		int split = (int)((double)h * (double)num / (double)den);
		x1 = x; y1 = y; w1 = w; h1 = split;
		x2 = x; y2 = y + split; w2 = w; h2 = h - split;
	}
}

static void AddSpecialNode(std::vector<TreemapNode> &nodes, const wchar_t *name,
	ui32 flags, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0) return;
	TreemapNode node;
	node.name = name;
	node.depth = -1;
	node.flags = TREEMAP_FLAG_SPECIAL | flags;
	node.x = (si16)x; node.y = (si16)y; node.w = (si16)w; node.h = (si16)h;
	nodes.push_back(node);
}

// Volume layout shared by the live scan and the finished map, so the map
// keeps its shape when the scan ends: free space split off first (right or
// bottom), then the used area split into scanned content and a remainder
// block (still pending while scanning, unreadable space once done).
static void ComputeVolumeLayoutImpl(
	int x, int y, int w, int h,
	CFolder *root,
	ui64 scanned, ui64 usedBytes, ui64 freeBytes,
	const wchar_t *freeName, ui32 freeIndex,
	const wchar_t *remainderName, ui32 remainderFlags, ui32 remainderIndex,
	const TreemapConfig &config,
	std::vector<TreemapNode> &outNodes)
{
	outNodes.clear();
	if (w <= 0 || h <= 0) return;

	// Files written during the scan can push the scanned total past the
	// volume-used figure taken at the start; the used area then just fills.
	if (scanned > usedBytes) usedBytes = scanned;

	int ux = x, uy = y, uw = w, uh = h;
	if (config.showFreeSpace && freeBytes > 0) {
		int fx, fy, fw, fh;
		SplitRect(x, y, w, h, usedBytes, usedBytes + freeBytes, ux, uy, uw, uh, fx, fy, fw, fh);
		AddSpecialNode(outNodes, freeName, TREEMAP_FLAG_NONE, fx, fy, fw, fh);
		if (!outNodes.empty() && freeIndex != (ui32)-1) {
			outNodes.back().source = root;
			outNodes.back().index = freeIndex;
		}
	}
	if (uw <= 0 || uh <= 0 || usedBytes == 0) return;

	int sx, sy, sw, sh, px, py, pw, ph;
	SplitRect(ux, uy, uw, uh, scanned, usedBytes, sx, sy, sw, sh, px, py, pw, ph);
	size_t before = outNodes.size();
	AddSpecialNode(outNodes, remainderName, remainderFlags, px, py, pw, ph);
	if (outNodes.size() > before && remainderIndex != (ui32)-1) {
		outNodes.back().source = root;
		outNodes.back().index = remainderIndex;
	}

	if (root == nullptr || root->cur == 0 || sw <= 0 || sh <= 0) return;
	int hmin = 0, vmin = 0;
	TreemapEngine::GetMinDimensions(config.density, config.dpi, hmin, vmin);
	ComputeLayoutInternal(sx, sy, sw, sh, root, 0, config, hmin, vmin, outNodes, true);
}

void TreemapEngine::ComputeLiveLayout(
	int x, int y, int w, int h,
	CFolder *root,
	ui64 usedBytes,
	ui64 freeBytes,
	const TreemapConfig &config,
	std::vector<TreemapNode> &outNodes)
{
	static const wchar_t freeName[] = L"<Free Space>";
	static const wchar_t pendingName[] = L">Pending";

	ui64 scanned = 0, hidden = 0;
	if (root != nullptr) {
		for (unsigned int i = 0; i < root->cur; ++i) {
			if (config.hideSystem && root == config.volumeRoot && SM_IsSystemRootEntry(root->names[i]))
				hidden += root->sizes[i];
			else
				scanned += root->sizes[i];
		}
	}
	// Hidden system content leaves the used area instead of showing as pending.
	usedBytes = usedBytes > hidden ? usedBytes - hidden : 0;
	ComputeVolumeLayoutImpl(x, y, w, h, root, scanned, usedBytes, freeBytes,
		freeName, (ui32)-1, pendingName, TREEMAP_FLAG_PENDING, (ui32)-1, config, outNodes);
}

void TreemapEngine::ComputeVolumeLayout(
	int x, int y, int w, int h,
	CFolder *root,
	const TreemapConfig &config,
	std::vector<TreemapNode> &outNodes)
{
	outNodes.clear();
	if (root == nullptr) return;

	ui64 scanned = 0, freeBytes = 0, unavailable = 0;
	ui32 freeIndex = (ui32)-1, unavailableIndex = (ui32)-1;
	for (unsigned int i = 0; i < root->cur; ++i) {
		const wchar_t *name = root->names[i];
		if (name != nullptr && name[0] == L'<') { freeBytes += root->sizes[i]; freeIndex = i; }
		else if (name != nullptr && name[0] == L'|') {
			// Unreadable space is system-owned too and goes with Hide System.
			if (config.hideSystem) continue;
			unavailable += root->sizes[i];
			unavailableIndex = i;
		}
		else if (config.hideSystem && root == config.volumeRoot && SM_IsSystemRootEntry(name)) continue;
		else scanned += root->sizes[i];
	}
	ComputeVolumeLayoutImpl(x, y, w, h, root, scanned, scanned + unavailable, freeBytes,
		freeIndex != (ui32)-1 ? root->names[freeIndex] : L"<Free Space>", freeIndex,
		unavailableIndex != (ui32)-1 ? root->names[unavailableIndex] : nullptr,
		TREEMAP_FLAG_UNAVAILABLE, unavailableIndex, config, outNodes);
}
const TreemapNode* TreemapEngine::HitTestItem(
	const std::vector<TreemapNode> &nodes,
	int px, int py,
	int titleHeight)
{
	const TreemapNode *best = nullptr;

	for (const auto &cur : nodes) {
		if (px > cur.x && py > cur.y && px < cur.x + cur.w && py < cur.y + cur.h) {
			if (cur.IsFolder()) {
				if (px < cur.x + TREEMAP_FOLDER_BORDER || py < cur.y + titleHeight
					|| px > cur.x + cur.w - TREEMAP_FOLDER_BORDER || py > cur.y + cur.h - TREEMAP_FOLDER_BORDER) {
					best = &cur;
					break;
				}
			} else {
				best = &cur;
				break;
			}
		}
	}

	if (best != nullptr && (best->name == nullptr || best->IsSpecial())) {
		best = nullptr;
	}

	return best;
}

const TreemapNode* TreemapEngine::HitTestContainer(
	const std::vector<TreemapNode> &nodes,
	int px, int py)
{
	const TreemapNode *best = nullptr;
	int min_left = -32768, min_top = -32768, min_right = 32767, min_bottom = 32767;

	for (const auto &cur : nodes) {
		if (cur.IsFolder()
			&& px > cur.x && py > cur.y
			&& px < cur.x + cur.w && py < cur.y + cur.h
			&& cur.x >= min_left && cur.y >= min_top
			&& cur.x + cur.w <= min_right && cur.y + cur.h <= min_bottom) {
			min_left = cur.x;
			min_top = cur.y;
			min_right = cur.x + cur.w;
			min_bottom = cur.y + cur.h;
			best = &cur;
		}
	}

	return best;
}
