#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "test.h"
#include "usctest.h"
#include "safe_macros.h"
#include "safe_file_ops.h"
#include "smack_common.h"

#define CLEANUP cleanup

char *TCID = "smack_onlycap";
int TST_TOTAL = 1;

#define TEST_FILE_PATH "test_file1"
#define PARENT_LABEL "parent_label"
#define CHILD_LABEL "child_label"
#define FILE_LABEL "file_label"

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

static void set_onlycap(const char* label)
{
	int fd;

	fd = open(SMACK_MNT_PATH "onlycap", O_WRONLY);
	if (fd == -1) {
		tst_resm(TFAIL, "Failed to open onlycap file: %s",
			 strerror(errno));
		return;
	}

	if (write(fd, label, strlen(label)) < 0)
		tst_resm(TFAIL, "Write to onlycap file failed: %s",
			 strerror(errno));
	close(fd);
}

static void test_parent(void)
{
	// changing labels should pass (our label == onlycap label)

	if (smack_set_file_label(TEST_FILE_PATH, FILE_LABEL,
	                         SMACK_LABEL_ACCESS, 0)
	    == -1)
		tst_resm(TFAIL, "smack_set_file_label() failed, errno = %s",
			 strerror(errno));

	// TODO: rules modifications, etc.
}

static void test_child(void)
{
	if (smack_set_self_label(CHILD_LABEL))
		tst_resm(TFAIL, "Failed to set current process label");

	// changing labels should fail (our label != onlycap label)

	if (smack_set_file_label(TEST_FILE_PATH, FILE_LABEL,
	                         SMACK_LABEL_ACCESS, 0)
	    == 0)
		tst_resm(TFAIL, "smack_set_file_label() should fail");

	// TODO: rules modifications, etc.
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	int status;
	pid_t pid;

	tst_require_root(NULL);
	if (verify_smackmnt() != 0)
		tst_brkm(TCONF, NULL, "Smack is not enabled");
	setup();

	if (smack_set_self_label(PARENT_LABEL))
		tst_resm(TFAIL, "Failed to set current process label");

	set_onlycap(PARENT_LABEL);

	test_parent();

	pid = fork();
	if (pid == -1)
		tst_resm(TFAIL, "fork() failed");
	else if (pid == 0) {
		// child
		test_child();
		exit(0);
	}

	// wait for child process
	if (waitpid(pid, &status, 0) == -1)
		tst_resm(TFAIL, "waitpid() failed: %s", strerror(errno));

	// cleanup
	set_onlycap("-");

	cleanup();
	tst_exit();
}
