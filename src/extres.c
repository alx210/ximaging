/*
 * Copyright (C) 2012-2024 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#include <stdlib.h>
#include <stdio.h>
#include <Xm/Xm.h>
#include "common.h"
#include "const.h"
#include "extres.h"
#include "debug.h"

/* Forward declarations */
static Bool xrm_enum_cb(XrmDatabase *rdb, XrmBindingList bindings,
	XrmQuarkList quarks, XrmRepresentation *type,
	XrmValue *value, XPointer closure);
static void fetch_attributes(XrmDatabase rdb, struct filter_rec*);

#define FILTER_GROWBY 10
#define FILTER_RES_NAME BASE_NAME ".filter"
#define FILTER_RES_CLASS APP_CLASS ".Filter"

static struct filter_rec *filters = NULL;
static unsigned int nfilters = 0;

static XrmQuark name_list[3] = {NULLQUARK};
static XrmQuark class_list[3] = {NULLQUARK};
static XrmQuark qsuffixes_name;
static XrmQuark qsuffixes_class;
static XrmQuark qdesc_name;
static XrmQuark qdesc_class;

static Bool xrm_enum_cb(XrmDatabase *rdb, XrmBindingList bindings,
	XrmQuarkList quarks, XrmRepresentation *type,
	XrmValue *value, XPointer closure)
{
	static size_t nalloc = 0;
	
	if( ((quarks[0] == name_list[0]) || (quarks[0] == class_list[0])) &&
		((quarks[1] == name_list[1]) || (quarks[1] == class_list[1])) ) {

		if(nfilters == nalloc) {
			struct filter_rec *p;

			nalloc += FILTER_GROWBY;
			p = realloc(filters, nalloc * sizeof(struct filter_rec));
			if(!p) return True;
			filters = p;
		}
		
		memset(&filters[nfilters], 0, sizeof(struct filter_rec));
		filters[nfilters].name = XrmQuarkToString(quarks[2]);
		filters[nfilters].command = strdup(value->addr);
		filters[nfilters].xrm_name = quarks[2];
		
		nfilters++;
	}
	return False;
}

static void fetch_attributes(XrmDatabase rdb, struct filter_rec *rec)
{
	XrmRepresentation suffixes_rep, desc_rep;
	XrmValue suffixes_value, desc_value;
	
	XrmQuark suffixes_name_list[] = {
		name_list[0], name_list[1], rec->xrm_name,
		qsuffixes_name, NULLQUARK
	};
	XrmQuark suffixes_class_list[] = {
		class_list[0], class_list[1], rec->xrm_name,
		qsuffixes_class, NULLQUARK
	};
	XrmQuark desc_name_list[] = {
		name_list[0], name_list[1], rec->xrm_name,
		qdesc_name, NULLQUARK
	};
	XrmQuark desc_class_list[] = {
		class_list[0], class_list[1], rec->xrm_name,
		qdesc_class, NULLQUARK
	};
	
	if(XrmQGetResource(rdb, suffixes_name_list, suffixes_class_list,
		&suffixes_rep, &suffixes_value)) {
		rec->suffixes = strdup(suffixes_value.addr);
	}

	if(XrmQGetResource(rdb, desc_name_list, desc_class_list,
		&desc_rep, &desc_value)) {
		rec->description = strdup(desc_value.addr);
	}
}

unsigned int fetch_filters(struct filter_rec** const p)
{
	if(filters == NULL) {
		unsigned int i;
		XrmDatabase rdb = XtDatabase(app_inst.display);

		XrmStringToQuarkList(FILTER_RES_NAME, name_list);
		XrmStringToQuarkList(FILTER_RES_CLASS, class_list);

		qsuffixes_name = XrmStringToQuark("suffixes");
		qsuffixes_class = XrmStringToQuark("Suffixes");
		qdesc_name = XrmStringToQuark("description");
		qdesc_class = XrmStringToQuark("Description");

		XrmEnumerateDatabase(rdb, name_list, class_list,
			XrmEnumOneLevel, xrm_enum_cb, NULL);
		
		for(i = 0; i < nfilters; i++)
			fetch_attributes(rdb, &filters[i]);
	}
	
	if(nfilters && p) *p = filters;
	return nfilters;
}
