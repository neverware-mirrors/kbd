/* dump.c
 *
 * This file is part of kbd project.
 * Copyright (C) 1993  Risto Kankkunen.
 * Copyright (C) 1993  Eugene G. Crosser.
 * Copyright (C) 1994-2007  Andries E. Brouwer.
 * Copyright (C) 2007-2013  Alexey Gladkov <gladkov.alexey@gmail.com>
 *
 * This file is covered by the GNU General Public License,
 * which should be included with kbd as the file COPYING.
 */
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "keymap.h"

#include "contextP.h"
#include "ksyms.h"
#include "modifiers.h"
#include "nls.h"

#define U(x) ((x) ^ 0xf000)

static void
outchar(FILE *fd, unsigned char c, int comma)
{
	fprintf(fd, "'");
	fprintf(fd, (c == '\'' || c == '\\') ? "\\%c"
	       : isgraph(c) ? "%c"
	       : "\\%03o", c);
	fprintf(fd, comma ? "', " : "'");
}

// FIXME: Merge outchar ?
static void
dumpchar(FILE *fd, unsigned char c, int comma)
{
	fprintf(fd, "'");
	fprintf(fd, (c == '\'' || c == '\\') ? "\\%c"
	       : (isgraph(c) || c == ' ' || c >= 0200) ? "%c"
	       : "\\%03o", c);
	fprintf(fd, comma ? "', " : "'");
}

int
lk_dump_bkeymap(struct lk_ctx *ctx, FILE *fd)
{
	unsigned int i, j;
	char magic[] = "bkeymap";

	if (lk_add_constants(ctx) < 0)
		return -1;

	if (fwrite(magic, 7, 1, fd) != 1)
		goto fail;

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		char flag;
		flag = lk_map_exists(ctx, i);

		if (fwrite(&flag, sizeof(flag), 1, fd) != 1)
			goto fail;
	}

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (!lk_map_exists(ctx, i))
			continue;

		for (j = 0; j < NR_KEYS / 2; j++) {
			unsigned short v;
			v = lk_get_key(ctx, i, j);

			if (fwrite(&v, sizeof(v), 1, fd) != 1)
				goto fail;
		}
	}

	return 0;

 fail:	ERR(ctx, _("Error writing map to file"));
	return -1;
}

static char *
mk_mapname(char modifier)
{
	static char *mods[8] = {
		"shift", "altgr", "ctrl", "alt", "shl", "shr", "ctl", "ctr"
	};
	static char buf[60];
	int i;

	if (!modifier)
		return "plain";
	buf[0] = 0;
	for (i = 0; i < 8; i++)
		if (modifier & (1 << i)) {
			if (buf[0])
				strcat(buf, "_");
			strcat(buf, mods[i]);
		}
	return buf;
}

