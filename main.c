#include <sys/param.h>
#include <sys/jail.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <jail.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <paths.h>
#include <unistd.h>

#ifdef _LIMITS_H_ // Make clangd defaults happy by referencing something out of <limits.h>
#endif

#define STRINGIFY(x) #x
#define EXPAND_AND_STRINGIFY(x) STRINGIFY(x)
#define INT_MAX_BUF (sizeof(EXPAND_AND_STRINGIFY(INT_MAX)))

_Static_assert(1, "Workaround for https://github.com/clangd/clangd/issues/1167");
#pragma clang diagnostic push
#ifdef RACONIC
#pragma clang diagnostic error   "-Weverything"
#endif
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wvla"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wgnu-folding-constant"
#pragma clang diagnostic ignored "-Wgnu-conditional-omitted-operand"
#if __has_warning("-Wpre-c11-compat")
#pragma clang diagnostic ignored "-Wpre-c11-compat"
#endif

#define PROG          "kassiber"
#define DEFAULT_RTLD  "/libexec/ld-elf.so.1"
#define GETOPT_STRING "haAnj:c:l:r:p:"

extern char **environ;

struct str_vec {
	size_t      count;
	const char *items[] __attribute__((counted_by(count)));
};

struct int_vec {
	size_t count;
	int    items[] __attribute__((counted_by(count)));
};

struct args {
	bool                                      usage;
	bool                                      help;
	bool					  early;
	bool					  late;
	int                                       optind;
	int                                       argc;
	char                                      padding4[_Alignof(char **) - sizeof(int)];
	const char          *_Nullable *_Nonnull  argv;
	const char                     *_Nullable jail;
	const char                     *_Nullable chroot;
	const char                     *_Nullable path;
	const char                     *_Nonnull  rtld;
	const struct str_vec           *_Nullable libs;
};

struct open {
	int                             rtld;
	int                             exec;
	const struct int_vec *_Nullable libs;
};


void                                       str_vec_free  (const struct str_vec *_Nullable *_Nonnull);
struct str_vec *_Nonnull                   str_vec_append(      struct str_vec *_Nullable, const char *_Nullable);
const char     *_Nullable                  str_vec_index (const struct str_vec *_Nullable, size_t index);
const char     *_Nullable const *_Nullable str_vec_iter  (const struct str_vec *_Nullable);
const char     *_Nullable const *_Nullable str_vec_next  (const struct str_vec *_Nullable, const char *_Nullable const *_Nullable);

void                      int_vec_free  (const struct int_vec *_Nullable *_Nonnull);
struct int_vec *_Nonnull  int_vec_append(      struct int_vec *_Nullable, int);
int                       int_vec_index (const struct int_vec *_Nullable, size_t index);
const int      *_Nullable int_vec_iter  (const struct int_vec *_Nullable);
const int      *_Nullable int_vec_next  (const struct int_vec *_Nullable, const int *_Nullable);

static inline bool
is_power_of_2(size_t n)
{
	return n != 0 && (n & (n - 1)) == 0;
}

void
str_vec_free(const struct str_vec *_Nullable *_Nonnull const vec_ptr)
{
	free((struct str_vec *_Nullable)(uintptr_t)*vec_ptr);
}

void
int_vec_free(const struct int_vec *_Nullable *_Nonnull const vec_ptr)
{
	free((struct int_vec *_Nullable)(uintptr_t)*vec_ptr);
}

struct str_vec *_Nonnull
str_vec_append(struct str_vec *_Nullable const vec, const char *const item)
{
	if (vec == NULL) {
		struct str_vec *_Nullable const new_vec = malloc(sizeof(*new_vec) + sizeof(new_vec->items[0]));
		if (new_vec == NULL) {
			err(EX_OSERR, "Failed to allocate new vector");
		}
		new_vec->count = 1;
		new_vec->items[0] = item;
		return new_vec;
	}

	const size_t old_count = vec->count;
	const size_t new_count = old_count + 1;
	if (!is_power_of_2(old_count)) {
		vec->count = new_count;
		vec->items[old_count] = item;
		return vec;
	}

	struct str_vec *_Nullable const new_vec = realloc(vec, sizeof(*new_vec) + 2*old_count * sizeof(new_vec->items[0]));
	if (new_vec == NULL) {
		err(EX_OSERR, "Failed to grow vector");
	}
	new_vec->count            = new_count;
	new_vec->items[old_count] = item;
	return new_vec;
}

