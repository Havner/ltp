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
 * Smack namespace - common macros and functions.
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#ifndef SMACK_NAMESPACE_FRAMEWORK_COMMON_H
#define SMACK_NAMESPACE_FRAMEWORK_COMMON_H

#include <string.h>

/* not defined yet in glibc */
#ifndef CLONE_NEWLSM
#define CLONE_NEWLSM 0x00001000
#endif

/*
 * test environment bits (6 combinations total):
 * bit 0:    root/regular user
 * bits 1-2: no_ns/user_ns/smack_ns
 */
#define TEST_ENV_USER_SHIFT	0
#define TEST_ENV_USER_MASK	(1 << TEST_ENV_USER_SHIFT)
#define TEST_ENV_USER_ROOT	(0 << TEST_ENV_USER_SHIFT)
#define TEST_ENV_USER_REGULAR	(1 << TEST_ENV_USER_SHIFT)

#define TEST_ENV_NS_SHIFT	1
#define TEST_ENV_NS_MASK	(3 << TEST_ENV_NS_SHIFT)
#define TEST_ENV_NS_NONE	(0 << TEST_ENV_NS_SHIFT)
#define TEST_ENV_NS_USER	(1 << TEST_ENV_NS_SHIFT)
#define TEST_ENV_NS_SMACK	(2 << TEST_ENV_NS_SHIFT)

#define TOTAL_TEST_ENVS		6

/* test case exit codes */
#define TEST_EXIT_USAGE	1
#define TEST_EXIT_FAIL	2
#define TEST_EXIT_IPC	3

/* console color codes */
#define ANSI_COLOR_RED		"\x1b[31m"
#define ANSI_COLOR_GREEN	"\x1b[32m"
#define ANSI_COLOR_YELLOW	"\x1b[33m"
#define ANSI_COLOR_BLUE		"\x1b[34m"
#define ANSI_COLOR_MAGENTA	"\x1b[35m"
#define ANSI_COLOR_CYAN		"\x1b[36m"
#define ANSI_COLOR_RESET	"\x1b[0m"

/* fatal error */
#define ERR_EXIT(msg) \
	do {								  \
		printf(ANSI_COLOR_RED "%s:%d: %s: %s\n" ANSI_COLOR_RESET, \
		       __FILE__, __LINE__, msg, strerror(errno));         \
		exit(EXIT_FAILURE);					  \
	} while(0)

static inline int safe_strcmp(const char *a, const char *b)
{
	if (a == NULL && b == NULL)
		return 0;

	if (a == NULL || b == NULL)
		return 1;

	return strcmp(a, b);
}

#define ID_INSIDE_NS "id-inside"
#define ID_OUTSIDE_NS "id-outside"

#define INSIDE_PROC_LABEL "inside"
#define OUTSIDE_PROC_LABEL "outside"
#define SHARED_OBJECT_LABEL "shared"
#define MAPPED_LABEL_PREFIX "n_"

#define NON_ROOT_ID 5001

#endif // SMACK_NAMESPACE_FRAMEWORK_COMMON_H
