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
 * Smack namespace - test case "signal"
 *
 * Triggers following LSM hooks:
 *  * smack_task_kill
 *
 * This test cases verifies write access from one process to another
 * via sending a signal to a process.
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
#include <sys/prctl.h>
#include <linux/securebits.h> /* for SECBIT_* */
#include "test_common.h"

#define LABEL "label"
#define UNMAPPED "unmapped"

static const struct test_smack_rule_desc test_rules[] = {
	{INSIDE_PROC_LABEL, LABEL, "w", manual},    /* 0: check 3 */
	{INSIDE_PROC_LABEL, UNMAPPED, "w", manual}, /* 1: check 4 */
	{NULL}
};

static const struct test_smack_mapping_desc test_mappings[] = {
	{LABEL, MAPPED_LABEL_PREFIX LABEL, automatic},
	{NULL}
};

void signal_handler(int sig)
{
#ifdef PRINT_DEBUG
	printf("%d: signal received: %d\n", getpid(), sig);
#else
	UNUSED(sig);
#endif
}

void main_inside_ns(void)
{
	int ret;

	/*
	 * Check 1:
	 * target label: "outside"
	 */
	test_sync(0);
	/*
	 * The EACCES in env == 3 is because those 2 processes
	 * are in the different user namespaces, hence CAP_MAC_OVERRIDE of
	 * the inside process is inefective (both labels mapped, no rule
	 * between them is a situation that CAP_MAC_OVERRIDE would deal with).
	 */
	int expected_ret1[] = { 0, -1,  0, -1,
	                       -1, -1, -1, -1};
	int expected_errno1[] = {0,      EACCES,      0, EACCES,
				 EACCES, EACCES, EACCES, EACCES };
	ret = kill(sibling_pid, SIGUSR1);
	TEST_CHECK(ret == expected_ret1[env_id] && errno == expected_errno1[env_id],
	           strerror(errno));
	test_sync(1);

	/*
	 * Check 2:
	 * target label: "label" (no "inside label w" rule)
	 */
	test_sync(2);
	int expected_ret2[] = { 0, -1,  0, -1,
			       -1, -1, -1, -1};
	int expected_errno2[] = {0,      EACCES,      0, EACCES,
				 EACCES, EACCES, EACCES, EACCES };
	errno = 0;
	ret = kill(sibling_pid, SIGUSR1);
	TEST_CHECK(ret == expected_ret2[env_id] && errno == expected_errno2[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	test_sync(3);

	/*
	 * Check 3:
	 * target label: "label" (with "inside label w")
	 */
	test_sync(4);
	int expected_ret3[] = {0, 0, 0, 0,
			       0, 0, 0, 0};
	ret = kill(sibling_pid, SIGUSR1);
	TEST_CHECK(ret == expected_ret3[env_id], strerror(errno));
	test_sync(5);

	/*
	 * Check 4:
	 * target label: "unmapped" (not valid in Smack NS)
	 */
	test_sync(6);
	int expected_ret4[] = {0, 0, -1, -1,
			       0, 0, -1, -1};
	int expected_errno4[] = {0, 0, EPERM, EPERM,
				 0, 0, EPERM, EPERM };
	errno = 0;
	ret = kill(sibling_pid, SIGUSR1);
	TEST_CHECK(ret == expected_ret4[env_id] && errno == expected_errno4[env_id],
		   "ret = %d, errno = %d: %s", ret, errno, strerror(errno));
	test_sync(7);
}

void main_outside_ns(void)
{
	int ret;

	init_test_resources(test_rules, test_mappings, NULL, NULL);

	/* preserve all capabilities when switching user from root */
	if (prctl(PR_SET_SECUREBITS,
		  SECBIT_NO_SETUID_FIXUP | SECBIT_NO_SETUID_FIXUP_LOCKED |
		  SECBIT_NOROOT | SECBIT_NOROOT_LOCKED) == -1)
		ERR_EXIT("prctl()");

	/* change UID to match sibling's UID */
	if (setuid(uid) == -1)
		ERR_EXIT("setuid()");

	signal(SIGUSR1, &signal_handler);

	test_sync(0);

	/* do check 1 */

	test_sync(1);

	/* check 2 preparation: */
	ret = smack_set_self_label(LABEL);
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(2);

	/* do check 2 */

	test_sync(3);

	/* check 3 preparation: */
	set_smack_rule(&test_rules[0]);

	test_sync(4);

	/* do check 3 */

	test_sync(5);

	/* check 4 preparation: */
	set_smack_rule(&test_rules[1]);
	ret = smack_set_self_label(UNMAPPED);
	TEST_CHECK(ret == 0, strerror(errno));

	test_sync(6);

	/* do check 4 */

	test_sync(7);
}

void test_cleanup(void)
{
}
