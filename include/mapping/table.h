/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAPPING_TABLE_H
#define MAPPING_TABLE_H

#include <stdbool.h>
#include <stdio.h>

/*
 * Terminal tables drawn with box-drawing characters: every column is as
 * wide as its widest cell, text is aligned left and numbers right.
 *
 *	struct mp_table *table = mp_table_new(3);
 *	mp_table_header(table, 0, "core", false);
 *	...
 *	mp_table_row(table);
 *	mp_table_cell(table, 0, "%d", id);
 *	...
 *	mp_table_print(table, stdout);
 *	mp_table_free(table);
 *
 * A header may have two lines separated by '\n' (name and unit).
 */

struct mp_table;

/* a table of @columns columns, or NULL without memory */
struct mp_table *mp_table_new(int columns);

/* header of column @column; @right_aligned for numbers */
void mp_table_header(struct mp_table *table, int column, const char *text,
		     bool right_aligned);

/* start a new row (its cells are empty until set) */
void mp_table_row(struct mp_table *table);

/* a horizontal rule before the next row, to separate groups */
void mp_table_rule(struct mp_table *table);

/* set a cell of the current row, printf-like */
void mp_table_cell(struct mp_table *table, int column, const char *format,
		   ...) __attribute__((format(printf, 3, 4)));

/* print with an indentation of two spaces */
void mp_table_print(const struct mp_table *table, FILE *out);

void mp_table_free(struct mp_table *table);

#endif /* MAPPING_TABLE_H */
