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
 * Smack namespace - test case "CIPSO UDP"
 *
 * IPv4 UDP packet transfer with CIPSO and Smack namespace.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
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

#define UNMAPPED_LABEL "unmapped"
#define NS_PATH_SIZE 64
#define BUF_SIZE 32
#define PORT_NUM 15372

static const char* SERVER_ADDRESS = "localhost";

/*
 * sfd - socket file descriptor
 * id  - message identificator
 */
static int helper_receive(int sfd, int id)
{
	int numBytes = 0;
	char buf[BUF_SIZE], expected_buf[BUF_SIZE];
	snprintf(expected_buf, BUF_SIZE, "message%d", id);

	test_sync(id);
	errno = 0;
	numBytes = recvfrom(sfd, buf, BUF_SIZE, 0, NULL, NULL);
	if (numBytes > 0)
		TEST_CHECK(strcmp(buf, expected_buf) == 0,
			   "Received '%s', should be: '%s'", buf, expected_buf);

	return numBytes;
}

/*
 * sfd  - socket file descriptor
 * id   - message identificator
 * addr - destination address
 */
static int helper_send(int sfd, int id, struct sockaddr_in *addr)
{
	size_t msgLen;
	int ret = 0;
	char buf[BUF_SIZE];
	snprintf(buf, BUF_SIZE, "message%d", id);

	test_sync(id);
	msgLen = strlen(buf) + 1;
	ret = sendto(sfd, buf, msgLen, 0, (struct sockaddr *)addr,
		     sizeof(struct sockaddr_in));
	TEST_CHECK((int)msgLen == ret, "sent %d bytes (should %zu), id = %d",
		   ret, msgLen, id);
	return ret;
}

/*
 * client
 */
