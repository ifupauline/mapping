// SPDX-License-Identifier: Apache-2.0
/* A small JSON reader (see json.h). */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/json.h"

#define MAX_DEPTH 64

struct parser {
	const char *p;
	int line;
	int depth;
	char *error;
	size_t size;
};

static int fail(struct parser *ps, const char *what)
{
	snprintf(ps->error, ps->size, "line %d: %s", ps->line, what);
	return -EINVAL;
}

static void skip_space(struct parser *ps)
{
	for (;;) {
		char c = *ps->p;

		if (c == '\n')
			ps->line++;
		else if (c != ' ' && c != '\t' && c != '\r')
			return;
		ps->p++;
	}
}

static int hex4(struct parser *ps, unsigned *value)
{
	int i;

	*value = 0;
	for (i = 0; i < 4; i++) {
		char c = ps->p[i];
		unsigned digit;

		if (c >= '0' && c <= '9')
			digit = (unsigned)(c - '0');
		else if (c >= 'a' && c <= 'f')
			digit = (unsigned)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			digit = (unsigned)(c - 'A' + 10);
		else
			return fail(ps, "bad \\u escape");
		*value = *value * 16 + digit;
	}
	ps->p += 4;
	return 0;
}

/* append the UTF-8 encoding of @code to @out at *@n */
static void put_utf8(char *out, size_t *n, unsigned code)
{
	if (code < 0x80) {
		out[(*n)++] = (char)code;
	} else if (code < 0x800) {
		out[(*n)++] = (char)(0xc0 | (code >> 6));
		out[(*n)++] = (char)(0x80 | (code & 0x3f));
	} else if (code < 0x10000) {
		out[(*n)++] = (char)(0xe0 | (code >> 12));
		out[(*n)++] = (char)(0x80 | ((code >> 6) & 0x3f));
		out[(*n)++] = (char)(0x80 | (code & 0x3f));
	} else {
		out[(*n)++] = (char)(0xf0 | (code >> 18));
		out[(*n)++] = (char)(0x80 | ((code >> 12) & 0x3f));
		out[(*n)++] = (char)(0x80 | ((code >> 6) & 0x3f));
		out[(*n)++] = (char)(0x80 | (code & 0x3f));
	}
}

/* a string at ps->p (on the opening quote) into a new buffer */
static int parse_string(struct parser *ps, char **result)
{
	const char *start = ++ps->p;
	size_t n = 0, capacity;
	char *out;
	int err;

	/* the decoded string is never longer than the source */
	while (*ps->p && *ps->p != '"') {
		if (*ps->p == '\\' && ps->p[1])
			ps->p++;
		ps->p++;
	}
	if (!*ps->p)
		return fail(ps, "unterminated string");
	capacity = (size_t)(ps->p - start) + 1;
	out = malloc(capacity);
	if (!out)
		return -ENOMEM;
	ps->p = start;
	while (*ps->p != '"') {
		unsigned char c = (unsigned char)*ps->p;

		if (c < 0x20) {
			err = fail(ps, "control character in a string");
			goto err_free;
		}
		if (c != '\\') {
			out[n++] = (char)c;
			ps->p++;
			continue;
		}
		ps->p++;
		switch (*ps->p++) {
		case '"': out[n++] = '"'; break;
		case '\\': out[n++] = '\\'; break;
		case '/': out[n++] = '/'; break;
		case 'b': out[n++] = '\b'; break;
		case 'f': out[n++] = '\f'; break;
		case 'n': out[n++] = '\n'; break;
		case 'r': out[n++] = '\r'; break;
		case 't': out[n++] = '\t'; break;
		case 'u': {
			unsigned code, low;

			err = hex4(ps, &code);
			if (err)
				goto err_free;
			if (code >= 0xd800 && code < 0xdc00) {
				if (ps->p[0] != '\\' || ps->p[1] != 'u') {
					err = fail(ps, "lone surrogate");
					goto err_free;
				}
				ps->p += 2;
				err = hex4(ps, &low);
				if (err)
					goto err_free;
				if (low < 0xdc00 || low >= 0xe000) {
					err = fail(ps, "bad surrogate pair");
					goto err_free;
				}
				code = 0x10000 + ((code - 0xd800) << 10) +
				       (low - 0xdc00);
			} else if (code >= 0xdc00 && code < 0xe000) {
				err = fail(ps, "lone surrogate");
				goto err_free;
			}
			if (!code) {
				err = fail(ps, "NUL character in a string");
				goto err_free;
			}
			put_utf8(out, &n, code);
			break;
		}
		default:
			err = fail(ps, "bad escape in a string");
			goto err_free;
		}
	}
	ps->p++;
	out[n] = '\0';
	*result = out;
	return 0;

err_free:
	free(out);
	return err;
}

