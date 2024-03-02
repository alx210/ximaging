/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
 
/*
 * File management prototypes.
 */

#ifndef FILEMGMT_H
#define FILEMGMT_H

enum file_proc_id{
	FPROC_COPY,
	FPROC_MOVE,
	FPROC_DELETE,
	FPROC_FINISHED
};

/*
 * Update notification callback. Invoked every time a file is processed.
 * 'file_name' and 'status' (errno) are valid for all but FPROC_FINISHED,
 * which indicates that all files were processed.
 */
typedef void (*fm_notify_cbt)(enum file_proc_id proc_id,
	char *file_name, int status, void *client_data);

/*
 * Copy 'count' of files specified in 'src' to 'dest' directory.
 */
int copy_files(char  **src, const char *dest, int count,
	fm_notify_cbt notify_cb, void *cb_data);

/*
 * Move 'count' of files specified in 'src' to 'dest' directory.
 */
int move_files(char **src, const char *dest, int count,
	fm_notify_cbt notify_cb, void *cb_data);

/*
 * Delete 'count' of files specified in 'names'.
 */
int delete_files(char **names, int count,
	fm_notify_cbt notify_cb, void *cb_data);

#endif /* FILEMGMT_H */
