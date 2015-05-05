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
 * Smack namespace - test case "rules"
 * * Smack rules
 * * smackfs interface
 *
 * This test case verifies Smack rules manipulations with Smack namespaces.
 * It covers:
 * * setting rules inside / outside namespace
 * * checking access after the rules are applies
 * * usage of unmapped labels
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

#define UNMAPPED1 "unmapped1"
#define UNMAPPED2 "unmapped2"
#define LABEL0 "label0"
#define LABEL1 "label1"
#define LABEL2 "label2"
#define LABEL3 "label3"
#define LABEL4 "label4"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, "*", "rwx", automatic},   /* allow the namespace access the smackfs */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL0, MAPPED_LABEL_PREFIX LABEL0, automatic},
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic},
	{LABEL3, MAPPED_LABEL_PREFIX LABEL3, automatic},
	{LABEL4, MAPPED_LABEL_PREFIX LABEL4, automatic},
	{"*", "star", automatic},   /* allow the namespace access the smackfs */
	{"will_be_floor", "_", automatic},
	{"will_be_star", "*", automatic},
	{NULL}
};

void main_inside_ns(void)
{
	int ret;

	test_sync(0);

	/*
	 * Unmapped labels
	 */
	int expected_ret[] = { 0, -1,
			      -1, -1,
			      -1, -1 };
	int expected_errno[] = {    0, EPERM,
				EPERM, EPERM,
				EPERM, EPERM };

	errno = 0;
	ret = smack_set_rule(UNMAPPED1, UNMAPPED2, "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_set_rule(LA(LABEL1), UNMAPPED1, "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_set_rule(UNMAPPED2, LA(LABEL1), "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	/*
	 * Mapped labels
	 */
	int expected_ret2[] = { 0, -1,
			       -1, -1,
			       -1, -1 };
	int expected_errno2[] = {    0, EPERM,
				 EPERM, EPERM,
				 EPERM, EPERM };
	errno = 0;
	ret = smack_set_rule(LA(LABEL1), LA(LABEL2), "rwx");
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	int expected_ret3[] = { 0, -1,
			       -1, -1,
			       -1, -1 };
	int expected_errno3[] = {    0, EPERM,
				 EPERM, EPERM,
				 EPERM, EPERM };
	errno = 0;
	ret = smack_revoke_subject(LA(LABEL1));
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_revoke_subject(UNMAPPED1);
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_revoke_subject(UNMAPPED2);
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	int expected_access0[] = {0, 0,
				  0, 0,
				  0, 0};
	ret = smack_have_access(LA(LABEL0), LA(LABEL1), "r");
	TEST_CHECK(ret == expected_access0[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Unmapped labels access check
	 */
	int expected_access1[] = {1, 1,
				  1, 1,
				  0, 0};
	int expected_access2[] = {0, 0,
				  0, 0,
				  0, 0};
	ret = smack_have_access(UNMAPPED1, "_", "rx");
	TEST_CHECK(ret == expected_access1[env_id], "ret = %d, %s", ret, strerror(errno));
	ret = smack_have_access(UNMAPPED1, "_", "wlt");
	TEST_CHECK(ret == expected_access2[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Mapped labels access check
	 */
	int expected_access3[] = {1, 1,
				  1, 1,
				  1, 1};
	int expected_access4[] = {0, 0,
				  0, 0,
				  0, 0};
	ret = smack_have_access(LA(LABEL1), "_", "rx");
	TEST_CHECK(ret == expected_access3[env_id], "ret = %d, %s", ret, strerror(errno));
	ret = smack_have_access(LA(LABEL1), "_", "wlt");
	TEST_CHECK(ret == expected_access4[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Invalid label test
	 */
	int expected_ret4[] = {-1, -1,
			       -1, -1,
			       -1, -1};
	int expected_errno4[] = {EINVAL, EPERM,
				 EPERM,  EPERM,
				 EPERM,  EPERM};
	errno = 0;
	ret = smack_set_rule("-", "_", "rwx");
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	test_sync(1);

	/* Add l3/l4 rules */

	test_sync(2);

	int expected_access5[] = {1, 1,
				  1, 1,
				  1, 1};
	ret = smack_have_access(LA(LABEL3), LA(LABEL4), "rwx");
	TEST_CHECK(ret == expected_access5[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Revoke subject test
	 */
	int expected_ret5[] = { 0, -1,
			       -1, -1,
			       -1, -1 };
	int expected_errno5[] = {    0, EPERM,
				 EPERM, EPERM,
				 EPERM, EPERM };
	errno = 0;
	ret = smack_revoke_subject(LA(LABEL3));
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	ret = smack_revoke_subject(UNMAPPED1);
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	int expected_access6[] = {0, 1,
				  1, 1,
				  1, 1};
	ret = smack_have_access(LA(LABEL3), LA(LABEL4), "rwx");
	TEST_CHECK(ret == expected_access6[env_id], "ret = %d, %s", ret, strerror(errno));

	test_sync(3);
}

void main_outside_ns(void)
{
	int ret;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	test_sync(0);

	/* Perform initial tests */

	test_sync(1);

	ret = smack_set_rule(LABEL3, LABEL4, "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(2);

	/* l3/l4 tests */

	test_sync(3);
}

void test_cleanup(void)
{
	int ret;

	ret = smack_set_rule(UNMAPPED1, UNMAPPED2, "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule(LABEL3, LABEL4, "-");
	TEST_CHECK(ret == 0, strerror(errno));
}
