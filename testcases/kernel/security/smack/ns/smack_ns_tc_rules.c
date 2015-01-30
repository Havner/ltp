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
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include "test_common.h"

void main_inside_ns(void)
{
	int ret;

	// set mapping ad rules

	test_sync(0);

	/*
	 * Unmapped labels
	 */
	int expected_ret[] = { 0, -1, -1, -1,	// UID = 0
			      -1, -1, -1, -1 };	// UID = 1000
	int expected_errno[] = {    0, EPERM, EBADR, EPERM,   // UID = 0
				EPERM, EPERM, EPERM, EPERM }; // UID = 1000

	errno = 0;
	ret = smack_set_rule("unmapped", "unmapped2", "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_set_rule("n_l1", "unmapped", "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	ret = smack_set_rule("unmapped2", "n_l1", "rwx");
	TEST_CHECK(ret == expected_ret[env_id] && errno == expected_errno[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	/*
	 * Mapped labels
	 */
	int expected_ret2[] = { 0, -1,  0, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno2[] = {    0, EPERM,     0, EPERM,   // UID = 0
				 EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = smack_set_rule("n_l1", "n_l2", "rwx");
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));


	int expected_ret3[] = { 0, -1,  0, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno3[] = {    0, EPERM, 0,     EPERM,   // UID = 0
				 EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = smack_revoke_subject("n_l1");
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	test_sync(1);

	/*
	 * TODO:
	 * Naming this arrays by expected_access*, etc. is not so convenient.
	 * Adding a new check before existing ones distorts the order.
	 */
	int expected_access6[] = {0, 0, 0, 0,   // UID = 0
				  0, 0, 0, 0 }; // UID = 1000
	ret = smack_have_access("n_l0", "n_l1", "r");
	TEST_CHECK(ret == expected_access6[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Unmapped labels access check
	 */
	int expected_access0[] = {1, 1, 0, 0,	// UID = 0
				  1, 1, 0, 0};	// UID = 1000
	int expected_access1[] = {0, 0, 0, 0,
				  0, 0, 0, 0 };
	ret = smack_have_access("l1", "_", "rx");
	TEST_CHECK(ret == expected_access0[env_id], "ret = %d, %s", ret, strerror(errno));
	ret = smack_have_access("l1", "_", "wlt");
	TEST_CHECK(ret == expected_access1[env_id], "ret = %d, %s", ret, strerror(errno));

	/*
	 * Mapped labels access check
	 */
	int expected_access2[] = {1, 1, 1, 1,   // UID = 0
				  1, 1, 1, 1 }; // UID = 1000
	int expected_access5[] = {0, 0, 0, 0,
				  0, 0, 0, 0};
	ret = smack_have_access("n_l1", "_", "rx");
	TEST_CHECK(ret == expected_access2[env_id], "ret = %d, %s", ret, strerror(errno));
	ret = smack_have_access("n_l1", "_", "wlt");
	TEST_CHECK(ret == expected_access5[env_id], "ret = %d, %s", ret, strerror(errno));

	int expected_access3[] = {0, 0, 1, 1,   // UID = 0
				  0, 0, 1, 1 }; // UID = 1000
	ret = smack_have_access("n_l3", "n_l4", "rwx");
	TEST_CHECK(ret == expected_access3[env_id], "ret = %d, %s", ret, strerror(errno));


	/*
	 * Revoke subject test
	 */
	int expected_ret4[] = { 0, -1,  0, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno4[] = {    0, EPERM,	   0, EPERM,   // UID = 0
				 EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = smack_revoke_subject("n_l3");
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	ret = smack_revoke_subject("unmapped");
	TEST_CHECK(ret == expected_ret4[env_id], strerror(errno));

	int expected_access4[] = {0, 0, 0, 1,   // UID = 0
				  0, 0, 1, 1 }; // UID = 1000
	ret = smack_have_access("n_l3", "n_l4", "rwx");
	TEST_CHECK(ret == expected_access4[env_id], "ret = %d, %s", ret, strerror(errno));


	/*
	 * Invalid label test
	 */
	int expected_ret5[] = {-1, -1, -1, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno5[] = {EINVAL, EPERM, EINVAL, EPERM,   // UID = 0
				 EPERM,  EPERM,  EPERM, EPERM}; // UID = 1000
	errno = 0;
	ret = smack_set_rule("-", "_", "rwx");
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));

	test_sync(2);
}

void main_outside_ns(void)
{
	int ret;

	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_map_label(sibling_pid, "will_be_star", "*");
		TEST_CHECK(ret == 0, strerror(errno));
	}

	// allow access to smackfs
	ret = smack_set_rule(INSIDE_NS_PROC_LABEL, "*", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));
	save_smackfs_permissions();

	test_sync(0);

	// TODO: make sure, that there are no other rules
	ret = smack_set_rule("l3", "l4", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(1);

	// TODO: chage rules

	test_sync(2);
}

void test_cleanup(void)
{
	int ret;

	ret = smack_set_rule("unmapped", "unmapped2", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "unmapped", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("unmapped2", "l1", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("unmapped2", "n_l1", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l1", "l2", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("l3", "l4", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("_", "*", "-");
	TEST_CHECK(ret == 0, strerror(errno));

	restore_smackfs_permissions();
}
