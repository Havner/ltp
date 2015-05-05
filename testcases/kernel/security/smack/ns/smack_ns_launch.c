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
 * Smack namespace framework. The framework prepares enviroment for test cases.
 * The framework accepts multiple arguments, specifying:
 * * usage of Smack namespace
 * * usage of user namespace
 * * target user and group IDs inside and outside user namespace
 * * testcase to launch
 * Then it forks two times and launches given testcase binary in two instances
 * running simultaneously:
 * * one as root user
 * * second in namespaces (optionally) as non-root user (optionaly)
 * Bidirectional communication between processes is provided via unnamed pipes.
 *
 * Authors: Michal Witanowski <m.witanowski@samsung.com>
 *          Lukasz Pawelczyk <l.pawelczyk@samsung.com>
 */

#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <argp.h>
#include <dirent.h>
#include <grp.h>

#include "smack_ns_common.h"
#include "../smack_common.h"

#define USER_NS_REAL_USER 1001
#define MAP_PATH_LEN	64
#define UID_MAPPING_LEN	64

#define READ_END	0
#define WRITE_END	1

#define CHILD_NAMESPACES	0
#define CHILD_HELPER		1


/*
 * Common rules for the test framework and all test cases:
 * * "inside -> _" is needed to do execve() on a testcase binary
 *   (this rule might seem unnecessary, it's bultin after all,
 *   but inside namespace '_' stops acting like a builtin floor)
 * * "inside -> shared" is needed to access shared data in "tmp"
 *   directory
 */
const char* smack_rules[][3] = {
	// TODO: I would very much like to remove this rule
	// and depend only on shared label, although it has proven
	// to be difficult/impossible.
	// It would require getting rid of '_' dependency on full
	// access path to the test binary.
	{INSIDE_PROC_LABEL, "_", "rx"},
	{INSIDE_PROC_LABEL, SHARED_OBJECT_LABEL, "rwxal"},
	/* {OUTSIDE, SHARED, rwxal} is implicit by caps */
	{NULL}
};

/*
 * Common map for the test framework and all test cases.
 */
const char* smack_map[][2] = {
	// TODO: I would very much like to remove this mapping
	// and depend only on shared label, although it has proven
	// to be difficult/impossible.
	// It would require getting rid of '_' dependency on full
	// access path to the test binary.
	{"_",   "floor"},

	{INSIDE_PROC_LABEL, MAPPED_LABEL_PREFIX INSIDE_PROC_LABEL},
	{OUTSIDE_PROC_LABEL, MAPPED_LABEL_PREFIX OUTSIDE_PROC_LABEL},
	{SHARED_OBJECT_LABEL, MAPPED_LABEL_PREFIX SHARED_OBJECT_LABEL},

	{NULL}
};

struct arguments {
	/* "real" UID/GID - visible outside USER NS */
	uid_t uid;
	gid_t gid;

	/* "mapped" UID/GID - visible inside USER NS */
	uid_t mapped_uid;
	gid_t mapped_gid;

	int smack_ns;
	int user_ns;

	char* exe_path;
};

static struct arguments args;
static int test_environment_id;

/* command line arguments */
static struct argp_option options[] = {
	{"user",       'I', 0,     0, "Enable USER namespace.", 0},
	{"smack",      'S', 0,     0, "Enable Smack namespace.", 0},
	{"uid",        'U', "UID", 0, "Real UID (visible outside user ns).", 0},
	{"gid",        'G', "GID", 0, "Real GID (visible outside user ns).", 0},
	{"mapped-uid", 'u', "UID", 0, "Mapped UID (visible inside user ns).", 0},
	{"mapped-gid", 'g', "GID", 0, "Mapped GID (visible inside user ns).", 0},
	{NULL}
};

/* arguments parser */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;

	switch (key) {
	case 'I':
		arguments->user_ns = 1;
		break;
	case 'S':
		arguments->smack_ns = 1;
		break;
	case 'U':
		arguments->uid = atoi(arg);
		break;
	case 'G':
		arguments->gid = atoi(arg);
		break;
	case 'u':
		arguments->mapped_uid = atoi(arg);
		break;
	case 'g':
		arguments->mapped_gid = atoi(arg);
		break;
	case ARGP_KEY_ARG:
		if (state->arg_num >= 1)
			argp_usage(state);
		arguments->exe_path = arg;
		// TODO: maybe support for arguments passing will be needed?
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 1)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static char doc[] = "Smack namespace test framework";
static char args_doc[] = "EXECUTABLE_PATH";
static struct argp argp = {options, parse_opt, args_doc, doc, NULL, NULL, NULL};

