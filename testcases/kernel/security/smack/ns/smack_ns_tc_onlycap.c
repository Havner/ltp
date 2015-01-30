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
 * Smack namespace - test case "onlycap" - tests /smack/onlycap interface
 *
 * This test case verifies Smack's onlycap interface when used inside and outside
 * a Smack namespace.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "test_common.h"

static const struct test_smack_rule_desc test_rules[] =
{
	{"inside", "*", "rwx"}, // allow access to smackfs
	{NULL, NULL, NULL}
};

void main_inside_ns(void)
{
	int ret;
	char* label = NULL;

	test_sync(0);

	const char* expected_label1[] = {"outside", "outside", "n_outside", "n_outside",
					 "outside", "outside", "n_outside", "n_outside" };
	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(safe_strcmp(label, expected_label1[env_id]) == 0,
		   strerror(errno));
	free(label);

	test_sync(1);



	test_sync(2);

	const char* expected_label2[] = {"unmapped", "unmapped", "?", "?",
					 "unmapped", "unmapped", "?", "?" };
	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(safe_strcmp(label, expected_label2[env_id]) == 0,
		   strerror(errno));
	free(label);

	/*
	 * try to change onlycap label
	 */
	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == -1 && errno == EPERM, "ret = %d, errno = %d: %s",
		   ret, errno, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(safe_strcmp(label, expected_label2[env_id]) == 0,
		   strerror(errno));
	free(label);

	test_sync(3);
}

void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	init_test_resources(test_rules, NULL, NULL, NULL);

	ret = smack_set_onlycap("outside");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(safe_strcmp(label, "outside") == 0,
	           strerror(errno));
	free(label);

	test_sync(0);
	// check in namespace
	test_sync(1);

	/*
	 * set onlycap label to unmapped label in the namespace
	 */

	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_self_label("unmapped");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_onlycap("unmapped");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(safe_strcmp(label, "unmapped") == 0, strerror(errno));
	free(label);

	test_sync(2);
	// check in namespace
	test_sync(3);
}

void test_cleanup(void)
{
	int ret;
	char* label = NULL;

	// reset "onlycap" label
	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_CHECK(strcmp(label, "") == 0, strerror(errno));
	free(label);
}
