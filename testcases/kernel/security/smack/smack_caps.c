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
#include <sys/capability.h>

#include "test.h"
#include "usctest.h"
#include "tst_safe_macros.h"
#include "smack_common.h"

#define CLEANUP cleanup
const char *TCID = "smack_caps";
int TST_TOTAL = 1;

#define TEST_FILE_PATH "test_file1"
#define LABEL1 "label1"
#define LABEL2 "label2"

static void cleanup(void)
{
	tst_rmdir();
}

static void setup(void)
{
	tst_tmpdir();
	int fd = SAFE_OPEN(cleanup, TEST_FILE_PATH, O_CREAT | O_RDWR, 0666);
	close(fd);
}

int main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);

	int i;
	cap_t caps;
	cap_value_t cap;

	int label_types[] = {SMACK_LABEL_ACCESS, SMACK_LABEL_EXEC,
			     SMACK_LABEL_MMAP, SMACK_LABEL_TRANSMUTE};
	const char* label_values[] = {LABEL1, LABEL1, LABEL1, "TRUE"};

	tst_require_root();
	if (verify_smackmnt() != 0)
		tst_brkm(TCONF, NULL, "Smack is not enabled");
	setup();

	/* set label for a file and current process */
	if (smack_set_file_label(TEST_FILE_PATH, LABEL2, SMACK_LABEL_ACCESS, 0)
	    < 0)
		tst_resm(TFAIL, "smack_set_file_label() failed");
	if (smack_set_self_label(LABEL2) < 0)
		tst_resm(TFAIL, "smack_set_self_label() failed");

	/* get current capabilities set */
	caps = cap_get_proc();
	if (caps == NULL) {
		tst_brkm(TFAIL, NULL, "cap_get_proc() failed");
	}

	/* drop CAP_MAC_ADMIN */
	cap = CAP_MAC_ADMIN;
	cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, CAP_CLEAR);
	if (cap_set_proc(caps))
		tst_resm(TFAIL, "cap_set_proc() failed");

	/* changing a file labels should fail */
	for (i = 0; i < 4; ++i) {
		if (smack_set_file_label(TEST_FILE_PATH, label_values[i],
					 label_types[i], 0) == 0)
			tst_resm(TFAIL, "smack_set_file_label() should fail");
		else if (errno != EPERM)
			tst_resm(TFAIL, "smack_set_file_label() returned errno"
				 " = %s", strerror(errno));
	}

	/* changing current process label should fail */
	if (smack_set_self_label(LABEL1) == 0)
		tst_resm(TFAIL, "smack_set_self_label() should fail");
	else if (errno != EPERM)
		tst_resm(TFAIL, "smack_set_self_label() returned errno = %s",
			 strerror(errno));

	cap_free(caps);

	cleanup();
	tst_exit();
}
