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
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include "test_common.h"

#define LABEL0    "label0"
#define LABEL1    "label1"
#define UNMAPPED  "unmapped"
#define UNMAPPED2 "unmapped2"
#define INSIDE    INSIDE_PROC_LABEL
#define SHARED    SHARED_OBJECT_LABEL

#define FILE0     "tmp/file0"
#define FILE1     "tmp/file1"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE,   LABEL0,    "rwx", automatic}, /* 0: rule allowing file access */
	{LABEL1,   LABEL0,    "rwx",    manual}, /* 1: scenario 2 */
	{LABEL1,   SHARED,    "rwxatl", manual}, /* 2: scenario 2 */
	{LABEL1,   "_",       "rx",     manual}, /* 3: scenario 2 */
	{UNMAPPED, LABEL0,    "rwx",    manual}, /* 4: scenario 2 */
	{UNMAPPED, SHARED,    "rwxatl", manual}, /* 5: scenario 2 */
	{UNMAPPED, "_",       "rx",     manual}, /* 6: scenario 2 */
	{INSIDE,   UNMAPPED2, "rwx",    manual}, /* 7: scenario 3 */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL0, MAPPED_LABEL_PREFIX LABEL0, automatic},
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{NULL}
};

static const struct test_file_desc test_files[] = {
	{FILE0, 0444, LABEL0, NULL, LABEL1, regular},
	{FILE1, 0444, LABEL0, NULL, UNMAPPED, regular},
	{NULL}
};

void main_inside_ns(void)
{
	int fd;
	void* map;

	/* wait for files creation */
	test_sync(0);

	/* Scenario 1 */

	fd = open(FILE0, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_CHECK(map == MAP_FAILED, "mmap() should fail");
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);


	fd = open(FILE1, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	TEST_CHECK(map == MAP_FAILED, "mmap() should fail");
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);

	test_sync(1);
	test_sync(2);

	/* Scenario 2 */

	fd = open(FILE0, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_0[] = {0, 0,
				 0, 0,
				 0, 0};
	TEST_CHECK(-(map == MAP_FAILED) == expected_mmap_0[env_id],
			   "mmap(): %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);


	fd = open(FILE1, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_1[] = { 0,  0,
				  0,  0,
				 -1, -1};
	int expected_errno_1[] = {     0,      0,
				       0,      0,
				  EACCES, EACCES};
	TEST_CHECK(-(map == MAP_FAILED) == expected_mmap_1[env_id] &&
		   errno == expected_errno_1[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);

	test_sync(3);
	test_sync(4);

	/* Scenario 3 */

	fd = open(FILE0, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_2[] = {-1, -1,
				 -1, -1,
				 -1, -1};
	int expected_errno_2[] = {EACCES, EACCES,
				  EACCES, EACCES,
				  EACCES, EACCES};
	TEST_CHECK(-(map == MAP_FAILED) == expected_mmap_2[env_id] &&
		   errno == expected_errno_2[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);


	fd = open(FILE1, O_RDONLY);
	TEST_CHECK(fd != -1, "open() failed: %s", strerror(errno));
	errno = 0;
	map = mmap(NULL, REGULAR_FILE_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
	int expected_mmap_3[] = {-1, -1,
				 -1, -1,
				 -1, -1};
	int expected_errno_3[] = {EACCES, EACCES,
				  EACCES, EACCES,
				  EACCES, EACCES};
	TEST_CHECK(-(map == MAP_FAILED) == expected_mmap_3[env_id] &&
		   errno == expected_errno_3[env_id],
		   "mmap() = %s", strerror(errno));
	if (map != MAP_FAILED)
		munmap(map, REGULAR_FILE_SIZE);
	close(fd);

	test_sync(5);
}

void main_outside_ns(void)
{
	init_test_resources(test_rules, test_mappings, NULL, test_files);

	/*
	 * Scenario 1 preparation: no permissions to map the files.
	 */
	test_sync(0);

	/* Scenarion 1 */

	test_sync(1);

	/*
	 * Scenario 2 preparation: add rules allowing file mapping.
	 *
	 * Framework sets "inside shared rwxatl" and "inside _ rx" before
	 * test case launch, so we need add equivalent rules to mmap
	 * succeed.
	 */
	set_smack_rule(&test_rules[1]);
	set_smack_rule(&test_rules[2]);
	set_smack_rule(&test_rules[3]);
	set_smack_rule(&test_rules[4]);
	set_smack_rule(&test_rules[5]);
	set_smack_rule(&test_rules[6]);

	test_sync(2);

	/* Scenario 2 */

	test_sync(3);

	/*
	 * Scenario 3 preparation:
	 *
	 * Add an extra rule which will disable permissions to map a file
	 * in the initial Smack namespace. The "unmapped2" won't be visible
	 * in the Smack namespace, but there are no negative permissions here
	 * so it will still affect the namespace by not allowing to mmap
	 * (which in theory could be possible on FILE0 as inside the namespace
	 * the rules are still met).
	 */
	set_smack_rule(&test_rules[7]);

	test_sync(4);

	/* Scenario 3 */

	test_sync(5);
}

void test_cleanup(void)
{
}
