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
 * Smack namespace - test case "default rules"
 * * smack rules
 * * smackfs interface
 *
 * This test case verifies behavior of Smack namespaces, when default Smack
 * labels ("_", "*", "^", "@") are being mapped.
 * Mapping a default label into an ordinary one (a string) inside a namespace
 * will make default rules connected with the label no longer valid.
 * And vice versa, mapping an ordinary label into default one inside a NS
 * will make the label gaining additional rules.
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

#define INSIDE        INSIDE_PROC_LABEL
#define LABEL1        "label1"
#define LABEL2        "label2"
#define WILL_BE_FLOOR "will_be_floor"
#define WILL_BE_STAR  "will_be_star"
#define WILL_BE_HAT   "will_be_hat"
#define WILL_BE_AT    "will_be_at"


static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE,       "*",          "rw", automatic}, /* 0: allow the namespace access the smackfs */
	{LABEL2,       WILL_BE_FLOOR, "w", manual},    /* 1: scenario 2 */
	{WILL_BE_STAR, LABEL2,        "r", manual},    /* 2: scenario 2 */
	{WILL_BE_HAT,  LABEL2,        "w", manual},    /* 3: scenario 2 */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic}, /* 0 */
	{LABEL2, MAPPED_LABEL_PREFIX LABEL2, automatic}, /* 1 */
	{"*", "star", automatic},            /* 2: allow the namespace access the smackfs */
	{WILL_BE_FLOOR, "_", manual},        /* 3: scenario 1 */
	{WILL_BE_STAR, "*", manual},         /* 4: scenario 1 */
	{WILL_BE_HAT, "^", manual},          /* 5: scenario 1 */
	{WILL_BE_AT, "@", manual},           /* 6: scenario 1 */
	{NULL}
};

void main_inside_ns(void)
{
	int ret;

	test_sync(0);

	/*
	 * Scenario 1:
	 * At this point default rules are unmapped (except '_')
	 */
	if ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK) {
		ret = smack_get_access("_", LA(LABEL1));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "_");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("*", LA(LABEL1));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "*");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", LA(LABEL1));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "^");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("@", LA(LABEL1));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "@");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
	}

	test_sync(1);
	/* mapping default labels occurs here */
	test_sync(2);

	/*
	 * Scenario 2:
	 * At this point default labels are mapped,
	 * so they will behave as expected.
	 */
	if ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK) {
		ret = smack_get_access(LA(LABEL1), "_");
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_EXE),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL2), "_");
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_EXE),
			   "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("*", LA(LABEL1));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("*", "_");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "*");
		TEST_CHECK(ret == ACCESS_FULL,
		           "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", LA(LABEL1));
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_EXE),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "^");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("@", LA(LABEL1));
		TEST_CHECK(ret == ACCESS_FULL,
		           "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL1), "@");
		TEST_CHECK(ret == ACCESS_FULL,
		           "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("@", LA(LABEL2));
		TEST_CHECK(ret == ACCESS_FULL,
		           "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL2), "@");
		TEST_CHECK(ret == ACCESS_FULL,
		           "ret = %d, %s", ret, strerror(errno));
	}

	test_sync(3);
	/* load rules for default labels */
	test_sync(4);

	/*
	 * Scenario 3:
	 * At this point default labels gained additional accesses.
	 */
	if ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK) {
		ret = smack_get_access(LA(LABEL1), "_");
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_EXE),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access(LA(LABEL2), "_");
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_WRITE | ACCESS_EXE),
		           "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("*", LA(LABEL2));
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", LA(LABEL1));
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_EXE),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("^", LA(LABEL2));
		TEST_CHECK(ret == (ACCESS_ANYREAD | ACCESS_WRITE | ACCESS_EXE),
		           "ret = %d, %s", ret, strerror(errno));
	}

	test_sync(5);
}

void main_outside_ns(void)
{
	init_test_resources(test_rules, test_mappings, NULL, NULL);

	test_sync(0);

	/* Scenario 1 */

	test_sync(1);
	if ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK) {
		set_smack_mapping(&test_mappings[3]);
		set_smack_mapping(&test_mappings[4]);
		set_smack_mapping(&test_mappings[5]);
		set_smack_mapping(&test_mappings[6]);
	}
	test_sync(2);

	/* Scenario 2 */

	test_sync(3);
	set_smack_rule(&test_rules[1]);
	set_smack_rule(&test_rules[2]);
	set_smack_rule(&test_rules[3]);
	test_sync(4);

	/* Scenario 3 */

	test_sync(5);
}

void test_cleanup(void)
{
}
