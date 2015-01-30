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
 * Smack namespace - test case "CIPSO TCP 2"
 *
 * IPv4 TCP connection with CIPSO and Smack namespace.
 *
 * Server is running inside namespaces.
 * Client is running outside namespaces.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "test_common.h"
#include "test_common_inet.h"

#define AMBIENT_LABEL "ambient"
#define NS_PATH_SIZE 64
#define BUF_SIZE 32

static char *old_ambient = NULL;
static const char* SERVER_ADDRESS = "localhost";

static const char* MESSAGE1 = "msg1";
static const char* MESSAGE2 = "msg2";

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
	int lsm_ns = env_id & TEST_ENV_SMACK_NS;

	// setup server address
	memset(&svaddr, 0, sizeof(struct sockaddr_in));
	svaddr.sin_family = AF_INET;
	svaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	svaddr.sin_port = 0;

	/* create server socket */
	svsock = create_server_socket(&svaddr);

	// get server port number and send it to the sibling process
	len = sizeof(svaddr);
	if (getsockname(svsock, (struct sockaddr *)&svaddr, &len) == -1)
		perror("getsockname");
	else
		port_num = ntohs(svaddr.sin_port);
	printf("server is running on port: %d\n", port_num);
	test_sync_ex(0, &port_num, sizeof(port_num), NULL, 0);

	/*
	 * Scenario 1
	 */
	test_sync(1);
	errno = 0;
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK(clsock == -1 && errno == EAGAIN, "accept(): %s",
	           strerror(errno));
	close(clsock);


	/*
	 * Scenario 2:
	 */
	test_sync(2);
	int exp_ret[] = {1, 1, 0, 0,
			 1, 1, 0, 0 };
	int exp_errno[] = {0, 0, EAGAIN, EAGAIN,
			   0, 0, EAGAIN, EAGAIN};
	errno = 0;
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK((clsock != -1) == exp_ret[env_id] && errno == exp_errno[env_id],
	           "accept(): %s", strerror(errno));
	close(clsock);

	/*
	 * Scenario 3:
	 */
	test_sync(3);
	errno = 0;
	clsock = accept(svsock, (struct sockaddr *)&claddr, &len);
	TEST_CHECK(clsock != -1, "accept(): %s", strerror(errno));


	/*
	 * Scenario 4
	 */
	test_sync(4);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));
	test_sync(5);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));


	/*
	 * Scenario 5: packet with unmapped labels shouldn't reach the socket
	 * inside LSM NS
	 */
	test_sync(6);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));
	test_sync(7);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK((ret != -1) == exp_ret[env_id] && errno == exp_errno[env_id],
	           "read(): %s", strerror(errno));


	/*
	 * Scenario 6
	 */
	test_sync(8);
	ret = tcp_send(clsock, MESSAGE1);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));
	test_sync(9);
	ret = tcp_receive(clsock, MESSAGE2);
	TEST_CHECK(ret != 0, "read(): %s", strerror(errno));


	/*
	 * Socket relabeling inside NS:
	 * Try to set IPIN label to an unmapped label
	 */
	int exp_ret1[] = { 0, -1, -1, -1,
			  -1, -1, -1, -1 };
	int exp_errno1[] = {    0, EPERM, EBADR, EBADR,
			    EPERM, EPERM, EPERM, EPERM};
	errno = 0;
	ret = smack_set_fd_label(clsock, "unmapped", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));


	/*
	 * Socket relabeling inside NS:
	 * Set IPIN label to a mapped label: object of our socket will be
	 * "client1" label visible from init ns.
	 */
	int exp_ret2[] = { 0, -1,  0,  0,
			  -1, -1, -1, -1 };
	int exp_errno2[] = {    0, EPERM,     0,     0,
			    EPERM, EPERM, EPERM, EPERM };
	errno = 0;
	ret = smack_set_fd_label(clsock, lsm_ns ? "n_l1" : "l1",
				 SMACK_LABEL_IPIN);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_fd_label(): %s", strerror(errno));
	errno = 0;
	ret = smack_get_fd_label(clsock, &label, SMACK_LABEL_IPIN);
	TEST_CHECK(errno == 0, "smack_get_fd_label(): %s", strerror(errno));


	test_sync(-1);
	close(clsock);
	close(svsock);
}


