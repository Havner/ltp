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
 * Helper module for maniupulating smack labels.
 * Based on libsmack (https://github.com/smack-team/smack/)
 *
 * Author: Michal Witanowski <m.witanowski@samsung.com>
 */

#ifndef __SMACK_COMMON_H
#define __SMACK_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <attr/xattr.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>

#define SMACK_MAGIC 0x43415d53 // "SMAC"
#define ACCESS_LEN 7
#define SMACK_MNT_PATH "/smack/"
#define SMACK_LABEL_MAX_LEN 255
#define LOAD_MAX_LEN (2 * (SMACK_LABEL_MAX_LEN + 1) + ACCESS_LEN)
#define CHG_RULE_MAX_LEN (2 * (SMACK_LABEL_MAX_LEN + 1) + 2 * ACCESS_LEN + 1)

enum smack_label_type {
	SMACK_LABEL_ACCESS = 0,
	SMACK_LABEL_EXEC,
	SMACK_LABEL_MMAP,
	SMACK_LABEL_TRANSMUTE,
	SMACK_LABEL_IPIN,
	SMACK_LABEL_IPOUT,
};

static inline const char* smack_xattr_name(enum smack_label_type type)
{
	switch (type) {
	case SMACK_LABEL_ACCESS:
		return "security.SMACK64";
	case SMACK_LABEL_EXEC:
		return "security.SMACK64EXEC";
	case SMACK_LABEL_MMAP:
		return "security.SMACK64MMAP";
	case SMACK_LABEL_TRANSMUTE:
		return "security.SMACK64TRANSMUTE";
	case SMACK_LABEL_IPIN:
		return "security.SMACK64IPIN";
	case SMACK_LABEL_IPOUT:
		return "security.SMACK64IPOUT";
	default:
		return NULL;
	}
}

/*
 * Translate access type to the format readable by SMACK's "access" interface.
 * For example: passing "arx" will return "r-xa---"
 */
static inline void parse_access_type(const char *in, char out[ACCESS_LEN + 1])
{
	int i;
	memset(out, '-', ACCESS_LEN);
	out[ACCESS_LEN] = '\0';

	for (i = 0; in[i] != '\0'; i++)
		switch (in[i]) {
		case 'r':
		case 'R':
			out[0] = 'r';
			break;
		case 'w':
		case 'W':
			out[1] = 'w';
			break;
		case 'x':
		case 'X':
			out[2] = 'x';
			break;
		case 'a':
		case 'A':
			out[3] = 'a';
			break;
		case 't':
		case 'T':
			out[4] = 't';
			break;
		case 'l':
		case 'L':
			out[5] = 'l';
			break;
		case 'b':
		case 'B':
			out[6] = 'b';
			break;
		default:
			break;
		}
}

/*
 * Get SMACK label of a file.
 * Returns 0 on success.
 * Obtained label is stored under "label" pointer.
 * User is responsible for freeing memory.
 */
static inline int smack_get_file_label(const char *file_path, char **label,
				       enum smack_label_type label_type,
				       int follow_links)
{
	char value[SMACK_LABEL_MAX_LEN + 1];
	int ret;
	const char* xattr_name;

	xattr_name = smack_xattr_name(label_type);

	if (follow_links)
		ret = getxattr(file_path, xattr_name, value,
		SMACK_LABEL_MAX_LEN + 1);
	else
		ret = lgetxattr(file_path, xattr_name, value,
		SMACK_LABEL_MAX_LEN + 1);

	if (ret == -1) {
		if (errno == ENODATA) {
			*label = NULL;
			return 0;
		}
		return -1;
	}

	value[ret] = '\0';
	*label = strdup(value);
	return 0;
}

/*
 * Set SMACK label of a file.
 * Returns 0 on success.
 */
static inline int smack_set_file_label(const char *file_path, const char *label,
				       enum smack_label_type label_type,
				       int follow_links)
{
	int ret = 0;
	const char* xattr_name;

	xattr_name = smack_xattr_name(label_type);

	if (label == NULL || label[0] == '\0') {
		if (follow_links)
			ret = removexattr(file_path, xattr_name);
		else
			ret = lremovexattr(file_path, xattr_name);

		if (ret == -1 && errno == ENODATA)
			ret = 0;
	} else {
		int len = strnlen(label, SMACK_LABEL_MAX_LEN + 1);
		if (len > SMACK_LABEL_MAX_LEN)
			return -1;

		if (follow_links)
			ret = setxattr(file_path, xattr_name, label, len, 0);
		else
			ret = lsetxattr(file_path, xattr_name, label, len, 0);
	}

	return ret;
}

/*
 * Get SMACK label of a process.
 * Returns 0 on success.
 * Obtained label is stored under "label" pointer.
 * User is responsible for freeing memory.
 */
static inline int smack_get_process_label(pid_t pid, char **label)
{
	char *result;
	int fd, ret;
	char path[255];

	result = calloc(SMACK_LABEL_MAX_LEN + 1, 1);
	if (result == NULL) {
		errno = ENOMEM;
		return -1;
	}

	snprintf(path, 255, "/proc/%d/attr/current", pid);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		free(result);
		return -1;
	}

	ret = read(fd, result, SMACK_LABEL_MAX_LEN);
	close(fd);
	if (ret == -1) {
		free(result);
		return -1;
	}

	*label = result;
	return 0;
}

