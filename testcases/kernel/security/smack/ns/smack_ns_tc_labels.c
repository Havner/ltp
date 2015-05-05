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
 * This test case manipulates files' and preocesses' labels, taking into account
 * labels mapping inside a Smack namespace.
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
#include "test_common.h"

#define LABEL1         "label1"
#define LABEL2         "label2"
#define LABEL3         "label3"
#define LABEL4         "label4"
#define UNMAPPED       "unmapped"
#define WILL_BE_FLOOR  "will_be_floor"
#define INSIDE         INSIDE_PROC_LABEL

#define TEST_PATH1     "tmp/file1"
#define TEST_PATH2     "tmp/file2"
#define TEST_PATH3     "tmp/file3"
#define TEST_TRANSMUTE "tmp/transmute"

static const struct test_smack_mapping_desc test_mappings[] = {
	{WILL_BE_FLOOR, "_", automatic},
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{LABEL3, MAPPED_LABEL_PREFIX LABEL3, automatic},
	{LABEL4, MAPPED_LABEL_PREFIX LABEL4, automatic},
	{NULL}
};

static const struct test_dir_desc test_dirs[] = {
	{TEST_TRANSMUTE, 0777, SHARED_OBJECT_LABEL, transmute},
	{NULL}
};

static const struct test_file_desc test_files[] = {
	{TEST_PATH1, 0666, LABEL1, LABEL2, LABEL3, regular},
	{TEST_PATH2, 0666, WILL_BE_FLOOR, NULL, NULL, regular},
	{TEST_PATH3, 0666, SHARED_OBJECT_LABEL, UNMAPPED, UNMAPPED, regular},
	{NULL}
};


void main_inside_ns(void)
{
	int ret;
	char *label = NULL;

	test_sync(0);

	/* Verify file labels */
	/*		  UID: 0, 1000 */
	int expected_ret[] = { 0, -1,   /* No NS */
			      -1, -1,   /* User NS */
			       0, -1 }; /* Smack NS */
	int expected_errno1[] = {     0, EACCES,
				 EACCES, EACCES,
				      0, EACCES };

	errno = 0;
	ret = smack_get_file_label(TEST_PATH1, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL1));
		free(label);
	}

	errno = 0;
	ret = smack_get_file_label(TEST_PATH1, &label, SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL2));
		free(label);
	}

	errno = 0;
	ret = smack_get_file_label(TEST_PATH1, &label, SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL3));
		free(label);
	}


	/* this time the file got "_" label in Smack NS, so we can access it */
	int expected_ret2[] = { 0, -1,
			       -1, -1,
				0,  0 };
	int expected_errno2[] = {     0, EACCES,
				 EACCES, EACCES,
				      0,      0 };

	errno = 0;
	ret = smack_get_file_label(TEST_PATH2, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(WILL_BE_FLOOR, "_"));
		free(label);
	}

	ret = smack_get_file_label(TEST_TRANSMUTE, &label, SMACK_LABEL_TRANSMUTE, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, "TRUE");
		free(label);
	}


	/* unmapped mmap and execute labels */

	ret = smack_get_file_label(TEST_PATH3, &label, SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(UNMAPPED, "?"));
		free(label);
	}

	ret = smack_get_file_label(TEST_PATH3, &label, SMACK_LABEL_MMAP, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(UNMAPPED, "?"));
		free(label);
	}


	/* Verify process label */

	ret = smack_get_process_label(getpid(), &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(INSIDE));
		free(label);
	}


	/* Now modify labels inside ns */

	int expected_ret3[] = { 0, -1,
			       -1, -1,
				0, -1 };
	int expected_errno3[] = {    0, EPERM,
				 EPERM, EPERM,
				     0, EPERM };

	errno = 0;
	ret = smack_set_file_label(TEST_PATH1, LA(LABEL4), SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	errno = 0;
	ret = smack_set_self_label(LA(LABEL1));
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Try unmapped labels */

	int expected_ret4[] = { 0, -1,
			       -1, -1,
			       -1, -1 };
	int expected_errno4[] = {    0, EPERM,
				 EPERM, EPERM,
				 EBADR, EPERM };

	errno = 0;
	ret = smack_set_file_label(TEST_PATH1, UNMAPPED, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(errno == expected_errno4[env_id] && ret == expected_ret4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	errno = 0;
	ret = smack_set_self_label(UNMAPPED);
	TEST_CHECK(errno == expected_errno4[env_id] && ret == expected_ret4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	test_sync(1);

	/* check file and process labels */

	test_sync(2);

	/* remove file label */

	int expected_ret5[] = { 0, -1,
			       -1, -1,
				0, -1 };
	int expected_errno5[] = {    0, EPERM,
				 EPERM, EPERM,
				     0, EPERM };

	errno = 0;
	ret = smack_set_file_label(TEST_PATH1, NULL, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(errno == expected_errno5[env_id] && ret == expected_ret5[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
}

/* this function will be executed outside the namespace */
void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	init_test_resources(NULL, test_mappings, test_dirs, test_files);

	test_sync(0);

	/* wait for checks... */
	test_sync(1);

	const char* exp_label[] = {UNMAPPED, LABEL1,
				     LABEL1, LABEL1,
				     LABEL4, LABEL1 };
	ret = smack_get_file_label(TEST_PATH1, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label[env_id]);
		free(label);
	}

	const char* exp_label2[] = {UNMAPPED, INSIDE,
				      INSIDE, INSIDE,
				      LABEL1, INSIDE };
	ret = smack_get_process_label(sibling_pid, &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label2[env_id]);
		free(label);
	}

	test_sync(2);

	// TODO: this test is broken because removing labels in Smack does not
	// work properly right now
#if 0
	const char* exp_label3[] = {   "_", LABEL1,
				    LABEL1, LABEL1,
				       "_", LABEL1 };
	ret = smack_get_file_label(TEST_PATH1, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label3[env_id]);
		free(label);
	}
#endif
}

void test_cleanup(void)
{
}
