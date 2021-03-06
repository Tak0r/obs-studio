/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <time.h>

#include "dstr.h"
#include "platform.h"

void *os_dlopen(const char *path)
{
	struct dstr dylib_name;

	if (!path)
		return NULL;

	dstr_init_copy(&dylib_name, path);
	if (!dstr_find(&dylib_name, ".so"))
		dstr_cat(&dylib_name, ".so");

	void *res = dlopen(dylib_name.array, RTLD_LAZY);
	if (!res)
		blog(LOG_ERROR, "os_dlopen(%s->%s): %s\n",
				path, dylib_name.array, dlerror());

	dstr_free(&dylib_name);
	return res;
}

void *os_dlsym(void *module, const char *func)
{
	return dlsym(module, func);
}

void os_dlclose(void *module)
{
	dlclose(module);
}

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t current = os_gettime_ns();
	if (time_target < current)
		return false;

	time_target -= current;

	struct timespec req, remain;
	memset(&req, 0, sizeof(req));
	memset(&remain, 0, sizeof(remain));
	req.tv_sec = time_target/1000000000;
	req.tv_nsec = time_target%1000000000;

	while (nanosleep(&req, &remain)) {
		req = remain;
		memset(&remain, 0, sizeof(remain));
	}

	return true;
}

void os_sleep_ms(uint32_t duration)
{
	usleep(duration*1000);
}

#if !defined(__APPLE__)

uint64_t os_gettime_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec);
}

/* should return $HOME/.[name] */
char *os_get_config_path(const char *name)
{
	char *path_ptr = getenv("HOME");
	if (path_ptr == NULL)
		bcrash("Could not get $HOME\n");

	struct dstr path;
	dstr_init_copy(&path, path_ptr);
	dstr_cat(&path, "/.");
	dstr_cat(&path, name);
	return path.array;
}

#endif

bool os_file_exists(const char *path)
{
	return access(path, F_OK) == 0;
}

struct os_dir {
	const char       *path;
	DIR              *dir;
	struct dirent    *cur_dirent;
	struct os_dirent out;
};

os_dir_t os_opendir(const char *path)
{
	struct os_dir *dir;
	DIR           *dir_val;

	dir_val = opendir(path);
	if (!dir_val)
		return NULL;

	dir = bzalloc(sizeof(struct os_dir));
	dir->dir  = dir_val;
	dir->path = path;
	return dir;
}

struct os_dirent *os_readdir(os_dir_t dir)
{
	struct dstr file_path = {0};
	struct stat stat_info;

	if (!dir) return NULL;

	dir->cur_dirent = readdir(dir->dir);
	if (!dir->cur_dirent)
		return NULL;

	strncpy(dir->out.d_name, dir->cur_dirent->d_name, 255);

	dstr_copy(&file_path, dir->path);
	dstr_cat(&file_path, "/");
	dstr_cat(&file_path, dir->out.d_name);

	if (stat(file_path.array, &stat_info) == 0)
		dir->out.directory = !!S_ISDIR(stat_info.st_mode);
	else
		blog(LOG_DEBUG, FILE_LINE "stat for %s failed, errno: %d",
				file_path.array, errno);

	dstr_free(&file_path);

	return &dir->out;
}

void os_closedir(os_dir_t dir)
{
	if (dir) {
		closedir(dir->dir);
		bfree(dir);
	}
}

int os_unlink(const char *path)
{
	return unlink(path);
}

int os_mkdir(const char *path)
{
	if (mkdir(path, 0777) == 0)
		return MKDIR_SUCCESS;

	return (errno == EEXIST) ? MKDIR_EXISTS : MKDIR_ERROR;
}