/*
 * Set SMACK label of a calling process.
 * Returns 0 on success.
 */
static inline int smack_set_self_label(const char *label)
{
	int len, fd, ret;

	len = strnlen(label, SMACK_LABEL_MAX_LEN + 1);
	if (len > SMACK_LABEL_MAX_LEN) {
		errno = EINVAL;
		return -1;
	}

	fd = open("/proc/self/attr/current", O_WRONLY);
	if (fd == -1)
		return -1;

	ret = write(fd, label, len);
	close(fd);
	return (ret == -1) ? -1 : 0;
}

/*
 * Set access rule for specified object and subject.
 * Returns 0 on success.
 */
static inline int smack_set_rule(const char *subject, const char *object,
				 const char *access)
{
	char buf[LOAD_MAX_LEN + 1];
	int ret;
	int fd;

	fd = open(SMACK_MNT_PATH "load2", O_WRONLY);
	if (fd < 0)
		// TODO: fallback to "load"
		return -1;

	ret = snprintf(buf, LOAD_MAX_LEN + 1, "%s %s %s", subject, object,
		       access);
	if (ret < 0) {
		errno = EINVAL;
		ret = -1;
	} else
		ret = write(fd, buf, strlen(buf));

	close(fd);
	return ret;
}

/*
 * Change existing rule.
 * Returns 0 on success.
 */
static inline int smack_change_rule(const char *subject, const char *object,
				    const char *allow, const char *deny)
{
	char buf[CHG_RULE_MAX_LEN + 1];
	int ret;
	int fd;

	if (allow == NULL || strlen(allow) == 0)
		allow = "-";
	if (deny == NULL || strlen(deny) == 0)
		deny = "-";

	fd = open(SMACK_MNT_PATH "change-rule", O_WRONLY);
	if (fd == -1)
		return -1;

	ret = snprintf(buf, CHG_RULE_MAX_LEN + 1, "%s %s %s %s", subject,
		       object, allow, deny);
	if (ret < 0) {
		errno = EINVAL;
		ret = -1;
		goto err_out;
	}

	ret = write(fd, buf, strlen(buf));

	err_out: close(fd);
	return ret;
}

/*
 * Check if a subject has 'access_type' access to an object.
 */
static inline int smack_have_access(const char *subject, const char *object,
				    const char *access_type)
{
	char buf[LOAD_MAX_LEN + 1];
	char access_type_kernel[ACCESS_LEN + 1];
	int ret;
	int fd;
	int access2 = 1;

	fd = open(SMACK_MNT_PATH "access2", O_RDWR);
	if (fd == -1) {
		if (errno != ENOENT)
			return -1;

		fd = open(SMACK_MNT_PATH "access", O_RDWR);
		if (fd == -1)
			return -1;
		access2 = 0;
	}

	parse_access_type(access_type, access_type_kernel);

	if (access2)
		ret = snprintf(buf, LOAD_MAX_LEN + 1, "%s %s %s", subject,
			       object, access_type_kernel);
	else
		ret = snprintf(buf, LOAD_MAX_LEN + 1, "%-23s %-23s %5.5s",
			       subject, object, access_type_kernel);

	if (ret < 0) {
		close(fd);
		return -1;
	}

	ret = write(fd, buf, strlen(buf));
	if (ret == -1) {
		close(fd);
		return -1;
	}

	ret = read(fd, buf, 1);
	close(fd);
	if (ret == -1)
		return -1;

	return buf[0] == '1';
}

/*
 * Check if Smack if present on the system.
 * This should be done in every testcase before calling any other function
 * from this header.
 */
static inline int verify_smackmnt(void)
{
	struct statfs sfbuf;
	int rc;

	do {
		rc = statfs(SMACK_MNT_PATH, &sfbuf);
	} while (rc < 0 && errno == EINTR);

	if (rc == 0 && (uint32_t)sfbuf.f_type == (uint32_t)SMACK_MAGIC)
		return 0;

	return -1;
}

#endif // __SMACK_COMMON_H
