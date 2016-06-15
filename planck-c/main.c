#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <JavaScriptCore/JavaScript.h>

#include "linenoise.h"

#include "bundle.h"
#include "zip.h"

#define PLANCK_VERSION "0.0.2"

#define CONSOLE_LOG_BUF_SIZE 1000
char console_log_buf[CONSOLE_LOG_BUF_SIZE];

int exit_value = 0;
struct src_path {
	char *type;
	char *path;
};
struct src_path *src_paths = NULL;
int num_src_paths = 0;
char *out_path = NULL;

JSStringRef to_string(JSContextRef ctx, JSValueRef val);
JSValueRef evaluate_script(JSContextRef ctx, char *script, char *source);

char *value_to_c_string(JSContextRef ctx, JSValueRef val);

JSValueRef evaluate_source(JSContextRef ctx, char *type, char *source_value, bool expression, bool print_nil, char *set_ns, char *theme);
char *munge(char *s);

void bootstrap(JSContextRef ctx, char *out_path);
JSObjectRef get_function(JSContextRef ctx, char *namespace, char *name);

char* get_contents(char *path, time_t *last_modified);
void write_contents(char *path, char *contents);
int mkdir_p(char *path);

int str_has_suffix(char *str, char *suffix);
int str_has_prefix(char *str, char *prefix);
char *str_concat(char *s1, char *s2);

#ifdef DEBUG
#define debug_print_value(prefix, ctx, val)	print_value(prefix ": ", ctx, val)
#else
#define debug_print_value(prefix, ctx, val)
#endif

void print_value(char *prefix, JSContextRef ctx, JSValueRef val) {
	if (val != NULL) {
		JSStringRef str = to_string(ctx, val);
		char *ex_str = value_to_c_string(ctx, JSValueMakeString(ctx, str));
		printf("%s%s\n", prefix, ex_str);
		free(ex_str);
	}
}

JSValueRef function_console_log(JSContextRef ctx, JSObjectRef function, JSObjectRef this_object,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	for (int i = 0; i < argc; i++) {
		if (i > 0) {
			fprintf(stdout, " ");
		}

		JSStringRef str = to_string(ctx, args[i]);
		JSStringGetUTF8CString(str, console_log_buf, CONSOLE_LOG_BUF_SIZE);
		fprintf(stdout, "%s", console_log_buf);
	}
	fprintf(stdout, "\n");

	return JSValueMakeUndefined(ctx);
}

JSValueRef function_console_error(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	for (int i = 0; i < argc; i++) {
		if (i > 0) {
			fprintf(stderr, " ");
		}

		JSStringRef str = to_string(ctx, args[i]);
		JSStringGetUTF8CString(str, console_log_buf, CONSOLE_LOG_BUF_SIZE);
		fprintf(stderr, "%s", console_log_buf);
	}
	fprintf(stderr, "\n");

	return JSValueMakeUndefined(ctx);
}

JSValueRef function_read_file(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	// TODO: implement fully

	if (argc == 1 && JSValueGetType(ctx, args[0]) == kJSTypeString) {
		char path[100];
		JSStringRef path_str = JSValueToStringCopy(ctx, args[0], NULL);
		assert(JSStringGetLength(path_str) < 100);
		JSStringGetUTF8CString(path_str, path, 100);
		JSStringRelease(path_str);

		// debug_print_value("read_file", ctx, args[0]);

		time_t last_modified = 0;
		char *contents = get_contents(path, &last_modified);
		if (contents != NULL) {
			JSStringRef contents_str = JSStringCreateWithUTF8CString(contents);
			free(contents);

			JSValueRef res[2];
			res[0] = JSValueMakeString(ctx, contents_str);
			res[1] = JSValueMakeNumber(ctx, last_modified);
			return JSObjectMakeArray(ctx, 2, res, NULL);
		}
	}

	return JSValueMakeNull(ctx);
}

