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
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "test.h"
#include "usctest.h"
#include "safe_macros.h"
#include "safe_file_ops.h"
#include "smack_common.h"

char *TCID = "smack_rule_simple";
int TST_TOTAL = 1;

// TODO: "l" and "b" access modes testing
static const size_t accesses_num = 5;
static const char* smack_accesses[] = {"r", "w", "x", "a", "t"};

#define LABEL1 "label1"
#define LABEL2 "label2"
#define LABEL3 "label3"

static inline void test_access(const char* subject, const char* object,
			       const char* expected_access, const char* file,
			       int line)
{
	size_t i, j;
	int ret, expected;

	for (i = 0; i < accesses_num; ++i) {
		expected = 0;
		for (j = 0; j < strlen(expected_access); ++j)
			if (expected_access[j] == smack_accesses[i][0]) {
				expected = 1;
				break;
			}

		ret = smack_have_access(subject, object, smack_accesses[i]);
		if (ret < 0)
			tst_resm(TFAIL, "smack_have_access failed at %s:%d, "
				 "sub = %s, obj = %s, access = %s",
				 file, line, subject, object,
				 smack_accesses[i]);
		if (ret != expected)
			tst_resm(
				TFAIL, "smack_have_access returned %d "
				"(should be %d) at %s:%d , sub = %s, obj = %s, "
				"access = %s",
				ret, expected, file, line, subject, object,
				smack_accesses[i]);
	}
}

#define TEST_ACCESS(subject, object, expected_access) \
	test_access(subject, object, expected_access, __FILE__, __LINE__)

void test_revoke_subject(void)
{
	int fd;

	// load test rules
	if (smack_set_rule(LABEL1, LABEL1, "-") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	if (smack_set_rule(LABEL1, LABEL2, "r") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	if (smack_set_rule(LABEL1, LABEL3, "w") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	if (smack_set_rule(LABEL2, LABEL1, "x") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	if (smack_set_rule(LABEL2, LABEL2, "a") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	if (smack_set_rule(LABEL2, LABEL3, "t") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");

	TEST_ACCESS(LABEL1, LABEL1, "rwxat");
	TEST_ACCESS(LABEL1, LABEL2, "r");
	TEST_ACCESS(LABEL1, LABEL3, "w");
	TEST_ACCESS(LABEL2, LABEL1, "x");
	TEST_ACCESS(LABEL2, LABEL2, "rwxat");
	TEST_ACCESS(LABEL2, LABEL3, "t");

	// revoke subject LABEL1
	fd = open(SMACK_MNT_PATH "revoke-subject", O_WRONLY);
	if (fd < 0)
		tst_resm(TFAIL, "Failed to open revoke-subject file, "
			 "errno = %s",
			 strerror(errno));
	else {
		const char* subject = LABEL1;
		if (write(fd, subject, strlen(subject)) < 0)
			tst_resm(TFAIL, "Write to revoke-subject file failed, "
				 "errno = %s",
				 strerror(errno));
		close(fd);
	}

	TEST_ACCESS(LABEL1, LABEL1, "rwxat");
	TEST_ACCESS(LABEL1, LABEL2, "");
	TEST_ACCESS(LABEL1, LABEL3, "");
	TEST_ACCESS(LABEL2, LABEL1, "x");
	TEST_ACCESS(LABEL2, LABEL2, "rwxat");
	TEST_ACCESS(LABEL2, LABEL3, "t");

	// cleanup
	smack_set_rule(LABEL1, LABEL1, "-");
	smack_set_rule(LABEL1, LABEL2, "-");
	smack_set_rule(LABEL1, LABEL3, "-");
	smack_set_rule(LABEL2, LABEL1, "-");
	smack_set_rule(LABEL2, LABEL2, "-");
	smack_set_rule(LABEL2, LABEL3, "-");
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	size_t i;

	tst_require_root(NULL);
	if (verify_smackmnt() != 0)
		tst_brkm(TCONF, NULL, "Smack is not enabled");

	/*
	 *"Any access requested by a task labeled "*" is denied."
	 */
	TEST_ACCESS("*", LABEL1, "");
	TEST_ACCESS("*", "*", "");

	/*
	 * "A read or execute access requested by a task labeled "^" is
	 * permitted."
	 */
	TEST_ACCESS("^", LABEL1, "rx");

	/*
	 * "A read or execute access requested on an object labeled "_" is
	 * permitted."
	 */
	TEST_ACCESS(LABEL1, "_", "rx");

	/*
	 * "Any access requested on an object labeled "*" is permitted."
	 */
	TEST_ACCESS(LABEL1, "*", "rwxatl");

	/*
	 * "Any access requested by a task on an object with the same label is
	 * permitted."
	 */
	TEST_ACCESS(LABEL1, LABEL1, "rwxatl");
	TEST_ACCESS(LABEL2, LABEL2, "rwxatl");

	/*
	 * "Any access requested that is explicitly defined in the loaded rule
	 * set is permitted."
	 */
	if (smack_set_rule(LABEL1, LABEL2, "-") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");
	TEST_ACCESS(LABEL1, LABEL2, "");
	TEST_ACCESS(LABEL2, LABEL1, "");

	for (i = 0; i < accesses_num; ++i) {
		const char* access = smack_accesses[i];
		tst_resm(TINFO, "access = %s", access);
		if (smack_set_rule(LABEL1, LABEL2, access) < 0)
			tst_resm(TFAIL, "smack_set_rule() failed");

		TEST_ACCESS(LABEL1, LABEL2, access);
		TEST_ACCESS(LABEL2, LABEL1, "");
	}

	// cleanup
	if (smack_set_rule(LABEL1, LABEL2, "-") < 0)
		tst_resm(TFAIL, "smack_set_rule() failed");

	// "Any other access is denied."
	TEST_ACCESS(LABEL1, LABEL2, "");
	TEST_ACCESS(LABEL2, LABEL1, "");

	test_revoke_subject();

	tst_exit();
}
