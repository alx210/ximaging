/*
 * Copyright (C) 2012-2025 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef FILTER_H
#define FILTER_H
 
struct filter_rec {
	char *name;
	char *command;
	char *suffixes;
	char *description; /* Optional */
	int xrm_name; /* XrmQuark actually (used internally) */
};

unsigned int fetch_filters(struct filter_rec** const p);

#endif /* FILTER_H */
