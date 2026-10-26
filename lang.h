
#ifndef LANG_H
#define LANG_H

struct Lang {
	int toolbar_bitmap_id;
	const wchar_t *about_spacemonger;
	const wchar_t *freeware;
	const wchar_t *warranty;
	const wchar_t *description;
	const wchar_t *maintainer;
	const wchar_t *project_website;
	const wchar_t *report_bug;
	const wchar_t *license_button;
	const wchar_t *open_url_failed_format;
	const wchar_t *ok, *cancel;
	const wchar_t *bytes, *kb, *mb, *gb;
	const wchar_t *freespace_format;
	const wchar_t *zoomin, *zoomout, *zoomfull, *run, *del;
	const wchar_t *opendrive, *rescandrive, *showfreespace;
	const wchar_t *percent_format, *size_format;
	const wchar_t *total, *free, *files_total, *folders_total, *files_found, *folders_found;
	char digitpt, digitcomma;
	const wchar_t **densitynames;
	const wchar_t **colornames;
	const wchar_t **monthnames;
	const wchar_t **attribnames;
	const wchar_t *deleting;
	const wchar_t *selectdrive;
	const wchar_t *scanning;
	const wchar_t *settings;
	// Settings dialog (densitynames: 4 detail levels, colornames: 4 schemes).
	const wchar_t *set_map, *set_detail, *set_colors;
	const wchar_t *files, *folders;	// also the scan status strip's counts
	const wchar_t *set_behavior, *set_hover_details, *set_highlight;
	const wchar_t *disabledelete;
	const wchar_t *set_appearance, *set_language, *set_language_auto, *set_theme;
	const wchar_t **themenames;	// automatic, light, dark
	const wchar_t *properties;
	const wchar_t *scan_failed_format, *scan_partial_format, *scan_partial_notice_format;
	const wchar_t *scan_partial_button_format, *scan_details_title;
	const wchar_t *scan_details_summary_format, *scan_details_copy, *scan_unavailable;
	// Info tip for the unavailable block: what the space is and what helps.
	const wchar_t *scan_unavailable_tip;
	// Info tip lines: size with share of the drive, folder contents, date,
	// and the note shown for Windows-managed (system) content.
	const wchar_t *tip_drive_share_format;
	const wchar_t *tip_contents_format;
	const wchar_t *tip_modified_format;
	const wchar_t *tip_system_note;
	// Clean-up dialog.
	const wchar_t *cleanup_title, *cleanup_summary_format, *cleanup_large_header;
	const wchar_t *cleanup_col_item, *cleanup_col_size, *cleanup_col_location, *cleanup_col_hint;
	const wchar_t *cleanup_measuring, *cleanup_delete, *cleanup_open, *cleanup_close;
	const wchar_t *cleanup_details, *cleanup_done_format, *cleanup_nothing, *cleanup_no_scan;
	const wchar_t *cleanup_recycle, *cleanup_recycled_format;
	// Clean-up progress: "%d of %d measured", "Cleaning %s (%d of %d)",
	// "%s of %s", the stop button, "Stopped - %s freed.", row states.
	const wchar_t *cleanup_measuring_format, *cleanup_running_format, *cleanup_progress_format;
	const wchar_t *cleanup_stop, *cleanup_stopped_format;
	const wchar_t *cleanup_row_cleaning, *cleanup_row_done;
	// Scan cache: "Showing the scan from %s while it is updated.", and the
	// short phase after the last folder was read.
	const wchar_t *scan_cached_format, *scan_finalizing;
	const wchar_t *copy_info;			// context menu: path, size and details to the clipboard
	const wchar_t *set_scan_cache;		// Settings: keep the last scan and show it while rescanning
	// Open a folder, export, search, elevation.
	const wchar_t *open_folder;			// drive dialog button
	const wchar_t *export_list;			// context menu
	const wchar_t *export_done_format;	// %s = file
	const wchar_t *search_cue;			// grey hint in the search box
	const wchar_t *search_result_format;	// %s matches
	const wchar_t *restart_admin_failed;
	const wchar_t **filetype_labels;	// SM_FileType order (legend)
	const wchar_t **cleanup_kind_labels;	// SM_CleanupKind order
	const wchar_t **cleanup_hint_labels;	// SM_LargeFileKind order
	// Toolbar labels in button order: open, rescan, zoom full, zoom in,
	// zoom out, free space, system, delete, clean up, report, run as administrator,
	// settings, about.
	const wchar_t **toolbarlabels;
};

struct LangEntry {
	const char *abbrev;
	const wchar_t *name;
	Lang *text;
};

extern Lang *CurLang;		// Current language

extern Lang LangUs;			// English (US)
extern Lang LangUk;			// English (UK/Aust.)
extern Lang LangFr;			// Français (Nationale)
extern Lang LangDe;			// Deutsch

extern LangEntry Langs[];	// List of languages

// The language for a settings code: a known two-letter code, or "au" to
// follow the Windows display language.
Lang *SM_ResolveLanguage(const char *code);

#endif
