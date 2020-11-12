/*
 * Copyright (C) 2012-2017 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef BROWSER_H
#define BROWSER_H

#ifdef ENABLE_CDE
#include <Tt/tt_c.h>
#else
#ifndef TT_MESSAGE_DEFINED
#define TT_MESSAGE_DEFINED
typedef void *Tt_message;
#endif
#endif /* ENABLE_CDE */

/*
 * Retrieve the first unpinned browser, create a new instance if none available.
 * 'req_msg' is optional and associates the instance with a ToolTalk request.
 * Returns its shell widget handle, or None on failure.
 */
Widget get_browser(const struct app_resources*, Tt_message req_msg);


/*
 * Browse 'path' for raster images. Returns True on success. 
 * 'req_msg' is optional and associates the instance with a ToolTalk request.
 */
Boolean browse_path(Widget, const char *path, Tt_message req_msg);

#ifdef ENABLE_CDE
/*
 * Handle the ttmedia Quit message.
 * Destroys the instance that 'req_msg' is targeted at and returns True.
 * Returns false if no matching instance was found.
 */
Boolean handle_browser_ttquit(Tt_message req_msg);
#endif /* ENABLE_CDE */

#endif /* BROWSER_H */
