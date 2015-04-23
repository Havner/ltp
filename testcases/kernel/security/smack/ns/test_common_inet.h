/*
 * Copyright (C) 2014 Samsung Electronics Co.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * Smack namespaces - common routines for network tests
 *
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#ifndef SMACK_NAMESPACE_TEST_COMMON_INET_H
#define SMACK_NAMESPACE_TEST_COMMON_INET_H

#include <arpa/inet.h>

#include "test_common.h"

/* packet receive timeout in microseconds */
#define TIMEOUT 50000
#define MAX_MSG_SIZE 128

/* Sets sockets receive/send timeouts */
static inline void set_socket_options(int sfd)
{
	int ret;
	struct timeval tv;
	int on = 1;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT;

	ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));
	ret = setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));

	ret = setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));
	ret = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));
}

static inline int create_client_socket(void)
{
	int sfd;

	/* Create client socket */
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_options(sfd);
	return sfd;
}

static inline int create_server_socket(struct sockaddr_in *svaddr)
{
	int svsock, ret;

	svsock = socket(AF_INET, SOCK_STREAM, 0);
	TEST_CHECK(svsock != -1, "socket(): %s", strerror(errno));
	set_socket_options(svsock);

	ret = bind(svsock, (struct sockaddr *)svaddr,
		   sizeof(struct sockaddr_in));
	TEST_CHECK(ret != -1, "bind(): %s", strerror(errno));

	ret = listen(svsock, 1);
	TEST_CHECK(ret != -1, "listen(): %s", strerror(errno));

	return svsock;
}

/*
 * sfd - socket fd
 * msg - message to send (NULL termination is also sent)
 * Returns 0 on success
 */
static inline int tcp_send(int sfd, const char *msg)
{
	ssize_t numBytes;
	size_t msgLen = strlen(msg) + 1;

	errno = 0;
	numBytes = write(sfd, msg, msgLen);
	if (numBytes >= 0)
		TEST_CHECK(numBytes == (ssize_t)msgLen, "write(): %s, "
			   "numBytes = %zd", strerror(errno), numBytes);
	return numBytes;
}

/*
 * sfd - socket fd
 * exp_msg - expected received message
 * Returns 0 on success
 */
static inline int tcp_receive(int sfd, const char *exp_msg)
{
	ssize_t numBytes;
	size_t msgLen = strlen(exp_msg) + 1;
	char buf[MAX_MSG_SIZE];

	errno = 0;
	numBytes = read(sfd, buf, msgLen);
	if (numBytes >= 0)
		TEST_CHECK(strcmp(buf, exp_msg) == 0,
			   "read: %s, '%s' (%zd bytes), should be '%s'",
			   strerror(errno), buf, numBytes, exp_msg);
	return numBytes;
}

#endif // SMACK_NAMESPACE_TEST_COMMON_INET_H
