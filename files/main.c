
#include <config.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>			/* Solaris */
#endif
#include "gettext.h"			/* Gnulib */
#include "configmake.h"
#define _(s) gettext(s)
#define N_(s) gettext_noop(s)
#include "version-etc.h"        	/* Gnulib */
#include "progname.h"			/* Gnulib */
//#include "strcase.h"			/* Gnulib */
#include "dirname.h"			/* Gnulib */
#include "common/error.h"
#include "xalloc.h"			/* Gnulib */
#include "common/intutil.h"
#include "common/io-utils.h"
#include "common/string-utils.h"
#include "wrestool.h"

#include <emscripten.h>

enum {
    OPT_VERSION = 1000,
    OPT_HELP
};

const char version_etc_copyright[] = "Copyright (C) 1998 Oskar Liljeblad";
bool arg_raw;
static FILE *verbose_file;
static int arg_verbosity;
static const char *arg_output;
static const char *arg_type;
static const char *arg_name;
static const char *arg_language;
static int arg_action;
static const char *res_types[] = {
    /* 0x01: */
    "cursor", "bitmap", "icon", "menu", "dialog", "string",
    "fontdir", "font", "accelerator", "rcdata", "messagelist",
    "group_cursor", NULL, "group_icon", NULL,
    /* the following are not defined in winbase.h, but found in wrc. */
    /* 0x10: */ 
    "version", "dlginclude", NULL, "plugplay", "vxd",
    "anicursor", "aniicon"
};
#define RES_TYPE_COUNT ((int)(sizeof(res_types)/sizeof(char *)))

static const char *res_type_string_to_id (const char *);
static const char *get_extract_extension (const char *);

/* res_type_id_to_string:
 *   Translate a numeric resource type to it's corresponding string type.
 *   (For informative-ness.)
 */
const char *
res_type_id_to_string (int id)
{
    if (id == 241)
	return "toolbar";
    if (id > 0 && id <= RES_TYPE_COUNT)
	return res_types[id-1];
    return NULL;
}

/* res_type_string_to_id:
 *   Translate a resource type string to integer.
 *   (Used to convert the --type option.)
 */
static const char *
res_type_string_to_id (const char *type)
{
    static const char *res_type_ids[] = {
	"-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "-10",
	"-11", "-12", NULL, "-14", NULL, "-16", "-17", NULL, "-19",
	"-20", "-21", "-22"
    };
    int c;

    if (type == NULL)
	return NULL;

    for (c = 0 ; c < RES_TYPE_COUNT ; c++) {
	if (res_types[c] != NULL && !strcasecmp(type, res_types[c]))
	    return res_type_ids[c];
    }

    return type;
}

/* get_extract_extension:
 *   Return extension for files of a certain resource type
 *
 */
static const char *
get_extract_extension (const char *type)
{
    uint16_t value;

    type = res_type_string_to_id(type);
    STRIP_RES_ID_FORMAT(type);
    if (parse_uint16(type, &value)) {
	if (value == 2)
	    return ".bmp";
	if (value == 14)
	    return ".ico";
	if (value == 12)
	    return ".cur";
    }

    return "";
}

#define SET_IF_NULL(x,def) ((x) = ((x) == NULL ? (def) : (x)))

/* get_destination_name:
 *   Make a filename for a resource that is to be extracted.
 */
const char *
get_destination_name (WinLibrary *fi, const char *type, const char *name, const char *lang)
{
    static char filename[1024];

    /* initialize --type, --name and --language options */
    SET_IF_NULL(type, "");
    SET_IF_NULL(name, "");
    if (!strcmp(lang, "1033"))
	lang = NULL;
    STRIP_RES_ID_FORMAT(type);
    STRIP_RES_ID_FORMAT(name);
    STRIP_RES_ID_FORMAT(lang);

    /* returning NULL means that output should be done to stdout */

    /* if --output not specified, write to STDOUT */
    if (arg_output == NULL)
	return NULL;

    /* if --output'ing to a directory, make filename */
    if (is_directory(arg_output) || ends_with(arg_output, "/")) {
	/* char *tmp = strdup(fi->name);
	if (tmp == NULL)
	    malloc_failure(); */

	snprintf (filename, 1024, "%s%s%s_%s_%s%s%s%s",
			  arg_output,
		      (ends_with(arg_output, "/") ? "" : "/"),
			  base_name(fi->name),
			  type,
			  name,
			  (lang != NULL && fi->is_PE_binary ? "_" : ""),
			  (lang != NULL && fi->is_PE_binary ? lang : ""),
			  get_extract_extension(type));
	/* free(tmp); */
	return filename;
    }

    /* otherwise, just return the --output argument */
    return arg_output;
}

EMSCRIPTEN_KEEPALIVE
int extract_ico(char *input, char *outdir) {
	int ret = 0;
	arg_type = res_type_string_to_id("14");
	arg_name = arg_language = NULL;
	arg_verbosity = 999;
	arg_raw = false;
	arg_action = ACTION_EXTRACT;
	arg_output = outdir;

	WinLibrary fi;
	
	/* initiate stuff */
	fi.file = NULL;
	fi.memory = NULL;

	/* get file size */
	fi.name = input;
	fi.total_size = file_size(fi.name);
	if (fi.total_size == -1) {
		die_errno("%s", fi.name);
		ret = 1;
		goto cleanup;
	}
	if (fi.total_size == 0) {
		warn(_("%s: file has a size of 0"), fi.name);
		goto cleanup;
	}

	/* open file */
	fi.file = fopen(fi.name, "rb");
	if (fi.file == NULL) {
		die_errno("%s", fi.name);
		ret = 2;
		goto cleanup;
	}
	
	/* read all of file */
	fi.memory = xmalloc(fi.total_size);
	if (fread(fi.memory, fi.total_size, 1, fi.file) != 1) {
		die_errno("%s", fi.name);
		ret = 3;
		goto cleanup;
	}

	/* identify file and find resource table */
	if (!read_library (&fi)) {
		/* error reported by read_library */
		ret = 4;
		goto cleanup;
	}

	/* warn about more unnecessary options */
	if (!fi.is_PE_binary && arg_language != NULL)
		warn(_("%s: --language has no effect because file is 16-bit binary"), fi.name);

	/* do the specified command */
	if (arg_action == ACTION_LIST) {
		do_resources (&fi, arg_type, arg_name, arg_language, print_resources_callback);
		/* errors will be printed by the callback */
	} else if (arg_action == ACTION_EXTRACT) {
		do_resources (&fi, arg_type, arg_name, arg_language, extract_resources_callback);
		/* errors will be printed by the callback */
	}

	/* free stuff and close file */
	cleanup:
	if (fi.file != NULL)
		fclose(fi.file);
	if (fi.memory != NULL)
		free(fi.memory);

	return ret;
}
