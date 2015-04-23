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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include "test_common.h"


#define SMACK_IFACE_PATH_LEN 256

#define TEST_PREP_MARKER  INT8_MIN+0
#define TEST_START_MARKER INT8_MIN+1
#define TEST_END_MARKER   INT8_MIN+2


static int fd_in = STDIN_FILENO;
static int fd_out = STDERR_FILENO;

int inside_ns = 0;
pid_t sibling_pid;
uid_t uid;
gid_t gid;
int env_id;

int test_fails = 0;

struct smackfs_interface {
	struct smackfs_interface *next;
	char path[SMACK_IFACE_PATH_LEN];
	mode_t mode;
};

static struct smackfs_interface *smackfs_interfaces = NULL;
static char *old_ambient = NULL;

/*
 * Rules and mappings builtin in the test framework.
 * The upper framework that runs the test binary already adds
 * few, like inside, outside labels mappings, mapped shared label
 * and permissions to it.
 */
// TODO: do we need those rules?
static const struct test_smack_rule_desc builtin_rules[] = {
	{AMBIENT_OBJECT_LABEL, INSIDE_PROC_LABEL, "w", 1},
	{AMBIENT_OBJECT_LABEL, OUTSIDE_PROC_LABEL, "w", 1},
	{NULL}
};
static const struct test_smack_mapping_desc builtin_mappings[] = {
	{AMBIENT_OBJECT_LABEL, MAPPED_LABEL_PREFIX AMBIENT_OBJECT_LABEL, 1},
	{NULL}
};

/*
 * Arrays of rules, created files and directories are remembered
 * so they can be cleaned automatically at the exit.
 */
static const struct test_smack_rule_desc *saved_test_rules = NULL;
static const struct test_dir_desc *saved_test_dirs = NULL;
static const struct test_file_desc *saved_test_files = NULL;


/*
 * Synchronize two processes. The loc_id parameter is expected to be the same
 * in both processes in a certain point.
 */
void test_sync_ex(char loc_id, const void *write_data, size_t write_data_size,
		  void *read_data, size_t read_data_size)
{
#ifdef PRINT_DEBUG
	printf("%d: reaching location: %d\n", getpid(), location_id);
#endif

	ssize_t bytes;
	char msg;

	if (!inside_ns) {
		/* trigger B */
		msg = loc_id;
		if (write(fd_out, &msg, 1) == -1)
			ERR_EXIT("write");
		/* write data */
		if (write_data_size > 0)
			if (write(fd_out, write_data, write_data_size)
			    != (ssize_t)write_data_size)
				ERR_EXIT("write");
	}

	/* wait for B */
	if ((bytes = read(fd_in, &msg, 1)) == -1)
		ERR_EXIT("read");
	if (bytes == 0) {
		printf("%d: pipe EOS\n", getpid());
		exit(TEST_EXIT_IPC);
	}
	if (msg != loc_id) {
		/* this should never happen if code is written well */
		printf("%d: IPC error, check code\n", getpid());
		exit(TEST_EXIT_IPC);
	}
	/* read data */
	if (read_data_size > 0) {
		if ((bytes = read(fd_in, read_data, read_data_size)) == -1)
			ERR_EXIT("read");
		if (bytes != (ssize_t)read_data_size) {
			printf("%d: pipe EOS\n", getpid());
			exit(TEST_EXIT_IPC);
		}
	}

	if (inside_ns) {
		/* trigger B */
		msg = loc_id;
		if (write(fd_out, &msg, 1) == -1)
			ERR_EXIT("write");
		/* write data */
		if (write_data_size > 0)
			if (write(fd_out, write_data, write_data_size)
			    != (ssize_t)write_data_size)
				ERR_EXIT("write");
	}
}

void test_sync(char loc_id)
{
	test_sync_ex(loc_id, NULL, 0, NULL, 0);
}

/*
 * Rules: set one, set list, reset list.
 */
void set_smack_rule(const struct test_smack_rule_desc *rule)
{
	int ret;

	ret = smack_set_rule(rule->subject, rule->object,
	                     rule->access);
	TEST_CHECK(ret == 0, "Failed to set smack acces rule "
	           "(%s %s %s): %s",
	           rule->subject, rule->object, rule->access,
	           strerror(errno));
}