struct int_vec *_Nonnull
int_vec_append(struct int_vec *_Nullable const vec, const int item)
{
	if (vec == NULL) {
		struct int_vec *_Nullable const new_vec = malloc(sizeof(*new_vec) + sizeof(new_vec->items[0]));
		if (new_vec == NULL) {
			err(EX_OSERR, "Failed to allocate new vector");
		}
		new_vec->count = 1;
		new_vec->items[0] = item;
		return new_vec;
	}

	const size_t old_count = vec->count;
	const size_t new_count = old_count + 1;
	if (!is_power_of_2(old_count)) {
		vec->count = new_count;
		vec->items[old_count] = item;
		return vec;
	}

	struct int_vec *_Nullable const new_vec = realloc(vec, sizeof(*new_vec) + 2*old_count * sizeof(new_vec->items[0]));
	if (new_vec == NULL) {
		err(EX_OSERR, "Failed to grow vector");
	}
	new_vec->count            = new_count;
	new_vec->items[old_count] = item;
	return new_vec;
}

const char *_Nullable
str_vec_index(const struct str_vec *_Nullable const vec, const size_t index)
{
	return vec == NULL || index >= vec->count ? NULL : vec->items[index];
}

int
int_vec_index(const struct int_vec *_Nullable const vec, const size_t index)
{
	return vec == NULL || index >= vec->count ? -1 : vec->items[index];
}

const char *_Nullable const *_Nullable
str_vec_iter(const struct str_vec *_Nullable const vec)
{
	return vec == NULL ? NULL : &vec->items[0];
}

const int *_Nullable
int_vec_iter(const struct int_vec *_Nullable const vec)
{
	return vec == NULL ? NULL : &vec->items[0];
}

const char *_Nullable const *_Nullable
str_vec_next(const struct str_vec *_Nullable const vec, const char *_Nullable const *_Nullable iter)
{
	if (vec == NULL || iter == NULL) {
		return NULL;
	}
	
	const size_t count = vec->count;
	const char *_Nullable const *_Nonnull const last = &vec->items[count - 1];
	return iter == last ? NULL : iter + 1;
}

const int *_Nullable
int_vec_next(const struct int_vec *_Nullable const vec, const int *_Nullable iter)
{
	if (vec == NULL || iter == NULL) {
		return NULL;
	}
	
	const size_t count = vec->count;
	const int *_Nonnull const last = &vec->items[count - 1];
	return iter == last ? NULL : iter + 1;
}

struct paths {
	size_t      count;
	const char *paths[] __attribute__((counted_by(count)));
};

struct path_fields {
	size_t count;
	size_t size;
};

static inline struct path_fields
count_paths(const char *_Nonnull const path)
{
	const char *_Nonnull position = path;
	size_t               count    = 0;
	size_t               size     = 0;

	while (*position != '\0') {
		const size_t length = strcspn(position, ":");
		if (length == 0) {
			position++;
		} else {
			position  += length;
			count     += 1;
			size      += sizeof(char *) + length + sizeof(char);
		}
	}

	return (struct path_fields) {
		.count = count,
		.size  = size
	};
}

