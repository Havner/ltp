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
 * Smack namespace - test case "inode"
 *
 * Triggers following LSM hooks:
 *  * smack_inode_getattr
 *  * smack_inode_init_security
 *  * smack_inode_link
 *  * smack_inode_permission
 *  * smack_inode_rename
 *  * smack_inode_rmdir
 *  * smack_inode_setattr
 *  * smack_inode_unlink
 *
 * This test case calls various syscalls in order to manipulate inodes:
 * directories, hard links, soft links.
 *
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>

#include "test_common.h"


/*
 Full directories structure will look like this:

	 tmp (full rwx access)
		- [dir] _dir1 (no access in Smack NS)
		- [dir] _dir2 (no rule - only privileged user can access)
		- [dir] _dir3 (rx access)
			 - [dir] rmdir1 (rx access)
			 - [dir] rmdir2 (rwx access)
			 - file1 <=> renamed_file1
			 - link1 (same i-node as file1)
		- [dir] _dir4 (rwx access)
			 - [dir] rmdir3 (rx access)
			 - [dir] rmdir4 (rwx access)
			 - file2 <=> renamed_file2
			 - link2 (same i-node as file2)

 */

#define DIR1          "tmp/dir1"
#define DIR2          "tmp/dir2"
#define DIR3          "tmp/dir3"
#define DIR4          "tmp/dir4"

#define RMDIR1        "tmp/dir3/rmdir1"
#define RMDIR2        "tmp/dir3/rmdir2"
#define RMDIR3        "tmp/dir4/rmdir3"
#define RMDIR4        "tmp/dir4/rmdir4"

#define FILE1         "tmp/dir3/file1"
#define NEW_LINK1     "tmp/dir3/link1"
#define RENAMED_FILE1 "tmp/dir3/renamed_file1"

#define FILE2         "tmp/dir4/file2"
#define NEW_LINK2     "tmp/dir4/link2"
#define RENAMED_FILE2 "tmp/dir4/renamed_file2"

#define LABEL2        "label2"
#define LABEL3        "label3"
#define LABEL4        "label4"
#define UNMAPPED      "unmapped"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, UNMAPPED, "rwx", automatic},
	{INSIDE_PROC_LABEL, LABEL3, "rx", automatic},
	{INSIDE_PROC_LABEL, LABEL4, "rwx", automatic},
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{LABEL3, MAPPED_LABEL_PREFIX LABEL3, automatic},
	{LABEL4, MAPPED_LABEL_PREFIX LABEL4, automatic},
	{NULL}
};

static const struct test_dir_desc test_dirs[] = {
	{DIR1, 0777, UNMAPPED, none},
	{DIR2, 0777, LABEL2, none},
	{DIR3, 0777, LABEL3, none},
	{DIR4, 0777, LABEL4, none},
	{RMDIR1, 0777, LABEL3, none},
	{RMDIR2, 0777, LABEL4, none},
	{RMDIR3, 0777, LABEL3, none},
	{RMDIR4, 0777, LABEL4, none},
	{NULL}
};

static const struct test_file_desc test_files[] = {
	{FILE1, 0777, SHARED_OBJECT_LABEL, NULL, NULL, regular},
	{FILE2, 0777, SHARED_OBJECT_LABEL, NULL, NULL, regular},
	{NULL}
};

void main_inside_ns(void)
{
	int ret;
	DIR *d;

	test_sync(0);

	/*
	 * Directory access checks
	 */

	int expected_ret1[] = { 0,  0,
				0,  0,
			       -1, -1};
	d = opendir(DIR1);
	if (expected_ret1[env_id] == 0)
		TEST_CHECK(d != NULL, strerror(errno));
	else
		TEST_CHECK(d == NULL, "opendir() should fail");
	closedir(d);

	int expected_ret2[] = { 0, -1,
			       -1, -1,
				0, -1 };
	d = opendir(DIR2);
	if (expected_ret2[env_id] == 0)
		TEST_CHECK(d != NULL, strerror(errno));
	else
		TEST_CHECK(d == NULL, "opendir() should fail");
	closedir(d);

	d = opendir(DIR3);
	TEST_CHECK(d != NULL, strerror(errno));
	closedir(d);


	/*
	 * Hard link tests
	 */

	int expected_ret3[] = { 0, -1,
			       -1, -1,
				0, -1 };
	ret = link(FILE1, NEW_LINK1);
	if (expected_ret3[env_id] == 0) {
		TEST_CHECK(ret == 0, strerror(errno));
		ret = unlink(NEW_LINK1);
		TEST_CHECK(ret == 0, strerror(errno));
	} else
		TEST_CHECK(ret == -1, "link() should fail");

	ret = link(FILE2, NEW_LINK2);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = unlink(NEW_LINK2);
	TEST_CHECK(ret == 0, strerror(errno));

	/*
	 * Rename test
	 */

	int expected_ret4[] = { 0, -1,
			       -1, -1,
				0, -1 };
	ret = rename(FILE1, RENAMED_FILE1);
	if (expected_ret4[env_id] == 0) {
		TEST_CHECK(ret == 0, strerror(errno));
		ret = rename(RENAMED_FILE1, FILE1);
		TEST_CHECK(ret == 0, strerror(errno));
	} else
		TEST_CHECK(ret == -1, "link() should fail");

	ret = rename(FILE2, RENAMED_FILE2);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = rename(RENAMED_FILE2, FILE2);
	TEST_CHECK(ret == 0, strerror(errno));


	/*
	 * rmdir tests
	 */

	int expected_ret5[] = { 0, -1,
			       -1, -1,
				0, -1 };
	ret = rmdir(RMDIR1);
	if (expected_ret5[env_id] == 0)
		TEST_CHECK(ret == 0, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(RMDIR2);
	if (expected_ret5[env_id] == 0)
		TEST_CHECK(ret == 0, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(RMDIR3);
	if (expected_ret5[env_id] == 0)
		TEST_CHECK(ret == 0, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(RMDIR4);
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(1);
}

void main_outside_ns(void)
{
	init_test_resources(test_rules, test_mappings, test_dirs, test_files);

	test_sync(0);

	/* perform tests */

	test_sync(1);
}

void test_cleanup(void)
{
}