int
lk_dump_ctable(struct lk_ctx *ctx, FILE *fd)
{
	int j;
	unsigned int i, imax;

	unsigned int maxfunc;
	unsigned int func_table_offs[MAX_NR_FUNC];
	unsigned int func_buf_offset = 0;
	struct lk_kbdiacr *kddiac;

	if (lk_add_constants(ctx) < 0)
		return -1;

	fprintf(fd,
/* not to be translated... */
		    "/* Do not edit this file! It was automatically generated by   */\n");
	fprintf(fd, "/*    loadkeys --mktable defkeymap.map > defkeymap.c          */\n\n");
	fprintf(fd, "#include <linux/keyboard.h>\n");
	fprintf(fd, "#include <linux/kd.h>\n\n");

	for (i = 0; i < MAX_NR_KEYMAPS; i++)
		if (lk_map_exists(ctx, i)) {
			if (i)
				fprintf(fd, "static ");
			fprintf(fd, "unsigned short %s_map[NR_KEYS] = {", mk_mapname(i));
			for (j = 0; j < NR_KEYS; j++) {
				if (!(j % 8))
					fprintf(fd, "\n");
				fprintf(fd, "\t0x%04x,", U(lk_get_key(ctx, i, j)));
			}
			fprintf(fd, "\n};\n\n");
		}

	for (imax = MAX_NR_KEYMAPS - 1; imax > 0; imax--)
		if (lk_map_exists(ctx, imax))
			break;
	fprintf(fd, "ushort *key_maps[MAX_NR_KEYMAPS] = {");
	for (i = 0; i <= imax; i++) {
		fprintf(fd, (i % 4) ? " " : "\n\t");
		if (lk_map_exists(ctx, i))
			fprintf(fd, "%s_map,", mk_mapname(i));
		else
			fprintf(fd, "0,");
	}
	if (imax < MAX_NR_KEYMAPS - 1)
		fprintf(fd, "\t0");
	fprintf(fd, "\n};\n\nunsigned int keymap_count = %u;\n\n", (unsigned int) ctx->keymap->count);

/* uglified just for xgettext - it complains about nonterminated strings */
	fprintf(fd,
	       "/*\n"
	       " * Philosophy: most people do not define more strings, but they who do\n"
	       " * often want quite a lot of string space. So, we statically allocate\n"
	       " * the default and allocate dynamically in chunks of 512 bytes.\n"
	       " */\n" "\n");
	for (maxfunc = MAX_NR_FUNC; maxfunc; maxfunc--)
		if (lk_array_get_ptr(ctx->func_table, maxfunc - 1))
			break;

	fprintf(fd, "char func_buf[] = {\n");
	for (i = 0; i < maxfunc; i++) {
		char *ptr, *func;

		func = ptr = lk_array_get_ptr(ctx->func_table, i);
		if (!ptr)
			continue;

		func_table_offs[i] = func_buf_offset;
		fprintf(fd, "\t");
		for (; *ptr; ptr++)
			outchar(fd, *ptr, 1);
		fprintf(fd, "0, \n");
		func_buf_offset += (ptr - func + 1);
	}
	if (!maxfunc)
		fprintf(fd, "\t0\n");
	fprintf(fd, "};\n\n");

	fprintf(fd,
	       "char *funcbufptr = func_buf;\n"
	       "int funcbufsize = sizeof(func_buf);\n"
	       "int funcbufleft = 0;          /* space left */\n" "\n");

	fprintf(fd, "char *func_table[MAX_NR_FUNC] = {\n");
	for (i = 0; i < maxfunc; i++) {
		if (lk_array_get_ptr(ctx->func_table, i))
			fprintf(fd, "\tfunc_buf + %u,\n", func_table_offs[i]);
		else
			fprintf(fd, "\t0,\n");
	}
	if (maxfunc < MAX_NR_FUNC)
		fprintf(fd, "\t0,\n");
	fprintf(fd, "};\n");

	if (ctx->flags & LK_FLAG_PREFER_UNICODE) {
		fprintf(fd, "\nstruct kbdiacruc accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < ctx->accent_table->count; i++) {
			kddiac = lk_array_get_ptr(ctx->accent_table, i);

			fprintf(fd, "\t{");
			outchar(fd, kddiac->diacr, 1);
			outchar(fd, kddiac->base, 1);
			fprintf(fd, "0x%04x},", kddiac->result);
			if (i % 2)
				fprintf(fd, "\n");
		}
		if (i % 2)
			fprintf(fd, "\n");
		fprintf(fd, "};\n\n");
	} else {
		fprintf(fd, "\nstruct kbdiacr accent_table[MAX_DIACR] = {\n");
		for (i = 0; i < ctx->accent_table->count; i++) {
			kddiac = lk_array_get_ptr(ctx->accent_table, i);

			fprintf(fd, "\t{");
			outchar(fd, kddiac->diacr, 1);
			outchar(fd, kddiac->base, 1);
			outchar(fd, kddiac->result, 0);
			fprintf(fd, "},");
			if (i % 2)
				fprintf(fd, "\n");
		}
		if (i % 2)
			fprintf(fd, "\n");
		fprintf(fd, "};\n\n");
	}
	fprintf(fd, "unsigned int accent_table_size = %u;\n",
		(unsigned int) ctx->accent_table->count);
	return 0;
}