static void set_smack_rules(const struct test_smack_rule_desc *rules)
{
	while (rules->subject && rules->object && rules->access) {
		if (rules->startup == automatic)
			set_smack_rule(rules);
		rules++;
	}
}

static void reset_smack_rules(const struct test_smack_rule_desc *rules)
{
	while (rules->subject && rules->object && rules->access) {
		smack_set_rule(rules->subject, rules->object, "-");
		rules++;
	}
}

/*
 * Mappings: set one, set list, (no reset).
 */
void set_smack_mapping(const struct test_smack_mapping_desc *mapping)
{
	int ret;

	ret = smack_map_label(sibling_pid, mapping->original, mapping->mapped);
	TEST_CHECK(ret == 0, "Failed to set smack label mapping"
	           " (%s -> %s): %s",
	           mapping->original, mapping->mapped,
	           strerror(errno));
}

static void set_smack_mappings(const struct test_smack_mapping_desc *mappings)
{
	while (mappings->original && mappings->mapped) {
		if (mappings->startup == automatic)
			set_smack_mapping(mappings);
		mappings++;
	}
}

/*
 * Dirs: create one, create list, remove list.
 */
static void create_dir(const struct test_dir_desc *dir)
{
	int ret;

	ret = dir_create(dir->path, dir->mode, uid, gid,
			 dir->label_access, dir->flags);
	TEST_CHECK(ret == 0,
		   "Failed to create directory (%s): %s",
		   dir->path, strerror(errno));
}

static void create_dirs(const struct test_dir_desc *dirs)
{
	while(dirs->path) {
		create_dir(dirs);
		dirs++;
	}
}

static void remove_dirs(const struct test_dir_desc *dirs)
{
	const struct test_dir_desc *start = dirs;

	while (dirs->path)
		dirs++;
	dirs--;

	do {
		remove(dirs->path);
	} while (dirs-- != start);
}

/*
 * Files: create one, create list, remove list.
 */
static void create_file(const struct test_file_desc *file)
{
	int ret;

	ret = file_create(file->path, file->mode, uid, gid,
			  file->type, file->label_access,
			  file->label_exec, file->label_mmap);
	TEST_CHECK(ret == 0, "Failed to create file (%s): %s",
		   file->path, strerror(errno));
}

static void create_files(const struct test_file_desc *files)
{
	while (files->path) {
		create_file(files);
		files++;
	}
}

static void remove_files(const struct test_file_desc *files)
{
	while (files->path) {
		remove(files->path);
		files++;
	}
}

/*
 * Group initializations.
 */
static void init_builtin_resources(void)
{
	set_smack_rules(builtin_rules);

	if (env_id & TEST_ENV_SMACK_NS)
		set_smack_mappings(builtin_mappings);
}

static void cleanup_builtin_resources(void)
{
	reset_smack_rules(builtin_rules);
}

void init_test_resources(const struct test_smack_rule_desc *rules,
			 const struct test_smack_mapping_desc *mappings,
			 const struct test_dir_desc *dirs,
			 const struct test_file_desc *files)
{
	saved_test_rules = rules;
	saved_test_dirs = dirs;
	saved_test_files = files;

	if (rules != NULL)
		set_smack_rules(rules);

	if ((mappings != NULL) && (env_id & TEST_ENV_SMACK_NS))
		set_smack_mappings(mappings);

	if (dirs != NULL)
		create_dirs(dirs);

	if (files != NULL)
		create_files(files);
}

static void cleanup_test_resources(void)
{
	if (saved_test_rules != NULL)
		reset_smack_rules(saved_test_rules);

	if (saved_test_files != NULL)
		remove_files(saved_test_files);

	if (saved_test_dirs != NULL)
		remove_dirs(saved_test_dirs);
}

/*
 * Some checks in test cases get EACCES because of DAC (most of files in smackfs
 * have 0644 access). To solve this (check Smack error codes instead of DAC) we
 * need to change permissions of the Smack interfaces. This function lists all
 * the files in /smack directory, stores their access modes and changes to 0666.
 *
 * restore_smackfs_permissions(), which is called at the exit, will restore them
 * to previous values.
 */
