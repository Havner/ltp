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
 * Smack namespace - test case "file access"
 *
 * Triggers following LSM hooks:
 *  * smack_file_open
 *
 * This test verifies read, write and execute access to files.
 * Other test cases trust in smackfs' "access" interface to check object
 * to subject access.
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
#include <sys/wait.h>
#include "test_common.h"

#define LABEL1   "label1"
#define LABEL2   "label2"
#define LABEL3   "label3"
#define LABEL4   "label4"

#define PATH1    "tmp/a"
#define PATH2    "tmp/b"
#define PATH3    "tmp/c"
#define PATH_EXE "tmp/exe"

static struct test_smack_rule_desc test_rules[] =
{
	{INSIDE_PROC_LABEL, LABEL1, "r", automatic},
	{INSIDE_PROC_LABEL, LABEL2, "w", automatic},
	{INSIDE_PROC_LABEL, LABEL3, "rw", automatic},
	{INSIDE_PROC_LABEL, LABEL4, "rx", automatic},
	{NULL}
};

static struct test_smack_mapping_desc test_mappings[] = {
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{LABEL3, MAPPED_LABEL_PREFIX LABEL3, automatic},
	{LABEL4, MAPPED_LABEL_PREFIX LABEL4, automatic},
	{NULL}
};

static struct test_file_desc test_files[] = {
	{PATH1,    0777, LABEL1, NULL, NULL, regular},    /* will get "r" permission */
	{PATH2,    0777, LABEL2, NULL, NULL, regular},    /* will get "w" permission */
	{PATH3,    0777, LABEL3, NULL, NULL, regular},    /* will get "rw" permission */
	{PATH_EXE, 0777, LABEL4, NULL, NULL, executable}, /* executable */
	{NULL}
};

int test_file_open(const char *path, mode_t mode)
{
	int fd = open(path, mode);
	if (fd < 0)
		return -1;
	close(fd);
	errno = 0;
	return 0;
}

int test_file_access(const char *path, mode_t mode)
{
	errno = 0;
	return access(path, mode);
}

/*
 * Try to execute a file. Returns 0 on success and -1 on failure.
 */
int test_file_exe_access(const char* path)
{
	int status;
	char *args[2];
	pid_t pid = fork();

	if (pid < 0)
		ERR_EXIT("fork");

	if (pid == 0) {
		args[0] = strdup(path);
		args[1] = NULL;
		execv(args[0], args);
		exit(EXIT_FAILURE);
	}

	if (waitpid(pid, &status, 0) == -1)
		ERR_EXIT("waitpid");

	if (WEXITSTATUS(status) != EXIT_SUCCESS) {
		errno = EPERM;
		return -1;
	}

	errno = 0;
	return 0;
}

void main_inside_ns(void)
{
	int ret;

	/* wait for file creation and rules setup */
	test_sync(0);

	int ret_allow_access[] = {0, 0,
				  0, 0,
				  0, 0 };
	int ret_deny_access[] =  {0,      EACCES,
				  EACCES, EACCES,
				  0,      EACCES };

	/*
	 * It's important to note that for open(W) to succeed you need
	 * to have both, WRITE and READ smack access to a file, this
	 * is intentionall. access() check on the other hand does not
	 * reaquire READ for WRITE check (not sure though if that is
	 * intentionall as well)
	 */
	test_file_access(PATH1, R_OK);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_open(PATH1, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(PATH1, W_OK);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	test_file_open(PATH1, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));

	test_file_access(PATH2, R_OK);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	test_file_open(PATH2, O_RDONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	test_file_access(PATH2, W_OK);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_open(PATH2, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));

	test_file_access(PATH3, R_OK);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_open(PATH3, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(PATH3, W_OK);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_open(PATH3, O_WRONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));

	test_file_access(PATH_EXE, R_OK);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_open(PATH_EXE, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(PATH_EXE, W_OK);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	test_file_open(PATH_EXE, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));

	ret = test_file_exe_access(PATH_EXE);
	TEST_CHECK(ret == ret_allow_access[env_id], "ret = %d, %s", ret,
		   strerror(errno));

	test_sync(1);
}

void main_outside_ns(void)
{
	init_test_resources(test_rules, test_mappings, NULL, test_files);

	test_sync(0);

	/* test will now execute */

	test_sync(1);
}

void test_cleanup(void)
{
}