JSValueRef function_load(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	// TODO: implement fully

	if (argc == 1 && JSValueGetType(ctx, args[0]) == kJSTypeString) {
		char path[100];
		JSStringRef path_str = JSValueToStringCopy(ctx, args[0], NULL);
		assert(JSStringGetLength(path_str) < 100);
		JSStringGetUTF8CString(path_str, path, 100);
		JSStringRelease(path_str);

		// debug_print_value("load", ctx, args[0]);

		time_t last_modified = 0;
		char *contents = NULL;

		bool developing = (num_src_paths == 1 &&
		                   strcmp(src_paths[0].type, "src") == 0 &&
		                   str_has_suffix(src_paths[0].path, "/planck-cljs/src/") == 0);

		if (!developing) {
			contents = bundle_get_contents(path);
			last_modified = 0;
		}

		// load from classpath
		if (contents == NULL) {
			for (int i = 0; i < num_src_paths; i++) {
				char *type = src_paths[i].type;
				char *location = src_paths[i].path;

				if (strcmp(type, "src") == 0) {
					char *full_path = str_concat(location, path);
					contents = get_contents(full_path, &last_modified);
					free(full_path);
				} else if (strcmp(type, "jar") == 0) {
					contents = get_contents_zip(location, path, &last_modified);
				}

				if (contents != NULL) {
					break;
				}
			}
		}

		// load from out/
		if (contents == NULL) {
			if (out_path != NULL) {
				char *full_path = str_concat(out_path, path);
				contents = get_contents(full_path, &last_modified);
				free(full_path);
			}
		}

		if (developing && contents == NULL) {
			contents = bundle_get_contents(path);
			last_modified = 0;
		}

		if (contents != NULL) {
			JSStringRef contents_str = JSStringCreateWithUTF8CString(contents);
			free(contents);

			JSValueRef res[2];
			res[0] = JSValueMakeString(ctx, contents_str);
			res[1] = JSValueMakeNumber(ctx, last_modified);
			return JSObjectMakeArray(ctx, 2, res, NULL);
		}
	}

	return JSValueMakeNull(ctx);
}

JSValueRef function_load_deps_cljs_files(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	int num_files = 0;
	char **deps_cljs_files = NULL;

	if (argc == 0) {
		for (int i = 0; i < num_src_paths; i++) {
			char *type = src_paths[i].type;
			char *location = src_paths[i].path;

			if (strcmp(type, "jar") == 0) {
				char *source = get_contents_zip(location, "deps.cljs", NULL);
				if (source != NULL) {
					num_files += 1;
					deps_cljs_files = realloc(deps_cljs_files, num_files * sizeof(char*));
					deps_cljs_files[num_files - 1] = source;
				}
			}
		}
	}

	JSValueRef files[num_files];
	for (int i = 0; i < num_files; i++) {
		JSStringRef file = JSStringCreateWithUTF8CString(deps_cljs_files[i]);
		files[i] = JSValueMakeString(ctx, file);
		free(deps_cljs_files[i]);
	}
	free(deps_cljs_files);

	return JSObjectMakeArray(ctx, num_files, files, NULL);
}

JSValueRef function_cache(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	if (argc == 4 &&
			JSValueGetType (ctx, args[0]) == kJSTypeString &&
			JSValueGetType (ctx, args[1]) == kJSTypeString &&
			(JSValueGetType (ctx, args[2]) == kJSTypeString
				|| JSValueGetType (ctx, args[2]) == kJSTypeNull) &&
			(JSValueGetType (ctx, args[3]) == kJSTypeString
				|| JSValueGetType (ctx, args[3]) == kJSTypeNull)) {
		// debug_print_value("cache", ctx, args[0]);

		char *cache_prefix = value_to_c_string(ctx, args[0]);
		char *source = value_to_c_string(ctx, args[1]);
		char *cache = value_to_c_string(ctx, args[2]);
		char *sourcemap = value_to_c_string(ctx, args[3]);

		char *suffix = NULL;
		int max_suffix_len = 20;
		int prefix_len = strlen(cache_prefix);
		char *path = malloc((prefix_len + max_suffix_len) * sizeof(char));
		memset(path, 0, prefix_len + max_suffix_len);

		suffix = ".js";
		strcpy(path, cache_prefix);
		strcat(path, suffix);
		write_contents(path, source);

		suffix = ".cache.json";
		strcpy(path, cache_prefix);
		strcat(path, suffix);
		write_contents(path, cache);

		suffix = ".js.map.json";
		strcpy(path, cache_prefix);
		strcat(path, suffix);
		write_contents(path, sourcemap);

		free(cache_prefix);
		free(source);
		free(cache);
		free(sourcemap);

		free(path);
	}

	return  JSValueMakeNull(ctx);
}

