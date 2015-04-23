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
 * Smack namespace - test case "UDP"
 *
 * IPv4 UDP packet transfer with Smack namespace.
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
#include <arpa/inet.h>
#include "test_common.h"
#include "test_common_inet.h"

#define NS_PATH_SIZE 64
#define PORT_NUM 15372

static const char* SERVER_ADDRESS = "localhost";

#define INSIDE   INSIDE_PROC_LABEL
#define OUTSIDE  OUTSIDE_PROC_LABEL
#define LABEL1   "label1"
#define LABEL2   "label2"
#define UNMAPPED "unmapped"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE,   LABEL1,   "w", automatic},
	{LABEL1,   INSIDE,   "w", automatic},
	{INSIDE,   UNMAPPED, "w", automatic},
	{UNMAPPED, INSIDE,   "w", automatic},
	{OUTSIDE,  LABEL1,   "w", automatic},
	{LABEL1,   OUTSIDE,  "w", automatic},
	{OUTSIDE,  UNMAPPED, "w", automatic},
	{UNMAPPED, OUTSIDE,  "w", automatic},
	{INSIDE,   OUTSIDE,  "w", automatic},
	{OUTSIDE,  INSIDE,   "w", automatic},
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{NULL}
};

/*
 * sfd  - socket file descriptor
 * id   - message identificator
 * addr - destination address
 */
static int helper_send(int sfd, int id, struct sockaddr_in *addr)
{
	size_t msgLen;
	ssize_t numBytes = 0;
	char buf[MAX_MSG_SIZE];
	snprintf(buf, MAX_MSG_SIZE, "message%d", id);

	test_sync(id);
	msgLen = strlen(buf) + 1;
	numBytes = sendto(sfd, buf, msgLen, 0, (struct sockaddr *)addr,
		     sizeof(struct sockaddr_in));
	if (numBytes > 0)
		TEST_CHECK((ssize_t)msgLen == numBytes, "sent %lu bytes "
			   "(should %zu), id = %d", numBytes, msgLen, id);
	return numBytes;
}

/*
 * sfd - socket file descriptor
 * id  - message identificator
 */
static int helper_receive(int sfd, int id, struct sockaddr_in *claddr,
			  socklen_t *lenptr)
{
	int numBytes = 0;
	socklen_t len;
	char buf[MAX_MSG_SIZE], expected_buf[MAX_MSG_SIZE];
	snprintf(expected_buf, MAX_MSG_SIZE, "message%d", id);

	test_sync(id);
	len = sizeof(struct sockaddr_in);
	numBytes = recvfrom(sfd, buf, MAX_MSG_SIZE, 0,
			    (struct sockaddr *)claddr, &len);
	if (numBytes > 0)
		TEST_CHECK(strcmp(buf, expected_buf) == 0,
			   "Received '%s', should be: '%s'",
			   buf, expected_buf);

	if (lenptr)
		*lenptr = len;
	return numBytes;
}

/*
 * client
 */
