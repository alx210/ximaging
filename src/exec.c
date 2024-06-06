/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include "exec.h"
#include "debug.h"

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
	const struct exec_subst_rec *arg_subst, FILE **pipe_ret)
{
	pid_t pid;
	char *str;
	char *p, *t;
	char pc = 0;
	int done = 0;
	char **argv = NULL;
	size_t argv_size = 0;
	unsigned int argc = 0;
	volatile int errval = 0;
	int pipe_fd[2];
	
	str = strdup(cmd_spec);

	p = str;
	t = NULL;
	
	/* split the command string into separate arguments */
	while(!done){
		if(!t){
			while(*p && isblank(*p)) p++;
			if(*p == '\0') break;
			t = p;
		}
		
		/* handle special characters */
		if(*p == '\"' || *p == '\'' || *p == '%'){
			if(pc == '\\'){
				/* literal " or ', remove escape character */
				memmove(p - 1, p, strlen(p) + 1);
			} else if(*p == '%' && p[1] && arg_subst) {
				/* substitution */
				const struct exec_subst_rec *r = arg_subst;
				char *match = NULL;
				
				p++;
				
				while(r->ident && r->subst) {
					if(*p == r->ident) {
						match = r->subst;
						break;
					}
					r++;
				}
				
				/* next char must be blank, but in case it's not,
				 * we assume it is a mistake and fix it */
				if(isblank(p[1])) p++; else *p = ' ';
				
				/* replace token; skip parameter it no match */
				if(!(t = match)) continue;
			} else {
				/* quotation marks; remove them, ignoring blanks within */
				memmove(p, p + 1, strlen(p));
				while(*p != '\"' && *p != '\''){
					if(*p == '\0'){
						if(argv) free(argv);
						return EINVAL;
					}
					p++;
				}
				memmove(p, p + 1, strlen(p));
			}
		}
		if(isblank(*p) || *p == '\0'){
			if(*p == '\0') done = 1;
			if(argv_size < (argc + 1)){
				char **new_ptr;
				new_ptr = realloc(argv, (argv_size += 64) * sizeof(char*));
				if(!new_ptr){
					free(str);
					if(argv) free(argv);
					return ENOMEM;
				}
				argv = new_ptr;
			}
			*p = '\0';
			
			argv[argc] = t;
			
			dbg_trace("argv[%d]: %s\n", argc, argv[argc]);
			
			t = NULL;
			argc++;
		}
		pc = *p;
		p++;
	}
	
	if(!argc) return EINVAL;
	argv[argc] = NULL;

	if(pipe_ret && pipe(pipe_fd)) {
		free(str);
		free(argv);
		return errno;
	}
	
	pid = vfork();
	if(pid == 0){
		if(pipe_ret) {
			close(pipe_fd[0]);
			dup2(pipe_fd[1], fileno(stdout));
		} else {
			setsid();
		}
		
		if(execvp(argv[0], argv) == (-1))
			errval = errno;
		_exit(0);

	} else if(pid == -1) {
		errval = errno;
	}

	if(!errval && pipe_ret) {
		close(pipe_fd[1]);
		*pipe_ret = fdopen(pipe_fd[0], "r");
		if(*pipe_ret == NULL) errval = errno;
	}
	
	free(str);
	free(argv);
	return errval;
}