/* void dump_funcs(void) */
void
lk_dump_funcs(struct lk_ctx *ctx, FILE *fd)
{
	unsigned int i;

	for (i = 0; i < ctx->func_table->total; i++) {
		char *ptr = lk_array_get_ptr(ctx->func_table, i);
		if (!ptr)
			continue;

		fprintf(fd, "string %s = \"", get_sym(ctx, KT_FN, i));

		for (; *ptr; ptr++) {
			if (*ptr == '"' || *ptr == '\\') {
				fputc('\\', fd);
				fputc(*ptr, fd);
			} else if (isgraph(*ptr) || *ptr == ' ') {
				fputc(*ptr, fd);
			} else {
				fprintf(fd, "\\%03hho", *ptr);
			}
		}
		fputc('"', fd);
		fputc('\n', fd);
	}
}

/* void dump_diacs(void) */
void
lk_dump_diacs(struct lk_ctx *ctx, FILE *fd)
{
	unsigned int i;
	struct lk_kbdiacr *ptr;
	const char *ksym;

	for (i = 0; i < ctx->accent_table->count; i++) {
		ptr = lk_array_get_ptr(ctx->accent_table, i);
		if (!ptr)
			continue;

		fprintf(fd, "compose ");
		dumpchar(fd, ptr->diacr, 0);
		fprintf(fd, " ");
		dumpchar(fd, ptr->base, 0);
#ifdef KDGKBDIACRUC
		if (ctx->flags & LK_FLAG_PREFER_UNICODE) {
			ksym = codetoksym(ctx, ptr->result ^ 0xf000);
			if (ksym) {
				fprintf(fd, " to %s\n", ksym);
			} else {
				fprintf(fd, " to U+%04x\n", ptr->result);
			}
		} else
#endif
		if (ptr->result > 0xff) {
			// impossible case?
			fprintf(fd, " to 0x%x\n", ptr->result);
		} else {
			ksym = codetoksym(ctx, ptr->result);
			if (ksym) {
				fprintf(fd, " to %s\n", ksym);
			} else {
				fprintf(fd, " to ");
				dumpchar(fd, ptr->result, 0);
				fprintf(fd, "\n");
			}
		}
	}
}

void
lk_dump_keymaps(struct lk_ctx *ctx, FILE *fd)
{
	unsigned int i;
	int n, m, s;
	i = n = m = s = 0;

	fprintf(fd, "keymaps");

	for (i = 0; i < ctx->keymap->total; i++) {
		if (ctx->keywords & LK_KEYWORD_ALTISMETA && i == (i | M_ALT))
			continue;

		if (!lk_map_exists(ctx, i)) {
			if (!m)
				continue;
			n--, m--;
			(n == m)
				? fprintf(fd, "%c%d"   , (s ? ',' : ' '), n)
				: fprintf(fd, "%c%d-%d", (s ? ',' : ' '), n, m);
			n = m = 0;
			s = 1;
		} else {
			if (!n)
				n = i+1;
			m = i+1;
		}
	}

	if (m) {
		n--, m--;
		(n == m)
			? fprintf(fd, "%c%d"   , (s ? ',' : ' '), n)
			: fprintf(fd, "%c%d-%d", (s ? ',' : ' '), n, m);
	}

	fprintf(fd, "\n");
}

static void
print_mod(FILE *fd, int x)
{
	if (x) {
		modifier_t *mod = (modifier_t *) modifiers;
		while (mod->name) {
			if (x & (1 << mod->bit))
				fprintf(fd, "%s\t", mod->name);
			mod++;
		}
	} else {
		fprintf(fd, "plain\t");
	}
}