JSValueRef function_eval(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	JSValueRef val = NULL;

	if (argc == 2
		&& JSValueGetType(ctx, args[0]) == kJSTypeString
		&& JSValueGetType(ctx, args[1]) == kJSTypeString) {
		// debug_print_value("eval", ctx, args[0]);

		JSStringRef sourceRef = JSValueToStringCopy(ctx, args[0], NULL);
		JSStringRef pathRef = JSValueToStringCopy(ctx, args[1], NULL);

		JSEvaluateScript(ctx, sourceRef, NULL, pathRef, 0, &val);

		JSStringRelease(pathRef);
		JSStringRelease(sourceRef);
	}

	return val != NULL ? val : JSValueMakeNull(ctx);
}

JSValueRef function_get_term_size(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	// if (return_term_size)
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	JSValueRef  arguments[2];
	arguments[0] = JSValueMakeNumber(ctx, w.ws_row);
	arguments[1] = JSValueMakeNumber(ctx, w.ws_col);
	return JSObjectMakeArray(ctx, 2, arguments, NULL);
	// return JSValueMakeNull(ctx);
}

JSValueRef function_print_fn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	if (argc == 1 && JSValueIsString(ctx, args[0])) {
		char *str = value_to_c_string(ctx, args[0]);

		fprintf(stdout, "%s", str);
		fflush(stdout);

		free(str);
	}

	return JSValueMakeNull(ctx);
}

JSValueRef function_print_err_fn(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	if (argc == 1 && JSValueIsString(ctx, args[0])) {
		char *str = value_to_c_string(ctx, args[0]);

		fprintf(stderr, "%s", str);
		fflush(stderr);

		free(str);
	}

	return JSValueMakeNull(ctx);
}

JSValueRef function_set_exit_value(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	if (argc == 1 && JSValueGetType (ctx, args[0]) == kJSTypeNumber) {
		exit_value = JSValueToNumber(ctx, args[0], NULL);
	}
	return JSValueMakeNull(ctx);
}

JSValueRef function_import_script(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
		size_t argc, const JSValueRef args[], JSValueRef* exception) {
	if (argc == 1 && JSValueGetType(ctx, args[0]) == kJSTypeString) {
		JSStringRef path_str_ref = JSValueToStringCopy(ctx, args[0], NULL);
		assert(JSStringGetLength(path_str_ref) < 100);
		char tmp[100];
		tmp[0] = '\0';
		JSStringGetUTF8CString(path_str_ref, tmp, 100);
		JSStringRelease(path_str_ref);

		char *path = tmp;
		if (str_has_prefix(path, "goog/../") == 0) {
			path = path + 8;
		}

		char *source = NULL;
		if (out_path == NULL) {
			source = bundle_get_contents(path);
		} else {
			char *full_path = str_concat(out_path, path);
			source = get_contents(full_path, NULL);
			free(full_path);
		}

		if (source != NULL) {
			evaluate_script(ctx, source, path);
			free(source);
		}
	}

	return JSValueMakeUndefined(ctx);
}

void register_global_function(JSContextRef ctx, char *name, JSObjectCallAsFunctionCallback handler) {
	JSObjectRef global_obj = JSContextGetGlobalObject(ctx);

	JSStringRef fn_name = JSStringCreateWithUTF8CString(name);
	JSObjectRef fn_obj = JSObjectMakeFunctionWithCallback(ctx, fn_name, handler);

	JSObjectSetProperty(ctx, global_obj, fn_name, fn_obj, kJSPropertyAttributeNone, NULL);
}

void usage(char *program_name) {
	printf("%s [flags]\n", program_name);
	printf("\n");
	printf(
		"  -h, --help       Display this help message\n"
		"  -v, --verbose    Print verbose diagnostics\n"
		"  -q, --quiet      Quiet mode\n"
		"  -r, --repl       Whether to start a REPL\n"
		"  -s, --static-fns Whether to use the :static-fns compiler option\n"
		"  -k, --cache      The directory to cache compiler results in\n"
		"  -j, --javascript Whether to run a JavaScript REPL\n"
		"  -e, --eval       Evaluate the given expression\n"
		"  -t, --theme      The theme to use (dumb, light, dark)\n"
		"  -i, --init       Evaluate the file at path\n"
	);
}

