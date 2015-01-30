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
 * Smack namespace - test case "mount"
 *
 * Triggers following LSM hooks:
 *  * smack_sb_kern_mount
 *  * smack_sb_copy_data
 *
 *  At the moment the test verifies two scenarios:
 *  1. Mounting a temp fs with various "smackfsdef" options.
 *  2. Mounting a prepared filesystem image (mount_test.img).
 *	   See prep_fs.sh for more details.
 *
 *  "smackfsdef" option specifies what label should a file get if it do not
 *  physically own "SECURITY.SMACK64" label. In the 1. scenario, a file
 *  created in tmpfs will always get a label of the process. In the 2.,
 *  the file existing in the provided .img file will get a label depending
 *  on aforementioned "smackfsdef" option.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <linux/loop.h>

#include "test_common.h"

#define LOOP_CONTROL_PATH "/dev/loop-control"
#define NS_PATH_SIZE 64

// mount_test.img will be mounted at tmp/dir1
static const char* test_img = "mount_test.img";
static const char* test_img_file = "tmp/dir1/file";

static struct test_file_desc test_dirs[] =
{
	{"tmp/dir0", 0777, "*"},
	{"tmp/dir1", 0777, "*"},
	{NULL, 0, NULL}
};

static struct test_smack_rule_desc test_rules[] =
{
	/* allow rwx access to created directories */
	{"inside", "*", "rwx"},
	{NULL, 0, NULL}
};

static const int mount_opts_num = 4;
static const char *mount_opts[] =
{
	"smackfsfloor",
	"smackfshat",
	"smackfsroot",
	"smackfstransmute"
};

/*
 * Mount 'src' file to 'dest' via loopback device
 */
static int mount_loopback(const char *src, const char *dest, const char *opts,
			  char **loop_dev)
{
	char loop_dev_path[16];
	int file_fd = -1, loop_fd = -1, ret = -1;
	int loopback_dev_id = 0;

	/* ensure that a free loopback device exists */
	int ctrl_fd = open(LOOP_CONTROL_PATH, O_RDONLY);
	if (ctrl_fd == -1) {
		TEST_ERROR("open(\"" LOOP_CONTROL_PATH "\", O_RDWR): %s",
			   strerror(errno));
	} else {
		loopback_dev_id = ioctl(ctrl_fd, LOOP_CTL_GET_FREE);
		TEST_CHECK(loopback_dev_id >= 0, "ioctl(): %s", strerror(errno));
#ifdef PRINT_DEBUG
		if (loopback_dev_id >= 0)
			printf("Free loopback device found: %d\n", loopback_dev_id);
#endif
		close(ctrl_fd);
	}

	/* open loopback device */
	sprintf(loop_dev_path, "/dev/loop%d", loopback_dev_id);
	loop_fd = open(loop_dev_path, O_RDWR);
	if (loop_fd == -1) {
		TEST_ERROR("open(\"%s\", O_RDWR): %s", loop_dev_path,
			   strerror(errno));
		return -1;
	}

	file_fd = open(src, O_RDWR);
	if (file_fd == -1) {
		TEST_ERROR("open(\"%s\", O_RDWR): %s", src, strerror(errno));
		goto error;
	}

	/* link image file with loopback device */
	ret = ioctl(loop_fd, LOOP_SET_FD, file_fd);
	if (ret == -1) {
		TEST_ERROR("ioctl(%d, LOOP_SET_FD, %d): %s", loop_fd, file_fd,
			   strerror(errno));
		goto error;
	}

	/* mount a directory to the loopback device */
	errno = 0;
	ret = mount(loop_dev_path, dest, "ext2", MS_MGC_VAL, opts);
	if (ret == -1) {
		printf("mount(\"%s\", \"%s\", \"ext2\", 0, \"%s\") failed: "
		       "%s\n", loop_dev_path, dest, opts, strerror(errno));
		ioctl(loop_fd, LOOP_CLR_FD, 0);
		goto error;
	}

	*loop_dev = strdup(loop_dev_path);

error:
	if (file_fd != -1)
		close(file_fd);
	if (loop_fd != -1)
		close(loop_fd);
	return ret;
}

int umount_loopback(const char *dest, char *loop_dev)
{
	int loop_fd, ret;
	TEST_CHECK(umount(dest) == 0, "umount(): %s", strerror(errno));

	loop_fd = open(loop_dev, O_RDWR);
	if (loop_fd == -1) {
		return -1;
	}

	ret = ioctl(loop_fd, LOOP_CLR_FD, 0);
	if (ret == -1) {
		close(loop_fd);
		return -1;
	}

	close(loop_fd);
	errno = 0;
	return 0;
}

