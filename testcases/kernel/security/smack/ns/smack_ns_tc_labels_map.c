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
 * Smack namespace - test case "labels_map"
 *
 * This test case verifies behavior of /proc/PID/attr/label_map interface via
 * writing various combinations of valid and invalid mapping into the map.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/capability.h>
#include "test_common.h"

#define MAPPING_NOT_FOUND "Mapping not found"
#define SELF_SMK_MAP_PATH "/proc/self/attr/" SMACK_LABEL_MAP_FILE

int cap_eff(cap_value_t cap, cap_flag_value_t flag)
{
	cap_t caps;
	int ret = 0;

	/* drop CAP_MAC_ADMIN */
	caps = cap_get_proc();
	if (caps == NULL)
		return -1;

	if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, flag) == -1) {
		cap_free(caps);
		return -1;
	}

	if (cap_set_proc(caps) == -1) {
		cap_free(caps);
		return -1;
	}

	cap_free(caps);
	return ret;
}

void main_inside_ns(void)
{
	int ret, fd;
	const int buff_size = 1024;
	char buffer[buff_size];
	pid_t self_pid = getpid();

	if ((env_id & TEST_ENV_NS_MASK) != TEST_ENV_NS_SMACK)
		return;

	test_sync(0);

	/*
	 * Mapping anything from inside is forbidden
	 */
	ret = smack_map_label(self_pid, "yyyy", "aaaa");
	TEST_CHECK(ret == -1 && errno == EPERM, strerror(errno));

	test_sync(1);

	/*
	 * Tests outside
	 */

	test_sync(2);

	/*
	 * Verify map content
	 */
	fd = open(SELF_SMK_MAP_PATH, O_RDONLY);
	TEST_CHECK(fd != -1, strerror(errno));
	ret = read(fd, buffer, buff_size);
	TEST_CHECK(ret != -1, "read(): %s", strerror(errno));
	close(fd);

	if (ret == -1)
		return;

	buffer[ret] = '\0';

	TEST_CHECK(strstr(buffer, "aaa -> bbb\n") != NULL, MAPPING_NOT_FOUND);
	TEST_CHECK(strstr(buffer, "@ -> ^\n") != NULL, MAPPING_NOT_FOUND);
	TEST_CHECK(strstr(buffer, "^ -> @\n") != NULL, MAPPING_NOT_FOUND);
	TEST_CHECK(strstr(buffer, "ccc -> ccc\n") != NULL, MAPPING_NOT_FOUND);
	TEST_CHECK(strstr(buffer, "ddd -> ddd\n") != NULL, MAPPING_NOT_FOUND);
}

