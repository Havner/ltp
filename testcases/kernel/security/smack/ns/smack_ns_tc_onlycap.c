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
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "test_common.h"

#define UNMAPPED "unmapped"
#define OUTSIDE OUTSIDE_PROC_LABEL

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, "*", "rwx", 1},  /* allow access to smackfs */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{"*", "star", 1},  /* allow access to smackfs */
	{NULL}
};

void main_inside_ns(void)
{
	int ret;
	char* label = NULL;

	test_sync(0);

	const int expected_ret[] = { 0, -1,
				    -1, -1,
				    -1, -1 };
	ret = smack_set_onlycap(LA(OUTSIDE));
	TEST_CHECK(ret == expected_ret[env_id], strerror(errno));

	test_sync(1);

	/* Set the onlycap, but this time properly, outside */

	test_sync(2);

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(OUTSIDE));
		free(label);
	}

	test_sync(3);

	/* Set onlycap to unmapped label */

	test_sync(4);

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(UNMAPPED, "?"));
		free(label);
	}

	/*
	 * try to change onlycap label
	 */
	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == -1 && errno == EPERM, "ret = %d, errno = %d: %s",
		   ret, errno, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(UNMAPPED, "?"));
		free(label);
	}

	test_sync(5);
}

void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	test_sync(0);

	/* Try to change onlycap to outside from the namespace */

	test_sync(1);

	const char *expexted_label[] = {OUTSIDE, "",
					     "", "",
					     "", "" };
	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, expexted_label[env_id]);
		free(label);
	}

	ret = smack_set_onlycap(OUTSIDE);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, OUTSIDE);
		free(label);
	}

	test_sync(2);

	/* check in namespace */

	test_sync(3);

	/*
	 * set onlycap label to unmapped label in the namespace
	 */

	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_self_label(UNMAPPED);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_set_onlycap(UNMAPPED);
	TEST_CHECK(ret == 0, strerror(errno));

	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, UNMAPPED);
		free(label);
	}

	test_sync(4);

	/* check in namespace */

	test_sync(5);
}

void test_cleanup(void)
{
	int ret;
	char* label = NULL;

	/* reset "onlycap" label */
	ret = smack_set_onlycap("-");
	TEST_CHECK(ret == 0, strerror(errno));

	/*
	 * Make sure we did, if not, the system might become
	 * impossible to administer.
	 */
	ret = smack_get_onlycap(&label);
	TEST_CHECK(ret == 0, strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, "");
		free(label);
	}
}
