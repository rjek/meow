/*
 * msim_hostfs.c: a directory on the host, lent to the program under
 * simulation through BNV #-18.  r0 is the operation and r1 to r4 its
 * arguments; the result, or a negative errno, comes back in ir.  Paths
 * are relative to the directory given with -H and may not climb out of
 * it.  Only a development aid: real hardware has no host.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "msim_core.h"
#include "msim_hostfs.h"

#define MAXHANDLES 32
#define PATHLEN 1024

/* Catflap's values, which the program uses */
#define HF_O_ACCMODE	3
#define HF_O_WRONLY	1
#define HF_O_RDWR	2
#define HF_O_CREAT	0x40
#define HF_O_TRUNC	0x200
#define HF_V_FILE	0
#define HF_V_DIR	1

enum { OP_PROBE, OP_OPEN, OP_CLOSE, OP_READ, OP_WRITE, OP_STAT, OP_READDIR,
       OP_MKDIR, OP_UNLINK };

struct hostfs {
	char root[PATHLEN];
	int fds[MAXHANDLES];
};

static int copy_in(struct msim_ctx *ctx, u_int32_t addr, char *buf, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		buf[i] = (char)msim_memget(ctx, addr + i, MSIM_ACCESS_BYTE);
		if (buf[i] == '\0') {
			return 0;
		}
	}
	return -ENAMETOOLONG;
}

static void copy_out(struct msim_ctx *ctx, u_int32_t addr, const void *src, size_t n)
{
	const unsigned char *p = src;
	size_t i;

	for (i = 0; i < n; i++) {
		msim_memset(ctx, addr + i, MSIM_ACCESS_BYTE, p[i]);
	}
}

static void put_word(struct msim_ctx *ctx, u_int32_t addr, u_int32_t v)
{
	msim_memset(ctx, addr, MSIM_ACCESS_WORD, v);
}

/* The host path for a program's path, or a negative errno.  Components
 * of ".." are refused outright rather than resolved. */
static int host_path(struct msim_ctx *ctx, struct hostfs *h, u_int32_t addr,
		     char *out)
{
	char rel[PATHLEN];
	const char *p;
	int rc = copy_in(ctx, addr, rel, sizeof rel);

	if (rc < 0) {
		return rc;
	}
	for (p = rel; *p != '\0';) {
		const char *end = strchr(p, '/');
		size_t n = end == NULL ? strlen(p) : (size_t)(end - p);

		if (n == 2 && p[0] == '.' && p[1] == '.') {
			return -EACCES;
		}
		p += n;
		while (*p == '/') {
			p++;
		}
	}
	while (rel[0] == '/') {
		memmove(rel, rel + 1, strlen(rel));
	}
	if (strlen(h->root) + 1 + strlen(rel) + 1 > PATHLEN) {
		return -ENAMETOOLONG;
	}
	sprintf(out, "%s/%s", h->root, rel);
	return 0;
}

static int handle_fd(struct hostfs *h, u_int32_t handle)
{
	if (handle >= MAXHANDLES || h->fds[handle] < 0) {
		return -EBADF;
	}
	return h->fds[handle];
}

static int do_open(struct msim_ctx *ctx, struct hostfs *h)
{
	char path[PATHLEN];
	u_int32_t flags = ctx->r[2];
	int oflags, fd, i, rc = host_path(ctx, h, ctx->r[1], path);

	if (rc < 0) {
		return rc;
	}
	switch (flags & HF_O_ACCMODE) {
	case HF_O_WRONLY: oflags = O_WRONLY; break;
	case HF_O_RDWR: oflags = O_RDWR; break;
	default: oflags = O_RDONLY; break;
	}
	if ((flags & HF_O_CREAT) != 0) {
		oflags |= O_CREAT;
	}
	if ((flags & HF_O_TRUNC) != 0) {
		oflags |= O_TRUNC;
	}
	for (i = 0; i < MAXHANDLES && h->fds[i] >= 0; i++) {
	}
	if (i == MAXHANDLES) {
		return -EMFILE;
	}
	fd = open(path, oflags, 0666);
	if (fd < 0) {
		return -errno;
	}
	h->fds[i] = fd;
	return i;
}

static int do_rw(struct msim_ctx *ctx, struct hostfs *h, int write)
{
	int fd = handle_fd(h, ctx->r[1]);
	u_int32_t buf = ctx->r[2], len = ctx->r[3], off = ctx->r[4];
	char *tmp;
	ssize_t n;

	if (fd < 0) {
		return fd;
	}
	tmp = malloc(len > 0 ? len : 1);
	if (tmp == NULL) {
		return -ENOMEM;
	}
	if (write != 0) {
		u_int32_t i;

		for (i = 0; i < len; i++) {
			tmp[i] = (char)msim_memget(ctx, buf + i, MSIM_ACCESS_BYTE);
		}
		n = pwrite(fd, tmp, len, off);
	} else {
		n = pread(fd, tmp, len, off);
		if (n > 0) {
			copy_out(ctx, buf, tmp, (size_t)n);
		}
	}
	free(tmp);
	return n < 0 ? -errno : (int)n;
}