static int parse_number(struct parser *ps, double *value)
{
	const char *start = ps->p;
	char *end;

	/* the JSON grammar: -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)? */
	if (*ps->p == '-')
		ps->p++;
	if (*ps->p == '0')
		ps->p++;
	else if (*ps->p >= '1' && *ps->p <= '9')
		while (*ps->p >= '0' && *ps->p <= '9')
			ps->p++;
	else
		return fail(ps, "bad number");
	if (*ps->p == '.') {
		ps->p++;
		if (*ps->p < '0' || *ps->p > '9')
			return fail(ps, "bad number");
		while (*ps->p >= '0' && *ps->p <= '9')
			ps->p++;
	}
	if (*ps->p == 'e' || *ps->p == 'E') {
		ps->p++;
		if (*ps->p == '+' || *ps->p == '-')
			ps->p++;
		if (*ps->p < '0' || *ps->p > '9')
			return fail(ps, "bad number");
		while (*ps->p >= '0' && *ps->p <= '9')
			ps->p++;
	}
	*value = strtod(start, &end);
	if (end != ps->p || !isfinite(*value))
		return fail(ps, "bad number");
	return 0;
}

static int parse_value(struct parser *ps, struct mp_json *value);

/* grow the items (and keys) of @value by one */
static int grow(struct mp_json *value, int *capacity)
{
	if (value->count < *capacity)
		return 0;
	int bigger = *capacity ? 2 * *capacity : 4;
	struct mp_json *items = realloc(value->items,
					(size_t)bigger * sizeof(*items));

	if (!items)
		return -ENOMEM;
	value->items = items;
	if (value->type == MP_JSON_OBJECT) {
		char **keys = realloc(value->keys,
				      (size_t)bigger * sizeof(*keys));

		if (!keys)
			return -ENOMEM;
		value->keys = keys;
	}
	*capacity = bigger;
	return 0;
}

static int parse_container(struct parser *ps, struct mp_json *value)
{
	bool object = *ps->p == '{';
	char close = object ? '}' : ']';
	int capacity = 0, err, i;

	value->type = object ? MP_JSON_OBJECT : MP_JSON_ARRAY;
	if (++ps->depth > MAX_DEPTH)
		return fail(ps, "nesting too deep");
	ps->p++;
	skip_space(ps);
	if (*ps->p == close) {
		ps->p++;
		ps->depth--;
		return 0;
	}
	for (;;) {
		char *key = NULL;

		skip_space(ps);
		if (object) {
			if (*ps->p != '"')
				return fail(ps, "expected a key in quotes");
			err = parse_string(ps, &key);
			if (err)
				return err;
			for (i = 0; i < value->count; i++)
				if (!strcmp(value->keys[i], key)) {
					free(key);
					return fail(ps, "duplicate key");
				}
			skip_space(ps);
			if (*ps->p != ':') {
				free(key);
				return fail(ps, "expected ':'");
			}
			ps->p++;
		}
		err = grow(value, &capacity);
		if (err) {
			free(key);
			return err;
		}
		memset(&value->items[value->count], 0,
		       sizeof(value->items[value->count]));
		if (object)
			value->keys[value->count] = key;
		value->count++;
		err = parse_value(ps, &value->items[value->count - 1]);
		if (err)
			return err;
		skip_space(ps);
		if (*ps->p == ',') {
			ps->p++;
			continue;
		}
		if (*ps->p == close) {
			ps->p++;
			ps->depth--;
			return 0;
		}
		return fail(ps, object ? "expected ',' or '}'" :
			    "expected ',' or ']'");
	}
}

