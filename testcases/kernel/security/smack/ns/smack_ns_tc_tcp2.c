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
 * Smack namespace - test case "TCP 2"
 *
 * IPv4 TCP connection with Smack namespace.
 *
 * Server is running inside namespaces.
 * Client is running outside namespaces.
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

#include "test_common.h"
#include "test_common_inet.h"

#define NS_PATH_SIZE 64
#define BUF_SIZE 32

static const char* SERVER_ADDRESS = "localhost";

static const char* MESSAGE1 = "msg1";
static const char* MESSAGE2 = "msg2";

#define LABEL           "label"
#define UNMAPPED        "unmapped"
#define CLIENT1         "client1"
#define CLIENT2         "client2"
#define CLIENT_UNMAPPED "client_unmapped"
#define INSIDE          INSIDE_PROC_LABEL

/*
 * Allow sending and receiving packets with "client1"
 * and "unmapped" labels.
 */
static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE,          CLIENT1,           "w", automatic},
	{CLIENT1,         INSIDE_PROC_LABEL, "w", automatic},
	{INSIDE,          CLIENT_UNMAPPED,   "w", automatic},
	{CLIENT_UNMAPPED, INSIDE_PROC_LABEL, "w", automatic},
	{NULL}
};

/* setup labels map for namespaced process */
static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{CLIENT1, MAPPED_LABEL_PREFIX CLIENT1, automatic},
	{CLIENT2, MAPPED_LABEL_PREFIX CLIENT2, automatic},
	{NULL}
};

/*
 * Server - using default ("inside") socket labels
 */
void main_inside_ns(void)
{
	struct sockaddr_in svaddr;
	struct sockaddr_in claddr;
	int svsock, clsock; /* server and client socket file descriptors */
	int ret, port_num = 0;
	socklen_t len;
	char *label = NULL;
	int smack_ns = ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK);

	/* setup server address */
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	svaddr.sin_port = 0;

	/* create server socket */
	svsock = create_server_socket(&svaddr);

	/* get server port number and send it to the sibling process */
	len = sizeof(svaddr);
	if (getsockname(svsock, (struct sockaddr *)&svaddr, &len) == -1) {
		TEST_ERROR("could not get the port number: %s", strerror(errno));
		port_num = -1;
		test_sync_ex(0, &port_num, sizeof(port_num), NULL, 0);
		goto no_network;
	}

	port_num = ntohs(svaddr.sin_port);
	printf("server is running on port: %d\n", port_num);
	test_sync_ex(0, &port_num, sizeof(port_num), NULL, 0);

	/*
	 * Scenario 1
	 */
	test_sync(1);
	errno = 0;
	len = sizeof(claddr);
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK(clsock == -1 && errno == EAGAIN, "accept(): %s",
	           strerror(errno));
	close(clsock);

	/*
	 * Scenario 2:
	 */
	test_sync(2);
	errno = 0;
	len = sizeof(claddr);
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK(smack_ns ? clsock == -1 : clsock != -1 &&
		   smack_ns ? errno == EAGAIN : errno == 0,
		   "accept(): %s", strerror(errno));
	close(clsock);

	/*
	 * Scenario 3:
	 */
	test_sync(3);
	errno = 0;
	len = sizeof(claddr);
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK(clsock != -1, "accept(): %s", strerror(errno));

	/*
	 * Scenario 4
	 */
	test_sync(4);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	test_sync(5);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));

	/*
	 * TODO: The following test is disabled. Something's wrong.
	 */
#if 0
	/*
	 * Scenario 5: packet with unmapped labels shouldn't reach the socket
	 * inside LSM NS
	 */
	test_sync(6);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	test_sync(7);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK(ret == -1, "read(): %s", strerror(errno));
#endif

	/*
	 * Scenario 6
	 */
	test_sync(8);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	test_sync(9);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK(ret == -1, "read(): %s", strerror(errno));


	/*
	 * Socket relabeling inside NS:
	 * Try to set IPIN label to an unmapped label
	 */
	int exp_ret1[] = { 0, -1,
			  -1, -1,
			  -1, -1 };
	int exp_errno1[] = {    0, EPERM,
			    EPERM, EPERM,
			    EBADR, EPERM };
	errno = 0;
	ret = smack_set_fd_label(clsock, UNMAPPED, SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));


	/*
	 * Socket relabeling inside NS:
	 * Set IPIN label to a mapped label
	 */
	int exp_ret2[] = { 0, -1,
			  -1, -1,
			   0, -1 };
	int exp_errno2[] = {    0, EPERM,
			    EPERM, EPERM,
				0, EPERM };
	errno = 0;
	ret = smack_set_fd_label(clsock, LA(LABEL), SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));
	errno = 0;
	ret = smack_get_fd_label(clsock, &label, SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_get_fd_label(): %s", strerror(errno));
	if (exp_ret2[env_id] == 0)
		TEST_LABEL(label, LA(LABEL));
	else
		TEST_LABEL(label, LA(INSIDE_PROC_LABEL));
	if (ret == 0)
		free(label);


	close(clsock);

no_network:
	close(svsock);
	test_sync(-1);
}


