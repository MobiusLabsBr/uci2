/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2024, Sartura d.d.
 */

// needed for flock(), O_CLOEXEC and open_memstream()
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils/debug.h"
#include "utils/memory.h"

#include "parser.h"
#include "lexer.h"
#include "ast.h"

#include "uci2.h"

#define XSTR(s) STR(s)
#define STR(s) #s

#define UCI_PATH_PREFIX "/etc/config"
// same mode libuci uses for newly created config files
#define UCI2_FILE_MODE (0600)
#define UCI2_SECTION_TYPE_BUFFER_SIZE_MAX (1024)
#define UCI2_SECTION_INDEX_BUFFER_SIZE_MAX (1024)

struct uci2_node_iterator_s {
	uci2_node_t *node_start;
	size_t offset_i;
	size_t offset_j;
};

static uci2_error_e uci2_node_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, uci2_node_type_e type, uci2_node_t **out);
static bool uci2_name_is_valid(const char *name, bool is_type);

uint32_t uci2_version_numeric(void)
{
	return (UCI2_VERSION_MAJOR << 16) | (UCI2_VERSION_MINOR << 8) | (UCI2_VERSION_PATCH);
}

const char *uci2_version_string(void)
{
	return XSTR(UCI2_VERSION_MAJOR) "." XSTR(UCI2_VERSION_MINOR) "." XSTR(UCI2_VERSION_PATCH);
}

