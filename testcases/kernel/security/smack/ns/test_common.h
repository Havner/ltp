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
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#ifndef SMACK_NAMESPACE_TEST_COMMON_H
#define SMACK_NAMESPACE_TEST_COMMON_H

#include "smack_ns_common.h"
#include "../smack_common.h"
#include "../files_common.h"

#define ERROR_BUFFER_SIZE 1024
#define AMBIENT_OBJECT_LABEL "ambient"


enum startup {
	manual = 0,
	automatic
};

struct test_smack_rule_desc {
	char* subject;
	char* object;
	char* access;
	enum startup startup;
};

struct test_smack_mapping_desc {
	char* original;
	char* mapped;
	enum startup startup;
};

struct test_file_desc {
	char *path;
	mode_t mode;
	char *label_access;
	char *label_exec;
	char *label_mmap;
	enum file_type type;
};

struct test_dir_desc {
	char *path;
	mode_t mode;
	char *label_access;
	enum dir_flags flags;
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

/*
 * This macro will chose a correct label depending whether it's called
 * inside or outside the Smack namespace. This way the string returned
 * will always refer to the same internal label. E.g.:
 * LM("will_be_floor", "_")
 */
#define LM(UNMAPPED, MAPPED)						 \
	(inside_ns && ((env_id & TEST_ENV_NS_MASK) == TEST_ENV_NS_SMACK) \
	? MAPPED : UNMAPPED)

/*
 * This macro will works just like LM() but can be used for labels that
 * have been created using MAPPED_LABEL_PREFIX. E.g.:
 * LA("label1")
 */
#define LA(LABEL)					\
	LM(LABEL, MAPPED_LABEL_PREFIX LABEL)


// TODO: use "tst_resm" when migrated to LTP
#define TEST_ERROR(...)							      \
	do {								      \
		char test_error_buf[ERROR_BUFFER_SIZE];			      \
		snprintf(test_error_buf, ERROR_BUFFER_SIZE, __VA_ARGS__);     \
		printf(ANSI_COLOR_RED "%d: [FAIL] %s:%d: %s" ANSI_COLOR_RESET \
		       "\n", getpid(), __FILE__, __LINE__, test_error_buf);   \
		fflush(stdout);						      \
		test_fails++;						      \
	} while(0)

#define TEST_CHECK(cond, ...)			\
	do {					\
		if (cond)			\
			break;			\
		TEST_ERROR(__VA_ARGS__);	\
	} while(0)

#define TEST_LABEL(current, expected)			\
	TEST_CHECK(					\
		((current) == 0 && (expected) == 0) ||	\
		((current) != 0 && (expected) != 0 &&	\
		 strcmp((current), (expected)) == 0),	\
	           "current label = %s, expected = %s",	\
	           current, expected)

void set_smack_rule(const struct test_smack_rule_desc *rule);
void set_smack_mapping(const struct test_smack_mapping_desc *mapping);
void init_test_resources(const struct test_smack_rule_desc *rules,
			 const struct test_smack_mapping_desc *mappings,
			 const struct test_dir_desc *dirs,
			 const struct test_file_desc *files);

#endif // SMACK_NAMESPACE_TEST_COMMON_H
