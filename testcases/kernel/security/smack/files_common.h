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

#ifndef __FILES_COMMON_H
#define __FILES_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "smack_common.h"

#define REGULAR_FILE_CONTENT 0xAA
#define REGULAR_FILE_SIZE 64

enum file_type {
	regular = 0,
	executable
};

enum dir_flags {
	none = 0,
	transmute
};

/*
 * Create a file with a given permissions and Smack label.
 * The file will be owned by an user of a process in namespaces.
 * The file will contain REGULAR_FILE_SIZE bytes equal to
 * REGULAR_FILE_CONTENT if regular file or a small shell script
 * if an executable file.
 */
static inline int file_create(const char *path, mode_t mode,
			      uid_t uid, gid_t gid,
			      enum file_type file_type,
			      const char *label_access,
			      const char *label_exec,
			      const char *label_mmap)
{
	int fd, data_size, ret;
	char data_regular[REGULAR_FILE_SIZE];
	const char* data_executable = "#!/bin/bash\n";
	const char* data;

	memset(data_regular, REGULAR_FILE_CONTENT, REGULAR_FILE_SIZE);

	fd = open(path, O_CREAT | O_WRONLY, mode);
	if (fd < 0)
		return -1;

	switch (file_type) {
	case regular:
		data = data_regular;
		data_size = REGULAR_FILE_SIZE;
		break;
	case executable:
		data = data_executable;
		data_size = strlen(data_executable);
		break;
	default:
		close(fd);
		errno = -EINVAL;
		return -1;
	}

	ret = write(fd, data, data_size);
	close(fd);
	if (ret != data_size)
		goto error;

	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		ret = chown(path, uid, gid);
		if (ret != 0)
			goto error;
	}

	if (label_access != NULL) {
		ret = smack_set_file_label(path, label_access,
					   SMACK_LABEL_ACCESS, 0);
		if (ret != 0)
			goto error;
	}

	if (label_exec != NULL) {
		ret = smack_set_file_label(path, label_exec,
					   SMACK_LABEL_EXEC, 0);
		if (ret != 0)
			goto error;
	}

	if (label_mmap != NULL) {
		ret = smack_set_file_label(path, label_mmap,
					   SMACK_LABEL_MMAP, 0);
		if (ret != 0)
			goto error;
	}

	return 0;

error:
	remove(path);
	return -1;
}

/*
 * Create a directory with a given permissions and Smack label.
 * The directory will be owned by an user of a process in namespaces.
 */
static inline int dir_create(const char *path, mode_t mode,
			     uid_t uid, gid_t gid,
			     const char *label_access,
			     enum dir_flags flags)
{
	int ret;

	ret = mkdir(path, mode);
	if (ret != 0)
		return -1;

	ret = chown(path, uid, gid);
	if (ret != 0)
		goto error;

	if (label_access != NULL) {
		ret = smack_set_file_label(path, label_access,
					   SMACK_LABEL_ACCESS, 0);
		if (ret != 0)
			goto error;
	}

	if (flags == transmute) {
		ret = smack_set_file_label(path, "TRUE",
					   SMACK_LABEL_TRANSMUTE, 0);
		if (ret != 0)
			goto error;
	}

	return 0;

error:
	rmdir(path);
	return -1;
}

#endif // __FILES_COMMON_H
