/* Copyright (C) 2002 Timo Sirainen */

#include "lib.h"
#include "ibuffer.h"
#include "hostpid.h"
#include "message-size.h"
#include "message-part-serialize.h"
#include "mail-index.h"
#include "mail-index-util.h"

#include <unistd.h>
#include <fcntl.h>

int index_set_error(MailIndex *index, const char *fmt, ...)
{
	va_list va;

	i_free(index->error);

	if (fmt == NULL)
		index->error = NULL;
	else {
		va_start(va, fmt);
		index->error = i_strdup_vprintf(fmt, va);
		va_end(va);

		i_error("%s", index->error);
	}

	return FALSE;
}

int index_set_corrupted(MailIndex *index, const char *fmt, ...)
{
	va_list va;

	INDEX_MARK_CORRUPTED(index);
	index->inconsistent = TRUE;

	va_start(va, fmt);
	t_push();
	index_set_error(index, "Corrupted index file %s: %s",
			index->filepath, t_strdup_vprintf(fmt, va));
	t_pop();
	va_end(va);

	return FALSE;
}

int index_set_syscall_error(MailIndex *index, const char *function)
{
	i_assert(function != NULL);

	index_set_error(index, "%s failed with index file %s: %m",
			function, index->filepath);
	return FALSE;
}

int index_file_set_syscall_error(MailIndex *index, const char *filepath,
				 const char *function)
{
	i_assert(filepath != NULL);
	i_assert(function != NULL);

	index_set_error(index, "%s failed with file %s: %m",
			function, filepath);

	return FALSE;
}

void index_reset_error(MailIndex *index)
{
	if (index->error != NULL) {
		i_free(index->error);
		index->error = NULL;
	}

	index->nodiskspace = FALSE;
}

int mail_index_create_temp_file(MailIndex *index, const char **path)
{
	int fd;

	hostpid_init();

	/* use ".temp.host.pid" as temporary file name. unlink() it first,
	   just to be sure it's not symlinked somewhere for some reason.. */
	*path = t_strconcat(index->dir, "/.temp.",
			    my_hostname, ".", my_pid, NULL);
	(void)unlink(*path);

	/* usage of O_EXCL isn't exactly needed since the path should be
	   trusted, but it shouldn't hurt either - if creating file fails
	   because of it, it's because something must be wrong (race
	   condition). also, might not won't work through NFS but that
	   can't be helped. */
	fd = open(*path, O_RDWR | O_CREAT | O_EXCL, 0660);
	if (fd == -1) {
		if (errno == ENOSPC)
			index->nodiskspace = TRUE;

		index_set_error(index, "Can't create temp index %s: %m", *path);
	}

	return fd;
}