static inline void
copy_paths(struct paths *_Nonnull const paths, const struct path_fields fields, const char *_Nonnull const path)
{
	const size_t           fields_offset = sizeof(struct paths) + fields.count * sizeof(char *);
	const char   *_Nonnull source        = path;
	char         *_Nonnull destination   = ((char *)paths) + fields_offset;

	paths->count = fields.count;
	for (size_t i = 0; i < fields.count; /* nothing */) {
		const size_t length = strcspn(source, ":");
		if (length == 0) {
			source++;
		} else {
			const size_t size = length + 1;
			memcpy(destination, source, length);
			destination[length] = '\0';
			paths->paths[i++] = destination;
			source      += length;
			destination += size;
		}
	}
}

static inline const struct paths *_Nonnull
extract_path(const char *_Nullable const maybe_path)
{
	const char *_Nonnull const path = maybe_path ?: getenv("PATH") ?: _PATH_DEFPATH;
	const struct path_fields fields = count_paths(path);

	const size_t paths_size = sizeof(struct paths) + fields.count * sizeof(char *) + fields.size;
	struct paths *_Nullable const paths = malloc(paths_size);
	if (paths == NULL) {
		err(EX_OSERR, "Failed to allocate struct paths for %zu paths", fields.count);
	}
	copy_paths(paths, fields, path);

	return paths;
}

static inline void
free_paths(const struct paths *_Nonnull const paths[const static 1])
{
	free((void *)(uintptr_t)*paths);
}


static int
openat_retry(const int dfd, const char *const path, int flags, const mode_t mode)
{
	const int fd = openat(dfd, path, flags, mode);
	if (fd >= 0 || errno != EINTR) {
		return fd;
	} else {
		__attribute__((musttail)) return openat_retry(dfd, path, flags, mode);
	}
}

static inline struct args
parse_args(const int argc, const char **const argv)
{
	struct args args = {
		.usage      = false,
		.help       = false,
		.early      = true,
		.late       = false,
		.argc       = argc,
		.argv       = argv,
		.jail       = NULL,
		.chroot     = NULL,
		.path       = NULL,
		.rtld       = DEFAULT_RTLD,
		.libs       = NULL,
	};
	struct str_vec *_Nullable libs = NULL;

	optind   = 1;
	optreset = 1;
	int ch;
	while ((ch = getopt(argc, (char **)(uintptr_t)argv, GETOPT_STRING)) != -1) {
		switch (ch) {
		case 'h':
			args.help = true;
			break;

		case 'a':
			args.early = true;
			args.late  = false;
			break;

		case 'A':
			args.early = false;
			args.late  = true;
			break;

		case 'n':
			args.early = false;
			args.late  = false;
			break;

		case 'c':
			args.chroot = optarg;
			break;

		case 'j':
			args.jail = optarg;
			break;

		case 'p':
			args.path = optarg;
			break;

		case 'r':
			args.rtld = optarg;
			break;

		case 'l':
			libs = str_vec_append(libs, optarg);
			break;

		default:
			args.usage = true;
			break;
		}
	}
	args.optind = optind;
	args.libs   = libs;

	if (optind == argc && !args.help) {
		args.usage = true;
	}

	return args;
}

static inline void
close_fd(const int fd[const static 1])
{
	close(*fd);
}

static inline int
open_rtld(const struct args args[const static 1])
{
	const char *_Nonnull const path = args->rtld;
	const int fd = openat_retry(AT_FDCWD, path, O_EXEC | O_VERIFY, 0555);
	if (fd < 0) {
		err(EX_NOINPUT, "Failed to open runtime loader \"%s\"", path);
	}
	return fd;
}