char *get_cljs_version() {
	char *bundle_js = bundle_get_contents("planck/bundle.js");
	if (bundle_js != NULL) {
		char *start = bundle_js + 29;
		char *version = strtok(start, " ");
		version = strdup(version);
		free(bundle_js);
		return version;
	} else {
		return "(Unknown)";
	}
}

void banner() {
	printf("Planck %s\n", PLANCK_VERSION);
	printf("ClojureScript %s\n", get_cljs_version());

	printf("    Docs: (doc function-name-here)\n");
	printf("          (find-doc \"part-of-name-here\")\n");
	printf("  Source: (source function-name-here)\n");
	printf("    Exit: Control+D or :cljs/quit or exit or quit\n");
	printf(" Results: Stored in vars *1, *2, *3, an exception in *e\n");

	printf("\n");
}

bool verbose = false;
bool quiet = false;
bool repl = false;
bool static_fns = false;
char *cache_path = NULL;
char *theme = "light";

bool javascript = false;

struct script {
	char *type;
	bool expression;
	char *source;
};
struct script *scripts = NULL;
int num_scripts = 0;

int main(int argc, char **argv) {
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"quiet", no_argument, NULL, 'q'},
		{"repl", no_argument, NULL, 'r'},
		{"static-fns", no_argument, NULL, 's'},
		{"cache", required_argument, NULL, 'k'},
		{"javascript", no_argument, NULL, 'j'},
		{"eval", required_argument, NULL, 'e'},
		{"theme", required_argument, NULL, 't'},
		{"classpath", required_argument, NULL, 'c'},
		{"out", required_argument, NULL, 'o'},
		{"auto-cache", no_argument, NULL, 'K'},
		{"init", required_argument, NULL, 'i'},

		{0, 0, 0, 0}
	};
	int opt, option_index;
	while ((opt = getopt_long(argc, argv, "hvrsk:je:t:c:o:Ki:q", long_options, &option_index)) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			exit(0);
		case 'v':
			verbose = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 'r':
			repl = true;
			break;
		case 's':
			static_fns = true;
			break;
		case 'k':
			cache_path = argv[optind - 1];
			break;
		case 'K':
			cache_path = ".planck_cache";
			{
				char *path_copy = strdup(cache_path);
				char *dir = dirname(path_copy);
				if (mkdir_p(dir) < 0) {
					fprintf(stderr, "Could not create %s: %s\n", cache_path, strerror(errno));
				}
				free(path_copy);
			}
			break;
		case 'j':
			javascript = true;
			break;
		case 'e':
			num_scripts += 1;
			scripts = realloc(scripts, num_scripts * sizeof(struct script));
			scripts[num_scripts - 1].type = "text";
			scripts[num_scripts - 1].expression = true;
			scripts[num_scripts - 1].source = argv[optind - 1];
			break;
		case 'i':
			num_scripts += 1;
			scripts = realloc(scripts, num_scripts * sizeof(struct script));
			scripts[num_scripts - 1].type = "path";
			scripts[num_scripts - 1].expression = false;
			scripts[num_scripts - 1].source = argv[optind - 1];
			break;
		case 't':
			theme = argv[optind - 1];
			break;
		case 'c':
			{
				char *classpath = argv[optind - 1];
				char *source = strtok(classpath, ":");
				while (source != NULL) {
					char *type = "src";
					if (str_has_suffix(source, ".jar") == 0) {
						type = "jar";
					}

					num_src_paths += 1;
					src_paths = realloc(src_paths, num_src_paths * sizeof(struct src_path));
					src_paths[num_src_paths - 1].type = type;
					src_paths[num_src_paths - 1].path = strdup(source);

					source = strtok(NULL, ":");
				}

				break;
			}
		case 'o':
			out_path = argv[optind - 1];
			break;
		case '?':
			usage(argv[0]);
			exit(1);
		default:
			printf("unhandled argument: %c\n", opt);
		}
	}

	int num_rest_args = 0;
	char **rest_args = NULL;
	if (optind < argc) {
		num_rest_args = argc - optind;
		rest_args = malloc((argc - optind) * sizeof(char*));
		int i = 0;
		while (optind < argc) {
			rest_args[i++] = argv[optind++];
		}
	}

	if (num_rest_args == 0 && num_scripts == 0) {
		repl = true;
	}

	JSGlobalContextRef ctx = JSGlobalContextCreate(NULL);

	JSStringRef nameRef = JSStringCreateWithUTF8CString("planck");
	JSGlobalContextSetName(ctx, nameRef);

	evaluate_script(ctx, "var global = this;", "<init>");

	register_global_function(ctx, "IMPORT_SCRIPT", function_import_script);
	bootstrap(ctx, out_path);

	register_global_function(ctx, "CONSOLE_LOG", function_console_log);
	register_global_function(ctx, "CONSOLE_ERROR", function_console_error);

	evaluate_script(ctx, "var console = {};"\
			"console.log = CONSOLE_LOG;"\
			"console.error = CONSOLE_ERROR;", "<init>");

	evaluate_script(ctx, "var PLANCK_VERSION = \"" PLANCK_VERSION "\";", "<init>");

	// require app namespaces
	evaluate_script(ctx, "goog.require('planck.repl');", "<init>");

	// without this things won't work
	evaluate_script(ctx, "var window = global;", "<init>");

	register_global_function(ctx, "PLANCK_READ_FILE", function_read_file);
	register_global_function(ctx, "PLANCK_LOAD", function_load);
	register_global_function(ctx, "PLANCK_LOAD_DEPS_CLJS_FILES", function_load_deps_cljs_files);
	register_global_function(ctx, "PLANCK_CACHE", function_cache);

	register_global_function(ctx, "PLANCK_EVAL", function_eval);

	register_global_function(ctx, "PLANCK_GET_TERM_SIZE", function_get_term_size);
	register_global_function(ctx, "PLANCK_PRINT_FN", function_print_fn);
	register_global_function(ctx, "PLANCK_PRINT_ERR_FN", function_print_err_fn);

	register_global_function(ctx, "PLANCK_SET_EXIT_VALUE", function_set_exit_value);

	evaluate_script(ctx, "cljs.core.set_print_fn_BANG_.call(null,PLANCK_PRINT_FN);", "<init>");
	evaluate_script(ctx, "cljs.core.set_print_err_fn_BANG_.call(null,PLANCK_PRINT_ERR_FN);", "<init>");

	{
		JSValueRef arguments[4];
		arguments[0] = JSValueMakeBoolean(ctx, repl);
		arguments[1] = JSValueMakeBoolean(ctx, verbose);
		JSValueRef cache_path_ref = NULL;
		if (cache_path != NULL) {
			JSStringRef cache_path_str = JSStringCreateWithUTF8CString(cache_path);
			cache_path_ref = JSValueMakeString(ctx, cache_path_str);
		}
		arguments[2] = cache_path_ref;
		arguments[3] = JSValueMakeBoolean(ctx, static_fns);
		JSValueRef ex = NULL;
		JSObjectCallAsFunction(ctx, get_function(ctx, "planck.repl", "init"), JSContextGetGlobalObject(ctx), 4, arguments, &ex);
		debug_print_value("planck.repl/init", ctx, ex);
	}

	if (repl) {
		evaluate_source(ctx, "text", "(require '[planck.repl :refer-macros [apropos dir find-doc doc source pst]])", true, false, "cljs.user", "dumb");
	}

	evaluate_script(ctx, "goog.provide('cljs.user');", "<init>");
	evaluate_script(ctx, "goog.require('cljs.core');", "<init>");

	evaluate_script(ctx, "cljs.core._STAR_assert_STAR_ = true;", "<init>");

	for (int i = 0; i < num_scripts; i++) {
		evaluate_source(ctx, scripts[i].type, scripts[i].source, scripts[i].expression, false, NULL, theme);
	}

	if (repl) {
		if (!quiet) {
			banner();
		}

		char *home = getenv("HOME");
		char *history_path = NULL;
		if (home != NULL) {
			char history_name[] = ".planck_history";
			int len = strlen(home) + strlen(history_name) + 2;
			history_path = malloc(len * sizeof(char));
			snprintf(history_path, len, "%s/%s", home, history_name);

			linenoiseHistoryLoad(history_path);
		}

		char *prompt = javascript ? " > " : " => ";

		char *line;
		while ((line = linenoise(prompt)) != NULL) {
			if (javascript) {
				JSValueRef res = evaluate_script(ctx, line, "<stdin>");
				print_value("", ctx, res);
			} else {
				evaluate_source(ctx, "text", line, true, true, "cljs.user", theme);
			}
			linenoiseHistoryAdd(line);
			if (history_path != NULL) {
				linenoiseHistorySave(history_path);
			}
			free(line);
		}
	}

	return exit_value;
}

