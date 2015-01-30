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
 * Smack namespace - test case "transmute"
 *
 * This test case check Smack's transmute access mode with cooperation with
 * Smack namespaces.
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

static const char* transmute_dir = "tmp/transmute";
static const char* transmute_file = "tmp/transmute/a";

void main_inside_ns(void)
{
	int ret;
	char *label = NULL;
	int lsm_ns = env_id & TEST_ENV_SMACK_NS;

	test_sync(0);

	ret = smack_get_file_label(transmute_dir, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_l1" : "l1");

	ret = smack_get_file_label(transmute_dir, &label, SMACK_LABEL_TRANSMUTE, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	TEST_LABEL(label, "TRUE");

	/*
	 * Create a file in transmuting directory. The file should get
	 * access label of the directory.
	 */
	ret = create_file(transmute_file, 0444);
	TEST_CHECK(ret == 0, "create_file(): %s", strerror(errno));

	ret = smack_get_file_label(transmute_file, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	TEST_LABEL(label, lsm_ns ? "n_l1" : "l1");

	ret = smack_get_file_label(transmute_file, &label, SMACK_LABEL_EXEC, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	TEST_LABEL(label, lsm_ns ? "?" : "unmapped");

	test_sync(1);
}

void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	// TODO: use init_test_resources
	TEST_CHECK(smack_set_rule("inside", "l1", "rwxt") == 0, strerror(errno));
	mkdir(transmute_dir, S_IRWXU | S_IRWXG | S_IRWXO);

	ret = smack_set_file_label(transmute_dir, "l1", SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_set_file_label(transmute_dir, "TRUE", SMACK_LABEL_TRANSMUTE, 0);
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(0);

	// wait for checks...
	test_sync(1);

	ret = smack_get_file_label(transmute_file, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	TEST_LABEL(label, "l1");
}

void test_cleanup(void)
{
	remove(transmute_file);
	remove(transmute_dir);
	TEST_CHECK(smack_set_rule("inside", "l1", "-") == 0, strerror(errno));
}
