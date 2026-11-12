/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_JSON_H
#define MAPPING_JSON_H

/*
 * A small JSON reader (RFC 8259) for the application and configuration
 * files: the whole text is parsed into a tree of values. Every value keeps
 * the line where it starts, for the error messages. Strings are UTF-8;
 * \u escapes are decoded (surrogate pairs included); a NUL character in a
 * string is refused.
 */

#include <stdbool.h>
#include <stddef.h>

enum mp_json_type {
	MP_JSON_NULL,
	MP_JSON_BOOL,
	MP_JSON_NUMBER,
	MP_JSON_STRING,
	MP_JSON_ARRAY,
	MP_JSON_OBJECT,
};

struct mp_json {
	enum mp_json_type type;
	int line;			/* where the value starts, from 1 */
	bool boolean;
	double number;
	char *string;			/* MP_JSON_STRING */
	struct mp_json *items;		/* array items, or object values */
	char **keys;			/* object keys, in file order */
	int count;
};

/*
 * mp_json_parse() - parse @text into @root
 * @error: set to a message with the line on failure
 * Return: 0, -EINVAL (syntax, duplicate key, nesting deeper than 64) or
 * -ENOMEM.
 */
int mp_json_parse(const char *text, struct mp_json *root, char *error,
		  size_t size);

/* free the tree of @value (not @value itself) */
void mp_json_free(struct mp_json *value);

/* the value of @key in @object, NULL if absent or not an object */
const struct mp_json *mp_json_get(const struct mp_json *object,
				  const char *key);

/* the name of @type, for the error messages ("a number", ...) */
const char *mp_json_type_name(enum mp_json_type type);

/* read the whole file @path into a new NUL-terminated buffer */
int mp_read_file(const char *path, char **text);

#endif /* MAPPING_JSON_H */