static int parse_value(struct parser *ps, struct mp_json *value)
{
	skip_space(ps);
	value->line = ps->line;
	switch (*ps->p) {
	case '{':
	case '[':
		return parse_container(ps, value);
	case '"':
		value->type = MP_JSON_STRING;
		return parse_string(ps, &value->string);
	case 't':
		if (strncmp(ps->p, "true", 4))
			break;
		value->type = MP_JSON_BOOL;
		value->boolean = true;
		ps->p += 4;
		return 0;
	case 'f':
		if (strncmp(ps->p, "false", 5))
			break;
		value->type = MP_JSON_BOOL;
		ps->p += 5;
		return 0;
	case 'n':
		if (strncmp(ps->p, "null", 4))
			break;
		value->type = MP_JSON_NULL;
		ps->p += 4;
		return 0;
	default:
		if (*ps->p == '-' || (*ps->p >= '0' && *ps->p <= '9')) {
			value->type = MP_JSON_NUMBER;
			return parse_number(ps, &value->number);
		}
		break;
	}
	return fail(ps, *ps->p ? "unexpected character" :
		    "unexpected end of the file");
}

int mp_json_parse(const char *text, struct mp_json *root, char *error,
		  size_t size)
{
	struct parser ps = {text, 1, 0, error, size};
	int err;

	memset(root, 0, sizeof(*root));
	/* a UTF-8 byte order mark is allowed */
	if (!strncmp(ps.p, "\xef\xbb\xbf", 3))
		ps.p += 3;
	err = parse_value(&ps, root);
	if (!err) {
		skip_space(&ps);
		if (*ps.p)
			err = fail(&ps, "text after the end of the document");
	}
	if (err) {
		if (err == -ENOMEM)
			snprintf(error, size, "out of memory");
		mp_json_free(root);
	}
	return err;
}

void mp_json_free(struct mp_json *value)
{
	int i;

	for (i = 0; i < value->count; i++) {
		mp_json_free(&value->items[i]);
		if (value->keys)
			free(value->keys[i]);
	}
	free(value->items);
	free(value->keys);
	free(value->string);
	memset(value, 0, sizeof(*value));
}

const struct mp_json *mp_json_get(const struct mp_json *object,
				  const char *key)
{
	int i;

	if (!object || object->type != MP_JSON_OBJECT)
		return NULL;
	for (i = 0; i < object->count; i++)
		if (!strcmp(object->keys[i], key))
			return &object->items[i];
	return NULL;
}

const char *mp_json_type_name(enum mp_json_type type)
{
	static const char *const names[] = {
		"null", "true or false", "a number", "a string", "an array",
		"an object",
	};

	return (unsigned)type < sizeof(names) / sizeof(names[0]) ?
	       names[type] : "?";
}

int mp_read_file(const char *path, char **text)
{
	FILE *in = fopen(path, "rb");
	size_t used = 0, capacity = 4096, got;
	char *buffer;

	*text = NULL;
	if (!in)
		return -errno;
	buffer = malloc(capacity);
	if (!buffer) {
		fclose(in);
		return -ENOMEM;
	}
	while ((got = fread(buffer + used, 1, capacity - used - 1, in)) > 0) {
		used += got;
		if (used + 1 == capacity) {
			char *bigger = realloc(buffer, 2 * capacity);

			if (!bigger) {
				free(buffer);
				fclose(in);
				return -ENOMEM;
			}
			buffer = bigger;
			capacity *= 2;
		}
	}
	if (ferror(in)) {
		free(buffer);
		fclose(in);
		return -EIO;
	}
	fclose(in);
	buffer[used] = '\0';
	if (strlen(buffer) != used) {
		free(buffer);
		return -EILSEQ;		/* a NUL byte in the file */
	}
	*text = buffer;
	return 0;
}