static void
print_keysym(struct lk_ctx *ctx, FILE *fd, int code, char numeric)
{
	unsigned int t, v;
	const char *p;
	int plus;

	fprintf(fd, " ");
	t = KTYP(code);
	v = KVAL(code);
	if (t >= syms_size) {
		if (!numeric && (p = codetoksym(ctx, code)) != NULL)
			fprintf(fd, "%-16s", p);
		else
			fprintf(fd, "U+%04x          ", code ^ 0xf000);
		return;
	}
	plus = 0;
	if (t == KT_LETTER) {
		t = KT_LATIN;
		fprintf(fd, "+");
		plus++;
	}
	if (!numeric && t == KT_LATIN &&
	    (p = codetoksym(ctx, code)))
		fprintf(fd, "%-*s", 16 - plus, p);
	else if (!numeric && t < syms_size && v < get_sym_size(ctx, t) &&
	    (p = get_sym(ctx, t, v))[0])
		fprintf(fd, "%-*s", 16 - plus, p);
	else if (!numeric && t == KT_META && v < 128 && v < get_sym_size(ctx, KT_LATIN) &&
		 (p = get_sym(ctx, KT_LATIN, v))[0])
		fprintf(fd, "Meta_%-11s", p);
	else
		fprintf(fd, "0x%04x         %s", code, plus ? "" : " ");
}

static void
print_bind(struct lk_ctx *ctx, FILE *fd, int bufj, int i, int j, char numeric)
{
	if(j)
		fprintf(fd, "\t");
	print_mod(fd, j);
	fprintf(fd, "keycode %3d =", i);
	print_keysym(ctx, fd, bufj, numeric);
	fprintf(fd, "\n");
}

