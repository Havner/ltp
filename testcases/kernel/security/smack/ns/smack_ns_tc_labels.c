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
 * Smack namespace - test case "labels"
 * * setting/getting file and process labels
 *
 * Triggers following LSM hooks:
 *  * smack_inode_init_security
 *  * smack_inode_getxattr
 *  * smack_inode_removexattr
 *  * smack_inode_setxattr
 *  * smack_setprocattr
 *  * smack_getprocattr
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

static const char* test_path = "tmp/a";
static const char* test_path2 = "tmp/b";
static const char* test_path3 = "tmp/c";
static const char* transmute_dir = "tmp/transmute";

void main_inside_ns(void)
{
	int ret;
	char *label = NULL;
	int lsm_ns = env_id & TEST_ENV_SMACK_NS;

	test_sync(0);

	// Verify file labels
	//		       -   U   L  U+L
	int expected_ret[] = { 0, -1,  0,  0,   // UID = 0
			      -1, -1, -1, -1 }; // UID = 1000
	int expected_errno1[] = {     0, EACCES,      0,      0,   // UID = 0
				 EACCES, EACCES, EACCES, EACCES }; // UID = 1000

	errno = 0;
	ret = smack_get_file_label(test_path, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_l1" : "l1");

	errno = 0;
	ret = smack_get_file_label(test_path, &label, SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_l2" : "l2");

	errno = 0;
	ret = smack_get_file_label(test_path, &label, SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_l3" : "l3");


	// this time the file got "_" label in Smack NS, so we can access it
	int expected_ret2[] = { 0, -1, 0,  0,
			       -1, -1, 0,  0};
	int expected_errno2[] = {0,      EACCES, 0, 0,   // UID = 0
				 EACCES, EACCES, 0, 0 }; // UID = 1000

	errno = 0;
	ret = smack_get_file_label(test_path2, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "_" : "will_be_floor");

	ret = smack_get_file_label(transmute_dir, &label, SMACK_LABEL_TRANSMUTE, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, "TRUE");
	}


	// unmapped mmap and execute labels

	ret = smack_get_file_label(test_path3, &label, SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "?" : "unmapped");

	ret = smack_get_file_label(test_path3, &label, SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "?" : "unmapped");


	// Verify process label

	ret = smack_get_process_label(getpid(), &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_inside" : "inside");


	// Now modify labels inside ns

	int expected_ret3[] = { 0, -1,  0,  0,
			       -1, -1, -1, -1};
	int expected_errno3[] = {    0, EPERM,     0,     0,
				 EPERM, EPERM, EPERM, EPERM };

	errno = 0;
	ret = smack_set_file_label(test_path, "n_l4", SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	errno = 0;
	ret = smack_set_self_label("n_l1");
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	// Try unmapped labels

	int expected_ret4[] = { 0, -1, -1, -1,
			       -1, -1, -1, -1};
	int expected_errno4[] = {    0, EPERM, EBADR, EBADR,
				 EPERM, EPERM, EPERM, EPERM };

	errno = 0;
	ret = smack_set_file_label(test_path, "unmapped", SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(errno == expected_errno4[env_id] && ret == expected_ret4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	errno = 0;
	ret = smack_set_self_label("unmapped");
	TEST_CHECK(errno == expected_errno4[env_id] && ret == expected_ret4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	// remove file label

	int expected_ret5[] = { 0, -1,  0,  0,
			       -1, -1, -1, -1};
	int expected_errno5[] = {    0, EPERM,     0,     0,
				 EPERM, EPERM, EPERM, EPERM };

	errno = 0;
	ret = smack_set_file_label(test_path, NULL, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(errno == expected_errno5[env_id] && ret == expected_ret5[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	test_sync(1);
}

// this function will be executed outside the namespace
void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	// create test files, directories, etc.
	if (create_file_labeled(test_path, 0666, "shared") == -1)
		ERR_EXIT("create_file");
	if (create_file_labeled(test_path2, 0666, "shared") == -1)
		ERR_EXIT("create_file");
	if (create_file_labeled(test_path3, 0666, "shared") == -1)
		ERR_EXIT("create_file");
	create_dir_labeled(transmute_dir, S_IRWXU | S_IRWXG | S_IRWXO, "shared");

	// set file labels to mapped values

	ret = smack_set_file_label(test_path, "l1", SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_file_label(test_path, "l2", SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_file_label(test_path, "l3", SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_file_label(transmute_dir, "TRUE", SMACK_LABEL_TRANSMUTE,
				   0);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_file_label(test_path2, "will_be_floor",
				   SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_file_label(test_path3, "unmapped", SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_file_label(test_path3, "unmapped", SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(0);

	// wait for checks...
	test_sync(1);

	const char* exp_label[] = {"unmapped", "l1", "l4", "l4",
				   "l1", "l1", "l1", "l1"};
	ret = smack_get_file_label(test_path, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	TEST_LABEL(label, exp_label[env_id]);


	const char* exp_label2[] = {"unmapped", "inside", "l1",     "l1",
				    "inside",   "inside", "inside", "inside"};
	ret = smack_get_process_label(sibling_pid, &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	TEST_LABEL(label, exp_label2[env_id]);
}

void test_cleanup(void)
{
	remove(test_path);
	remove(test_path2);
	remove(test_path3);
	remove(transmute_dir);
}