JSStringRef to_string(JSContextRef ctx, JSValueRef val) {
	if (JSValueIsUndefined(ctx, val)) {
		return JSStringCreateWithUTF8CString("undefined");
	} else if (JSValueIsNull(ctx, val)) {
		return JSStringCreateWithUTF8CString("null");
	} else {
		JSStringRef to_string_name = JSStringCreateWithUTF8CString("toString");
		JSObjectRef obj = JSValueToObject(ctx, val, NULL);
		JSValueRef to_string = JSObjectGetProperty(ctx, obj, to_string_name, NULL);
		JSObjectRef to_string_obj = JSValueToObject(ctx, to_string, NULL);
		JSValueRef obj_val = JSObjectCallAsFunction(ctx, to_string_obj, obj, 0, NULL, NULL);

		return JSValueToStringCopy(ctx, obj_val, NULL);
	}
}

JSValueRef evaluate_script(JSContextRef ctx, char *script, char *source) {
	JSStringRef script_ref = JSStringCreateWithUTF8CString(script);
	JSStringRef source_ref = NULL;
	if (source != NULL) {
		source_ref = JSStringCreateWithUTF8CString(source);
	}

	JSValueRef ex = NULL;
	JSValueRef val = JSEvaluateScript(ctx, script_ref, NULL, source_ref, 0, &ex);
	JSStringRelease(script_ref);
	if (source != NULL) {
		JSStringRelease(source_ref);
	}

	debug_print_value("evaluate_script", ctx, ex);

	return val;
}