static void write_smack_rules(const char *rules[][3])
{
	size_t i;
#ifdef PRINT_DEBUG
	printf("%d: setting smack rules...\n", getpid());
#endif
	for (i = 0; rules[i][0] != NULL; ++i) {
#ifdef PRINT_DEBUG
		printf("%s -> %s: %s\n", rules[i][0], rules[i][1], rules[i][2]);
#endif
		if (smack_set_rule(rules[i][0], rules[i][1], rules[i][2]) != 0)
			ERR_EXIT("smack_set_rule()");
	}
}

static void write_smack_map(pid_t pid, const char *map[][2])
{
	size_t i;

#ifdef PRINT_DEBUG
	printf("%d: green light, setting smack map...\n", getpid());
#endif

	for (i = 0; map[i][0] != NULL; ++i) {
#ifdef PRINT_DEBUG
		printf("%s -> %s\n", map[i][0], map[i][1]);
#endif
		if (smack_map_label(pid, map[i][0], map[i][1]) != 0)
			ERR_EXIT("smack_map_label()");
	}
}

/* set uid/gid map for pid process */
static void write_uid_maps(pid_t pid)
{
	FILE *f;
	char uid_buf[UID_MAPPING_LEN], gid_buf[UID_MAPPING_LEN];
	char path[MAP_PATH_LEN];

	if (args.mapped_uid != 0)
		snprintf(uid_buf, UID_MAPPING_LEN, "%d %d 1\n%d %d 1",
		         0, USER_NS_REAL_USER, args.mapped_uid, args.uid);
	else
		snprintf(uid_buf, UID_MAPPING_LEN, "%d %d 1",
		         args.mapped_uid, args.uid);

	if (args.mapped_gid != 0)
		snprintf(gid_buf, UID_MAPPING_LEN, "%d %d 1\n%d %d 1",
		         0, USER_NS_REAL_USER, args.mapped_gid, args.gid);
	else
		snprintf(gid_buf, UID_MAPPING_LEN, "%d %d 1",
		         args.mapped_gid, args.gid);

#ifdef PRINT_DEBUG
	printf("UID map:\n%s\n", uid_buf);
	printf("GID map:\n%s\n", gid_buf);
#endif

	snprintf(path, MAP_PATH_LEN, "/proc/%d/uid_map", pid);
	if ((f = fopen(path, "w")) == NULL)
		ERR_EXIT("fopen UID map");
	if (fwrite(uid_buf, 1, strlen(uid_buf), f) < strlen(uid_buf))
		ERR_EXIT("fwrite UID map");
	fclose(f);

	snprintf(path, MAP_PATH_LEN, "/proc/%d/gid_map", pid);
	if ((f = fopen(path, "w")) == NULL)
		ERR_EXIT("fopen GID map");
	if (fwrite(gid_buf, 1, strlen(gid_buf), f) < strlen(gid_buf))
		ERR_EXIT("fwrite GID map");
	fclose(f);
}

static void parse_arguments(int argc, char *argv[])
{
	/* setup and parse arguments */
	args.gid = getgid();
	args.uid = getuid();
	args.mapped_gid = args.mapped_uid = 0;
	args.user_ns = args.smack_ns = 0;
	args.exe_path = NULL;
	argp_parse(&argp, argc, argv, 0, 0, &args);

	/* Categorize test environment ID. */
	test_environment_id = 0;

	/* smack_ns implies user_ns */
	if (args.smack_ns) {
		test_environment_id |= TEST_ENV_NS_SMACK;
		args.user_ns = 1;
	} else if (args.user_ns) {
		test_environment_id |= TEST_ENV_NS_USER;
	}

	if (args.user_ns && args.mapped_uid != 0)
		test_environment_id |= TEST_ENV_USER_REGULAR;
	else if (!args.user_ns && args.uid != 0)
		test_environment_id |= TEST_ENV_USER_REGULAR;

	// TODO: remove in the final version
	printf("===========================================================\n");
	printf("Test env: %d (user ns: %s, smack ns: %s, "
	       "real UID = %d, real GID = %d",
	       test_environment_id, args.user_ns ? "ON" : "OFF",
	       args.smack_ns ? "ON" : "OFF", args.uid, args.gid);

	if (args.user_ns)
		printf(", mapped UID = %d, mapped GID = %d)\n", args.mapped_uid,
		       args.mapped_gid);
	else
		printf(")\n");
}

/*
 * Remove directory recursively
 */
