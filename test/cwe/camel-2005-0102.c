// RUN: %cc -m32 %s | intck | diagdiff %s --prefix=exp
// RUN: %cc -m64 %s | intck | diagdiff %s --prefix=exp
// RUN: %cc -D__PATCH__ -m32 %s | intck | diagdiff %s
// RUN: %cc -D__PATCH__ -m64 %s | intck | diagdiff %s
//
// http://git.gnome.org/browse/evolution-data-server/commit/camel/camel-lock-helper.c?id=0d1d403fab78b869867d50fcc6ee95f503925318

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned int	guint32;
typedef int		gint;
typedef char		gchar;

struct _CamelLockHelperMsg {
	guint32 magic;
	guint32 seq;
	guint32 id;
	guint32 data;
};

/* magic values */
enum {
	CAMEL_LOCK_HELPER_MAGIC = 0xABADF00D,
};

/* return status */
enum {
	CAMEL_LOCK_HELPER_STATUS_OK = 0,
	CAMEL_LOCK_HELPER_STATUS_PROTOCOL,
	CAMEL_LOCK_HELPER_STATUS_NOMEM,
};

/* commands */
enum {
	CAMEL_LOCK_HELPER_LOCK = 0xf0f,
	CAMEL_LOCK_HELPER_UNLOCK = 0xf0f0
};

int read_n(int fd, void *buf, size_t len);

gint lock_path (const gchar *path, guint32 *lockid);

int main()
{
	struct _CamelLockHelperMsg msg;
	gint len;
	gint res;
	gchar *path;

	len = read_n(STDIN_FILENO, &msg, sizeof(msg));
	if (len == 0)
		return 0;

	res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
	if (len == sizeof (msg) && msg.magic == CAMEL_LOCK_HELPER_MAGIC) {
		switch(msg.id) {
		case CAMEL_LOCK_HELPER_LOCK:
			res = CAMEL_LOCK_HELPER_STATUS_NOMEM;
#ifndef __PATCH__
			path = malloc(msg.data+1); // exp: {{uadd}}
			if (path != NULL) {
#else
			if (msg.data > 0xffff) {
				res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
			} else if ((path = malloc(msg.data+1)) != NULL) {
#endif
				res = CAMEL_LOCK_HELPER_STATUS_PROTOCOL;
				len = read_n(STDIN_FILENO, path, msg.data);
				if (len == msg.data) {
					path[len] = 0;
					lock_path(path, &msg.data);
				}
				free(path);
			}
		}
	}
}