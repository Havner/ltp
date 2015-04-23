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
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
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

#define LABEL    "label"
#define UNMAPPED "unmapped"

#define DIR0     "tmp/dir0"
#define DIR1     "tmp/dir1"

/* mount_test.img will be mounted at tmp/dir1 */
static const char* TEST_IMG = "mount_test.img";
static const char* TEST_IMG_FILE = "tmp/dir1/file";

static struct test_smack_mapping_desc test_mappings[] = {
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{NULL}
};

static struct test_dir_desc test_dirs[] =
{
	{DIR0, 0777, SHARED_OBJECT_LABEL, 0},
	{DIR1, 0777, SHARED_OBJECT_LABEL, 0},
	{NULL}
};

static const char *mount_opts[] =
{
	"smackfsfloor",
	"smackfshat",
	"smackfsroot",
	"smackfstransmute"
};

/*
 * Attach 'img' file to a loopback device
 */
static int prepare_image_loopback(const char *img, char **loop_dev)
{
	char loop_dev_path[16];
	int file_fd = -1, loop_fd = -1, ret = -1;
	int loopback_dev_id = 0;

	/* ensure that a free loopback device exists */
	int ctrl_fd = open(LOOP_CONTROL_PATH, O_RDONLY);
	if (ctrl_fd == -1) {
		TEST_ERROR("open(\"" LOOP_CONTROL_PATH "\", O_RDWR): %s",
			   strerror(errno));
		return -1;
	}

	loopback_dev_id = ioctl(ctrl_fd, LOOP_CTL_GET_FREE);
	close(ctrl_fd);
	if (loopback_dev_id < 0) {
		TEST_ERROR("ioctl(): %s", strerror(errno));
		return -1;
	}

#ifdef PRINT_DEBUG
	printf("Free loopback device found: %d\n", loopback_dev_id);
#endif

	/* open loopback device */
	sprintf(loop_dev_path, "/dev/loop%d", loopback_dev_id);
	loop_fd = open(loop_dev_path, O_RDWR);
	if (loop_fd == -1) {
		TEST_ERROR("open(\"%s\", O_RDWR): %s", loop_dev_path,
			   strerror(errno));
		return -1;
	}

	file_fd = open(img, O_RDWR);
	if (file_fd == -1) {
		TEST_ERROR("open(\"%s\", O_RDWR): %s", img, strerror(errno));
		goto error;
	}

	/* link image file with loopback device */
	ret = ioctl(loop_fd, LOOP_SET_FD, file_fd);
	if (ret == -1) {
		TEST_ERROR("ioctl(%d, LOOP_SET_FD, %d): %s", loop_fd, file_fd,
			   strerror(errno));
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

int close_loopback(char *loop_dev)
{
	int loop_fd, ret;

	loop_fd = open(loop_dev, O_RDWR);
	if (loop_fd == -1)
		return -1;

	ret = ioctl(loop_fd, LOOP_CLR_FD, 0);
	close(loop_fd);
	if (ret == -1)
		return -1;

	return 0;
}

void main_inside_ns(void)
{
	int ret;
	size_t i;
	const int mount_flags = MS_NOSUID | MS_NODEV | MS_STRICTATIME | MS_RDONLY;
	char *label = NULL, *loop_dev;
	char buf[128];

	test_sync(0);

	/*
	 * Scenario 1:
	 * simple mount tmpfs
	 */
	int expected_ret1[] = { 0,  0,  0,  0,   /* UID = 0 */
			       -1, -1, -1, -1 }; /* UID = 1000 */
	int expected_errno1[] = {    0,     0,     0,     0,   /* UID = 0 */
				 EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
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
	int expected_ret2[] = { 0, -1,  0,  0,   /* UID = 0 */
			       -1, -1, -1, -1 }; /* UID = 1000 */
	int expected_errno2[] = {    0, EPERM,     0,     0,   /* UID = 0 */
				 EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
	sprintf(buf, "smackfsdef=%s", LA(LABEL));
	errno = 0;
	ret = mount("tmpfs", test_dirs[0].path, "tmpfs", mount_flags, buf);
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	// TODO: at this point a file could be created by sibling process with setns.
	if (ret == 0)
		umount(test_dirs[0].path);


	/*
	 * Scenario 3:
	 * mount tmpfs with unmapped labels
	 */
	int expected_ret3[] = { 0, -1, -1, -1,   /* UID = 0 */
			       -1, -1, -1, -1 }; /* UID = 1000 */
	int expected_errno3[] = {    0, EPERM, EBADR, EBADR,   /* UID = 0 */
				 EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
	sprintf(buf, "smackfsdef=%s", UNMAPPED);
	errno = 0;
	ret = mount("tmpfs", test_dirs[0].path, "tmpfs", mount_flags, buf);
	TEST_CHECK(ret == expected_ret3[env_id] && errno == expected_errno3[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0)
		umount(test_dirs[0].path);

	/*
	 * Execute rest of the tests only as real root.
	 * The reasons are:
	 * - mount() requires CAP_SYS_ADMIN
	 * - we need rw access to /dev/loop*
	 * - setns may be required to enter MNTNS
	 * - mounting with smack* options requires CAP_MAC_ADMIN
	 */
	// TODO: The first three can be overcome to test the fourth.
	if (env_id != 0 && env_id != 2)
		goto finish;

	/* no point to continue if we can't prepare loopback device */
	// TODO: as above, extend number of configs where it is possible
	ret = prepare_image_loopback(TEST_IMG, &loop_dev);
	if (ret == -1)
		goto finish;

	/*
	 * Scenario 4:
	 * smackfsdef with mapped label
	 */
	int expected_ret4[] = { 0, -1,  0, -1,   /* UID = 0 */
			       -1, -1, -1, -1 }; /* UID = 1000 */
	int expected_errno4[] = {    0, EPERM,     0, EPERM,   /* UID = 0 */
				 EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
	sprintf(buf, "smackfsdef=%s", LA(LABEL));
	errno = 0;
	ret = mount(loop_dev, test_dirs[1].path, "ext2", MS_MGC_VAL, buf);
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		ret = smack_get_file_label(TEST_IMG_FILE, &label, SMACK_LABEL_ACCESS, 0);
		TEST_CHECK(ret == 0, "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			TEST_LABEL(label, LA(LABEL));
			free(label);
		}

		ret = umount(test_dirs[1].path);
		TEST_CHECK(ret == 0, "umount(): %s", strerror(errno));
	}


	/*
	 * Scenario 5:
	 * smackfsdef with unmapped label
	 */
	int expected_ret5[] = { 0, -1, -1, -1,   /* UID = 0 */
			       -1, -1, -1, -1 }; /* UID = 1000 */
	int expected_errno5[] = {    0, EPERM, EBADR, EPERM,   /* UID = 0 */
				 EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
	sprintf(buf, "smackfsdef=%s", UNMAPPED);
	errno = 0;
	ret = mount(loop_dev, test_dirs[1].path, "ext2", MS_MGC_VAL, buf);
	TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	if (ret == 0) {
		ret = smack_get_file_label(TEST_IMG_FILE, &label, SMACK_LABEL_ACCESS, 0);
		TEST_CHECK(ret == 0, "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			TEST_LABEL(label, UNMAPPED);
			free(label);
		}

		ret = umount(test_dirs[1].path);
		TEST_CHECK(ret == 0, "umount(): %s", strerror(errno));
	}

	/*
	 * Scenario 4:
	 * mount using remainging mount options
	 */
	for (i = 0; i < ARRAY_SIZE(mount_opts); ++i) {
		int expected_ret4[] = { 0, -1,  0, -1,   /* UID = 0 */
				       -1, -1, -1, -1 }; /* UID = 1000 */
		int expected_errno4[] = {    0, EPERM,     0, EPERM,   /* UID = 0 */
		                         EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
		sprintf(buf, "%s=%s", mount_opts[i], LA(LABEL));
		errno = 0;
		ret = mount(loop_dev, test_dirs[1].path, "ext2", MS_MGC_VAL, buf);
		TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			ret = umount(test_dirs[1].path);
			TEST_CHECK(ret == 0, "umount(): %s", strerror(errno));
		}

		int expected_ret5[] = { 0, -1, -1, -1,   /* UID = 0 */
				       -1, -1, -1, -1 }; /* UID = 1000 */
		int expected_errno5[] = {    0, EPERM, EBADR, EPERM,   /* UID = 0 */
		                         EPERM, EPERM, EPERM, EPERM }; /* UID = 1000 */
		sprintf(buf, "%s=%s", mount_opts[i], UNMAPPED);
		errno = 0;
		ret = mount(loop_dev, test_dirs[1].path, "ext2", MS_MGC_VAL, buf);
		TEST_CHECK(ret == expected_ret5[env_id] && errno == expected_errno5[env_id],
			   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
		if (ret == 0) {
			ret = umount(test_dirs[1].path);
			TEST_CHECK(ret == 0, "umount(): %s", strerror(errno));
		}
	}

	close_loopback(loop_dev);
	free(loop_dev);

finish:
	test_sync(1);
}

void main_outside_ns(void)
{
	init_test_resources(NULL, test_mappings, test_dirs, NULL);

	test_sync(0);

	test_sync(1);
}

void test_cleanup(void)
{
}
