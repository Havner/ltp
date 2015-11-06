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
#define LABELA         "label_allowed"
#define UNMAPPED       "unmapped"
#define INSIDE         INSIDE_PROC_LABEL

static const struct test_smack_rule_desc test_rules[] = {
	/* allow the relabeled namespace process to access the smackfs */
	{INSIDE, "*", "rwx", automatic},
	{LABEL1, "*", "rwx", automatic},
	{LABELA, "*", "rwx", automatic},
	{LABEL1, "_", "rwx", automatic},
	{LABELA, "_", "rwx", automatic},
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{LABEL3, MAPPED_LABEL_PREFIX LABEL3, automatic},
	{LABELA, MAPPED_LABEL_PREFIX LABELA, automatic},
	{"*", "star", automatic},
	{NULL}
};


void main_inside_ns(void)
{
	int ret;
	char *label = NULL;

	test_sync(0);

	/* Verify process label */

	ret = smack_get_process_label(getpid(), &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(INSIDE));
		free(label);
	}


	/* Check if we can modify self label */

	int expected_ret1[] = { 0, -1,
			       -1, -1,
				0, -1 };
	int expected_errno1[] = {    0, EPERM,
				 EPERM, EPERM,
				     0, EPERM };

	errno = 0;
	ret = smack_set_self_label(LA(LABEL1));
	TEST_CHECK(ret == expected_ret1[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Try add unmapped label to the relabel-self */

	int expected_ret2[] = { 0, -1,
			       -1, -1,
			       -1, -1 };
	int expected_errno2[] = {    0, EPERM,
				 EPERM, EPERM,
				 EBADR, EPERM };

	errno = 0;
	ret = smack_set_relabel_self(UNMAPPED);
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Try to read that */

	const char* exp_label1[] = { UNMAPPED, "",
					   "", "",
					   "", "" };

	errno = 0;
	ret = smack_get_relabel_self(&label);
	TEST_CHECK(ret == 0 && "smack_get_relabel_self(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label1[env_id]);
		free(label);
	}


	/* Try to add normal labels to the relabel-self */

	int expected_ret3[] = { 0, -1,
			       -1, -1,
				0, -1 };
	int expected_errno3[] = {    0, EPERM,
				 EPERM, EPERM,
				     0, EPERM };

	errno = 0;
	ret = smack_set_relabel_self(LA(LABELA));
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Try to read that */

	const char* exp_label2[] = { LA(LABELA), "",
					     "", "",
				     LA(LABELA), "" };

	errno = 0;
	ret = smack_get_relabel_self(&label);
	TEST_CHECK(ret == 0 && "smack_get_relabel_self(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label2[env_id]);
		free(label);
	}


	/* Change our ID to non-root user */

	int expected_ret4[] = { 0, -1,
			        0, -1,
				0, -1 };
	int expected_errno4[] = { 0, EPERM,
				  0, EPERM,
				  0, EPERM };
	errno = 0;
	ret = setuid(NON_ROOT_ID);
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	/* Try to read relabel-self again */

	errno = 0;
	ret = smack_get_relabel_self(&label);
	TEST_CHECK(ret == 0 && "smack_get_relabel_self(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label2[env_id]);
		free(label);
	}


	/* Now that we lost caps lets try to change self label to something not in relabel-self */

	int expected_ret5[] = { -1, -1,
				-1, -1,
				-1, -1 };
	int expected_errno5[] = { EPERM, EPERM,
				  EPERM, EPERM,
				  EPERM, EPERM };

	errno = 0;
	ret = smack_set_self_label(LA(LABEL2));
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Now that we lost caps lets try to change self label to something in relabel-self */

	errno = 0;
	ret = smack_set_self_label(LA(LABELA));
	TEST_CHECK(ret == expected_ret1[env_id] && errno == expected_errno1[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	/* Try to read relabel-self again, it should be empty now */

	errno = 0;
	ret = smack_get_relabel_self(&label);
	TEST_CHECK(ret == 0 && "smack_get_relabel_self(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, "");
		free(label);
	}


	/* Try to add normal labels to the relabel-self again */

	int expected_ret6[] = { -1, -1,
				-1, -1,
				-1, -1 };
	int expected_errno6[] = { EPERM, EPERM,
				  EPERM, EPERM,
				  EPERM, EPERM };

	errno = 0;
	ret = smack_set_relabel_self(LA(LABEL3));
	TEST_CHECK(ret == expected_ret6[env_id] && errno == expected_errno6[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	test_sync(1);
}

void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	test_sync(0);

	/* wait for checks... */
	test_sync(1);

	const char* exp_label1[] = { LABELA, INSIDE,
				     INSIDE, INSIDE,
				     LABELA, INSIDE };
	ret = smack_get_process_label(sibling_pid, &label);
	TEST_CHECK(ret == 0, "smack_get_process_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, exp_label1[env_id]);
		free(label);
	}
}

void test_cleanup(void)
{
}
