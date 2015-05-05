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
 * Smack namespace - test case "ambient"
 *
 * This testcase verifies behavior of "ambient" smackfs interfaces.
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

#define LABEL    "label"
#define UNMAPPED "unmapped"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, "*", "rwx", automatic},    /* allow access to smackfs */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{"*", "star", automatic},    /* allow access to smackfs */
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{NULL}
};


void main_inside_ns(void)
{
	int ret;
	char *label;

	test_sync(0);

	/*
	 * Verify "ambient" label
	 */
	ret = smack_get_ambient(&label);
	TEST_CHECK(ret == 0, "smack_get_ambient(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL));
		free(label);
	}

	test_sync(1);

	/* change ambient label */

	test_sync(2);

	/*
	 * Verify unmapped label
	 */
	ret = smack_get_ambient(&label);
	TEST_CHECK(ret == 0, "smack_get_ambient(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LM(UNMAPPED, "?"));
		free(label);
	}

	/*
	 * Try to set mapped label
	 */
	int exp_ret1[] = { 0, -1,
			  -1, -1,
			  -1, -1 };
	int exp_errno1[] = {0,     EPERM,
			    EPERM, EPERM,
			    EPERM, EPERM };
	errno = 0;
	ret = smack_set_ambient(LA(LABEL));
	TEST_CHECK(ret == exp_ret1[env_id] && errno == exp_errno1[env_id],
		   "smack_set_ambient(): %s", strerror(errno));

	/*
	 * Try to set unmapped label
	 */
	int exp_ret2[] = { 0, -1,
			  -1, -1,
			  -1, -1 };
	int exp_errno2[] = {0,     EPERM,
			    EPERM, EPERM,
			    EPERM, EPERM };
	errno = 0;
	ret = smack_set_ambient(UNMAPPED);
	TEST_CHECK(ret == exp_ret2[env_id] && errno == exp_errno2[env_id],
		   "smack_set_ambient(): %s", strerror(errno));

	test_sync(3);
}

void main_outside_ns(void)
{
	int ret;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	ret = smack_set_ambient(LABEL);
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));

	test_sync(0);

	/* check label in NS */

	test_sync(1);

	ret = smack_set_ambient(UNMAPPED);
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));

	test_sync(2);

	/* check label in NS and wait for sibling to exit */

	test_sync(3);
}

void test_cleanup(void)
{
}
