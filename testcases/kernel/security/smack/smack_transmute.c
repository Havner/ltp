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
#include "tst_safe_macros.h"
#include "smack_common.h"

#define CLEANUP cleanup
const char *TCID = "smack_simple";
int TST_TOTAL = 1;

#define TEST_FILE_PATH "test_file1"
#define LABEL1 "label1"
#define LABEL2 "label2"

#define DIR_NAME "dir"
#define FILE_PATH_1 DIR_NAME "/aaa"
#define FILE_PATH_2 DIR_NAME "/bbb"

static void cleanup(void)
{
	tst_rmdir();
}

static void setup(void)
{
	tst_tmpdir();
}

int main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);

	int fd;
	char* ret = NULL;

	tst_require_root();
	if (verify_smackmnt() != 0)
		tst_brkm(TCONF, NULL, "Smack is not enabled");
	setup();

	smack_set_rule(LABEL1, LABEL2, "rwx");

	if (smack_set_self_label(LABEL1) < 0)
		tst_resm(TFAIL, "smack_set_self_label() failed: %s",
			 strerror(errno));

	/* create a directory with "transmute" label on */
	if (mkdir("dir", 0777) < 0)
		tst_resm(TFAIL, "mkdir() failed: %s", strerror(errno));
	if (smack_set_file_label(DIR_NAME, "TRUE", SMACK_LABEL_TRANSMUTE, 0) < 0)
		tst_resm(TFAIL, "smack_set_file_label() failed: %s",
			 strerror(errno));
	if (smack_set_file_label(DIR_NAME, LABEL2, SMACK_LABEL_ACCESS, 0) < 0)
		tst_resm(TFAIL, "smack_set_file_label() failed: %s",
			 strerror(errno));

	fd = SAFE_OPEN(cleanup, FILE_PATH_1, O_CREAT | O_RDWR, 0666);
	close(fd);

	if (smack_get_file_label(FILE_PATH_1, &ret, SMACK_LABEL_ACCESS, 0))
		tst_resm(TFAIL, "Failed to get file label: %s",
			 strerror(errno));
	else {
		if (strcmp(ret, LABEL1) != 0)
			tst_resm(TFAIL, "File %s got invalid label: %s "
				 "(should be: %s)",
				 FILE_PATH_1, ret, LABEL1);
		free(ret);
	}

	/* gain "transmute" access rule for test directory */
	smack_set_rule(LABEL1, LABEL2, "rwxt");

	fd = SAFE_OPEN(cleanup, FILE_PATH_2, O_CREAT | O_RDWR, 0666);
	close(fd);

	if (smack_get_file_label(FILE_PATH_2, &ret, SMACK_LABEL_ACCESS, 0))
		tst_resm(TFAIL, "Failed to get file label: %s",
			 strerror(errno));
	else {
		if (strcmp(ret, LABEL2) != 0)
			tst_resm(TFAIL, "File %s got invalid label: %s "
				 "(should be: %s)",
				 FILE_PATH_2, ret, LABEL2);
		free(ret);
	}

	cleanup();
	tst_exit();
}