uci2_error_e uci2_config_parse(const char *config, uci2_ast_t **out)
{
	int error = 0;
	uci2_error_e uci2_error = UE_NONE;
	char config_file_path[PATH_MAX] = {0};
	struct stat stat_buffer = {0};
	FILE *config_file = NULL;
	long config_file_size = 0;
	char *config_file_content = NULL;
	size_t bytes_read = 0;
	yyscan_t scanner = {0};
	YY_BUFFER_STATE yy_buffer = NULL;
	uci2_ast_t *uci2_ast = NULL;

	if (config == NULL) {
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (config[0] == '/') {
		snprintf(config_file_path, sizeof(config_file_path), "%s", config);
	} else {
		snprintf(config_file_path, sizeof(config_file_path), "%s/%s", UCI_PATH_PREFIX, config);
	}

	errno = 0;
	error = stat(config_file_path, &stat_buffer);
	if (error) {
		DEBUG("stat(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = (errno == ENOENT) ? UE_FILE_NOT_FOUND : UE_FILE_IO;
		goto error_out;
	}

	if (S_ISREG(stat_buffer.st_mode) == 0) {
		DEBUG("%s is not a regular file", config_file_path);
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	errno = 0;
	config_file = fopen(config_file_path, "r");
	if (config_file == NULL) {
		DEBUG("fopen(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	// like libuci, hold a shared lock while reading so an ongoing write is finished first
	// (the lock is released when the file is closed)
	while (flock(fileno(config_file), LOCK_SH) < 0 && errno != ENOSYS) {
		if (errno != EINTR) {
			DEBUG("flock(%s) error(%d): %s", config_file_path, errno, strerror(errno));
			uci2_error = UE_FILE_IO;
			goto error_out;
		}
	}

	errno = 0;
	error = fseek(config_file, 0, SEEK_END);
	if (error < 0) {
		DEBUG("fseek(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	errno = 0;
	config_file_size = ftell(config_file);
	if (config_file_size < 0) {
		DEBUG("ftell(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	errno = 0;
	error = fseek(config_file, 0, SEEK_SET);
	if (error < 0) {
		DEBUG("fseek(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	if (config_file_size == 0) {
		uci2_ast_create(&uci2_ast);
	} else {
		// one extra byte for a line break, see below
		config_file_content = xcalloc((size_t) config_file_size + 2, 1);

		errno = 0;
		bytes_read = fread(config_file_content, 1, (size_t) config_file_size, config_file);
		if (bytes_read != (size_t) config_file_size) {
			DEBUG("fread(%s) error: %s", config_file_path, ferror(config_file) ? strerror(errno) : "error");
			uci2_error = UE_FILE_IO;
			goto error_out;
		}
		config_file_content[config_file_size] = '\0';

		// like libuci, a backslash at the end of the file is dropped:
		// adding a line break turns it into a line continuation
		if (config_file_content[config_file_size - 1] == '\\') {
			config_file_content[config_file_size] = '\n';
			config_file_content[config_file_size + 1] = '\0';
		}

		// setup parser
		errno = 0;
		error = yylex_init(&scanner);
		if (error) {
			DEBUG("yylex_init error (%d): %s", errno, strerror(errno));
			uci2_error = UE_PARSER;
			goto error_out;
		}

		yy_buffer = yy_scan_string(config_file_content, scanner);
		if (yy_buffer == NULL) {
			DEBUG("yy_scan_string error");
			uci2_error = UE_PARSER;
			goto error_out;
		}

		// create AST structure
		uci2_ast = xcalloc(1, sizeof(uci2_ast_t));

		// if parser error occurred
		error = yyparse(scanner, uci2_ast);
		if (error) {
			DEBUG("yyparse error (%d)", error);
			uci2_error = UE_PARSER;
			goto error_out;
		}
	}

	*out = uci2_ast;

	goto out;

error_out:
	uci2_ast_destroy(&uci2_ast);

out:
	if (config_file) {
		fclose(config_file);
	}
	if (yy_buffer) {
		yy_delete_buffer(yy_buffer, scanner);
	}
	if (scanner) {
		yylex_destroy(scanner);
	}
	if (config_file_content) {
		XFREE(config_file_content);
	}

	return uci2_error;
}

uci2_error_e uci2_config_remove(const char *config)
{
	int error = 0;
	uci2_error_e uci2_error = UE_NONE;
	char config_file_path[PATH_MAX] = {0};

	if (config[0] == '/') {
		snprintf(config_file_path, sizeof(config_file_path), "%s", config);
	} else {
		snprintf(config_file_path, sizeof(config_file_path), "%s/%s", UCI_PATH_PREFIX, config);
	}

	errno = 0;
	error = remove(config_file_path);
	if (error) {
		DEBUG("remove(%s) error(%d): %s", config_file_path, errno, strerror(errno));
		uci2_error = (errno == ENOENT) ? UE_FILE_NOT_FOUND : UE_FILE_IO;
		goto error_out;
	}

	goto out;

error_out:
out:
	return uci2_error;
}

uci2_error_e uci2_ast_create(uci2_ast_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;

	uci2_ast = xcalloc(1, sizeof(uci2_ast_t));
	ast_init(uci2_ast);

	uci2_ast->root = ast_node_new(uci2_ast, ANT_ROOT, xstrdup(AST_NODE_ROOT_NAME), NULL);
	node = ast_node_new(uci2_ast, ANT_CONFIG, xstrdup(AST_NODE_CONFIG_NAME), NULL);
	ast_node_add(uci2_ast->root, node);

	*out = uci2_ast;

	return error;
}

// same rules as libuci's uci_validate_name() and uci_validate_type():
// names only contain [a-zA-Z0-9_], types may contain any printable non-space ASCII character
static bool uci2_name_is_valid(const char *name, bool is_type)
{
	unsigned char c = 0;

	if (name == NULL || name[0] == '\0') {
		return false;
	}

	for (; *name; name++) {
		c = (unsigned char) *name;
		if ((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '_') {
			continue;
		}

		if (is_type == false || c < 33 || c > 126) {
			return false;
		}
	}

	return true;
}

// true if `string` can be written without quotes and is read back unchanged by uci2 and libuci
static bool uci2_string_is_plain(const char *string)
{
	for (; *string; string++) {
		if (strchr("'\"\\#;", *string)) {
			return false;
		}
	}

	return true;
}

// write a value in single quotes, escaping single quotes the same way libuci does: ' -> '\''
static int uci2_value_print(FILE *stream, const char *value)
{
	if (fputc('\'', stream) == EOF) {
		return -1;
	}

	for (; *value; value++) {
		if (*value == '\'') {
			if (fputs("'\\''", stream) == EOF) {
				return -1;
			}
		} else if (fputc((unsigned char) *value, stream) == EOF) {
			return -1;
		}
	}

	if (fputc('\'', stream) == EOF) {
		return -1;
	}

	return 0;
}

static uci2_error_e uci2_ast_export(uci2_node_t *config_node, FILE *stream)
{
	uci2_node_t *section_type_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *option_node = NULL;
	uci2_node_t *list_element_node = NULL;

	for (size_t i = 0; i < config_node->children_number; i++) {
		section_type_node = config_node->children[i];
		if (section_type_node->parent == NULL) { // skip deleted section type nodes
			continue;
		}

		if (section_type_node->name == NULL) { // skip section type nodes with no name
			continue;
		}

		for (size_t j = 0; j < section_type_node->children_number; j++) {
			section_node = section_type_node->children[j];
			if (section_node->parent == NULL) { // skip deleted section nodes
				continue;
			}

			if (section_node->name == NULL) { // skip section nodes with no name
				continue;
			}

			if (uci2_name_is_valid(section_type_node->name, true) == false) {
				DEBUG("invalid section type: %s", section_type_node->name);
				return UE_INVALID_ARGUMENT;
			}

			// types are written unquoted like libuci does, unless they contain special characters
			if (uci2_string_is_plain(section_type_node->name)) {
				if (fprintf(stream, "config %s", section_type_node->name) < 0) {
					return UE_FILE_IO;
				}
			} else if (fputs("config ", stream) == EOF || uci2_value_print(stream, section_type_node->name) < 0) {
				return UE_FILE_IO;
			}

			if (section_node->name[0] != '@') {
				if (uci2_name_is_valid(section_node->name, false) == false) {
					DEBUG("invalid section name: %s", section_node->name);
					return UE_INVALID_ARGUMENT;
				}

				if (fputc(' ', stream) == EOF || uci2_value_print(stream, section_node->name) < 0) {
					return UE_FILE_IO;
				}
			}

			if (fputc('\n', stream) == EOF) {
				return UE_FILE_IO;
			}

			for (size_t k = 0; k < section_node->children_number; k++) {
				option_node = section_node->children[k];
				if (option_node->parent == NULL) { // skip deleted option nodes
					continue;
				}

				if (option_node->name == NULL) { // skip option and list nodes with no name
					continue;
				}

				if (option_node->type != ANT_OPTION && option_node->type != ANT_LIST) {
					DEBUG("unreachable");
					abort();
				}

				if (uci2_name_is_valid(option_node->name, false) == false) {
					DEBUG("invalid option or list name: %s", option_node->name);
					return UE_INVALID_ARGUMENT;
				}

				if (option_node->type == ANT_OPTION) {
					// skip option nodes with no value, libuci reads an empty value as "not set"
					if (option_node->value == NULL || option_node->value[0] == '\0') {
						continue;
					}

					if (fprintf(stream, "\toption %s ", option_node->name) < 0 ||
						uci2_value_print(stream, option_node->value) < 0 ||
						fputc('\n', stream) == EOF) {
						return UE_FILE_IO;
					}
				} else {
					// lists without elements are not written, libuci can't represent them
					for (size_t l = 0; l < option_node->children_number; l++) {
						list_element_node = option_node->children[l];
						if (list_element_node->parent == NULL) { // skip deleted list element nodes
							continue;
						}

						if (list_element_node->name == NULL) { // skip list element nodes with no name
							continue;
						}

						if (fprintf(stream, "\tlist %s ", option_node->name) < 0 ||
							uci2_value_print(stream, list_element_node->name) < 0 ||
							fputc('\n', stream) == EOF) {
							return UE_FILE_IO;
						}
					}
				}
			}

			if (fputc('\n', stream) == EOF) {
				return UE_FILE_IO;
			}
		}
	}

	return UE_NONE;
}

static int uci2_write_all(int fd, const char *buffer, size_t size)
{
	ssize_t written = 0;

	while (size > 0) {
		written = write(fd, buffer, size);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}

			return -1;
		}

		buffer += written;
		size -= (size_t) written;
	}

	return 0;
}

// Atomically replace the file at `path` with `content`, following libuci's uci_file_commit():
// the target is locked with flock(), the content is written to a temporary file in the same
// directory which gets the mode of the target and is then renamed over the target.
static uci2_error_e uci2_file_replace(const char *path, const char *content, size_t content_size)
{
	uci2_error_e uci2_error = UE_NONE;
	char directory_path[PATH_MAX] = {0};
	char temporary_path[PATH_MAX] = {0};
	const char *separator = NULL;
	int target_fd = -1;
	int temporary_fd = -1;
	int directory_fd = -1;
	bool target_created = false;
	struct stat stat_buffer = {0};
	int length = 0;

	separator = strrchr(path, '/');
	if (separator == NULL || separator[1] == '\0') {
		DEBUG("invalid config path: %s", path);
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	length = snprintf(directory_path, sizeof(directory_path), "%.*s", (int) (separator - path), path);
	if (length == 0) {
		snprintf(directory_path, sizeof(directory_path), "/");
	}

	length = snprintf(temporary_path, sizeof(temporary_path), "%s/.%s.uci2-XXXXXX", directory_path, separator + 1);
	if (length < 0 || (size_t) length >= sizeof(temporary_path)) {
		DEBUG("config path too long: %s", path);
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	errno = 0;
	target_fd = open(path, O_RDWR | O_CLOEXEC);
	if (target_fd < 0 && errno == ENOENT) {
		target_fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, UCI2_FILE_MODE);
		target_created = (target_fd >= 0);
	}

	if (target_fd < 0) {
		DEBUG("open(%s) error(%d): %s", path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	// like libuci, ignore file systems that don't support locking
	while (flock(target_fd, LOCK_EX) < 0 && errno != ENOSYS) {
		if (errno != EINTR) {
			DEBUG("flock(%s) error(%d): %s", path, errno, strerror(errno));
			uci2_error = UE_FILE_IO;
			goto error_out;
		}
	}

	if (fstat(target_fd, &stat_buffer) < 0) {
		DEBUG("fstat(%s) error(%d): %s", path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	if (S_ISREG(stat_buffer.st_mode) == 0) {
		DEBUG("%s is not a regular file", path);
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	temporary_fd = mkstemp(temporary_path);
	if (temporary_fd < 0) {
		DEBUG("mkstemp(%s) error(%d): %s", temporary_path, errno, strerror(errno));
		temporary_path[0] = '\0';
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	fcntl(temporary_fd, F_SETFD, FD_CLOEXEC);

	if (fchmod(temporary_fd, stat_buffer.st_mode & 07777) < 0) {
		DEBUG("fchmod(%s) error(%d): %s", temporary_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	if (uci2_write_all(temporary_fd, content, content_size) < 0) {
		DEBUG("write(%s) error(%d): %s", temporary_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	if (fsync(temporary_fd) < 0) {
		DEBUG("fsync(%s) error(%d): %s", temporary_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	if (close(temporary_fd) < 0) {
		temporary_fd = -1;
		DEBUG("close(%s) error(%d): %s", temporary_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	temporary_fd = -1;

	if (rename(temporary_path, path) < 0) {
		DEBUG("rename(%s, %s) error(%d): %s", temporary_path, path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	temporary_path[0] = '\0';
	target_created = false;

	// persist the rename itself
	directory_fd = open(directory_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (directory_fd < 0 || (fsync(directory_fd) < 0 && errno != EINVAL)) {
		DEBUG("fsync(%s) error(%d): %s", directory_path, errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	goto out;

error_out:
	if (temporary_path[0] != '\0') {
		unlink(temporary_path);
	}

	// don't leave an empty file behind if we created it
	if (target_created) {
		unlink(path);
	}

out:
	if (directory_fd >= 0) {
		close(directory_fd);
	}

	if (temporary_fd >= 0) {
		close(temporary_fd);
	}

	// closing the target file also releases the lock
	if (target_fd >= 0) {
		close(target_fd);
	}

	return uci2_error;
}

uci2_error_e uci2_ast_sync(uci2_ast_t *uci2_ast, const char *config)
{
	uci2_error_e uci2_error = UE_NONE;
	uci2_node_t *config_node = NULL;
	char config_file_path[PATH_MAX] = {0};
	FILE *stream = NULL;
	char *content = NULL;
	size_t content_size = 0;

	if (uci2_ast == NULL) {
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (config == NULL) {
		uci2_error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_ast->root == NULL) {
		DEBUG("could not find root node");
		uci2_error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	// start from config node
	for (size_t i = 0; i < uci2_ast->root->children_number; i++) {
		if (uci2_ast->root->children[i] &&
			uci2_ast->root->children[i]->parent &&
			uci2_ast->root->children[i]->type == ANT_CONFIG) {
			config_node = uci2_ast->root->children[i];
			break;
		}
	}

	if (config_node == NULL) {
		DEBUG("could not find config node");
		uci2_error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (config[0] == '/') {
		snprintf(config_file_path, sizeof(config_file_path), "%s", config);
	} else {
		snprintf(config_file_path, sizeof(config_file_path), "%s/%s", UCI_PATH_PREFIX, config);
	}

	// serialize into memory first, so the config file is never touched if this fails
	stream = open_memstream(&content, &content_size);
	if (stream == NULL) {
		DEBUG("open_memstream error(%d): %s", errno, strerror(errno));
		uci2_error = UE_FILE_IO;
		goto error_out;
	}

	uci2_error = uci2_ast_export(config_node, stream);
	if (fclose(stream) != 0 && uci2_error == UE_NONE) {
		uci2_error = UE_FILE_IO;
	}

	if (uci2_error) {
		DEBUG("uci2_ast_export error (%d): %s", uci2_error, uci2_error_description_get(uci2_error));
		goto error_out;
	}

	uci2_error = uci2_file_replace(config_file_path, content, content_size);

	goto out;

error_out:
out:
	free(content);

	return uci2_error;
}

void uci2_ast_compact(uci2_ast_t *uci2_ast)
{
	ast_compact(uci2_ast);
}

void uci2_ast_destroy(uci2_ast_t **uci2_ast)
{
	if (uci2_ast && *uci2_ast) {
		ast_destroy(*uci2_ast);
		*uci2_ast = NULL;
	}
}

uci2_error_e uci2_node_get(uci2_ast_t *uci2_ast, const char *section, const char *option, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *option_node = NULL;
	uci2_node_t *list_node = NULL;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (section == NULL && option) {
		DEBUG("section must not be NULL if option is specified");
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_ast->root == NULL) {
		DEBUG("could not find root node");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	// start from config node
	for (size_t i = 0; i < uci2_ast->root->children_number; i++) {
		if (uci2_ast->root->children[i] &&
			uci2_ast->root->children[i]->parent &&
			uci2_ast->root->children[i]->type == ANT_CONFIG) {
			node = uci2_ast->root->children[i];
			break;
		}
	}

	if (node == NULL) {
		DEBUG("could not find config node");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (section) {
		for (size_t i = 0; i < node->children_number; i++) {
			if (node->children[i]->parent != node) { // skip removed section type nodes
				continue;
			}

			for (size_t j = 0; j < node->children[i]->children_number; j++) {
				if (node->children[i]->children[j]->parent == node->children[i] &&
					node->children[i]->children[j]->name &&
					strcmp(node->children[i]->children[j]->name, section) == 0) {
					section_node = node->children[i]->children[j];
					break;
				}
			}

			if (section_node) {
				break;
			}
		}

		if (section_node == NULL || section_node->parent == NULL) {
			DEBUG("could not find section node");
			error = UE_NODE_NOT_FOUND;
			goto error_out;
		}

		node = section_node;

		if (option) {
			for (size_t i = 0; i < section_node->children_number; i++) {
				if (section_node->children[i]->parent == section_node && // skip removed nodes
					section_node->children[i]->name &&
					strcmp(section_node->children[i]->name, option) == 0) {
					if (section_node->children[i]->type == ANT_OPTION) {
						option_node = section_node->children[i];
						break;

					} else if (section_node->children[i]->type == ANT_LIST) {
						list_node = section_node->children[i];
						break;

					} else {
						DEBUG("unreachable");
						abort();
					}
				}
			}

			if (option_node && option_node->parent) {
				node = option_node;
			} else if (list_node && list_node->parent) {
				node = list_node;
			} else {
				DEBUG("could not find option or list node");
				error = UE_NODE_NOT_FOUND;
				goto error_out;
			}
		}
	}

	*out = node;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_section_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, const char *type, const char *name, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (type == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	// validate before any node is created
	if (uci2_name_is_valid(type, true) == false || (name && uci2_name_is_valid(name, false) == false)) {
		DEBUG("invalid section type or name");
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_add(uci2_ast, parent, UNT_SECTION, &node);
	if (error) {
		DEBUG("uci2_node_add error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	error = uci2_node_section_type_set(node, type);
	if (error) {
		DEBUG("uci2_node_section_type_set error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (name) {
		error = uci2_node_section_name_set(node, name);
		if (error) {
			DEBUG("uci2_node_section_name_set error (%d): %s", error, uci2_error_description_get(error));
			goto error_out;
		}
	}

	*out = node;

	goto out;

error_out:
	uci2_node_remove(node);

out:
	return error;
}

uci2_error_e uci2_node_option_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, const char *name, const char *value, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(name, false) == false) {
		DEBUG("invalid name: %s", name);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_add(uci2_ast, parent, UNT_OPTION, &node);
	if (error) {
		DEBUG("uci2_node_add error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	error = uci2_node_option_name_set(node, name);
	if (error) {
		DEBUG("uci2_node_option_name_set error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	error = uci2_node_option_value_set(node, value);
	if (error) {
		DEBUG("uci2_node_option_value_set error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	*out = node;

	goto out;

error_out:
	uci2_node_remove(node);

out:
	return error;
}

uci2_error_e uci2_node_list_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, const char *name, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(name, false) == false) {
		DEBUG("invalid name: %s", name);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_add(uci2_ast, parent, UNT_LIST, &node);
	if (error) {
		DEBUG("uci2_node_add error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	error = uci2_node_list_name_set(node, name);
	if (error) {
		DEBUG("uci2_node_list_name_set error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	*out = node;

	goto out;

error_out:
	uci2_node_remove(node);

out:
	return error;
}

uci2_error_e uci2_node_list_element_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, const char *value, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_add(uci2_ast, parent, UNT_LIST_ELEMENT, &node);
	if (error) {
		DEBUG("uci2_node_add error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	error = uci2_node_list_element_value_set(node, value);
	if (error) {
		DEBUG("uci2_node_list_element_value_set error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	*out = node;

	goto out;

error_out:
	uci2_node_remove(node);

out:
	return error;
}

static uci2_error_e uci2_node_add(uci2_ast_t *uci2_ast, uci2_node_t *parent, uci2_node_type_e type, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e parent_type = UNT_ROOT;
	uci2_node_t *type_node = NULL;
	uci2_node_t *node = NULL;
	enum ast_node_type node_type = ANT_ROOT;

	if (uci2_ast == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (parent->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	// validate parent and child types
	error = uci2_node_type_get(parent, &parent_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (type == UNT_SECTION) {
		if (parent_type != UNT_ROOT) {
			DEBUG("node type mismatch");
			error = UE_NODE_TYPE_MISMATCH;
			goto error_out;
		}

		// add section type node into the pool
		type_node = ast_node_new(uci2_ast, ANT_SECTION_TYPE, NULL, NULL);
		// add type node to its parent node
		ast_node_add(parent, type_node);

		// add section name node into the pool
		node = ast_node_new(uci2_ast, ANT_SECTION_NAME, NULL, NULL);
		// add section node to its parent node which is type_node
		ast_node_add(type_node, node);
	} else {
		switch (type) {
			case UNT_OPTION:
				if (parent_type != UNT_SECTION) {
					DEBUG("node type mismatch");
					error = UE_NODE_TYPE_MISMATCH;
					goto error_out;
				}

				node_type = ANT_OPTION;
				break;

			case UNT_LIST:
				if (parent_type != UNT_SECTION) {
					DEBUG("node type mismatch");
					error = UE_NODE_TYPE_MISMATCH;
					goto error_out;
				}

				node_type = ANT_LIST;
				break;

			case UNT_LIST_ELEMENT:
				if (parent_type != UNT_LIST) {
					DEBUG("node type mismatch");
					error = UE_NODE_TYPE_MISMATCH;
					goto error_out;
				}

				node_type = ANT_LIST_ITEM;
				break;

			default:
				DEBUG("unknown node type: %d", type);
				error = UE_INVALID_ARGUMENT;
				goto error_out;
		}

		// add new node into the pool
		node = ast_node_new(uci2_ast, node_type, NULL, NULL);
		// add new node to its parent node
		ast_node_add(parent, node);
	}

	*out = node;

	goto out;

error_out:
out:
	return error;
}

void uci2_node_remove(uci2_node_t *node)
{
	if (node) {
		node->parent = NULL;
	}
}

uci2_error_e uci2_node_iterator_new(uci2_node_t *node, uci2_node_iterator_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;
	uci2_node_iterator_t *node_iterator = NULL;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_ROOT && node_type != UNT_SECTION && node_type != UNT_LIST) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	node_iterator = xcalloc(1, sizeof(uci2_node_iterator_t));

	node_iterator->node_start = node;
	node_iterator->offset_i = 0;
	node_iterator->offset_j = 0;

	*out = node_iterator;

	goto out;

error_out:
	uci2_node_iterator_destroy(&node_iterator);

out:
	return error;
}

void uci2_node_iterator_destroy(uci2_node_iterator_t **node_iterator)
{
	if (node_iterator && *node_iterator) {
		free(*node_iterator);
		*node_iterator = NULL;
	}
}

uci2_error_e uci2_node_iterator_next(uci2_node_iterator_t *node_iterator, uci2_node_t **out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_t *node = NULL;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node_iterator == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	error = uci2_node_type_get(node_iterator->node_start, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type == UNT_ROOT) {
		while (node_iterator->offset_i < node_iterator->node_start->children_number &&
			   node_iterator->node_start->children[node_iterator->offset_i]->children_number == 0) {
			node_iterator->offset_i++;
			node_iterator->offset_j = 0;
		}
		do {
			if (node_iterator->offset_i >= node_iterator->node_start->children_number) {
				DEBUG("iterator end");
				node = NULL;
				error = UE_ITERATOR_END;
				goto error_out;
			}

			node = node_iterator->node_start->children[node_iterator->offset_i]->children[node_iterator->offset_j++];

			if (node_iterator->offset_j >= node_iterator->node_start->children[node_iterator->offset_i]->children_number) {
				node_iterator->offset_i++;
				node_iterator->offset_j = 0;
			}
		} while (node->parent == NULL);
	} else {
		do {
			if (node_iterator->offset_i >= node_iterator->node_start->children_number) {
				DEBUG("iterator end");
				node = NULL;
				error = UE_ITERATOR_END;
				goto error_out;
			}

			node = node_iterator->node_start->children[node_iterator->offset_i++];
		} while (node->parent == NULL);
	}

	*out = node;

	goto out;

error_out:

out:
	return error;
}

uci2_error_e uci2_node_type_get(uci2_node_t *node, uci2_node_type_e *out)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	switch (node->type) {
		case ANT_CONFIG:
			node_type = UNT_ROOT;
			break;

		case ANT_SECTION_NAME:
			node_type = UNT_SECTION;
			break;

		case ANT_OPTION:
			node_type = UNT_OPTION;
			break;

		case ANT_LIST:
			node_type = UNT_LIST;
			break;

		case ANT_LIST_ITEM:
			node_type = UNT_LIST_ELEMENT;
			break;

		default:
			DEBUG("unreachable");
			abort();
	}

	*out = node_type;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_section_type_get(uci2_node_t *node, const char **type)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (type == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->parent->name == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_SECTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*type = node->parent->name;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_section_type_set(uci2_node_t *node, const char *type)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;
	uci2_node_t *type_node = NULL;
	uci2_node_t *config_node = NULL;
	uci2_node_t *new_type_node = NULL;
	bool is_type_shared = false;
	ast_t ast_pool = {0};

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (type == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(type, true) == false) {
		DEBUG("invalid name: %s", type);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_SECTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	// Sections of the same type share a section type node.
	// Like libuci, only the type of this section is changed:
	// it is moved to the section type node of the new type.
	type_node = node->parent;
	config_node = type_node->parent;

	if (type_node->name && strcmp(type_node->name, type) == 0) {
		goto out;
	}

	for (size_t i = 0; i < config_node->children_number; i++) {
		if (config_node->children[i] != type_node &&
			config_node->children[i]->parent == config_node &&
			config_node->children[i]->name &&
			strcmp(config_node->children[i]->name, type) == 0) {
			new_type_node = config_node->children[i];
			break;
		}
	}

	for (size_t i = 0; i < type_node->children_number; i++) {
		if (type_node->children[i] != node && type_node->children[i]->parent == type_node) {
			is_type_shared = true;
			break;
		}
	}

	if (new_type_node) {
		ast_node_detach(node);
		ast_node_add(new_type_node, node);
		if (is_type_shared == false) {
			// the old section type node is not used anymore
			uci2_node_remove(type_node);
		}
	} else if (is_type_shared) {
		ast_node_detach(node);
		ast_pool.pool = ast_node_pool_get(config_node);
		new_type_node = ast_node_new(&ast_pool, ANT_SECTION_TYPE, xstrdup(type), NULL);
		ast_node_add(config_node, new_type_node);
		ast_node_add(new_type_node, node);
	} else {
		// no other section uses this section type node, so it can be renamed
		XFREE(type_node->name);
		type_node->name = xstrdup(type);
		type_node->unnamed_children_number = 0;
	}

	// set correct names for unnamed section nodes
	if (node->name == NULL || node->name[0] == '@') {
		XFREE(node->name);
		node->name = xstrdup(UNNAMED_SECTION_NAME_PLACEHOLDER);
		unnamed_section_name_set(config_node);
	}

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_section_name_get(uci2_node_t *node, const char **name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->name == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_SECTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*name = node->name;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_section_name_set(uci2_node_t *node, const char *name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(name, false) == false) {
		DEBUG("invalid name: %s", name);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_SECTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	for (size_t i = 0; i < node->parent->children_number; i++) {
		if (node->parent->children[i] &&
			node->parent->children[i]->parent &&
			node->parent->children[i]->name &&
			strcmp(node->parent->children[i]->name, name) == 0) {
			DEBUG("section named '%s' already exists", node->parent->children[i]->name);
			error = UE_NODE_DUPLICATE;
			goto error_out;
		}
	}

	XFREE(node->name);
	node->name = xstrdup(name);

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_option_name_get(uci2_node_t *node, const char **name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->name == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_OPTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*name = node->name;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_option_name_set(uci2_node_t *node, const char *name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(name, false) == false) {
		DEBUG("invalid name: %s", name);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_OPTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	for (size_t i = 0; i < node->parent->children_number; i++) {
		if (node->parent->children[i] &&
			node->parent->children[i]->parent &&
			node->parent->children[i]->name &&
			strcmp(node->parent->children[i]->name, name) == 0) {
			DEBUG("option named '%s' already exists", node->parent->children[i]->name);
			error = UE_NODE_DUPLICATE;
			goto error_out;
		}
	}

	XFREE(node->name);
	node->name = xstrdup(name);

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_option_value_get(uci2_node_t *node, const char **value)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->value == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_OPTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*value = node->value;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_option_value_set(uci2_node_t *node, const char *value)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_OPTION) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	XFREE(node->value);
	node->value = xstrdup(value);

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_list_name_get(uci2_node_t *node, const char **name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->name == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_LIST) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*name = node->name;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_list_name_set(uci2_node_t *node, const char *name)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (name == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (uci2_name_is_valid(name, false) == false) {
		DEBUG("invalid name: %s", name);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_LIST) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	for (size_t i = 0; i < node->parent->children_number; i++) {
		if (node->parent->children[i] &&
			node->parent->children[i]->parent &&
			node->parent->children[i]->name &&
			strcmp(node->parent->children[i]->name, name) == 0) {
			DEBUG("list named '%s' already exists", node->parent->children[i]->name);
			error = UE_NODE_DUPLICATE;
			goto error_out;
		}
	}

	XFREE(node->name);
	node->name = xstrdup(name);

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_list_element_value_get(uci2_node_t *node, const char **value)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	if (node->name == NULL) {
		DEBUG("node attribute missing");
		error = UE_NODE_ATTRIBUTE_MISSING;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_LIST_ELEMENT) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	*value = node->name;

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_node_list_element_value_set(uci2_node_t *node, const char *value)
{
	uci2_error_e error = UE_NONE;
	uci2_node_type_e node_type = UNT_ROOT;

	if (node == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (node->parent == NULL) {
		DEBUG("node deleted");
		error = UE_NODE_NOT_FOUND;
		goto error_out;
	}

	error = uci2_node_type_get(node, &node_type);
	if (error) {
		DEBUG("uci2_node_type_get error (%d): %s", error, uci2_error_description_get(error));
		goto error_out;
	}

	if (node_type != UNT_LIST_ELEMENT) {
		DEBUG("node type mismatch");
		error = UE_NODE_TYPE_MISMATCH;
		goto error_out;
	}

	XFREE(node->name);
	node->name = xstrdup(value);

	goto out;

error_out:
out:
	return error;
}

uci2_error_e uci2_string_to_boolean(const char *string_value, bool *out)
{
	uci2_error_e error = UE_NONE;
	bool boolean_value = false;

	if (string_value == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (out == NULL) {
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	if (strcmp(string_value, "0") == 0 ||
		strcmp(string_value, "no") == 0 ||
		strcmp(string_value, "off") == 0 ||
		strcmp(string_value, "false") == 0 ||
		strcmp(string_value, "disabled") == 0) {
		boolean_value = false;
	} else if (strcmp(string_value, "1") == 0 ||
			   strcmp(string_value, "yes") == 0 ||
			   strcmp(string_value, "on") == 0 ||
			   strcmp(string_value, "true") == 0 ||
			   strcmp(string_value, "enabled") == 0) {
		boolean_value = true;
	} else {
		DEBUG("unknown string value: %s", string_value);
		error = UE_INVALID_ARGUMENT;
		goto error_out;
	}

	*out = boolean_value;

	goto out;

error_out:
out:
	return error;
}

const char *uci2_error_description_get(uci2_error_e error)
{
	switch (error) {
#define XM(ENUM, CODE, DESCRIPTION) \
	case ENUM:                      \
		return DESCRIPTION;

		UCI2_ERROR_TABLE
#undef XM
		default:
			return "undefined uci2_error_e";
	}
}
