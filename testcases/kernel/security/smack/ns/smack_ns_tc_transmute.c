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

#define LABEL1 "label1"

#define TRANSMUTE_DIR  "tmp/transmute"
#define TRANSMUTE_FILE "tmp/transmute/a"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, LABEL1, "rwxt", automatic},
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL1, MAPPED_LABEL_PREFIX LABEL1, automatic},
	{NULL}
};

static const struct test_dir_desc test_dirs[] = {
	{TRANSMUTE_DIR, 0777, LABEL1, transmute},
	{NULL}
};

void main_inside_ns(void)
{
	int ret;
	char *label = NULL;

	test_sync(0);

	ret = smack_get_file_label(TRANSMUTE_DIR, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL1));
		free(label);
	}

	ret = smack_get_file_label(TRANSMUTE_DIR, &label, SMACK_LABEL_TRANSMUTE, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, "TRUE");
		free(label);
	}

	/*
	 * Create a file in transmuting directory. The file should get
	 * access label of the directory.
	 */
	ret = file_create(TRANSMUTE_FILE, 0444, -1, -1, regular, NULL, NULL, NULL);
	TEST_CHECK(ret == 0, "file_create(): %s", strerror(errno));

	ret = smack_get_file_label(TRANSMUTE_FILE, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LA(LABEL1));
		free(label);
	}

	test_sync(1);
}

void main_outside_ns(void)
{
	int ret;
	char* label = NULL;

	init_test_resources(test_rules, test_mappings, test_dirs, NULL);

	test_sync(0);

	/* wait for checks */

	test_sync(1);

	ret = smack_get_file_label(TRANSMUTE_FILE, &label, SMACK_LABEL_ACCESS, 0);
	TEST_CHECK(ret == 0, "smack_get_file_label(): %s", strerror(errno));
	if (ret == 0) {
		TEST_LABEL(label, LABEL1);
		free(label);
	}
}

void test_cleanup(void)
{
	remove(TRANSMUTE_FILE);
}
