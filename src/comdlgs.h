/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef COMDLGS_H
#define COMDLGS_H

/* 
 * message_box types
 */
enum mb_type {
	MB_QUESTION,	/* Yes/No */
	MB_CQUESTION,	/* Yes/No/Cancel */
	MB_CONFIRM,		/* Ok/Cancel */
	MB_NOTIFY,
	MB_ERROR,
	MB_NOTIFY_NB,	/* non blocking */
	MB_ERROR_NB
};

enum mb_result {
	MBR_CONFIRM,
	MBR_DECLINE,
	MBR_CANCEL, /* Cancel in MB_CQUESTION */
	_MBR_NVALUES
};

/*
 * Displays a modal message dialog. MB_QUESTION and MB_CONFIRM return True
 * when YES or OK was clicked.
 */
enum mb_result message_box(Widget parent, enum mb_type type,
	const char *msg_title, const char *msg_str);


/* 
 * Display MB_ERROR message dialog with user specified message and errno
 * string appended to it following a colon. If session shell isn't
 * available the string is printed to stderr.
 */
void errno_message_box(Widget parent, int errno_val,
	const char *msg, Boolean blocking);

/*
 * Display a blocking directory selection dialog.
 * Returns a valid path name or NULL if selection was cancelled.
 * If a valid path name is returned it must be freed by the caller.
 */
char* dir_select_dlg(Widget parent, const char *title,
	const char *init_path);

/*
 * Display a blocking input dialog.
 * Returns a valid string or NULL if cancelled.
 */
char* rename_file_dlg(Widget parent, char *file_title);

/*
 * Display the 'About' dialog box
 */
void display_about_dlgbox(Widget parent);

#endif /* COMDLGS_H */