void
lk_dump_keys(struct lk_ctx *ctx, FILE *fd, lk_table_shape table, char numeric)
{
	unsigned int i, j;
	int buf[MAX_NR_KEYMAPS];
	int isletter, islatin, isasexpected;
	int typ, val;
	int alt_is_meta = 0;
	int all_holes;
	int zapped[MAX_NR_KEYMAPS];
	unsigned int keymapnr = ctx->keymap->total;

	if (!keymapnr)
		return;

	if (table == LK_SHAPE_FULL_TABLE || table == LK_SHAPE_SEPARATE_LINES)
		goto no_shorthands;

	/* first pass: determine whether to set alt_is_meta */
	for (j = 0; j < ctx->keymap->total; j++) {
		unsigned int ja = (j | M_ALT);

		if (!(j != ja && lk_map_exists(ctx, j) && lk_map_exists(ctx, ja)))
			continue;

		for (i = 1; i < NR_KEYS; i++) {
			int buf0, buf1, type;

			buf0 = lk_get_key(ctx, j, i);

			if (buf0 == -1)
				break;

			type = KTYP(buf0);

			if ((type == KT_LATIN || type == KT_LETTER) && KVAL(buf0) < 128) {
				buf1 = lk_map_exists(ctx, ja)
					? lk_get_key(ctx, ja, i)
					: -1;

				if (buf1 != K(KT_META, KVAL(buf0)))
					goto not_alt_is_meta;
			}
		}
	}
	alt_is_meta = 1;
	fprintf(fd, "alt_is_meta\n");

not_alt_is_meta:
no_shorthands:

	for (i = 1; i < NR_KEYS; i++) {
		all_holes = 1;

		for (j = 0; j < keymapnr; j++) {
			buf[j] = K_HOLE;

			if (lk_map_exists(ctx, j))
				buf[j] = lk_get_key(ctx, j, i);

			if (buf[j] != K_HOLE)
				all_holes = 0;
		}

		if (all_holes && table != LK_SHAPE_FULL_TABLE)
			continue;

		if (table == LK_SHAPE_FULL_TABLE) {
			fprintf(fd, "keycode %3d =", i);

			for (j = 0; j < keymapnr; j++) {
				if (lk_map_exists(ctx, j))
					print_keysym(ctx, fd, buf[j], numeric);
			}

			fprintf(fd, "\n");
			continue;
		}

		if (table == LK_SHAPE_SEPARATE_LINES) {
			for (j = 0; j < keymapnr; j++) {
				//if (buf[j] != K_HOLE)
				print_bind(ctx, fd, buf[j], i, j, numeric);
			}

			fprintf(fd, "\n");
			continue;
		}

		typ = KTYP(buf[0]);
		val = KVAL(buf[0]);
		islatin = (typ == KT_LATIN || typ == KT_LETTER);
		isletter = (islatin &&
			((val >= 'A' && val <= 'Z') ||
			 (val >= 'a' && val <= 'z')));

		isasexpected = 0;
		if (isletter) {
			unsigned short defs[16];
			defs[0] = K(KT_LETTER, val);
			defs[1] = K(KT_LETTER, val ^ 32);
			defs[2] = defs[0];
			defs[3] = defs[1];

			for (j = 4; j < 8; j++)
				defs[j] = K(KT_LATIN, val & ~96);

			for (j = 8; j < 16; j++)
				defs[j] = K(KT_META, KVAL(defs[j-8]));

			for (j = 0; j < keymapnr; j++) {
				if ((j >= 16 && buf[j] != K_HOLE) || (j < 16 && buf[j] != defs[j]))
					goto unexpected;
			}

			isasexpected = 1;
		}
unexpected:

		/* wipe out predictable meta bindings */
		for (j = 0; j < keymapnr; j++)
			zapped[j] = 0;

		if (alt_is_meta) {
			for(j = 0; j < keymapnr; j++) {
				unsigned int ja, ktyp;
				ja = (j | M_ALT);

				if (j != ja && lk_map_exists(ctx, ja)
				    && ((ktyp=KTYP(buf[j])) == KT_LATIN || ktyp == KT_LETTER)
				    && KVAL(buf[j]) < 128) {
					if (buf[ja] != K(KT_META, KVAL(buf[j])))
						fprintf(stderr, _("impossible: not meta?\n"));
					buf[ja] = K_HOLE;
					zapped[ja] = 1;
				}
			}
		}

		fprintf(fd, "keycode %3d =", i);

		if (isasexpected) {
			/* print only a single entry */
			/* suppress the + for ordinary a-zA-Z */
			print_keysym(ctx, fd, K(KT_LATIN, val), numeric);
			fprintf(fd, "\n");
		} else {
			/* choose between single entry line followed by exceptions,
			   and long line followed by exceptions; avoid VoidSymbol */
			unsigned int bad, count;
			bad = count = 0;

			for (j = 1; j < keymapnr; j++) {
				if (zapped[j])
					continue;

				if (buf[j] != buf[0])
					bad++;

				if (buf[j] != K_HOLE)
					count++;
			}

			if (bad <= count && bad < keymapnr-1) {
				if (buf[0] != K_HOLE) {
					print_keysym(ctx, fd, buf[0], numeric);
				}
				fprintf(fd, "\n");

				for (j = 1; j < keymapnr; j++) {
					if (buf[j] != buf[0] && !zapped[j]) {
						print_bind(ctx, fd, buf[j], i, j, numeric);
					}
				}
			} else {
				for (j = 0;
				     j < keymapnr && buf[j] != K_HOLE &&
				        (table != LK_SHAPE_UNTIL_HOLE || lk_map_exists(ctx, j));
				     j++) {
					//print_bind(ctx, fd, buf[j], i, j, numeric);
					print_keysym(ctx, fd, buf[j], numeric);
				}
				fprintf(fd, "\n");

				for (; j < keymapnr; j++) {
					if (buf[j] != K_HOLE) {
						print_bind(ctx, fd, buf[j], i, j, numeric);
					}
				}
			}
		}
	}
}

void
lk_dump_keymap(struct lk_ctx *ctx, FILE *fd, lk_table_shape table, char numeric)
{
	lk_dump_keymaps(ctx, fd);
	lk_dump_keys(ctx, fd, table, numeric);
	lk_dump_funcs(ctx, fd);
}
