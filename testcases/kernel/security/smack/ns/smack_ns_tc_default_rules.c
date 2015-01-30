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

	test_sync(0);

	/*
	 * Scenario 1:
	 * At this point default rules are unmapped (except '_')
	 */
	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_get_access("*", "n_l1");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("n_l1", "*");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", "n_l1");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("n_l1", "^");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("@", "n_l1");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("n_l1", "@");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
	}

	test_sync(1);
	// mapping default labels occurs here
	test_sync(2);

	/*
	 * Scenario 2:
	 * At this point default labels are mapped,
	 * so they will behave as expected.
	 */
	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_get_access("*", "n_l1");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("*", "_");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", "n_l1");
		TEST_CHECK(ret == (ACCESS_READ | ACCESS_EXE | ACCESS_LOCK),
			   "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("n_l1", "_");
		TEST_CHECK(ret == (ACCESS_READ | ACCESS_EXE | ACCESS_LOCK),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("n_l2", "_");
		TEST_CHECK(ret == (ACCESS_READ | ACCESS_EXE | ACCESS_LOCK),
			   "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("n_l1", "*");
		TEST_CHECK(
			ret == (ACCESS_READ | ACCESS_WRITE | ACCESS_EXE
				| ACCESS_APPEND | ACCESS_TRANS | ACCESS_LOCK
				| ACCESS_BRINGUP),
			"ret = %d, %s", ret, strerror(errno));
	}

	test_sync(3);
	// load rules for default labels
	test_sync(4);

	/*
	 * Scenario 3:
	 * At this point default labels gained additional accesses.
	 */
	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_get_access("*", "n_l2");
		TEST_CHECK(ret == 0, "ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("^", "n_l1");
		TEST_CHECK(ret == (ACCESS_READ | ACCESS_EXE | ACCESS_LOCK),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("^", "n_l2");
		TEST_CHECK(
			ret == (ACCESS_READ | ACCESS_WRITE | ACCESS_EXE
				| ACCESS_LOCK),
			"ret = %d, %s", ret, strerror(errno));

		ret = smack_get_access("n_l1", "_");
		TEST_CHECK(ret == (ACCESS_READ | ACCESS_EXE | ACCESS_LOCK),
			   "ret = %d, %s", ret, strerror(errno));
		ret = smack_get_access("n_l2", "_");
		TEST_CHECK(
			ret == (ACCESS_READ | ACCESS_WRITE | ACCESS_EXE
				| ACCESS_LOCK),
			"ret = %d, %s", ret, strerror(errno));
	}

	test_sync(5);
}

void main_outside_ns(void)
{
	int ret;

	// allow access to smackfs
	ret = smack_set_rule("inside", "*", "rwx");
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(0);

	// Scenario 1

	test_sync(1);
	if (env_id & TEST_ENV_SMACK_NS) {
		ret = smack_map_label(sibling_pid, "will_be_star", "*");
		TEST_CHECK(ret == 0, strerror(errno));
		ret = smack_map_label(sibling_pid, "will_be_hat", "^");
		TEST_CHECK(ret == 0, strerror(errno));
		ret = smack_map_label(sibling_pid, "will_be_at", "@");
		TEST_CHECK(ret == 0, strerror(errno));
	}
	test_sync(2);

	// Scenario 2

	test_sync(3);
	ret = smack_set_rule("l2", "will_be_floor", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("will_be_star", "l2", "r");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("will_be_hat", "l2", "w");
	TEST_CHECK(ret == 0, strerror(errno));
	test_sync(4);

	// Scenario 3

	test_sync(5);
}

void test_cleanup(void)
{
	int ret;

	ret = smack_set_rule("l2", "will_be_floor", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("will_be_star", "l2", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("will_be_hat", "l2", "-");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_rule("inside", "*", "-");
	TEST_CHECK(ret == 0, strerror(errno));
}