/*
 * client
 */
void main_outside_ns(void)
{
	struct sockaddr_in svaddr;
	int sfd, ret;
	int port_num;

	ret = smack_get_ambient(&old_ambient);
	TEST_CHECK(ret != -1, "smack_get_ambient(): %s", strerror(errno));
	ret = smack_set_ambient(AMBIENT_LABEL);
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));

	// setup labels map for namespaced process
	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_map_label(sibling_pid, "client1", "n_client1");
		TEST_CHECK(ret == 0, strerror(errno));
		ret = smack_map_label(sibling_pid, "client2", "n_client2");
		TEST_CHECK(ret == 0, strerror(errno));
	}

	/*
	 * Allow sending and receiving packets with "client1"
	 * and "unmapped" labels.
	 */
	ret = smack_set_rule("inside", "client1", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("client1", "inside", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", "client_unmapped", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("client_unmapped", "inside", "w");
	TEST_CHECK(ret == 0, strerror(errno));


	// get server's port number
	test_sync_ex(0, NULL, 0, &port_num, sizeof(port_num));

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
	ret = smack_set_fd_label(sfd, "client2", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, "client2", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
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
	int exp_ret1[] = {1, 1, 0, 0,
			 1, 1, 0, 0 };
	int exp_errno1[] = {0, 0, EINPROGRESS, EINPROGRESS,
			    0, 0, EINPROGRESS, EINPROGRESS};
	sfd = create_client_socket();
	ret = smack_set_fd_label(sfd, "client_unmapped", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, "client_unmapped", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	test_sync(2);
	errno = 0;
	ret = connect(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK((ret != -1) == exp_ret1[env_id] && errno == exp_errno1[env_id],
	           "connect(): %s", strerror(errno));
	close(sfd);


	/*
	 * Scenario 3:
	 * client sends packets with mapped label with "w" access
	 * - connecting should always pass
	 */
	sfd = create_client_socket();
	ret = smack_set_fd_label(sfd, "client1", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, "client1", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	test_sync(3);
	ret = connect(sfd, (struct sockaddr *)&svaddr, sizeof(svaddr));
	TEST_CHECK(ret != -1, "connect(): %s", strerror(errno));


	/*
	 * Scenario 4:
	 * sending packets with mapped labels with rw access
	 * - send/receive will allways succeed
	 */
	test_sync(4);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));
	test_sync(5);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));


	/*
	 * Scenario 5:
	 * sending packets with unmapped label
	 */
	ret = smack_set_fd_label(sfd, "client_unmapped", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, "client_unmapped", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	test_sync(6);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));
	test_sync(7);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));


	/*
	 * Scenario 6:
	 * sending packets with mapped labels without rw rule
	 */
	ret = smack_set_fd_label(sfd, "client2", SMACK_LABEL_IPIN);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	ret = smack_set_fd_label(sfd, "client2", SMACK_LABEL_IPOUT);
	TEST_CHECK(ret == 0, "smack_set_fd_label(): %s", strerror(errno));
	test_sync(8);
	ret = tcp_receive(sfd, MESSAGE1);
	TEST_CHECK(ret == -1 && errno == EAGAIN, "read(): %s", strerror(errno));
	test_sync(9);
	ret = tcp_send(sfd, MESSAGE2);
	TEST_CHECK(ret == 0, "read(): %s", strerror(errno));


	close(sfd);
	test_sync(-1);
}

void test_cleanup(void)
{
	smack_set_rule("inside", "client1", "-");
	smack_set_rule("client1", "inside", "-");
	smack_set_rule("inside", "client_unmapped", "-");
	smack_set_rule("client_unmapped", "inside", "-");

	smack_set_ambient(old_ambient);
}