static void save_and_loosen_smackfs_permissions(void)
{
	DIR *dir;
	struct dirent *ent;
	struct smackfs_interface *interface, *prev_interface;
	struct stat sb;
	char path[SMACK_IFACE_PATH_LEN];

	dir = opendir(SMACK_MNT_PATH);

	if (dir == NULL)
		return;

	prev_interface = NULL;
	while ((ent = readdir(dir)) != NULL) {
		snprintf(path, SMACK_IFACE_PATH_LEN, SMACK_MNT_PATH "%s",
			 ent->d_name);
		if (stat(path, &sb) == -1)
			ERR_EXIT("stat()");
		if (!S_ISREG(sb.st_mode))
			continue;

		interface = malloc(sizeof(struct smackfs_interface));
		if (interface == NULL)
			ERR_EXIT("malloc()");

		interface->mode = sb.st_mode & 0777;
		interface->next = NULL;
		strcpy(interface->path, path);

		if (chmod(interface->path, 0666) == -1)
			ERR_EXIT("stat()");

		if (prev_interface == NULL)
			smackfs_interfaces = interface;
		else
			prev_interface->next = interface;
		prev_interface = interface;
	}
	closedir(dir);
}

static void restore_smackfs_permissions(void)
{
	struct smackfs_interface *next, *interface = smackfs_interfaces;
	while (interface != NULL) {
		chmod(interface->path, interface->mode);
		next = interface->next;
		free(interface);
		interface = next;
	}
	smackfs_interfaces = NULL;
}

/*
 * Ambient save_n_set and restore.
 */
static void save_and_set_ambient_label(void)
{
	int ret;

	ret = smack_get_ambient(&old_ambient);
	TEST_CHECK(ret != -1, "smack_get_ambient(): %s", strerror(errno));
	ret = smack_set_ambient(AMBIENT_OBJECT_LABEL);
	TEST_CHECK(ret != -1, "smack_set_ambient(): %s", strerror(errno));
}

static void restore_ambient_label(void)
{
	if (old_ambient != NULL)
		smack_set_ambient(old_ambient);
}

/*
 * Exit handlers.
 */
static void test_on_exit(void)
{
	if (!inside_ns) {
		test_cleanup();
		cleanup_test_resources();
		cleanup_builtin_resources();
		restore_ambient_label();
		restore_smackfs_permissions();
	}
}

static void test_signal_handler(int sig)
{
	printf(ANSI_COLOR_YELLOW "%d: signal received: %s\n" ANSI_COLOR_RESET,
	       getpid(), strsignal(sig));
	exit(TEST_EXIT_FAIL);
}

/*
 * Main that handles both, the inside and outside cases.
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	if (argc < 6)
		goto invalid_usage;

	signal(SIGTERM, test_signal_handler);
	signal(SIGSEGV, test_signal_handler);
	signal(SIGINT, test_signal_handler);
	signal(SIGPIPE, test_signal_handler);

	sibling_pid = atoi(argv[2]);
	env_id = atoi(argv[3]);
	uid = atoi(argv[4]);
	gid = atoi(argv[5]);

	if (env_id < 0 || env_id >= TOTAL_TEST_ENVS)
		goto invalid_usage;

	/* register exit callback */
	atexit(test_on_exit);

	if (strcmp(argv[1], ID_INSIDE_NS) == 0) {
		printf("%d: running inside namespace\n", getpid());
		inside_ns = 1;

		test_sync(TEST_PREP_MARKER);

		test_sync(TEST_START_MARKER);
		main_inside_ns();
		test_sync(TEST_END_MARKER);
	} else if (strcmp(argv[1], ID_OUTSIDE_NS) == 0) {
		printf("%d: running outside namespace\n", getpid());
		umask(0);

		test_sync(TEST_PREP_MARKER);
		save_and_loosen_smackfs_permissions();
		save_and_set_ambient_label();
		init_builtin_resources();

		test_sync(TEST_START_MARKER);
		main_outside_ns();
		test_sync(TEST_END_MARKER);
	} else
		goto invalid_usage;

	if (test_fails > 0)
		ret = TEST_EXIT_FAIL;
	else
		ret = EXIT_SUCCESS;

	return ret;

invalid_usage:
	printf("Invalid usage. Please launch via Smack namespace framework\n");
	return TEST_EXIT_USAGE;
}