void main_outside_ns(void)
{
	char path[PROC_PATH_MAX_LEN];
	char buf[LABEL_MAPPING_LEN];
	int ret, fd;
	ssize_t bytes;
	pid_t child;

	if ((env_id & TEST_ENV_NS_MASK) != TEST_ENV_NS_SMACK)
		return;

	char too_long_label[SMACK_LABEL_MAX_LEN + 2];
	memset(too_long_label, 'y', SMACK_LABEL_MAX_LEN + 1);
	too_long_label[SMACK_LABEL_MAX_LEN + 1] = '\0';

	test_sync(0);

	/*
	 * Tests inside
	 */

	test_sync(1);

	child = fork();
	if (child < 0)
		ERR_EXIT("fork");

	if (child == 0) {
		int ret;

		if (cap_eff(CAP_MAC_ADMIN, CAP_CLEAR) != 0)
			ERR_EXIT("caps operations");

		/*
		 * Setting map by unprivileged process is forbidden
		 */
		ret = smack_map_label(sibling_pid, "aaaa", "bbbb");
		TEST_CHECK(ret == -1 && errno == EPERM, strerror(errno));

		exit(test_fails);
	}

	waitpid(child, &ret, 0);
	test_fails += ret;

	/*
	 * Drop caps, open the map file, fork, get caps back and try to write
	 * to it. Should fail, because the opener did not have privileges.
	 */
	if (cap_eff(CAP_MAC_ADMIN, CAP_CLEAR) != 0)
		ERR_EXIT("caps operations");

	snprintf(path, PROC_PATH_MAX_LEN, "/proc/%d/attr/" SMACK_LABEL_MAP_FILE,
		 sibling_pid);
	fd = open(path, O_WRONLY);
	TEST_CHECK(fd != -1, strerror(errno));

	child = fork();
	if (child < 0)
		ERR_EXIT("fork");

	if (child == 0) {
		int ret;

		if (cap_eff(CAP_MAC_ADMIN, CAP_SET) != 0)
			ERR_EXIT("caps operations");

		strcpy(buf, "bbbb aaaa");

		/*
		 * Setting map when unprivilged process opened the map file
		 * is forbidden
		 */
		ret = write(fd, buf, strlen(buf));
		TEST_CHECK(ret == -1 && errno == EPERM, strerror(errno));

		exit(test_fails);
	}

	/*
	 * Get caps back for the remaining tests
	 */
	if (cap_eff(CAP_MAC_ADMIN, CAP_SET) != 0)
		ERR_EXIT("caps operations");

	waitpid(child, &ret, 0);
	test_fails += ret;

	ret = smack_map_label(sibling_pid, "aaa", "bbb");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_map_label(sibling_pid, "@", "^");
	TEST_CHECK(ret == 0, strerror(errno));
	ret = smack_map_label(sibling_pid, "^", "@");
	TEST_CHECK(ret == 0, strerror(errno));

	/*
	 * remapping is forbidden
	 */
	ret = smack_map_label(sibling_pid, "aaa", "yyyy");
	TEST_CHECK(ret == -1 && errno == EEXIST, strerror(errno));
	ret = smack_map_label(sibling_pid, "yyyy", "bbb");
	TEST_CHECK(ret == -1 && errno == EEXIST, strerror(errno));

	/*
	 * invalid labels
	 */

	ret = smack_map_label(sibling_pid, "-", "yyyy");
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));
	ret = smack_map_label(sibling_pid, "yyyy", "-");
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));

	ret = smack_map_label(sibling_pid, "", "yyyy");
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));
	ret = smack_map_label(sibling_pid, "yyyy", "");
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));

	ret = smack_map_label(sibling_pid, too_long_label, "yyyy");
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));
	ret = smack_map_label(sibling_pid, "yyyy", too_long_label);
	TEST_CHECK(ret == -1 && errno == EINVAL, strerror(errno));

	/*
	 * mutliple writes within an open/close block
	 */

	snprintf(path, PROC_PATH_MAX_LEN, "/proc/%d/attr/" SMACK_LABEL_MAP_FILE,
		 sibling_pid);
	fd = open(path, O_WRONLY);
	TEST_CHECK(fd != -1, strerror(errno));

	strcpy(buf, "ccc ccc");
	errno = 0;
	bytes = write(fd, buf, strlen(buf));
	TEST_CHECK(bytes == (ssize_t)strlen(buf), strerror(errno));

	strcpy(buf, "eee ccc");
	bytes = write(fd, buf, strlen(buf));
	TEST_CHECK(bytes == -1 && errno == EEXIST, "bytes = %zd: %s",
		   bytes, strerror(errno));

	strcpy(buf, "ccc eee");
	bytes = write(fd, buf, strlen(buf));
	TEST_CHECK(bytes == -1 && errno == EEXIST, "bytes = %zd: %s",
		   bytes, strerror(errno));

	strcpy(buf, "ddd ddd");
	errno = 0;
	bytes = write(fd, buf, strlen(buf));
	TEST_CHECK(bytes == (ssize_t)strlen(buf), strerror(errno));

	close(fd);

	test_sync(2);
}

void test_cleanup(void)
{
}