static inline int
open_exec(const struct args args[const static 1])
{
	const struct paths *_Nonnull const path_dirs __attribute__((cleanup(free_paths))) = extract_path(args->path);
	const char *_Nonnull const argv0 = args->argv[args->optind];

	int fd = -1;
	if (strchr(argv0, '/') != NULL) {
		fd = openat_retry(AT_FDCWD, argv0, O_RDONLY | O_VERIFY, 0444);
	} else {
		const size_t count = path_dirs->count;
		const char *_Nonnull const *_Nonnull const paths = path_dirs->paths;
		for (size_t i = 0; i < count; i++) {
			const char *_Nonnull const path = paths[i];
			const int dfd __attribute__((cleanup(close_fd))) = openat_retry(AT_FDCWD, path, O_SEARCH, 0555);
			if (dfd < 0) {
				continue;
			}

			fd = openat_retry(dfd, argv0, O_RDONLY | O_VERIFY, 0444);
			if (fd >= 0) {
				break;
			} else if (errno == ENOENT) {
				continue;
			} else {
				err(EX_NOINPUT, "Failed to open executable \"%s\" in \"%s\"", argv0, path);
			}
		}
	}

	if (fd < 0) {
		errx(EX_NOINPUT, "Failed to open executable \"%s\".", argv0);
	}
	return fd;
}

static struct int_vec *_Nullable
open_required_libs(struct int_vec *_Nullable libs, const struct args args[const static 1], const int rtld, const int exec)
{
	char exec_num[INT_MAX_BUF];
	snprintf(exec_num, sizeof(exec_num), "%i", exec);

	const char *_Nullable const argv[] = {
		args->rtld,
		"-o", "TRACE_LOADED_OBJECTS=yes",
		"-o", "TRACE_LOADED_OBJECTS_FMT1=%p\\n",
		"-f", exec_num,
		args->argv[args->optind],
		NULL
	};
	
	int pipes[2];
	if (pipe(pipes) != 0) {
		err(EX_OSERR, "Failed to create pipe pair");
	}

	const pid_t child_pid = fork();
	switch (child_pid) {
	case -1:
		err(EX_OSERR, "Failed to fork child process");
		break;

	case 0:
		close(pipes[0]);
		if (dup2(pipes[1], STDOUT_FILENO) < 0) {
			err(EX_OSERR, "Failed to redirect child output");
		}
		fexecve(rtld, (char *const *const)(uintptr_t)argv, environ);
		err(EX_OSERR, "Failed to fexecve() runtime loader");
		break;

	default:
		break;
	}

	close(pipes[1]);
	FILE *_Nullable const lines = fdopen(pipes[0], "r");
	if (lines == NULL) {
		err(EX_OSERR, "Failed to wrap pipe with FILE handle");
	}
	char *_Nullable path = NULL;
	size_t capacity = 0;
	ssize_t length = 0;
	while ((length = getline(&path, &capacity, lines)) > 0) {
		path[length - 1] = '\0';
		if (path[0] == '\t') {
			continue;
		}
		const int fd = openat_retry(AT_FDCWD, path, O_RDONLY | O_VERIFY, 0444);
		if (fd < 0) {
			err(EX_NOINPUT, "Failed to open library \"%s\"", path);
		}
		libs = int_vec_append(libs, fd);
	}
	free(path);
	fclose(lines);

	return libs;
}

static inline struct int_vec *_Nullable
open_libs(struct int_vec *_Nullable libs, const struct args args[const static 1], const int rtld, const int exec)
{
	const char *_Nonnull const *_Nullable const paths = args->libs->items;

	if (args->early) {
		libs = open_required_libs(libs, args, rtld, exec);
	}

	const size_t count = !!args->libs && args->libs->count;
	for (size_t i = 0; i < count; i++) {
		const char *_Nonnull const path = paths[i];
		const int fd = openat_retry(AT_FDCWD, path, O_RDONLY | O_VERIFY, 0444);
		if (fd < 0) {
			err(EX_NOINPUT, "Failed to open library \"%s\"", path);
		}
		libs = int_vec_append(libs, fd);
	}

	if (args->late) {
		libs = open_required_libs(libs, args, rtld, exec);
	}

	return libs;
}

static inline struct open 
open_args(const struct args *_Nonnull const args)
{
	struct int_vec *_Nullable libs = NULL;
	struct open open = {
		.rtld = -1,
		.exec = -1,
		.libs = NULL
	};
	open.rtld = open_rtld(args);
	open.exec = open_exec(args);

	libs = open_libs(libs, args, open.rtld, open.exec);
	open.libs = libs;

	return open;
}

