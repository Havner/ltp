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
 * Smack namespace - test case "mmap"
 *
 * This test case verifies if Smack's MMAP label and rules are correctly
 * interpreted when calling mmap() syscall.
 *
 * Triggers following LSM hooks:
 *  * smack_mmap_file
 *  * smack_file_open
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include "test_common.h"

static struct test_file_desc test_files[] =
{
	{"tmp/file_0", 0444, "l0"}, // has mapped mmap label
	{"tmp/file_1", 0444, "l0"}, // has unmapped mmap label
	{NULL, 0, NULL}
};

void main_inside_ns(void)
{
	int fd;
	void* map;

	// wait for files creation
	test_sync(0);

	// Scenario 1

	fd = open(test_files[0].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_CHECK(map == MAP_FAILED, "mmap() should fail");
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);


	fd = open(test_files[1].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_CHECK(map == MAP_FAILED, "mmap() should fail");
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);

	test_sync(1);
	test_sync(2);

	// Scenario 2

	fd = open(test_files[0].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_0[] = {1, 1, 1, 1,   // UID = 0
				 1, 1, 1, 1 }; // UID = 1000
	TEST_CHECK((map != MAP_FAILED) == expected_mmap_0[env_id],
			   "mmap(): %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);


	fd = open(test_files[1].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_1[] = {1, 1, 0, 0,   // UID = 0
				 1, 1, 0, 0 }; // UID = 1000
	int expected_errno_1[] = {0, 0, EPERM, EPERM,  // UID = 0
				  0, 0, EPERM, EPERM }; // UID = 1000
	TEST_CHECK((map != MAP_FAILED) == expected_mmap_1[env_id] &&
		   errno == expected_errno_1[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);

	test_sync(3);
	test_sync(4);

	// Scenario 3

	fd = open(test_files[0].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_2[] = {0, 0, 1, 1,   // UID = 0
				 0, 0, 1, 1 }; // UID = 1000
	int expected_errno_2[] = {EACCES, EACCES, 0, 0,  // UID = 0
				  EACCES, EACCES, 0, 0 }; // UID = 1000
	TEST_CHECK((map != MAP_FAILED) == expected_mmap_2[env_id] &&
		   errno == expected_errno_2[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);


	fd = open(test_files[1].path, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, CREATED_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_3[] = {0, 0, 0, 0,   // UID = 0
				 0, 0, 0, 0 }; // UID = 1000
	int expected_errno_3[] = {EACCES, EACCES, EPERM, EPERM,  // UID = 0
				  EACCES, EACCES, EPERM, EPERM }; // UID = 1000
	TEST_CHECK((map != MAP_FAILED) == expected_mmap_3[env_id] &&
		   errno == expected_errno_3[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, CREATED_FILE_SIZE);
	close(fd);

	test_sync(5);
}

void main_outside_ns(void)
{
	int ret;

	// add rule allowing file access
	ret = smack_set_rule("inside", "l0", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	init_test_resources(NULL, NULL, NULL, test_files);
	ret = smack_set_file_label(test_files[0].path, "l1", SMACK_LABEL_MMAP,
				   0);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_file_label(test_files[1].path, "unmapped",
				   SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == 0, strerror(errno));

	/*
	 * Scenario 1: no permissions to map the files.
	 */
	test_sync(0);
	test_sync(1);

	/*
	 * Scenario 2: add rules allowing file mapping.
	 *
	 * Framework sets "inside shared rwxatl" and "inside _ rx" before
	 * test case launch, so we need add equivalent rules to mmap
	 * succeed.
	 */
	ret = smack_set_rule("l1", "l0", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "shared", "rwxatl");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "_", "rx");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_rule("unmapped", "l0", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("unmapped", "shared", "rwxatl");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("unmapped", "_", "rx");
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(2);
	test_sync(3);

	/*
	 * Scenario 3:
	 *
	 * Add an extra rule which will disable permissions to map a file
	 * in the initial Smack namespace. The "unmapped2" won't be visible
	 * in the Smack namspace, so the conditions to permit mmap() should be
	 * met.
	 */
	ret = smack_set_rule("inside", "unmapped2", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));
	test_sync(4);
	test_sync(5);
}

void test_cleanup(void)
{
	TEST_CHECK(smack_set_rule("inside", "l0", "-") == 0, strerror(errno));

	TEST_CHECK(smack_set_rule("l1", "l0", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("l1", "shared", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("l1", "_", "-") == 0, strerror(errno));

	TEST_CHECK(smack_set_rule("unmapped", "l0", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("unmapped", "shared", "-") == 0, strerror(errno));
	TEST_CHECK(smack_set_rule("unmapped", "_", "-") == 0, strerror(errno));

	TEST_CHECK(smack_set_rule("inside", "unmapped2", "-") == 0, strerror(errno));
}
