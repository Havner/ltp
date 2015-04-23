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
 * Smack namespace - test case "setns"
 *
 * This test case checks behavior of setns() syscall when used to enter Smack
 * namespace.
 *
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "test_common.h"

#define NS_PATH_SIZE 64

#define LABEL    "label"
#define UNMAPPED "unmapped"

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{NULL}
};

void main_inside_ns(void)
{
	if (!(env_id & TEST_ENV_SMACK_NS))
		return;

	/* helper process will enter the namespace here */

	test_sync(0);
}

void main_outside_ns(void)
{
	pid_t child;
	int status;

	init_test_resources(NULL, test_mappings, NULL, NULL);

	if (!(env_id & TEST_ENV_SMACK_NS))
		return;

	child = fork();
	if (child < 0)
		ERR_EXIT("fork");

	/* do the setns() tests in the child process not to break the environment */
	if (child == 0) {
		int fd, ret;
		char* label = NULL;
		char path[NS_PATH_SIZE];

		snprintf(path, NS_PATH_SIZE, "/proc/%d/ns/lsm", sibling_pid);
		fd = open(path, O_RDONLY);
		TEST_CHECK(fd != -1, "open(): %s", strerror(errno));

		/* try entering the namespace having unmapped label */
		ret = smack_set_self_label(UNMAPPED);
		TEST_CHECK(ret == 0, strerror(errno));

		ret = setns(fd, 0);
		TEST_CHECK(ret == -1 && errno == EPERM, "setns() should fail with EPERM, "
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

		ret = setns(fd, CLONE_NEWLSM);
		TEST_CHECK(ret == -1 && errno == EPERM, "setns() should fail with EPERM, "
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

		/* try entering the namespace having mapped label */
		ret = smack_set_self_label(LABEL);
		TEST_CHECK(ret == 0, strerror(errno));
		ret = setns(fd, 0);
		TEST_CHECK(ret == 0, strerror(errno));

		/* check if we are really in the namespace */
		ret = smack_get_process_label(getpid(), &label);
		TEST_CHECK(ret == 0, strerror(errno));
		if (ret == 0) {
			TEST_LABEL(label, MAPPED_LABEL_PREFIX LABEL);
			free(label);
		}

		close(fd);
		exit(0);
	}

	waitpid(child, &status, 0);

	test_sync(0);
}

void test_cleanup(void)
{
}
