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
 * Smack namespace  test case "uds" - Unix Domain Sockets
 *
 * Triggers following LSM hooks:
 *  * unix_stream_connect
 *
 * This test case check access to communication via UDS with cooperation
 * with Smack namespaces.
 *
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "test_common.h"
#include "test_common_inet.h"

#define TEST_MESSAGE "blah"
#define BACKLOG 5
#define SOCKET_PATH "tmp/test_socket"
#define BUF_SIZE 100

#define LABEL   "label"
#define INSIDE  INSIDE_PROC_LABEL
#define OUTSIDE OUTSIDE_PROC_LABEL

/*
 * Add rule to allow namespaced process access socket file
 * and connect client.
 */
static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE,  LABEL,  "rw", automatic},
	{INSIDE,  OUTSIDE, "w", automatic},
	{OUTSIDE, INSIDE,  "w", automatic},
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{NULL}
};

void main_inside_ns(void)
{
	int ret;
	struct sockaddr_un addr;
	int sfd;
	size_t num_bytes, bytes_written;
	const char *test_msg = TEST_MESSAGE;

	test_sync(0);

	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	TEST_CHECK(sfd != -1, strerror(errno));
	set_socket_options(sfd);

	/* Connect to the server */
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
	ret = connect(sfd, (struct sockaddr *)&addr,
		      sizeof(struct sockaddr_un));
	TEST_CHECK(ret != -1, strerror(errno));
	if (ret == -1)
		return;

	num_bytes = strlen(test_msg) + 1;
	bytes_written = write(sfd, test_msg, num_bytes);
	TEST_CHECK(num_bytes == bytes_written, "Wrote %zu bytes: %s", num_bytes,
		   strerror(errno));

	test_sync(1);

	// TODO: negative tests
}

void main_outside_ns(void)
{
	int ret;
	struct sockaddr_un addr;
	int sfd, cfd;
	ssize_t num_read;
	char buf[BUF_SIZE];

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	sfd = socket(AF_UNIX, SOCK_STREAM, 0);
	TEST_CHECK(sfd != -1, strerror(errno));
	set_socket_options(sfd);

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
	ret = bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un));
	TEST_CHECK(ret != -1, strerror(errno));

	ret = listen(sfd, BACKLOG);
	TEST_CHECK(ret != -1, strerror(errno));

	/* set socket file label */
	ret = smack_set_file_label(SOCKET_PATH, LABEL, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret != -1, strerror(errno));

	test_sync(0);

	/* accept a client */
	cfd = accept(sfd, NULL, NULL);
	TEST_CHECK(cfd != -1, strerror(errno));

	num_read = read(cfd, buf, BUF_SIZE);
	if (num_read != -1)
		TEST_CHECK(strcmp(buf, TEST_MESSAGE) == 0, "received = %s",
			   buf);

	test_sync(1);

	TEST_CHECK(num_read != -1, strerror(errno));
	close(cfd);
}

void test_cleanup(void)
{
	remove(SOCKET_PATH);
}