void main_inside_ns(void)
{
	struct sockaddr_in svaddr;
	int sfd, ret;
	ssize_t numBytes;

	/* wait for sibling to start the server */
	test_sync(0);

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_options(sfd);

	/* Create client socket */
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_port = htons(PORT_NUM);
	ret = inet_aton(SERVER_ADDRESS, &svaddr.sin_addr);
	TEST_CHECK(ret == 0, "inet_aton(): %s", strerror(errno));

	/* Send a message to the server */
	helper_send(sfd, 1, &svaddr);


	/* Receive a message from the server */
	numBytes = helper_receive(sfd, 2, NULL, NULL);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);

	/* Try to set IPIN label to an unmapped label */
	int exp_ret1[] = { 0, -1, -1, -1,
			  -1, -1, -1, -1 };
	int exp_errno1[] = {    0, EPERM, EBADR, EBADR,
			    EPERM, EPERM, EPERM, EPERM};
	errno = 0;
	ret = smack_set_fd_label(sfd, UNMAPPED, SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));


	/*
	 * Set IPIN label to a mapped label: object of our socket will be "l1"
	 * label visible from init ns.
	 */
	int exp_ret2[] = { 0, -1,  0,  0,
			  -1, -1, -1, -1 };
	int exp_errno2[] = {    0, EPERM,     0,     0,
			    EPERM, EPERM, EPERM, EPERM };
	errno = 0;
	ret = smack_set_fd_label(sfd, LA(LABEL1), SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));
	/* Receive a message from the server */
	numBytes = helper_receive(sfd, 3, NULL, NULL);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	/*
	 * Set IPIN label to a mapped label: object of our socket will be "l2"
	 * label visible from init ns.
	 */
	errno = 0;
	ret = smack_set_fd_label(sfd, LA(LABEL2), SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	// TODO: why the ret codes look the way they do?
	/* Receive a message from the server */
	int exp_errno3[] = {EAGAIN, EPERM, EAGAIN, EAGAIN,
			    EPERM,  EPERM, EPERM,  EPERM };
	numBytes = helper_receive(sfd, 4, NULL, NULL);
	TEST_CHECK(errno == exp_errno3[env_id],
	           "recvfrom(): %s, numBytes = %zd", strerror(errno), numBytes);



	/* Try to set IPOUT label to an unmapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, UNMAPPED, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* Set IPOUT label to a mapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, LA(LABEL1), SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* this packet will reach destination, because we have rule */
	helper_send(sfd, 5, &svaddr);

	/* Set IPOUT label to a mapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, LA(LABEL2), SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* this packet won't reach destination, because we don't have a rule */
	helper_send(sfd, 6, &svaddr);


	/* now incomming packet will have "unmapped" label. */

	ret = smack_set_fd_label(sfd, LA(INSIDE), SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	// TODO: why the ret codes look the way they do?
	/* Receive a message from the server */
	int exp_errno4[] = {    0, EPERM, EAGAIN, EAGAIN,
			    EPERM, EPERM, EAGAIN, EAGAIN};
	numBytes = helper_receive(sfd, 7, NULL, NULL);
	TEST_CHECK(errno == exp_errno4[env_id],
		   "recvfrom(): %s, numBytes = %zd", strerror(errno), numBytes);

	test_sync(-1);
	close(sfd);
}

/*
 * server
 */
void main_outside_ns(void)
{
	struct sockaddr_in svaddr; /* server address */
	struct sockaddr_in claddr; /* client address */
	int sfd, ret;
	ssize_t numBytes;
	socklen_t len;
	char* label = NULL;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	/* create a socket */
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_options(sfd);

	/* bind address to the socket */
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	svaddr.sin_port = htons(PORT_NUM);
	ret = bind(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK(ret != -1, "bind(): %s", strerror(errno));

	/* trigger client to send data */
	test_sync(0);

	/*
	 * Receive a message form the client.
	 */
	numBytes = helper_receive(sfd, 1, &claddr, &len);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	helper_send(sfd, 2, &claddr);
	helper_send(sfd, 3, &claddr);
	helper_send(sfd, 4, &claddr);


	/*
	 * IPOUT label of a socket is being changed in the client now...
	 */

	/* Receive a message form the client */
	numBytes = helper_receive(sfd, 5, NULL, NULL);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	// TODO: why the ret codes are the way they are?
	/* Receive a message form the client */
	int exp_errno1[] = {EAGAIN, 0, EAGAIN, EAGAIN,
			    0,      0, 0,      0      };
	numBytes = helper_receive(sfd, 6, NULL, NULL);
	TEST_CHECK(errno == exp_errno1[env_id], "recvfrom(): %s, "
	           "numBytes = %zd", strerror(errno), numBytes);


	/*
	 * Change label of outgoing to an unmapped.
	 */
	ret = smack_set_fd_label(sfd, UNMAPPED, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_get_fd_label(sfd, &label, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_get_fd_label(): %s", strerror(errno));
	if (ret == 0)
		TEST_CHECK(strcmp(label, UNMAPPED) == 0, "label = %s, "
			   "should be %s", label, UNMAPPED);

	/* Send a message to the client */
	helper_send(sfd, 7, &claddr);


	test_sync(-1);
	close(sfd);
}

void test_cleanup(void)
{
}