int remove_directory(const char *path)
{
	struct stat statbuf;
	struct dirent *p;
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	int r = -1;

	if (d) {
		r = 0;
		while (!r && (p = readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;

			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf) {
				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf)) {
					if (S_ISDIR(statbuf.st_mode))
						r2 = remove_directory(buf);
					else
						r2 = unlink(buf);
				}
				free(buf);
			}
			r = r2;
		}
		closedir(d);
	}

	if (!r)
		r = rmdir(path);

	return r;
}

int main(int argc, char *argv[])
{
	int unshare_flags = 0;
	char pid_str[8], env_str[8];
	char uid_str[16], gid_str[16];
	char *child_argv[9];
	pid_t children[2], sibling_pid;
	int status, exit_helper, exit_namespace;
	int pipe_to_helper[2]; /* parent -> helper pipe */
	int pipe_to_parent[2]; /* helper -> parent pipe */

	if (getuid() != 0 || getgid() != 0) {
		printf("Must be root!\n");
		return EXIT_FAILURE;
	}

	/*
	 * disable stdout buffering (there will be multiple processes writing
	 * to the same stdout)
	 */
	setbuf(stdout, NULL);

	parse_arguments(argc, argv);

	write_smack_rules(smack_rules);

	if (pipe(pipe_to_parent) < 0)
		ERR_EXIT("pipe()");
	if (pipe(pipe_to_helper) < 0)
		ERR_EXIT("pipe()");

	/* create "tmp" directory */
	umask(0);
	remove_directory("tmp");
	if (mkdir("tmp", S_IRWXU | S_IRWXG | S_IRWXO) != 0)
		ERR_EXIT("mkdir()");
	if (smack_set_file_label("tmp", "shared", SMACK_LABEL_ACCESS, 0) != 0)
		ERR_EXIT("smack_set_file_label()");

	/* first fork (helper) */
	children[CHILD_HELPER] = fork();

	if (children[CHILD_HELPER] < 0)
		ERR_EXIT("fork()");

	/* helper process (run always as root) */
	if (children[CHILD_HELPER] == 0) {
		close(pipe_to_helper[WRITE_END]);
		close(pipe_to_parent[READ_END]);

#ifdef PRINT_DEBUG
		printf("%d: waiting for signal from sibling...\n", getpid());
#endif
		/* retrieve sibling pid */
		if (read(pipe_to_helper[READ_END], &sibling_pid, sizeof(pid_t))
		    == -1)
			ERR_EXIT("pipe_ab read");

		if (args.smack_ns)
			write_smack_map(sibling_pid, smack_map);
		if (args.user_ns)
			write_uid_maps(sibling_pid);

		if (smack_set_self_label(OUTSIDE_PROC_LABEL))
			ERR_EXIT("smack_set_self_label()");

		/* trigger sibling to do execve() */
#ifdef PRINT_DEBUG
		printf("%d: sending signal back to parent...\n", getpid());
#endif
		status = 1;
		if (write(pipe_to_parent[WRITE_END], &status, sizeof(int)) == -1)
			ERR_EXIT("write()");

		close(STDIN_FILENO);
		close(STDERR_FILENO);
		dup2(pipe_to_helper[READ_END], STDIN_FILENO);
		dup2(pipe_to_parent[WRITE_END], STDERR_FILENO);

		sprintf(pid_str, "%d", sibling_pid);
		sprintf(env_str, "%d", test_environment_id);
		sprintf(uid_str, "%d", args.uid);
		sprintf(gid_str, "%d", args.gid);
		child_argv[0] = args.exe_path;
		child_argv[1] = ID_OUTSIDE_NS;
		child_argv[2] = pid_str;
		child_argv[3] = env_str;
		child_argv[4] = uid_str;
		child_argv[5] = gid_str;
		child_argv[6] = NULL;
		execve(args.exe_path, child_argv, environ);
		ERR_EXIT("execve()");
	}

	close(pipe_to_helper[READ_END]);
	close(pipe_to_parent[WRITE_END]);


	/* second fork */
	children[CHILD_NAMESPACES] = fork();
	if (children[CHILD_NAMESPACES] < 0)
		ERR_EXIT("fork()");

	/* child in namespaces */
	if (children[CHILD_NAMESPACES] == 0) {
		if (smack_set_self_label(INSIDE_PROC_LABEL))
			ERR_EXIT("smack_set_self_label()");

		/* drop auxilary groups, they can interfere */
		setgroups(0, NULL);

		/* enter user namespace without capabilities */
		if (args.user_ns) {
			/* Switch to a target "real" UID / GID */
			if (args.uid != 0 && setuid(args.uid) != 0)
				ERR_EXIT("setuid()");
			if (args.gid != 0 && setgid(args.gid) != 0)
				ERR_EXIT("setgid()");
		}

		if (args.user_ns)
			unshare_flags |= CLONE_NEWUSER;
		//if (args.smack_ns)
		//	unshare_flags |= CLONE_NEWLSM;
		unshare_flags |= CLONE_NEWNS; /* mount namespace */

		/* enter namespaces */
		if (unshare(unshare_flags) == -1)
			ERR_EXIT("unshare()");

#ifdef PRINT_DEBUG
		printf("%d: unshare done, triggering helper...\n", getpid());
#endif
		/* send namespaced process PID to the sibling */
		size_t pid = getpid();
		if (write(pipe_to_helper[WRITE_END], &pid, sizeof(pid_t)) == -1)
			ERR_EXIT("write()");
		/* mappings fill occurs now */
		if (read(pipe_to_parent[READ_END], &status, sizeof(int)) == -1)
			ERR_EXIT("read()");

		/* Switch to a target "mapped" UID / GID inside namespace. */
		if (args.user_ns) {
			if (args.mapped_uid != 0 &&
			    setuid(args.mapped_uid) != 0)
				ERR_EXIT("setuid()");
			if (args.mapped_gid != 0 &&
			    setgid(args.mapped_gid) != 0)
				ERR_EXIT("setgid()");
		} else {
			/*
			 * When not using USER NS, switch user after namespace
			 * creation. Unshare to any namespace without USER is
			 * a privileged operation.
			 */
			if (args.uid != 0 && setuid(args.uid) != 0)
				ERR_EXIT("setuid()");
			if (args.gid != 0 && setgid(args.gid) != 0)
				ERR_EXIT("setgid()");
		}

		close(STDIN_FILENO);
		close(STDERR_FILENO);
		dup2(pipe_to_parent[READ_END], STDIN_FILENO);
		dup2(pipe_to_helper[WRITE_END], STDERR_FILENO);

		sprintf(pid_str, "%d", children[CHILD_HELPER]);
		sprintf(env_str, "%d", test_environment_id);
		sprintf(uid_str, "%d", args.uid);
		sprintf(gid_str, "%d", args.gid);
		child_argv[0] = args.exe_path;
		child_argv[1] = ID_INSIDE_NS;
		child_argv[2] = pid_str;
		child_argv[3] = env_str;
		child_argv[4] = uid_str;
		child_argv[5] = gid_str;
		child_argv[6] = NULL;
		execve(args.exe_path, child_argv, environ);
		ERR_EXIT("execve()");
	}

	/* we dont't need any pipes in parent process */
	close(pipe_to_helper[WRITE_END]);
	close(pipe_to_parent[READ_END]);

	/* wait for children */

	waitpid(children[CHILD_HELPER], &status, 0);
	exit_helper = WEXITSTATUS(status);
#ifdef PRINT_DEBUG
	printf("Helper exited with exit code: %d\n", exit_helper);
#endif

	waitpid(children[CHILD_NAMESPACES], &status, 0);
	exit_namespace = WEXITSTATUS(status);
#ifdef PRINT_DEBUG
	printf("NS child exited with exit code: %d\n", exit_namespace);
#endif

	/*
	 * Cleanup
	 */
	if (rmdir("tmp") == -1) {
		printf(ANSI_COLOR_YELLOW "WARNING: the test did not clean up "
		       "after itself.\n" ANSI_COLOR_RESET);
		if (remove_directory("tmp") == -1)
			perror("remove_directory()");
	}

	/*
	 * TODO: consider restoring the rules (maybe they are used by system
	 * already?)
	 */
	smack_set_rule(INSIDE_PROC_LABEL, "_", "-");
	smack_set_rule(INSIDE_PROC_LABEL, SHARED_OBJECT_LABEL, "-");

	/* Success */
	if (exit_helper == 0 && exit_namespace == 0) {
		printf(ANSI_COLOR_GREEN "Passed.\n" ANSI_COLOR_RESET);
		return 0;
	}

	/* Failure */
	if (exit_helper != 0)
		printf(ANSI_COLOR_RED "%d: Failed.\n" ANSI_COLOR_RESET,
		       children[CHILD_HELPER]);
	if (exit_namespace != 0)
		printf(ANSI_COLOR_RED "%d: Failed.\n" ANSI_COLOR_RESET,
		       children[CHILD_NAMESPACES]);
	return 1;
}
