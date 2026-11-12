// SPDX-License-Identifier: Apache-2.0
/* Terminal tables (see table.h). */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "mapping/table.h"

#define CELL_SIZE 96

struct table_row {
	char (*cells)[CELL_SIZE];
	bool rule_before;	/* a horizontal rule above this row */
};

struct mp_table {
	int columns;
	char (*headers)[CELL_SIZE];
	bool *right_aligned;
	struct table_row *rows;
	int row_count;
	int row_capacity;
	bool pending_rule;
};

struct mp_table *mp_table_new(int columns)
{
	struct mp_table *table = calloc(1, sizeof(*table));

	if (!table)
		return NULL;
	table->columns = columns;
	table->headers = calloc((size_t)columns, sizeof(*table->headers));
	table->right_aligned = calloc((size_t)columns,
				      sizeof(*table->right_aligned));
	if (!table->headers || !table->right_aligned) {
		mp_table_free(table);
		return NULL;
	}
	return table;
}

void mp_table_header(struct mp_table *table, int column, const char *text,
		     bool right_aligned)
{
	if (!table || column < 0 || column >= table->columns)
		return;
	snprintf(table->headers[column], CELL_SIZE, "%s", text);
	table->right_aligned[column] = right_aligned;
}

void mp_table_row(struct mp_table *table)
{
	struct table_row *row;

	if (!table)
		return;
	if (table->row_count == table->row_capacity) {
		int capacity = table->row_capacity ? 2 * table->row_capacity : 16;
		struct table_row *rows = realloc(table->rows, (size_t)capacity *
						 sizeof(*rows));

		if (!rows)
			return;
		table->rows = rows;
		table->row_capacity = capacity;
	}
	row = &table->rows[table->row_count];
	row->cells = calloc((size_t)table->columns, sizeof(*row->cells));
	if (!row->cells)
		return;
	row->rule_before = table->pending_rule && table->row_count;
	table->pending_rule = false;
	table->row_count++;
}

void mp_table_rule(struct mp_table *table)
{
	if (table)
		table->pending_rule = true;
}

void mp_table_cell(struct mp_table *table, int column, const char *format,
		   ...)
{
	va_list args;

	if (!table || !table->row_count || column < 0 ||
	    column >= table->columns)
		return;
	va_start(args, format);
	vsnprintf(table->rows[table->row_count - 1].cells[column], CELL_SIZE,
		  format, args);
	va_end(args);
}

/* characters on screen of the UTF-8 string @text up to @end (or '\0') */
static int text_width(const char *text, const char *end)
{
	int width = 0;

	for (; *text && (!end || text < end); text++)
		if (((unsigned char)*text & 0xc0) != 0x80)
			width++;
	return width;
}

/* line @line (0 or 1) of a header: start and end pointers */
static void header_line(const char *header, int line, const char **start,
			const char **end)
{
	const char *newline = strchr(header, '\n');

	if (line == 0) {
		*start = header;
		*end = newline ? newline : header + strlen(header);
	} else {
		*start = newline ? newline + 1 : header + strlen(header);
		*end = *start + strlen(*start);
	}
}

static void print_padded(FILE *out, const char *start, const char *end,
			 int width, bool right)
{
	int padding = width - text_width(start, end);

	if (right)
		fprintf(out, "%*s", padding, "");
	fprintf(out, "%.*s", (int)(end - start), start);
	if (!right)
		fprintf(out, "%*s", padding, "");
}

/* a horizontal line: left, middle and right junctions */
static void print_rule(FILE *out, const int *widths, int columns,
		       const char *left, const char *middle, const char *right)
{
	int c, i;

	fprintf(out, "  %s", left);
	for (c = 0; c < columns; c++) {
		for (i = 0; i < widths[c] + 2; i++)
			fputs("─", out);
		fputs(c + 1 < columns ? middle : right, out);
	}
	fputc('\n', out);
}

void mp_table_print(const struct mp_table *table, FILE *out)
{
	int *widths, c, r, line, header_lines = 1;

	if (!table)
		return;
	widths = calloc((size_t)table->columns, sizeof(*widths));
	if (!widths)
		return;
	for (c = 0; c < table->columns; c++) {
		for (line = 0; line < 2; line++) {
			const char *start, *end;

			header_line(table->headers[c], line, &start, &end);
			if (text_width(start, end) > widths[c])
				widths[c] = text_width(start, end);
			if (line && end > start)
				header_lines = 2;
		}
		for (r = 0; r < table->row_count; r++)
			if (text_width(table->rows[r].cells[c], NULL) > widths[c])
				widths[c] = text_width(table->rows[r].cells[c],
						       NULL);
	}

	print_rule(out, widths, table->columns, "┌", "┬", "┐");
	for (line = 0; line < header_lines; line++) {
		fputs("  │", out);
		for (c = 0; c < table->columns; c++) {
			const char *start, *end;

			header_line(table->headers[c], line, &start, &end);
			fputc(' ', out);
			print_padded(out, start, end, widths[c],
				     table->right_aligned[c]);
			fputs(" │", out);
		}
		fputc('\n', out);
	}
	print_rule(out, widths, table->columns, "├", "┼", "┤");
	for (r = 0; r < table->row_count; r++) {
		const struct table_row *row = &table->rows[r];

		if (row->rule_before)
			print_rule(out, widths, table->columns, "├", "┼", "┤");
		fputs("  │", out);
		for (c = 0; c < table->columns; c++) {
			const char *cell = row->cells[c];

			fputc(' ', out);
			print_padded(out, cell, cell + strlen(cell), widths[c],
				     table->right_aligned[c]);
			fputs(" │", out);
		}
		fputc('\n', out);
	}
	print_rule(out, widths, table->columns, "└", "┴", "┘");
	free(widths);
}

void mp_table_free(struct mp_table *table)
{
	int r;

	if (!table)
		return;
	for (r = 0; r < table->row_count; r++)
		free(table->rows[r].cells);
	free(table->rows);
	free(table->headers);
	free(table->right_aligned);
	free(table);
}
