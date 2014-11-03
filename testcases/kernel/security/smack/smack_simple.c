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

#define CLEANUP cleanup

char *TCID = "smack_simple";
int TST_TOTAL = 1;

#define TEST_FILE_PATH "test_file1"
#define PROC_LABEL1 "process_label"

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
	(void)argc;
	(void)argv;

	int i;
	char* ret = NULL;
	int label_types[] = {SMACK_LABEL_ACCESS, SMACK_LABEL_EXEC,
			     SMACK_LABEL_MMAP, SMACK_LABEL_TRANSMUTE};
	const char* label_values[] = {"access_label", "exec_label",
				      "mmap_label", "TRUE"};

	tst_require_root(NULL);
	if (verify_smackmnt() != 0)
		tst_brkm(TCONF, NULL, "Smack is not enabled");
	setup();

	// set current process label
	if (smack_set_self_label(PROC_LABEL1))
		tst_resm(TFAIL, "Failed to set current process label");
	if (smack_get_process_label(getpid(), &ret))
		tst_resm(TFAIL, "Failed to get current process label");
	else if (ret == NULL)
		tst_resm(TFAIL, "Current process label is NULL");
	else if (strcmp(ret, PROC_LABEL1) != 0)
		tst_resm(TFAIL, "Process has invalid label: %s (should be: %s)",
			 ret, PROC_LABEL1);

	// set file lables
	for (i = 0; i < ARRAY_SIZE(label_types); ++i) {
		if (smack_set_file_label(TEST_FILE_PATH, label_values[i],
					 label_types[i], 0)
		    < 0)
			tst_resm(TFAIL, "Failed to set %s label for file: %s ",
				 smack_xattr_name(label_types[i]),
				 TEST_FILE_PATH);

		if (smack_get_file_label(TEST_FILE_PATH, &ret, label_types[i],
					 0)
		    != 0)
			tst_resm(TFAIL, "Failed to get %s label for file: %s ",
				 smack_xattr_name(label_types[i]),
				 TEST_FILE_PATH);

		if (strcmp(ret, label_values[i]) != 0)
			tst_resm(TFAIL, "File %s has invalid %s label: %s "
				 "(should be: %s)",
				 TEST_FILE_PATH,
				 smack_xattr_name(label_types[i]), ret,
				 label_values[i]);
	}

	cleanup();
	tst_exit();
}
