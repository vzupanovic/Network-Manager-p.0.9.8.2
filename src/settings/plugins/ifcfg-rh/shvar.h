/*
 * shvar.h
 *
 * Interface for non-destructively reading/writing files containing
 * only shell variable declarations and full-line comments.
 *
 * Includes explicit inheritance mechanism intended for use with
 * Red Hat Linux ifcfg-* files.  There is no protection against
 * inheritance loops; they will generally cause stack overflows.
 * Furthermore, they are only intended for one level of inheritance;
 * the value setting algorithm assumes this.
 *
 * Copyright 1999 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#ifndef _SHVAR_H
#define _SHVAR_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _shvarFile shvarFile;
struct _shvarFile {
	char		*fileName;	/* read-only */
	int		fd;		/* read-only */
	char		*arena;		/* ignore */
	GList		*lineList;	/* read-only */
	GList		*current;	/* set implicitly or explicitly,
					   points to element of lineList */
	shvarFile	*parent;	/* set explicitly */
	int		modified;	/* ignore */
};


/* Create the file <name>, return shvarFile on success, NULL on failure */
shvarFile *
svCreateFile(const char *name);

/* Open the file <name>, return shvarFile on success, NULL on failure */
shvarFile *
svNewFile(const char *name);

/* Get the value associated with the key, and leave the current pointer
 * pointing at the line containing the value.  The char* returned MUST
 * be freed by the caller.
 */
char *
svGetValue(shvarFile *s, const char *key, gboolean verbatim);

/* return 1 if <key> resolves to any truth value (e.g. "yes", "y", "true")
 * return 0 if <key> resolves to any non-truth value (e.g. "no", "n", "false")
 * return <def> otherwise
 */
int
svTrueValue(shvarFile *s, const char *key, int def);

/* Set the variable <key> equal to the value <value>.
 * If <key> does not exist, and the <current> pointer is set, append
 * the key=value pair after that line.  Otherwise, prepend the pair
 * to the top of the file.
 */
void
svSetValue(shvarFile *s, const char *key, const char *value, gboolean verbatim);


/* Write the current contents iff modified.  Returns -1 on error
 * and 0 on success.  Do not write if no values have been modified.
 * The mode argument is only used if creating the file, not if
 * re-writing an existing file, and is passed unchanged to the
 * open() syscall.
 */
int
svWriteFile(shvarFile *s, int mode);

/* Close the file descriptor (if open) and delete the shvarFile.
 * Returns -1 on error and 0 on success.
 */
int
svCloseFile(shvarFile *s);

/* Return a new escaped string */
char *
svEscape(const char *s);

/* Unescape a string in-place */
void
svUnescape(char *s);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ! _SHVAR_H */
