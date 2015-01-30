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

/*
 * Arrays of rules, created files and directories are remembered
 * so they can be cleaned automatically at the exit.
 */
static const struct test_smack_rule_desc *test_rules = NULL;
static const struct test_file_desc *test_dirs = NULL;
static const struct test_file_desc *test_files = NULL;

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
		// trigger B
		msg = loc_id;
		if (write(fd_out, &msg, 1) == -1)
			ERR_EXIT("write");
		// write data
		if (write_data_size > 0)
			if (write(fd_out, write_data, write_data_size)
			    != (ssize_t)write_data_size)
				ERR_EXIT("write");
	}

	// wait for B
	if ((bytes = read(fd_in, &msg, 1)) == -1)
		ERR_EXIT("read");
	if (bytes == 0) {
		printf("%d: pipe EOS\n", getpid());
		exit(TEST_EXIT_IPC);
	}
	if (msg != loc_id) {
		// this should never happen if code is written well
		printf("%d: IPC error, check code\n", getpid());
		exit(TEST_EXIT_IPC);
	}
	// read data
	if (read_data_size > 0) {
		if ((bytes = read(fd_in, read_data, read_data_size)) == -1)
			ERR_EXIT("read");
		if (bytes != (ssize_t)read_data_size) {
			printf("%d: pipe EOS\n", getpid());
			exit(TEST_EXIT_IPC);
		}
	}

	if (inside_ns) {
		// trigger B
		msg = loc_id;
		if (write(fd_out, &msg, 1) == -1)
			ERR_EXIT("write");
		// write data
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

void init_test_resources(const struct test_smack_rule_desc *rules,
			 const struct test_smack_mapping_desc *labels_map,
			 const struct test_file_desc *dirs,
			 const struct test_file_desc *files)
{
	int ret;
	test_rules = rules;
	test_dirs = dirs;
	test_files = files;

	if (rules != NULL) {
		while (rules->subject && rules->object && rules->access) {
			ret = smack_set_rule(rules->subject, rules->object,
					     rules->access);
			TEST_CHECK(ret == 0, "Failed to set smack acces rule "
				   "(%s %s %s): %s",
				   rules->subject, rules->object, rules->access,
				   strerror(errno));
			rules++;
		}
	}

	if ((labels_map != NULL) && (env_id & TEST_ENV_SMACK_NS)) {
		while (labels_map->original && labels_map->mapped) {
			ret = smack_map_label(sibling_pid, labels_map->original,
					      labels_map->mapped);
			TEST_CHECK(ret == 0, "Failed to set smack label mapping"
				   " (%s -> %s): %s",
				   labels_map->original, labels_map->mapped,
				   strerror(errno));
			labels_map++;
		}
	}

	if (dirs != NULL) {
		while (dirs->path) {
			ret = create_dir_labeled(dirs->path, dirs->mode,
						 dirs->label);
			TEST_CHECK(ret == 0,
				   "Failed to create directory (%s): %s",
				   dirs->path, strerror(errno));
			dirs++;
		}
	}

	if (files != NULL) {
		while (files->path) {
			ret = create_file_labeled(files->path, files->mode,
						  files->label);
			TEST_CHECK(ret == 0, "Failed to create file (%s): %s",
				   files->path, strerror(errno));
			files++;
		}
	}
}

// TODO: this could be done via recursive ./tmp dir removal
void cleanup_test_resources(void)
{
	if (test_rules != NULL) {
		while (test_rules->subject && test_rules->object
		       && test_rules->access) {
			smack_set_rule(test_rules->subject, test_rules->object,
				       "-");
			test_rules++;
		}
	}

	if (test_dirs != NULL) {
		while (test_dirs->path) {
			remove(test_dirs->path);
			test_dirs++;
		}
	}

	if (test_files != NULL) {
		while (test_files->path) {
			remove(test_files->path);
			test_files++;
		}
	}
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
void save_smackfs_permissions(void)
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

void restore_smackfs_permissions(void)
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

void test_on_exit(int code, void* arg)
{
	(void)code;
	(void)arg;

	if (!inside_ns) {
		test_cleanup();
		restore_smackfs_permissions();
		cleanup_test_resources();
		smack_set_onlycap("-");
	}
}

void test_signal_handler(int sig)
{
	printf(ANSI_COLOR_YELLOW "%d: signal received: %s\n" ANSI_COLOR_RESET,
	       getpid(), strsignal(sig));
	exit(TEST_EXIT_FAIL);
}

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

	// register exit callback
	on_exit(test_on_exit, NULL);

	if (strcmp(argv[1], INSIDE_NS_IDENTIFIER) == 0) {
		printf("%d: running inside namespace\n", getpid());
		inside_ns = 1;
		main_inside_ns();
	} else if (strcmp(argv[1], OUTSIDE_NS_IDENTIFIER) == 0) {
		printf("%d: running outside namespace\n", getpid());
		umask(0);
		save_smackfs_permissions();

		main_outside_ns();
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
