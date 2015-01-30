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
 * Smack namespaces - common routines for CIPSO tests
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#ifndef SMACK_NAMESPACE_TEST_COMMON_INET_H
#define SMACK_NAMESPACE_TEST_COMMON_INET_H

#include <arpa/inet.h>

#include "test_common.h"

/* packet receive timeout in microseconds */
#define TIMEOUT 50000

/* Sets sockets receive/send timeouts */
static inline void set_socket_timeout(int sfd)
{
	int ret;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT;
	ret = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));
	ret = setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	TEST_CHECK(ret != -1, "setsockopt(): %s", strerror(errno));
}

static inline int create_client_socket(void)
{
	int sfd;

	/* Create client socket */
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_timeout(sfd);
	return sfd;
}

static inline int create_server_socket(struct sockaddr_in *svaddr)
{
	int svsock, ret;

	svsock = socket(AF_INET, SOCK_STREAM, 0);
	TEST_CHECK(svsock != -1, "socket(): %s", strerror(errno));
	set_socket_timeout(svsock);

	ret = bind(svsock, (struct sockaddr *)svaddr,
		   sizeof(struct sockaddr_in));
	TEST_CHECK(ret != -1, "bind(): %s", strerror(errno));

	ret = listen(svsock, 5);
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
	TEST_CHECK(numBytes == (ssize_t)msgLen, "write(): %s, numBytes = %zd",
		   strerror(errno), numBytes);
	return numBytes == (ssize_t)msgLen ? 0 : -1;
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
	char *buf = malloc(msgLen);
	TEST_CHECK(buf != 0, "malloc(): %s", strerror(errno));

	errno = 0;
	numBytes = read(sfd, buf, sizeof(exp_msg) + 1);
	if (numBytes != -1)
		TEST_CHECK(strcmp(buf, exp_msg) == 0, "Received '%s' (%zd bytes),"
			   " should be '%s'", buf, numBytes, exp_msg);
	free(buf);
	return numBytes != -1 ? 0 : -1;
}

#endif // SMACK_NAMESPACE_TEST_COMMON_INET_H