char *value_to_c_string(JSContextRef ctx, JSValueRef val) {
	if (!JSValueIsString(ctx, val)) {
#ifdef DEBUG
		fprintf(stderr, "WARN: not a string\n");
#endif
		return NULL;
	}

	JSStringRef str_ref = JSValueToStringCopy(ctx, val, NULL);
	size_t len = JSStringGetLength(str_ref) + 1;
	char *str = malloc(len * sizeof(char));
	memset(str, 0, len);
	JSStringGetUTF8CString(str_ref, str, len);
	JSStringRelease(str_ref);

	return str;
}

JSValueRef get_value_on_object(JSContextRef ctx, JSObjectRef obj, char *name) {
	JSStringRef name_str = JSStringCreateWithUTF8CString(name);
	JSValueRef val = JSObjectGetProperty(ctx, obj, name_str, NULL);
	JSStringRelease(name_str);
	return val;
}

JSValueRef get_value(JSContextRef ctx, char *namespace, char *name) {
	JSValueRef ns_val = NULL;

	// printf("get_value: '%s'\n", namespace);
	int len = strlen(namespace) + 1;
	char *ns_tmp = malloc(len * sizeof(char));
	strncpy(ns_tmp, namespace, len);
	char *ns_part = strtok(ns_tmp, ".");
	ns_tmp = NULL;
	while (ns_part != NULL) {
		char *munged_ns_part = munge(ns_part);
		if (ns_val) {
			ns_val = get_value_on_object(ctx, JSValueToObject(ctx, ns_val, NULL), munged_ns_part);
		} else {
			ns_val = get_value_on_object(ctx, JSContextGetGlobalObject(ctx), munged_ns_part);
		}
		free(munged_ns_part); // TODO: Use a fixed buffer for this?  (Which would restrict namespace part length...)

		ns_part = strtok(NULL, ".");
	}
	//free(ns_tmp);

	char *munged_name = munge(name);
	JSValueRef val = get_value_on_object(ctx, JSValueToObject(ctx, ns_val, NULL), munged_name);
	free(munged_name);
	return val;
}

JSObjectRef get_function(JSContextRef ctx, char *namespace, char *name) {
	JSValueRef val = get_value(ctx, namespace, name);
	assert(!JSValueIsUndefined(ctx, val));
	return JSValueToObject(ctx, val, NULL);
}

