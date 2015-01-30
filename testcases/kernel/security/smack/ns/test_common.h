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
 * Smack namespaces - common test case routines
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#ifndef SMACK_NAMESPACE_TEST_COMMON_H
#define SMACK_NAMESPACE_TEST_COMMON_H

#include "common.h"
#include "../smack_common.h"

struct test_smack_rule_desc {
	char* subject;
	char* object;
	char* access;
};

struct test_smack_mapping_desc {
	char* original;
	char* mapped;
};

struct test_file_desc {
	char* path;
	mode_t mode;
	char* label;
};

/* are we inside namespaces (USER/Smack)? */
extern int inside_ns;

/* pid of process outside the ns when accessed inside the ns, and vice versa */
extern pid_t sibling_pid;

/* UID and GID of process in namespaces */
extern uid_t uid;
extern gid_t gid;

/* test environment ID (see test environment flags in common.h) */
extern int env_id;

/*
 * Synchronize the two processes and send some additional data between them.
 * loc_id - expected to be the same in both processes in a common sync point.
 * write_data, write_data_size - optional data bufer to be send to the second process
 * read_data, read_data_size - optional data bufer to be read from the second process
 */
void test_sync_ex(char loc_id, const void *write_data, size_t write_data_size,
		  void *read_data, size_t read_data_size);

/*
 * Synchronize the two processes without sending additional data between them.
 */
void test_sync(char loc_id);

/*
 * These functions must be provided by a test case.
 */
void main_inside_ns(void);
void main_outside_ns(void);
void test_cleanup(void);

/* test failures counter */
extern int test_fails;

// TODO: use "tst_resm" when migrated to LTP
#define TEST_ERROR(...)	\
	do {								      \
		char test_error_buf[1024];				      \
		snprintf(test_error_buf, 1024, __VA_ARGS__);		      \
		printf(ANSI_COLOR_RED "%d: [FAIL] %s:%d: %s" ANSI_COLOR_RESET \
		       "\n", getpid(), __FILE__, __LINE__, test_error_buf);   \
		fflush(stdout);						      \
		test_fails++;						      \
	} while(0)

#define TEST_CHECK(cond, ...) \
	do {					\
		if (cond)			\
			break;			\
		TEST_ERROR(__VA_ARGS__);	\
	} while(0)

#define TEST_LABEL(current, expected)					\
		TEST_CHECK((current) == 0 || (expected) == 0 ||		\
			   strcmp((current), (expected)) == 0,		\
			   "current label = %s, expected = %s",		\
			   current, expected)

#define CREATED_FILE_SIZE 64
#define CREATED_FILE_CONTENT 0xAA

/*
 * Create a file with a given permissions and Smack label.
 * The file will be owned by an user of a process in namespaces.
 * The file will contain TOUCHED_FILE_SIZE bytes equal to
 * TOUCHED_FILE_CONTENT.
 */
static inline int create_file_labeled(const char *path, mode_t mode,
				      const char *label)
{
	int fd;
	unsigned char data[CREATED_FILE_SIZE];

	fd = open(path, O_CREAT | O_WRONLY, mode);
	if (fd < 0)
		return -1;

	memset(data, CREATED_FILE_CONTENT, CREATED_FILE_SIZE);
	if (write(fd, data, CREATED_FILE_SIZE) != CREATED_FILE_SIZE)
		ERR_EXIT("write()");
	close(fd);

	if (!inside_ns)
		TEST_CHECK(chown(path, uid, gid) == 0,
			   "chown(\"%s\", %d, %d): %s", path, uid, gid,
			   strerror(errno));

	if (label != NULL)
		TEST_CHECK(
			smack_set_file_label(path, label, SMACK_LABEL_ACCESS, 0)
			== 0, "smack_set_file_label(\"%s\", %o, \"%s\"): %s",
			path, mode, label, strerror(errno));

	return 0;
}

static inline int create_file(const char *path, mode_t mode)
{
	return create_file_labeled(path, mode, NULL);
}

/*
 * Create a directory with a given permissions and Smack label.
 * The directory will be owned by an user of a process in namespaces.
 */
static inline int create_dir_labeled(const char* path, mode_t mode,
				     const char* label)
{
	char str[256];
	sprintf(str, "mkdir(\"%s\", %o)", path, mode);

	if (mkdir(path, mode) == -1)
		ERR_EXIT(str);

	if (chown(path, uid, gid) == -1)
		ERR_EXIT("chown()");

	if (label != NULL)
		if (smack_set_file_label(path, label, SMACK_LABEL_ACCESS, 0)
		    == -1)
			ERR_EXIT("smack_set_file_label()");

	return 0;
}

void save_smackfs_permissions(void);
void restore_smackfs_permissions(void);

void init_test_resources(const struct test_smack_rule_desc *rules,
			 const struct test_smack_mapping_desc *labels_map,
			 const struct test_file_desc *dirs,
			 const struct test_file_desc *files);

#endif // SMACK_NAMESPACE_TEST_COMMON_H
