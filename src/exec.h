/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
#ifndef EXEC_H
#define EXEC_H

struct exec_subst_rec {
	char ident;
	char *subst;
};

/*
 * Runs a file with arguments specified in cmd_spec, substituting arguments
 * from the arg_subst array, and optionally opening and returning a read pipe
 * attached to the standard output of the executed file.
 *
 * If not NULL, arg_subst array must contain a NULL terminated list of
 * substitutions for any arguments in cmd_spec prefixed with unescaped % sign.
 *
 * If pipe_ret is not NULL, it will receive the executed file's stdout handle.
 * Returns zero on success, errno on failure.
 */
int exec_file(const char *cmd_spec,
	const struct exec_subst_rec *arg_subst, FILE **pipe_ret);

#endif /* EXEC_H */