/*
 * client
 */
void main_outside_ns(void)
{
	struct sockaddr_in svaddr;
	int sfd, ret;
	int port_num;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	/* get server's port number */
	test_sync_ex(0, NULL, 0, &port_num, sizeof(port_num));
	if (port_num <= 0) {
		TEST_ERROR("invalid port number received");
		goto no_network;
	}

	/* initialize server address */
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_port = htons(port_num);
	ret = inet_aton(SERVER_ADDRESS, &svaddr.sin_addr);
	TEST_CHECK(ret == 0, "inet_aton(): %s", strerror(errno));

	/*
	 * Scenario 1:
	 * client sends packets with label without rw access
	 * - connecting should always fail
	 */
	sfd = create_client_socket();
	ret = smack_set_fd_label(sfd, CLIENT2, SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, CLIENT2, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_self_label(CLIENT2);
	TEST_CHECK(ret != -1, "smack_set_self_label(): %s", strerror(errno));
	test_sync(1);
	errno = 0;
	ret = connect(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK(ret == -1 && errno == EINPROGRESS, "connect(): %s",
		   strerror(errno));
	close(sfd);

	/*
	 * Scenario 2:
	 * client sends packets with unmapped label with "w" access
	 * - connecting should fail only in environment using LSM NS
	 */
	int exp_ret1[] = { 0,  0,
			   0,  0,
			  -1, -1 };
	int exp_errno1[] = {          0,           0,
			              0,           0,
			    EINPROGRESS, EINPROGRESS };
	sfd = create_client_socket();
	ret = smack_set_fd_label(sfd, CLIENT_UNMAPPED, SMACK_LABEL_IPIN);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, CLIENT_UNMAPPED, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_self_label(CLIENT_UNMAPPED);
	TEST_CHECK(ret != -1, "smack_set_self_label(): %s", strerror(errno));
	test_sync(2);
	errno = 0;
	ret = connect(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
	           "connect(): %s", strerror(errno));
	close(sfd);


	/*
	 * Scenario 3:
	 * client sends packets with mapped label with "w" access
	 * - connecting should always pass
	 */
	sfd = create_client_socket();
	ret = smack_set_fd_label(sfd, CLIENT1, SMACK_LABEL_IPIN);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, CLIENT1, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_self_label(CLIENT1);
	TEST_CHECK(ret != -1, "smack_set_self_label(): %s", strerror(errno));
	test_sync(3);
	ret = connect(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK(ret != -1, "connect(): %s", strerror(errno));


	/*
	 * Scenario 4:
	 * sending packets with mapped labels with "w" access
	 * - send/receive will allways succeed
	 */
	test_sync(4);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	test_sync(5);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));

	/*
	 * TODO: The following test is disabled. In this scenario for
	 * some reason checks happen in the init_lsm namespace
	 * effectively allowing unmapped label.
	 */
#if 0
	/*
	 * Scenario 5:
	 * sending packets with unmapped label
	 */
	ret = smack_set_fd_label(sfd, CLIENT_UNMAPPED, SMACK_LABEL_IPIN);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, CLIENT_UNMAPPED, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_self_label(CLIENT_UNMAPPED);
	TEST_CHECK(ret != -1, "smack_set_self_label(): %s", strerror(errno));
	test_sync(6);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	test_sync(7);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
#endif


	/*
	 * Scenario 6:
	 * sending packets with mapped labels without "w" rule
	 */
	ret = smack_set_fd_label(sfd, CLIENT2, SMACK_LABEL_IPIN);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, CLIENT2, SMACK_LABEL_IPOUT);
	TEST_CHECK(ret != -1, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_self_label(CLIENT2);
	TEST_CHECK(ret != -1, "smack_set_self_label(): %s", strerror(errno));
	test_sync(8);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret == -1 && errno == EAGAIN, "read(): %s", strerror(errno));
	test_sync(9);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));

	close(sfd);

no_network:
	test_sync(-1);
}

void test_cleanup(void)
{
}
