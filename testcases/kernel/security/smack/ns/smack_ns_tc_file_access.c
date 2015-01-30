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
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include "test_common.h"

static const char* exe_file_path = "tmp/exe_file";

static struct test_file_desc files[] =
{
	{"tmp/a", 0777, "l1"}, /* will get "r" permission */
	{"tmp/b", 0777, "l2"}, /* will get "w" permission */
	{"tmp/c", 0777, "l3"}, /* will get "rw" permission */
	{NULL, 0, NULL}
};

static struct test_smack_rule_desc rules[] =
{
	{"inside", "l1", "r"},
	{"inside", "l2", "w"},
	{"inside", "l3", "rw"},
	{"inside", "l4", "rx"},
	{NULL, NULL, NULL}
};

/*
 * Create a test executable file
 */
static inline int create_exe_file(const char *path, mode_t mode,
				  const char *label)
{
	const char* content = "#!/bin/bash\n";

	int fd = open(path, O_CREAT | O_WRONLY, 0700);
	if (fd < 0)
		return -1;
	if (write(fd, content, strlen(content)) != (ssize_t)strlen(content))
		ERR_EXIT("write()");
	close(fd);

	if (chmod(path, mode) == -1)
		ERR_EXIT("chmod()");

	if (chown(path, uid, gid) == -1)
		ERR_EXIT("chown()");

	if (label != NULL)
		if (smack_set_file_label(path, label, SMACK_LABEL_ACCESS, 0)
		    == -1)
			ERR_EXIT("smack_set_file_label()");

	return 0;
}

int test_file_access(const char *path, mode_t mode)
{
	int fd = open(path, mode);
	if (fd < 0)
		return -1;
	close(fd);
	errno = 0;
	return 0;
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

	// wait for file creation and rules setup
	test_sync(0);

	int ret_allow_access[] = {0, 0, 0, 0,
				  0, 0, 0, 0};
	int ret_deny_access[] =  {0,      EACCES, 0,      0,
				  EACCES, EACCES, EACCES, EACCES};

	test_file_access(files[0].path, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(files[0].path, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));

	test_file_access(files[1].path, O_RDONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	test_file_access(files[1].path, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));

	test_file_access(files[2].path, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(files[2].path, O_WRONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));

	test_file_access(exe_file_path, O_RDONLY);
	TEST_CHECK(errno == ret_allow_access[env_id], strerror(errno));
	test_file_access(exe_file_path, O_WRONLY);
	TEST_CHECK(errno == ret_deny_access[env_id], strerror(errno));
	ret = test_file_exe_access(exe_file_path);
	TEST_CHECK(ret == ret_allow_access[env_id], "ret = %d, %s", ret,
		   strerror(errno));

	test_sync(1);
}

void main_outside_ns(void)
{
	init_test_resources(rules, NULL, NULL, files);

	// Create test exe file
	// TODO: include this in init_test as well
	if (create_exe_file(exe_file_path, 0777, "l4") == -1)
		ERR_EXIT("create_exe_file");

	test_sync(0);
	// test will now execute
	test_sync(1);
}

void test_cleanup(void)
{
	remove(exe_file_path);
}