JSValueRef evaluate_source(JSContextRef ctx, char *type, char *source, bool expression, bool print_nil, char *set_ns, char *theme) {
	JSValueRef args[6];
	int num_args = 6;

	{
		JSValueRef source_args[2];
		JSStringRef type_str = JSStringCreateWithUTF8CString(type);
		source_args[0] = JSValueMakeString(ctx, type_str);
		JSStringRef source_str = JSStringCreateWithUTF8CString(source);
		source_args[1] = JSValueMakeString(ctx, source_str);
		args[0] = JSObjectMakeArray(ctx, 2, source_args, NULL);
	}

	args[1] = JSValueMakeBoolean(ctx, expression);
	args[2] = JSValueMakeBoolean(ctx, print_nil);
	JSValueRef set_ns_val = NULL;
	if (set_ns != NULL) {
		JSStringRef set_ns_str = JSStringCreateWithUTF8CString(set_ns);
		set_ns_val = JSValueMakeString(ctx, set_ns_str);
	}
	args[3] = set_ns_val;
	JSStringRef theme_str = JSStringCreateWithUTF8CString(theme);
	args[4] = JSValueMakeString(ctx, theme_str);
	args[5] = JSValueMakeNumber(ctx, 0);

	JSObjectRef execute_fn = get_function(ctx, "planck.repl", "execute");
	JSObjectRef global_obj = JSContextGetGlobalObject(ctx);
	JSValueRef ex = NULL;
	JSValueRef val = JSObjectCallAsFunction(ctx, execute_fn, global_obj, num_args, args, &ex);

	debug_print_value("planck.repl/execute", ctx, ex);

	return ex != NULL ? ex : val;
}

char *munge(char *s) {
	int len = strlen(s);
	int new_len = 0;
	for (int i = 0; i < len; i++) {
		switch (s[i]) {
		case '!':
			new_len += 6; // _BANG_
			break;
		case '?':
			new_len += 7; // _QMARK_
			break;
		default:
			new_len += 1;
		}
	}

	char *ms = malloc((new_len+1) * sizeof(char));
	int j = 0;
	for (int i = 0; i < len; i++) {
		switch (s[i]) {
		case '-':
			ms[j++] = '_';
			break;
		case '!':
			ms[j++] = '_';
			ms[j++] = 'B';
			ms[j++] = 'A';
			ms[j++] = 'N';
			ms[j++] = 'G';
			ms[j++] = '_';
			break;
		case '?':
			ms[j++] = '_';
			ms[j++] = 'Q';
			ms[j++] = 'M';
			ms[j++] = 'A';
			ms[j++] = 'R';
			ms[j++] = 'K';
			ms[j++] = '_';
			break;

		default:
			ms[j++] = s[i];
		}
	}
	ms[new_len] = '\0';

	return ms;
}