void main_inside_ns(void)
{
	struct sockaddr_in svaddr;
	int sfd, ret;
	ssize_t numBytes;
	int lsm_ns = env_id & TEST_ENV_SMACK_NS;

	/* wait for sibling to start the server */
	test_sync(0);

	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_timeout(sfd);

	/* Create client socket */
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_port = htons(PORT_NUM);
	ret = inet_aton(SERVER_ADDRESS, &svaddr.sin_addr);
	TEST_CHECK(ret == 0, "inet_aton(): %s", strerror(errno));

	/* Send a message to the server */
	helper_send(sfd, 1, &svaddr);


	/* Receive a message from the server */
	numBytes = helper_receive(sfd, 2);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);

	/* Try to set IPIN label to an unmapped label */
	int exp_ret1[] = { 0, -1, -1, -1,
			  -1, -1, -1, -1 };
	int exp_errno1[] = {    0, EPERM, EBADR, EBADR,
			    EPERM, EPERM, EPERM, EPERM};
	errno = 0;
	ret = smack_set_fd_label(sfd, UNMAPPED_LABEL, SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));


	/*
	 * Set IPIN label to a mapped label: object of our socket will be "l1" label
	 * visible from init ns.
	 */
	int exp_ret2[] = { 0, -1,  0,  0,
			  -1, -1, -1, -1 };
	int exp_errno2[] = {    0, EPERM,     0,     0,
			    EPERM, EPERM, EPERM, EPERM };
	errno = 0;
	ret = smack_set_fd_label(sfd, lsm_ns ? "n_l1" : "l1", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));
	/* Receive a message from the server */
	numBytes = helper_receive(sfd, 3);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	/*
	 * Set IPIN label to a mapped label: object of our socket will be "l2" label
	 * visible from init ns.
	 */
	errno = 0;
	ret = smack_set_fd_label(sfd, lsm_ns ? "n_l2" : "l2", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* Receive a message from the server */
	int exp_errno3[] = {EAGAIN, 0, EAGAIN, EAGAIN,
			    0,      0, 0,      0      };
	numBytes = helper_receive(sfd, 4);
	TEST_CHECK(errno == exp_errno3[env_id],
	           "recvfrom(): %s, numBytes = %zd", strerror(errno), numBytes);



	/* Try to set IPOUT label to an unmapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, UNMAPPED_LABEL, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* Set IPOUT label to a mapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, lsm_ns ? "n_l1" : "l1", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* this packet will reach destination, because we have rule */
	helper_send(sfd, 5, &svaddr);

	/* Set IPOUT label to a mapped label */
	errno = 0;
	ret = smack_set_fd_label(sfd, lsm_ns ? "n_l2" : "l2", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* this packet won't reach destination, because we don't have a rule */
	helper_send(sfd, 6, &svaddr);


	/* now incomming packet will have "unmapped" label. */

	ret = smack_set_fd_label(sfd, lsm_ns ? "n_inside" : "inside",
				 SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));

	/* Receive a message from the server */
	int exp_errno4[] = {0, 0, EAGAIN, EAGAIN,
			    0, 0, EAGAIN, EAGAIN};
	numBytes = helper_receive(sfd, 7);
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
	char buf[BUF_SIZE];
	char* label = NULL;

	// TODO: reduce the rules to absolute minimum
	// allow sending and receiving packets with "l1" and "unmapped" labels
	ret = smack_set_rule("inside", "l1", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "inside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", UNMAPPED_LABEL, "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule(UNMAPPED_LABEL, "inside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("outside", "l1", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "outside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("outside", UNMAPPED_LABEL, "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule(UNMAPPED_LABEL, "outside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", "outside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("outside", "inside", "w");
	TEST_CHECK(ret == 0, strerror(errno));


	/* create a socket */
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	TEST_CHECK(sfd != -1, "socket(): %s", strerror(errno));
	set_socket_timeout(sfd);

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
	 * NOTE: we should use helper_receive(), but we don't know client
	 * address yet.
	 */
	test_sync(1);
	len = sizeof(struct sockaddr_in);
	numBytes = recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *)&claddr,
			    &len);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);
	if (numBytes > 0) {
		TEST_CHECK(strcmp(buf, "message1") == 0,
			   "Received '%s', should be: '%s'", buf, "message1");
	}

	helper_send(sfd, 2, &claddr);
	helper_send(sfd, 3, &claddr);
	helper_send(sfd, 4, &claddr);


	/*
	 * IPOUT label of a socket is being changed in the client now...
	 */

	/* Receive a message form the client */
	numBytes = helper_receive(sfd, 5);
	TEST_CHECK(numBytes > 0, "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	/* Receive a message form the client */
	int exp_errno1[] = {EAGAIN, 0, EAGAIN, EAGAIN,
			    0,      0, 0,      0      };
	numBytes = helper_receive(sfd, 6);
	TEST_CHECK(errno == exp_errno1[env_id], "recvfrom(): %s, numBytes = %zd",
		   strerror(errno), numBytes);


	/*
	 * Change label of outgoing to an unmapped.
	 */
	ret = smack_set_fd_label(sfd, UNMAPPED_LABEL, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_get_fd_label(sfd, &label, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_get_fd_label(): %s", strerror(errno));
	if (ret == 0)
		TEST_CHECK(strcmp(label, UNMAPPED_LABEL) == 0, "label = %s, "
			   "should be %s", label, UNMAPPED_LABEL);

	/* Send a message to the client */
	helper_send(sfd, 7, &claddr);


	test_sync(-1);
	close(sfd);
}

void test_cleanup(void)
{
	smack_set_rule("inside", "l1", "-");
	smack_set_rule("l1", "inside", "-");
	smack_set_rule("inside", UNMAPPED_LABEL, "-");
	smack_set_rule(UNMAPPED_LABEL, "inside", "-");

	smack_set_rule("outside", "l1", "-");
	smack_set_rule("l1", "outside", "-");
	smack_set_rule("outside", UNMAPPED_LABEL, "-");
	smack_set_rule(UNMAPPED_LABEL, "outside", "-");

	smack_set_rule("inside", "outside", "-");
	smack_set_rule("outside", "inside", "-");
}
