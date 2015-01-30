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
 * Smack namespace - test case "CIPSO ambient"
 *
 * This testcase verifies behavior of "ambient" smackfs interfaces.
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
	char *label;

	test_sync(0);

	/*
	 * Verify "ambient" label
	 */
	const char* exp_ambient_label1[] = {"l1", "l1", "n_l1", "n_l1",
					    "l1", "l1", "n_l1", "n_l1"};
	ret = smack_get_ambient(&label);
	TEST_CHECK(ret != -1, "smack_get_ambient(): %s", strerror(errno));
	if (ret != -1) {
		TEST_CHECK(strcmp(label, exp_ambient_label1[env_id]) == 0,
			   "Invalid ambient label. Actual = %s, "
			   "expected = %s", label, exp_ambient_label1[env_id]);
	}

	test_sync(1);

	test_sync(2);

	/*
	 * Verify unmapped label
	 */
	const char* exp_ambient_label2[] = {"@", "@", "?", "?",
					    "@", "@", "?", "?"};
	ret = smack_get_ambient(&label);
	TEST_CHECK(ret != -1, "smack_get_ambient(): %s", strerror(errno));
	if (ret != -1) {
		TEST_CHECK(strcmp(label, exp_ambient_label2[env_id]) == 0,
			   "Invalid ambient label. Actual = %s, "
			   "expected = %s", label, exp_ambient_label2[env_id]);
	}

	/*
	 * Try to set unmapped label
	 */
	int exp_ret1[] = { 0, -1, -1, -1,
			  -1, -1, -1, -1 };
	int exp_errno1[] = {0,     EPERM, EBADR, EPERM,
			    EPERM, EPERM, EPERM, EPERM };
	errno = 0;
	ret = smack_set_ambient("l1");
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_ambient(): %s", strerror(errno));

	/*
	 * Try to set unmapped label
	 */
	int exp_ret2[] = { 0, -1,  0, -1,
			  -1, -1, -1, -1 };
	int exp_errno2[] = {0,     EPERM, 0,     EPERM,
			    EPERM, EPERM, EPERM, EPERM };
	errno = 0;
	ret = smack_set_ambient("n_l1");
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_ambient(): %s", strerror(errno));

	test_sync(3);
}

void main_outside_ns(void)
{
	int ret;

	// allow access to smackfs
	ret = smack_set_rule("inside", "*", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_ambient("l1");
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));

	test_sync(0);

	// check label in NS

	test_sync(1);

	// '@' is unmapped
	ret = smack_set_ambient("@");
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));

	test_sync(2);

	// wait for sibling to exit
	test_sync(3);
}

void test_cleanup(void)
{
	smack_set_ambient("_");
	smack_set_rule("inside", "*", "-");
}
