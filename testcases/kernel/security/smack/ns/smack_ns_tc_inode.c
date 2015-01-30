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
 * Author: Michal Witanowski <m.witanowski@samsung.com>
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

static const char *dir1 = "tmp/dir1";
static const char *dir2 = "tmp/dir2";
static const char *dir3 = "tmp/dir3";
static const char *dir4 = "tmp/dir4";

static const char *rmdir1 = "tmp/dir3/rmdir1";
static const char *rmdir2 = "tmp/dir3/rmdir2";
static const char *rmdir3 = "tmp/dir4/rmdir3";
static const char *rmdir4 = "tmp/dir4/rmdir4";

static const char *file1 = "tmp/dir3/file1";
static const char *new_link1 = "tmp/dir3/link1";
static const char *renamed_file1 = "tmp/dir3/renamed_file1";

static const char *file2 = "tmp/dir4/file2";
static const char *new_link2 = "tmp/dir4/link2";
static const char *renamed_file2 = "tmp/dir4/renamed_file2";

void main_inside_ns(void)
{
	int ret;
	DIR *d;

	test_sync(0);

	/*
	 * Directory access checks
	 */

	int expected_ret1[] = {1, 1, 0, 0,  // UID = 0
			       1, 1, 0, 0}; // UID = 1000
	d = opendir(dir1);
	if (expected_ret1[env_id])
		TEST_CHECK(d != NULL || errno != EPERM, strerror(errno));
	else
		TEST_CHECK(d == NULL, "opendir() should fail");
	closedir(d);

	int expected_ret2[] = {1, 0, 1, 1,  // UID = 0
			       0, 0, 0, 0}; // UID = 1000
	d = opendir(dir2);
	if (expected_ret2[env_id])
		TEST_CHECK(d != NULL || errno != EPERM, strerror(errno));
	else
		TEST_CHECK(d == NULL, "opendir() should fail");
	closedir(d);

	d = opendir(dir3);
	TEST_CHECK(d != NULL || errno != EPERM, strerror(errno));
	closedir(d);


	/*
	 * Hard link tests
	 */

	int expected_ret3[] = {1, 0, 1, 1,  // UID = 0
			       0, 0, 0, 0}; // UID = 1000
	ret = link(file1, new_link1);
	if (expected_ret3[env_id]) {
		TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));
		ret = unlink(new_link1);
		TEST_CHECK(ret == 0, strerror(errno));
	} else
		TEST_CHECK(ret == -1, "link() should fail");

	ret = link(file2, new_link2);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = unlink(new_link2);
	TEST_CHECK(ret == 0, strerror(errno));

	/*
	 * Rename test
	 */

	int expected_ret4[] = {1, 0, 1, 1,  // UID = 0
			       0, 0, 0, 0}; // UID = 1000
	ret = rename(file1, renamed_file1);
	if (expected_ret4[env_id]) {
		TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));
		ret = rename(renamed_file1, file1);
		TEST_CHECK(ret == 0, strerror(errno));
	} else
		TEST_CHECK(ret == -1, "link() should fail");

	ret = rename(file2, renamed_file2);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = rename(renamed_file2, file2);
	TEST_CHECK(ret == 0, strerror(errno));


	/*
	 * rmdir tests
	 */

	int expected_ret5[] = {1, 0, 1, 1,  // UID = 0
			       0, 0, 0, 0}; // UID = 1000
	ret = rmdir(rmdir1);
	if (expected_ret5[env_id])
		TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(rmdir2);
	if (expected_ret5[env_id])
		TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(rmdir3);
	if (expected_ret5[env_id])
		TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));
	else
		TEST_CHECK(ret == -1, "rmdir() should fail");

	ret = rmdir(rmdir4);
	TEST_CHECK(ret == 0 || errno != EPERM, strerror(errno));

	test_sync(1);
}

void main_outside_ns(void)
{
	int ret;

	/* setup rules */
	ret = smack_set_rule("inside", "unmapped", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", "l3", "rx");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", "l4", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	/* setup files */

	create_dir_labeled(dir1, 0777, "unmapped");
	create_dir_labeled(dir2, 0777, "l2");
	create_dir_labeled(dir3, 0777, "l3");
	create_dir_labeled(dir4, 0777, "l4");

	create_dir_labeled(rmdir1, 0777, "l3");
	create_dir_labeled(rmdir2, 0777, "l4");
	create_dir_labeled(rmdir3, 0777, "l3");
	create_dir_labeled(rmdir4, 0777, "l4");

	create_file_labeled(file1, 0777, "shared");
	create_file_labeled(file2, 0777, "shared");

	test_sync(0);

	/* cleanup rules */

	test_sync(1);
}

void test_cleanup(void)
{
	/* cleanup rules */
	TEST_CHECK(smack_set_rule("inside", "unmapped", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("inside", "l3", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("inside", "l4", "-") == 0, strerror(errno));

	/* cleanup files */

	remove(file1);
	remove(file2);

	remove(rmdir1);
	remove(rmdir2);
	remove(rmdir3);
	remove(rmdir4);

	remove(dir1);
	remove(dir2);
	remove(dir3);
	remove(dir4);
}