void main_inside_ns(void)
{
	int ret, i;
	const int mount_flags = MS_NOSUID | MS_NODEV | MS_STRICTATIME | MS_RDONLY;
	char *label = NULL, *loop_dev;
	char buf[128];

	test_sync(0);

	/*
	 * Scenario 1:
	 * simple mount tmpfs
	 */
	int expected_ret1[] = { 0,  0,  0,  0,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno1[] = {    0,     0,     0,     0,   // UID = 0
			         EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = mount("tmpfs", test_dirs[0].path, "tmpfs", mount_flags, NULL);
	TEST_CHECK(ret == expected_ret1[env_id] && errno == expected_errno1[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0)
		umount(test_dirs[0].path);


	/*
	 * Scenario 2:
	 * mount tmpfs with mapped labels
	 */
	int expected_ret2[] = { 0, -1,  0,  0,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno2[] = {    0, EPERM,     0,     0,   // UID = 0
				 EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = mount("tmpfs", test_dirs[0].path, "tmpfs", mount_flags,
		    "smackfsdef=n_l1");
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	/*
	 * TODO: at this point a file could be created by sibling process.
	 * Usage of setns() may be required.
	 */
	if (ret == 0)
		umount(test_dirs[0].path);


	/*
	 * Execute rest of the tests only as real root.
	 * The reason is we need to have RW access to loopback device
	 * in order to prepare it for mounting to a directory.
	 * One of possibility to workaround this would be to
	 * pass file descriptior to loop* device from sibling process.
	 */
	if (env_id != 0 && env_id != 2)
		goto finish;


	/*
	 * Scenario 3:
	 * mount tmpfs with unmapped labels
	 */
	int expected_ret3[] = { 0, -1, -1, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno3[] = {    0, EPERM, EBADR, EBADR,   // UID = 0
				 EPERM, EPERM, EPERM, EPERM }; // UID = 1000
	errno = 0;
	ret = mount("tmpfs", test_dirs[0].path, "tmpfs", mount_flags, "smackfsdef=unmapped");
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0)
		umount(test_dirs[0].path);


	/*
	 * Scenario 4:
	 * smackfsdef with mapped label
	 */
	int expected_ret4[] = { 0, -1,  0, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno4[] = {    0, EBUSY,     0, EBUSY,   // UID = 0
				 EBUSY, EBUSY, EBUSY, EBUSY }; // UID = 1000
	// EBUSY, because we won't have DAC access to /dev/loop* files
	errno = 0;
	ret = mount_loopback(test_img, test_dirs[1].path, "smackfsdef=n_l1", &loop_dev);
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		ret = smack_get_file_label(test_img_file, &label, SMACK_LABEL_ACCESS, 0);
		TEST_CHECK(ret == 0, "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		TEST_LABEL(label, "n_l1");
		// TODO: verify the label outside the namespace
		umount_loopback(test_dirs[1].path, loop_dev);
		free(loop_dev);
	}


	/*
	 * Scenario 5:
	 * smackfsdef with unmapped label
	 */
	int expected_ret5[] = { 0, -1, -1, -1,   // UID = 0
			       -1, -1, -1, -1 }; // UID = 1000
	int expected_errno5[] = {    0, EBUSY, EBADR, EBUSY,   // UID = 0
				 EBUSY, EBUSY, EBUSY, EBUSY }; // UID = 1000
	errno = 0;
	ret = mount_loopback(test_img, test_dirs[1].path, "smackfsdef=unmapped", &loop_dev);
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		ret = smack_get_file_label(test_img_file, &label, SMACK_LABEL_ACCESS, 0);
		TEST_CHECK(ret == 0, "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		TEST_LABEL(label, "unmapped");
		umount_loopback(test_dirs[1].path, loop_dev);
		free(loop_dev);
	}


	/*
	 * Scenario 4:
	 * mount using remainging mount options
	 */
	for (i = 0; i < mount_opts_num; ++i) {
		int expected_ret4[] = { 0, -1,  0, -1,   // UID = 0
				       -1, -1, -1, -1 }; // UID = 1000
		int expected_errno4[] = {    0, EBUSY,     0, EBUSY,   // UID = 0
					 EBUSY, EBUSY, EBUSY, EBUSY }; // UID = 1000
		sprintf(buf, "%s=n_l1", mount_opts[i]);
		errno = 0;
		ret = mount_loopback(test_img, test_dirs[1].path, buf, &loop_dev);
		TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			umount_loopback(test_dirs[1].path, loop_dev);
			free(loop_dev);
		}

		int expected_ret5[] = { 0, -1, -1, -1,   // UID = 0
				       -1, -1, -1, -1 }; // UID = 1000
		int expected_errno5[] = {    0, EBUSY, EBADR, EBUSY,   // UID = 0
					 EBUSY, EBUSY, EBUSY, EBUSY }; // UID = 1000
		sprintf(buf, "%s=unmapped", mount_opts[i]);
		errno = 0;
		ret = mount_loopback(test_img, test_dirs[1].path, buf, &loop_dev);
		TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			umount_loopback(test_dirs[1].path, loop_dev);
			free(loop_dev);
		}
	}

finish:
	test_sync(1);
}

void main_outside_ns(void)
{
	init_test_resources(test_rules, NULL, test_dirs, NULL);

	test_sync(0);

	test_sync(1);
}

void test_cleanup(void)
{
}