void bootstrap(JSContextRef ctx, char *out_path) {
	char *deps_file_path = "main.js";
	char *goog_base_path = "goog/base.js";
	if (out_path != NULL) {
		deps_file_path = str_concat(out_path, deps_file_path);
		goog_base_path = str_concat(out_path, goog_base_path);
	}

	char source[] = "<bootstrap>";

	// Setup CLOSURE_IMPORT_SCRIPT
	evaluate_script(ctx, "CLOSURE_IMPORT_SCRIPT = function(src) { IMPORT_SCRIPT('goog/' + src); return true; }", source);

	// Load goog base
	char *base_script_str = NULL;
	if (out_path) {
		base_script_str = get_contents(goog_base_path, NULL);
		free(goog_base_path);
	} else {
		base_script_str = bundle_get_contents(goog_base_path);
	}
	if (base_script_str == NULL) {
		fprintf(stderr, "The goog base JavaScript text could not be loaded\n");
		exit(1);
	}
	evaluate_script(ctx, base_script_str, "<bootstrap:base>");
	free(base_script_str);

	// Load the deps file
	char *deps_script_str = NULL;
	if (out_path) {
		deps_script_str = get_contents(deps_file_path, NULL);
		free(deps_file_path);
	} else {
		deps_script_str = bundle_get_contents(deps_file_path);
	}
	if (deps_script_str == NULL) {
		fprintf(stderr, "The deps JavaScript text could not be loaded\n");
		exit(1);
	}
	evaluate_script(ctx, deps_script_str, "<bootstrap:deps>");
	free(deps_script_str);

	evaluate_script(ctx, "goog.isProvided_ = function(x) { return false; };", source);

	evaluate_script(ctx, "goog.require = function (name) { return CLOSURE_IMPORT_SCRIPT(goog.dependencies_.nameToPath[name]); };", source);

	evaluate_script(ctx, "goog.require('cljs.core');", source);

	// redef goog.require to track loaded libs
	evaluate_script(ctx, "cljs.core._STAR_loaded_libs_STAR_ = cljs.core.into.call(null, cljs.core.PersistentHashSet.EMPTY, [\"cljs.core\"]);\n"
			"goog.require = function (name, reload) {\n"
			"    if(!cljs.core.contains_QMARK_(cljs.core._STAR_loaded_libs_STAR_, name) || reload) {\n"
			"        var AMBLY_TMP = cljs.core.PersistentHashSet.EMPTY;\n"
			"        if (cljs.core._STAR_loaded_libs_STAR_) {\n"
			"            AMBLY_TMP = cljs.core._STAR_loaded_libs_STAR_;\n"
			"        }\n"
			"        cljs.core._STAR_loaded_libs_STAR_ = cljs.core.into.call(null, AMBLY_TMP, [name]);\n"
			"        CLOSURE_IMPORT_SCRIPT(goog.dependencies_.nameToPath[name]);\n"
			"    }\n"
			"};", source);
}

char *get_contents(char *path, time_t *last_modified) {
/*#ifdef DEBUG
	printf("get_contents(\"%s\")\n", path);
#endif*/

	char *err_prefix;

	FILE *f = fopen(path, "r");
	if (f == NULL) {
		err_prefix = "fopen";
		goto err;
	}

	struct stat f_stat;
	if (fstat(fileno(f), &f_stat) < 0) {
		err_prefix = "fstat";
		goto err;
	}

	if (last_modified != NULL) {
		*last_modified = f_stat.st_mtime;
	}

	char *buf = malloc(f_stat.st_size + 1);
	memset(buf, 0, f_stat.st_size);
	fread(buf, f_stat.st_size, 1, f);
	buf[f_stat.st_size] = '\0';
	if (ferror(f)) {
		err_prefix = "fread";
		free(buf);
		goto err;
	}

	if (fclose(f) < 0) {
		err_prefix = "fclose";
		goto err;
	}

	return buf;

err:
	//printf("get_contents(\"%s\"): %s: %s\n", path, err_prefix, strerror(errno));
	return NULL;
}

void write_contents(char *path, char *contents) {
	char *err_prefix;

	FILE *f = fopen(path, "w");
	if (f == NULL) {
		err_prefix = "fopen";
		goto err;
	}

	int len = strlen(contents);
	int offset = 0;
	do {
		int res = fwrite(contents+offset, 1, len-offset, f);
		if (res < 0) {
			err_prefix = "fwrite";
			goto err;
		}
		offset += res;
	} while (offset < len);

	if (fclose(f) < 0) {
		err_prefix = "fclose";
		goto err;
	}

	return;

err:
	// printf("write_contents(\"%s\", ...): %s: %s\n", path, err_prefix, strerror(errno));
	return;
}

int mkdir_p(char *path) {
	int res = mkdir(path, 0755);
	if (res < 0 && errno == EEXIST) {
		return 0;
	}
	return res;
}

int str_has_suffix(char *str, char *suffix) {
	int len = strlen(str);
	int suffix_len = strlen(suffix);

	if (len < suffix_len) {
		return -1;
	}

	return strcmp(str + (len-suffix_len), suffix);
}

int str_has_prefix(char *str, char *prefix) {
	int len = strlen(str);
	int prefix_len = strlen(prefix);

	if (len < prefix_len) {
		return -1;
	}

	return strncmp(str, prefix, prefix_len);
}

char *str_concat(char *s1, char *s2) {
	int l1 = strlen(s1), l2 = strlen(s2);
	int len = l1 + l2 + 1;
	char *s = malloc(len * sizeof(char));
	memset(s, 0, len);

	strncpy(s, s1, l1);
	strncpy(s+l1, s2, l2);
	return s;
}
