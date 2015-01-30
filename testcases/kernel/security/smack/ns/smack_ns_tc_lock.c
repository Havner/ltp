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
 * Smack namespace - test case "lock": lock access mode
 *
 * This test case check Smack's lock access mode with cooperation with
 * Smack namespaces.
 *
 * Triggers following LSM hooks:
 *  * smack_file_lock
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
#include <sys/file.h> // for lock
#include "test_common.h"

static struct test_file_desc test_files[] =
{
	{"tmp/file_0", 0666, "l0"},
	{"tmp/file_1", 0666, "l1"},
	{"tmp/file_2", 0666, "l0"},
	{"tmp/file_3", 0666, "l1"},
	{NULL, 0, NULL}
};

static struct test_smack_rule_desc test_rules[] =
{
	{"inside", "l0", "rxt"},
	{"inside", "l1", "rl"},
	{NULL, 0, NULL}
};

void main_inside_ns(void)
{
	int fd, ret;
	int override_tab[] = {1, 0, 1, 1,
			      0, 0, 0, 0};
	// is CAP_MAC_OVERRIDE granting access?
	int override = override_tab[env_id];

	/*
	 * Scenario 0:
	 * try to lock file, without w/l permission
	 */
	test_sync(0);
	fd = open(test_files[0].path, O_RDONLY);
	if (fd == -1)
		ERR_EXIT("open()");
	if (override) {
		ret = flock(fd, LOCK_EX | LOCK_NB);
		TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
		ret = flock(fd, LOCK_UN | LOCK_NB);
		TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
		ret = flock(fd, LOCK_SH | LOCK_NB);
		TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
		ret = flock(fd, LOCK_UN | LOCK_NB);
		TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
	} else {
		ret = flock(fd, LOCK_EX | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_UN | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_SH | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
	}
	close(fd);

	/*
	 * Scenario 1:
	 * try to lock file, with l permission
	 */
	test_sync(1);
	fd = open(test_files[1].path, O_RDONLY);
	if (fd == -1)
		ERR_EXIT("open()");
	ret = flock(fd, LOCK_EX | LOCK_NB);
	TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
	ret = flock(fd, LOCK_UN | LOCK_NB);
	TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
	ret = flock(fd, LOCK_SH | LOCK_NB);
	TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
	ret = flock(fd, LOCK_UN | LOCK_NB);
	TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
	close(fd);

	/*
	 * Scenario 2:
	 * try to lock already locked file file (shared), without w/l permission
	 */
	test_sync(2);
	fd = open(test_files[2].path, O_RDONLY);
	if (fd == -1)
		ERR_EXIT("open()");
	if (override) {
		ret = flock(fd, LOCK_EX | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_EX | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_SH | LOCK_NB);
		TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
		ret = flock(fd, LOCK_UN | LOCK_NB);
		TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
	} else {
		ret = flock(fd, LOCK_EX | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_UN | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
		ret = flock(fd, LOCK_SH | LOCK_NB);
		TEST_CHECK(ret == -1, "Should not be able to lock file");
	}
	close(fd);

	/*
	 * Scenario 3:
	 * try to lock already locked file file (shared), with l permission
	 */
	test_sync(3);
	fd = open(test_files[3].path, O_RDONLY);
	if (fd == -1)
		ERR_EXIT("open()");
	ret = flock(fd, LOCK_EX | LOCK_NB);
	TEST_CHECK(ret == -1, "Should not be able to lock file");
	ret = flock(fd, LOCK_EX | LOCK_NB);
	TEST_CHECK(ret == -1, "Should not be able to lock file");
	ret = flock(fd, LOCK_SH | LOCK_NB);
	TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
	ret = flock(fd, LOCK_UN | LOCK_NB);
	TEST_CHECK(ret == 0, "Unlocking failed: %s", strerror(errno));
	close(fd);

	test_sync(5); // TODO
}

void main_outside_ns(void)
{
	int ret, fd2, fd3;

	init_test_resources(test_rules, NULL, NULL, test_files);

	// Scenario 0
	test_sync(0);

	// Scenario 1
	test_sync(1);

	// Scenario 2
	fd2 = open(test_files[2].path, O_RDONLY);
	if (fd2 == -1)
		ERR_EXIT("open()");
	ret = flock(fd2, LOCK_SH | LOCK_NB);
	TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
	test_sync(2);

	// Scenario 3
	fd3 = open(test_files[3].path, O_RDONLY);
	if (fd3 == -1)
		ERR_EXIT("open()");
	ret = flock(fd3, LOCK_SH | LOCK_NB);
	TEST_CHECK(ret == 0, "Locking failed: %s", strerror(errno));
	test_sync(3);

	// TODO: Scenario 4 - lock exclusive here

	// wait for scenarios
	test_sync(5);

	close(fd2);
	close(fd3);
}

void test_cleanup(void)
{
}