static int do_stat(struct msim_ctx *ctx, struct hostfs *h)
{
	char path[PATHLEN];
	struct stat st;
	int rc = host_path(ctx, h, ctx->r[1], path);

	if (rc < 0) {
		return rc;
	}
	if (stat(path, &st) != 0) {
		return -errno;
	}
	put_word(ctx, ctx->r[2], S_ISDIR(st.st_mode) ? HF_V_DIR : HF_V_FILE);
	put_word(ctx, ctx->r[2] + 4, (u_int32_t)st.st_size);
	put_word(ctx, ctx->r[2] + 8, (u_int32_t)st.st_ino);
	return 0;
}

/* The index-th entry, skipping . and ..: a name of up to 31 bytes, its
 * type and its size, laid out as Catflap's struct dirent. */
static int do_readdir(struct msim_ctx *ctx, struct hostfs *h)
{
	char path[PATHLEN], full[PATHLEN + 256], name[32];
	u_int32_t index = ctx->r[2], out = ctx->r[3], seen = 0;
	struct dirent *de;
	struct stat st;
	DIR *d;
	int rc = host_path(ctx, h, ctx->r[1], path);

	if (rc < 0) {
		return rc;
	}
	d = opendir(path);
	if (d == NULL) {
		return -errno;
	}
	while ((de = readdir(d)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0 ||
		    strlen(de->d_name) > 31) {
			continue;
		}
		if (seen++ != index) {
			continue;
		}
		memset(name, 0, sizeof name);
		strcpy(name, de->d_name);
		copy_out(ctx, out, name, sizeof name);
		snprintf(full, sizeof full, "%s/%s", path, de->d_name);
		if (stat(full, &st) != 0) {
			memset(&st, 0, sizeof st);
		}
		put_word(ctx, out + 32, S_ISDIR(st.st_mode) ? HF_V_DIR : HF_V_FILE);
		put_word(ctx, out + 36, S_ISDIR(st.st_mode) ? 0 : (u_int32_t)st.st_size);
		closedir(d);
		return 1;
	}
	closedir(d);
	return 0;
}

static void msim_hostfs_call(struct msim_ctx *ctx, signed int op, void *bnvctx)
{
	struct hostfs *h = bnvctx;
	char path[PATHLEN];
	int rc;

	if (h == NULL) {		/* no -H: nothing lent */
		ctx->r[MSIM_IR] = (u_int32_t)-ENOSYS;
		return;
	}
	switch (ctx->r[0]) {
	case OP_PROBE:
		rc = 0;
		break;
	case OP_OPEN:
		rc = do_open(ctx, h);
		break;
	case OP_CLOSE:
		rc = handle_fd(h, ctx->r[1]);
		if (rc >= 0) {
			close(rc);
			h->fds[ctx->r[1]] = -1;
			rc = 0;
		}
		break;
	case OP_READ:
		rc = do_rw(ctx, h, 0);
		break;
	case OP_WRITE:
		rc = do_rw(ctx, h, 1);
		break;
	case OP_STAT:
		rc = do_stat(ctx, h);
		break;
	case OP_READDIR:
		rc = do_readdir(ctx, h);
		break;
	case OP_MKDIR:
		rc = host_path(ctx, h, ctx->r[1], path);
		if (rc == 0 && mkdir(path, 0777) != 0) {
			rc = -errno;
		}
		break;
	case OP_UNLINK:
		rc = host_path(ctx, h, ctx->r[1], path);
		if (rc == 0 && unlink(path) != 0) {
			rc = errno == EISDIR || errno == EPERM ?
				(rmdir(path) == 0 ? 0 : -errno) : -errno;
		}
		break;
	default:
		rc = -ENOSYS;
		break;
	}
	ctx->r[MSIM_IR] = (u_int32_t)rc;
}

/* root may be NULL, when the call answers that there is no host */
void msim_add_hostfs(struct msim_ctx *ctx, const char *root)
{
	struct hostfs *h;
	int i;

	if (root == NULL) {
		msim_add_bnv(ctx, -18, msim_hostfs_call, NULL);
		return;
	}
	h = calloc(1, sizeof *h);
	if (h == NULL || strlen(root) >= PATHLEN) {
		fprintf(stderr, "msim: cannot lend %s\n", root);
		exit(2);
	}
	strcpy(h->root, root);
	while (strlen(h->root) > 1 && h->root[strlen(h->root) - 1] == '/') {
		h->root[strlen(h->root) - 1] = '\0';
	}
	for (i = 0; i < MAXHANDLES; i++) {
		h->fds[i] = -1;
	}
	msim_add_bnv(ctx, -18, msim_hostfs_call, h);
}