static inline void
free_open(const struct open open_ptr[const static 1])
{
	const struct open open = *open_ptr;

	if (open.rtld >= 0) {
		close(open.rtld);
	}

	if (open.exec >= 0) {
		close(open.exec);
	}

	const size_t count = open.libs->count;
	const int *_Nonnull const libs = open.libs->items;
	for (size_t i = 0; i < count; i++) {
		const int fd = libs[i];
		if (fd >= 0) {
			close(fd);
		}
	}

	free((void *)(uintptr_t)open.libs);
}

static inline void
free_ptr(const char *_Nullable const ptr[const static 1])
{
	free((void *)(uintptr_t)*ptr);
}

static inline const char *_Nullable
format_preload(const struct open open[const static 1])
{
	if (open->libs == NULL) {
		return NULL;
	}	

	char *_Nullable buffer = NULL;
	size_t size = 0;
	FILE *_Nullable const stream = open_memstream(&buffer, &size);
	if (stream == NULL) {
		err(EX_OSERR, "Failed to open memstream");
	}

	const size_t count = open->libs->count;
	const int *_Nonnull const libs = open->libs->items;
	const char *join = "PRELOAD_FDS=";
	for (size_t i = 0; i < count; i++) {
		if (fprintf(stream, "%s%i", join, libs[i]) < 0) {
			err(EX_OSERR, "Failed to fprintf() to memstream");
		}
		join = ":";
	}

	if (fclose(stream) != 0) {
		err(EX_OSERR, "Failed to fclose() memstream");
	} else if (buffer == NULL) {
		err(EX_OSERR, "Failed to create PRELOAD_FDS list");
	}
	return buffer;
}

int
main(int argc, const char **argv)
{
	const struct args args = parse_args(argc, argv);
	if (args.usage || args.help) {
		FILE *const file = args.usage ? stderr : stdout;
		fputs("usage: " PROG " -h | [-a] [-A] [-n] [-r <rtld>] [-p <path>] [-l <lib>]* [-j <jail>] [-c <chroot>] <cmd> [<arg>*]\n", file);
		return args.usage ? EX_USAGE : 0;
	}
	
	const struct open fds __attribute__((cleanup(free_open))) = open_args(&args);

	argc = 1 + 2 + 2 + (args.argc - args.optind) + 1;
	argv = malloc(sizeof(char*) * (size_t)argc);
	if (argv == NULL) {
		err(EX_OSERR, "Failed to allocate argument list");
	}
	char exec_num[INT_MAX_BUF];
	snprintf(exec_num, sizeof(exec_num), "%i", fds.exec);

	const char *_Nullable const preload __attribute__((cleanup(free_ptr))) = format_preload(&fds);

	size_t i = 0;
	argv[i++] = args.rtld;
	argv[i++] = "-f";
	argv[i++] = exec_num;
	if (preload != NULL) {
		argv[i++] = "-o";
		argv[i++] = preload;
	}
	const size_t old_argc = (size_t)args.argc;
	for (size_t j = (size_t)args.optind; j < old_argc; j++) {
		argv[i++] = args.argv[j];
	}
	argv[i++] = NULL;

	if (args.jail != NULL) {
		const int jail_id = jail_getid(args.jail);
		if (jail_id < 0) {
			err(EX_CONFIG, "Jail \"%s\" does not exist", args.jail);
		} else if (jail_attach(jail_id) != 0) {
			err(EX_OSERR, "Failed to attach to \"%s\" jail", args.jail);
		}
	}

	if (args.chroot != NULL) {
		if (chroot(args.chroot) != 0) {
			err(EX_OSERR, "Failed to chroot to \"%s\"", args.chroot);
		}
	}

	fexecve(fds.rtld, (char *const*)(uintptr_t)argv, environ);
	err(EX_OSERR, "Failed to fexecve()");
}
#pragma clang diagnostic pop
