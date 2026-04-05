/*
 * Copyright (C) 2012-2026 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#include <unistd.h>

/*
 * Same as read(2), except it will resume reading if interrupted
 * until all requested data is read, or an error occurs.
 */
ssize_t readn(int fd, void *pbuf, size_t len)
{
	ssize_t nread = 0;
	
	while((size_t)nread < len) {
		ssize_t n = read(fd, pbuf + nread, len - nread);
		if(n <= 0) {
			return nread ? nread : n;
		} else {
			nread += n;
		}
	}
	return nread;
}

/*
 * Same as write(2), except it will resume writing if interrupted
 * until all data is written, or an error occurs.
 */
ssize_t writen(int fd, const void *pbuf, size_t len)
{
	ssize_t nwrote = 0;
	
	while((size_t)nwrote < len) {
		ssize_t n = write(fd, pbuf + nwrote, len - nwrote);
		if(n <= 0) {
			return nwrote ? nwrote : n;
		} else {
			nwrote += n;
		}
	}
	return nwrote;
}
