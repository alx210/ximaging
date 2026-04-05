/*
 * Copyright (C) 2012-2026 alx@fastestcode.org
 * This software is distributed under the terms of the MIT license.
 * See the included LICENSE file for further information.
 */

#ifndef IOUTIL_H
#define IOUTIL_H

ssize_t readn(int fd, void *pbuf, size_t len);
ssize_t writen(int fd, const void *pbuf, size_t len);

#endif /* IOUTIL_H */
