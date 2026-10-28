#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>

#include <cmocka.h>

#include "utils/debug.h"

#include "parser.h"
#include "lexer.h"
#include "uci2.h"

#define CONFIG_DIRECTORY_PATH_TMP CONFIG_DIRECTORY_PATH "config/"

static int setup(void **state);
static int teardown(void **state);

static void test_uci2_node_get(void **state);
static void test_uci2_node_section_add(void **state);
static void test_uci2_node_option_add(void **state);
static void test_uci2_node_list_add(void **state);
static void test_uci2_node_list_element_add(void **state);

static void test_uci2_node_type_get(void **state);

static void test_uci2_node_section_type_get(void **state);
static void test_uci2_node_section_type_set(void **state);
static void test_uci2_node_section_name_get(void **state);
static void test_uci2_node_section_name_set(void **state);

static void test_uci2_node_option_name_get(void **state);
static void test_uci2_node_option_name_set(void **state);
static void test_uci2_node_option_value_get(void **state);
static void test_uci2_node_option_value_set(void **state);

static void test_uci2_node_list_name_get(void **state);
static void test_uci2_node_list_name_set(void **state);

static void test_uci2_node_list_element_value_get(void **state);
static void test_uci2_node_list_element_value_set(void **state);
static void test_uci2_string_to_boolean(void **state);

static void test_uci2_config_parse_success(void **state);
static void test_uci2_config_parse_error(void **state);
static void test_uci2_config_file_change(void **state);
static void test_uci2_ast_create(void **state);
static void test_uci2_config_remove(void **state);
static void test_uci2_node_iterator(void **state);
static void test_uci2_config_firewall(void **state);
static void test_uci2_value_escaping(void **state);
static void test_uci2_invalid_name(void **state);
static void test_uci2_ast_sync_file(void **state);
static void test_uci2_ast_compact(void **state);
static void test_uci2_section_type_set_single(void **state);
static void test_uci2_node_get_after_remove(void **state);
static void test_uci2_config_parse_syntax(void **state);

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_uci2_node_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_section_add, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_option_add, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_add, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_element_add, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_type_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_section_type_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_section_type_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_section_name_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_section_name_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_option_name_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_option_name_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_option_value_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_option_value_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_name_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_name_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_element_value_get, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_list_element_value_set, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_string_to_boolean, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_parse_success, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_parse_error, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_file_change, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_ast_create, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_remove, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_iterator, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_firewall, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_value_escaping, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_invalid_name, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_ast_sync_file, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_ast_compact, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_section_type_set_single, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_node_get_after_remove, setup, teardown),
		cmocka_unit_test_setup_teardown(test_uci2_config_parse_syntax, setup, teardown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}

static int setup(void **state)
{
	system("mkdir -p " CONFIG_DIRECTORY_PATH_TMP);
	system("cp " CONFIG_DIRECTORY_PATH "test_config_correct " CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	system("cp " CONFIG_DIRECTORY_PATH "test_config_incorrect " CONFIG_DIRECTORY_PATH_TMP "test_config_incorrect");
	system("cp " CONFIG_DIRECTORY_PATH "test_config_remove " CONFIG_DIRECTORY_PATH_TMP "test_config_remove");
	system("cp " CONFIG_DIRECTORY_PATH "test_config_iterator " CONFIG_DIRECTORY_PATH_TMP "test_config_iterator");
	system("cp " CONFIG_DIRECTORY_PATH "test_config_firewall " CONFIG_DIRECTORY_PATH_TMP "test_config_firewall");

	return 0;
}

static int teardown(void **state)
{
	system("rm -rf " CONFIG_DIRECTORY_PATH_TMP);

	return 0;
}

static void test_uci2_node_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_section_add(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *new_section_node = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	uci2_node_t *node_next = NULL;
	size_t index = 0;
	const char *section_type_match[] = {
		"system",
		"system",
		"timeserver",
		"new_section_type",
	};
	const char *section_name_match[] = {
		"@system[0]",
		"@system[1]",
		"ntp",
		"new_section_name",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "system", NULL, &new_section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(new_section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(new_section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "@system[1]");

	error = uci2_node_get(uci2_ast, "@system[1]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "@system[1]");

	error = uci2_node_section_add(uci2_ast, root_node, "new_section_type", "new_section_name", &new_section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(new_section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "new_section_type");

	error = uci2_node_section_name_get(new_section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "new_section_name");

	error = uci2_node_get(uci2_ast, "new_section_name", NULL, &section_node);
	error = uci2_node_section_type_get(section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "new_section_type");

	error = uci2_node_section_name_get(section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "new_section_name");

	error = uci2_node_section_add(uci2_ast, root_node, "timeserver", "ntp", &new_section_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "new_section_type", "new_section_name", &new_section_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_option_add(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *new_section_node = NULL;
	uci2_node_t *new_option_node = NULL;
	const char *option_name = NULL;
	const char *option_value = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	uci2_node_t *option_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	uci2_node_t *node_next = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	size_t index = 0;
	const char *section_type_match[] = {
		"system",
		"timeserver",
	};
	const char *section_name_match[] = {
		"@system[0]",
		"ntp",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "system", NULL, &new_section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(new_section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(new_section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "@system[1]");

	error = uci2_node_option_add(uci2_ast, new_section_node, "system_option", "system_value", &new_option_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(new_option_node, &option_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_name, "system_option");

	error = uci2_node_option_value_get(new_option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_value, "system_value");

	error = uci2_node_get(uci2_ast, "@system[1]", "system_option", &option_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(option_node, &option_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_name, "system_option");

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_value, "system_value");

	error = uci2_node_option_add(uci2_ast, new_section_node, "system_option", "system_value", &new_option_node);
	assert_int_not_equal(error, UE_NONE);

	uci2_node_remove(new_section_node);

	error = uci2_node_option_add(uci2_ast, new_section_node, "new_option", "new_value", &new_option_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(section_node, &iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);

		if (node_type == UNT_OPTION) {
			error = uci2_node_option_name_get(node_next, &option_name);
			assert_int_equal(error, UE_NONE);

			if (strcmp(option_name, "enabled") == 0) {
				uci2_node_remove(node_next);
			}
		}
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_option_add(uci2_ast, section_node, "enabled", "false", &new_option_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(new_option_node, &option_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_name, "enabled");

	error = uci2_node_option_value_get(new_option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(option_value, "false");

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[index]);

		if (strcmp(section_name, "ntp") == 0) {
			error = uci2_node_get(uci2_ast, section_name, "enabled", &option_node);
			assert_int_equal(error, 0);

			error = uci2_node_option_name_get(option_node, &option_name);
			assert_int_equal(error, UE_NONE);
			assert_string_equal(option_name, "enabled");

			error = uci2_node_option_value_get(option_node, &option_value);
			assert_int_equal(error, UE_NONE);
			assert_string_equal(option_value, "false");
		}

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_add(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *new_section_node = NULL;
	uci2_node_t *new_list_node = NULL;
	const char *list_name = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	uci2_node_t *list_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	uci2_node_t *node_next = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	size_t index = 0;
	const char *section_type_match[] = {
		"system",
		"timeserver",
	};
	const char *section_name_match[] = {
		"@system[0]",
		"ntp",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "system", NULL, &new_section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(new_section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(new_section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "@system[1]");

	error = uci2_node_list_add(uci2_ast, new_section_node, "system_list", &new_list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(new_list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(list_name, "system_list");

	error = uci2_node_get(uci2_ast, "@system[1]", "system_list", &list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(list_name, "system_list");

	error = uci2_node_list_add(uci2_ast, new_section_node, "system_list", &new_list_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_node_option_add(uci2_ast, new_section_node, "system_list", "system_value", &new_list_node);
	assert_int_not_equal(error, UE_NONE);

	uci2_node_remove(new_section_node);

	error = uci2_node_list_add(uci2_ast, new_section_node, "new_list", &new_list_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_add(uci2_ast, section_node, "server", &new_list_node);
	assert_int_not_equal(error, UE_NONE);

	error = uci2_node_iterator_new(section_node, &iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);

		if (node_type == UNT_LIST) {
			error = uci2_node_list_name_get(node_next, &list_name);
			assert_int_equal(error, UE_NONE);

			if (strcmp(list_name, "server") == 0) {
				uci2_node_remove(node_next);
			}
		}
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_list_add(uci2_ast, section_node, "server", &new_list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(new_list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(list_name, "server");

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[index]);

		if (strcmp(section_name, "ntp") == 0) {
			// the re-added "server" list has no elements, so it is not written
			error = uci2_node_get(uci2_ast, section_name, "server", &list_node);
			assert_int_equal(error, UE_NODE_NOT_FOUND);
		}

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_element_add(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *new_section_node = NULL;
	uci2_node_t *new_list_node = NULL;
	const char *list_name = NULL;
	uci2_node_t *new_list_element_node_1 = NULL;
	uci2_node_t *new_list_element_node_2 = NULL;
	const char *list_element_value = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	uci2_node_t *list_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	uci2_node_t *node_next = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "system", NULL, &new_section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(new_section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(new_section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(section_name, "@system[1]");

	error = uci2_node_list_add(uci2_ast, new_section_node, "system_list", &new_list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(new_list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(list_name, "system_list");

	error = uci2_node_list_element_add(uci2_ast, new_list_node, "list_element_1", &new_list_element_node_1);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_element_add(uci2_ast, new_list_node, "list_element_2", &new_list_element_node_2);
	assert_int_equal(error, UE_NONE);

	uci2_node_remove(new_list_element_node_1);

	error = uci2_node_get(uci2_ast, "@system[1]", "system_list", &list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(list_name, "system_list");

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(list_element_value, "list_element_2");
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		uci2_node_remove(node_next);
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "@system[1]", "system_list", &list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(list_element_value, "list_element_2");
	}

	uci2_node_iterator_destroy(&iterator);

	// all elements of "server" were removed, so the list is not written
	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_type_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	uci2_node_type_e node_type = UNT_ROOT;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_type_get(node, &node_type);
	assert_int_equal(error, UE_NONE);
	assert_int_equal(node_type, UNT_OPTION);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_section_type_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *type = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(node, &type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(type, "timeserver");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_section_type_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *type = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(node, &type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(type, "timeserver");

	error = uci2_node_section_type_set(node, "test_type");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(node, &type);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(type, "test_type");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_section_name_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "ntp");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_section_name_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "ntp");

	error = uci2_node_section_name_set(node, "test_name");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "test_name");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_option_name_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "enabled");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_option_name_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "enabled");

	error = uci2_node_option_name_set(node, "test_name");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "test_name");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_option_value_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "true");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_option_value_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "true");

	error = uci2_node_option_value_set(node, "test_value");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "test_value");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_name_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "server");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_name_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *name = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "server");

	error = uci2_node_list_name_set(node, "test_name");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_name_get(node, &name);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(name, "test_name");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_element_value_get(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	size_t index = 0;
	uci2_node_t *node_next = NULL;
	const char *value = NULL;
	static const char *list_element_value_match[] = {
		"0.openwrt.pool.ntp.org",
		"1.openwrt.pool.ntp.org",
		"2.openwrt.pool.ntp.org",
		"3.openwrt.pool.ntp.org",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_get(node_next, &value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(value, list_element_value_match[index]);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_list_element_value_set(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	size_t index = 0;
	uci2_node_t *node_next = NULL;
	const char *value = NULL;
	static const char *list_element_value_match[] = {
		"0.openwrt.pool.ntp.org",
		"1.openwrt.pool.ntp.org",
		"2.openwrt.pool.ntp.org",
		"3.openwrt.pool.ntp.org",
	};
	static const char *list_element_value_new[] = {
		"test_0",
		"test_1",
		"test_2",
		"test_3",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_get(node_next, &value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(value, list_element_value_match[index]);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_iterator_new(node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_set(node_next, list_element_value_new[index]);
		assert_int_equal(error, UE_NONE);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_iterator_new(node, &iterator);
	assert_int_equal(error, UE_NONE);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_list_element_value_get(node_next, &value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(value, list_element_value_new[index]);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_string_to_boolean(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;
	bool boolean_value = false;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "true");

	error = uci2_string_to_boolean(value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_true(boolean_value);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_config_parse_success(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	uci2_node_t *node_next = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	const char *section_type_match[] = {
		"system",
		"timeserver",
	};
	const char *section_name_match[] = {
		"@system[0]",
		"ntp",
	};
	size_t index = 0;
	uci2_node_t *section_node = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	uci2_node_t *option_node = NULL;
	const char *option_name = NULL;
	const char *option_value = NULL;
	bool boolean_value = false;
	uci2_node_t *list_node = NULL;
	const char *list_name = NULL;
	const char *list_element_value = NULL;
	const char *system_list_element_value_match[] = {
		"10.9.0.0 255.255.255.252",
		"192.168.100.0 255.255.255.0",
	};
	const char *ntp_list_element_value_match[] = {
		"0.openwrt.pool.ntp.org",
		"1.openwrt.pool.ntp.org",
		"2.openwrt.pool.ntp.org",
		"3.openwrt.pool.ntp.org",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(uci2_ast, NULL);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		assert_int_equal(node_type, UNT_SECTION);

		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_type, NULL);
		assert_string_equal(section_type, section_type_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_name, NULL);
		assert_string_equal(section_name, section_name_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "@system[0]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_node, NULL);

	error = uci2_node_section_type_get(section_node, &section_type);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_type, NULL);
	assert_string_equal(section_type, "system");

	error = uci2_node_section_name_get(section_node, &section_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_name, NULL);
	assert_string_equal(section_name, "@system[0]");

	error = uci2_node_get(uci2_ast, "@system[0]", "hostname", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_name_get(option_node, &option_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_name, NULL);
	assert_string_equal(option_name, "hostname");

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "OpenWrt");

	error = uci2_node_get(uci2_ast, "@system[0]", "timezone", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "UTC");

	error = uci2_node_get(uci2_ast, "@system[0]", "ttylogin", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "1");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_true(boolean_value);

	error = uci2_node_get(uci2_ast, "@system[0]", "log_size", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "64");

	error = uci2_node_get(uci2_ast, "@system[0]", "log_port", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "514");

	error = uci2_node_get(uci2_ast, "@system[0]", "urandom_seed", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "0");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_false(boolean_value);

	error = uci2_node_get(uci2_ast, "@system[0]", "route", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_name, NULL);
	assert_string_equal(list_name, "route");

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(list_element_value, NULL);
		assert_string_equal(list_element_value, system_list_element_value_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_node, NULL);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "true");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_true(boolean_value);

	error = uci2_node_get(uci2_ast, "ntp", "enable_server", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "0");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_false(boolean_value);

	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_name, NULL);
	assert_string_equal(list_name, "server");

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(list_element_value, NULL);
		assert_string_equal(list_element_value, ntp_list_element_value_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_config_parse_error(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_incorrect", &uci2_ast);
	assert_int_not_equal(error, 0);
	assert_ptr_equal(uci2_ast, NULL);
}

static void test_uci2_config_file_change(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *option_node = NULL;
	const char *option_value = NULL;
	bool boolean_value = false;
	uci2_node_t *list_node = NULL;
	uci2_node_t *list_element_node = NULL;
	uci2_node_iterator_t *iterator = NULL;
	size_t index = 0;
	uci2_node_t *node_next = NULL;
	const char *list_element_value = NULL;
	const char *list_element_value_match[] = {
		"0.openwrt.pool.ntp.org",
		"1.openwrt.pool.ntp.org",
		"2.openwrt.pool.ntp.org",
		"3.openwrt.pool.ntp.org",
		"new_value",
	};
	uci2_node_t *section_ntp_node = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	const char *option_name = NULL;
	const char *list_name = NULL;
	uci2_node_t *section_system_node = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *new_section_node = NULL;
	uci2_node_t *new_option_node = NULL;
	uci2_node_t *new_list_node = NULL;
	uci2_node_t *new_list_element_node = NULL;
	const char *section_type = NULL;
	const char *section_name = NULL;
	const char *section_type_match[] = {
		"timeserver",
		"new_section_type",
	};
	const char *section_name_match[] = {
		"ntp",
		"new_section_name",
	};
	uci2_node_iterator_t *list_iterator = NULL;
	const char *new_list_element_value_match[] = {
		"new_list_element_value",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(uci2_ast, NULL);

	error = uci2_node_get(uci2_ast, "ntp", "enable_server", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_set(option_node, "1");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "enable_server", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "1");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_true(boolean_value);

	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_list_element_add(uci2_ast, list_node, "new_value", &list_element_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(list_element_value, NULL);
		assert_string_equal(list_element_value, list_element_value_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	uci2_node_remove(option_node);

	error = uci2_node_get(uci2_ast, "ntp", "enabled", &option_node);
	assert_int_not_equal(error, 0);

	error = uci2_node_get(uci2_ast, "ntp", NULL, &section_ntp_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_ntp_node, NULL);
	error = uci2_node_iterator_new(section_ntp_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		switch (node_type) {
			case UNT_OPTION:
				error = uci2_node_option_name_get(node_next, &option_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_name, "enable_server");
				break;

			case UNT_LIST:
				error = uci2_node_list_name_get(node_next, &list_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(list_name, "server");
				break;

			default:
				abort();
		}
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "@system[0]", NULL, &section_system_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_system_node, NULL);

	uci2_node_remove(section_system_node);

	error = uci2_node_get(uci2_ast, "@system[0]", NULL, &section_system_node);
	assert_int_not_equal(error, 0);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_section_add(uci2_ast, root_node, "new_section_type", "new_section_name", &new_section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(new_section_node, NULL);

	error = uci2_node_option_add(uci2_ast, new_section_node, "new_option_name", "new_option_value", &new_option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(new_option_node, NULL);

	error = uci2_node_list_add(uci2_ast, new_section_node, "new_list_name", &new_list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(new_list_node, NULL);

	error = uci2_node_list_element_add(uci2_ast, new_list_node, "new_list_element_value", &new_list_element_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(new_list_element_node, NULL);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		assert_int_equal(node_type, UNT_SECTION);
		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[index]);
		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[index]);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_iterator_new(new_section_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		switch (node_type) {
			case UNT_OPTION:
				error = uci2_node_option_name_get(node_next, &option_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_name, "new_option_name");

				error = uci2_node_option_value_get(node_next, &option_value);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_value, "new_option_value");
				break;

			case UNT_LIST:
				error = uci2_node_list_name_get(node_next, &list_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(list_name, "new_list_name");

				error = uci2_node_iterator_new(node_next, &list_iterator);
				assert_int_equal(error, UE_NONE);
				assert_ptr_not_equal(list_iterator, NULL);

				while (uci2_node_iterator_next(list_iterator, &list_element_node) == UE_NONE) {
					error = uci2_node_list_element_value_get(list_element_node, &list_element_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(list_element_value, "new_list_element_value");
				}

				uci2_node_iterator_destroy(&list_iterator);
				break;

			default:
				abort();
		}
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	// parse the file again after writing the new configuration

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(uci2_ast, NULL);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_iterator_new(root_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[index]);
		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[index]);
		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "ntp", "enable_server", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "1");
	error = uci2_string_to_boolean(option_value, &boolean_value);
	assert_int_equal(error, UE_NONE);
	assert_true(boolean_value);

	error = uci2_node_get(uci2_ast, "ntp", "server", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_name, NULL);
	assert_string_equal(list_name, "server");

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(list_element_value, NULL);
		assert_string_equal(list_element_value, list_element_value_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	error = uci2_node_get(uci2_ast, "new_section_name", "new_option_name", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "new_option_value");

	error = uci2_node_get(uci2_ast, "new_section_name", "new_list_name", &list_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_node, NULL);

	error = uci2_node_list_name_get(list_node, &list_name);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(list_name, NULL);
	assert_string_equal(list_name, "new_list_name");

	error = uci2_node_iterator_new(list_node, &iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_list_element_value_get(node_next, &list_element_value);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(list_element_value, NULL);
		assert_string_equal(list_element_value, new_list_element_value_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_config_remove(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_remove", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_remove(CONFIG_DIRECTORY_PATH_TMP "test_config_remove");
	assert_int_equal(error, UE_NONE);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_remove", &uci2_ast);
	assert_int_equal(error, UE_FILE_NOT_FOUND);
}

static void test_uci2_ast_create(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *option_node = NULL;
	uci2_node_t *list_node = NULL;
	uci2_node_t *list_element_node = NULL;
	uci2_node_iterator_t *option_iterator = NULL;
	uci2_node_t *option_next = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	const char *option_name = NULL;
	const char *option_value = NULL;
	uci2_node_iterator_t *list_element_iterator = NULL;
	uci2_node_t *list_element_next = NULL;
	size_t list_element_index = 0;
	const char *list_element_value = NULL;
	const char *list_element_value_match[] = {
		"item 1",
		"item 2",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_create", &uci2_ast);
	assert_int_equal(error, UE_FILE_NOT_FOUND);

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "interface", "test", &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_add(uci2_ast, section_node, "name", "eth0", &option_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_add(uci2_ast, section_node, "items", &list_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_element_add(uci2_ast, list_node, "item 1", &list_element_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_list_element_add(uci2_ast, list_node, "item 2", &list_element_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_create");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_create", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "test", NULL, &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_iterator_new(section_node, &option_iterator);
	assert_int_equal(error, UE_NONE);

	while (uci2_node_iterator_next(option_iterator, &option_next) == UE_NONE) {
		error = uci2_node_type_get(option_next, &node_type);
		switch (node_type) {
			case UNT_OPTION:
				error = uci2_node_option_name_get(option_next, &option_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_name, "name");

				error = uci2_node_option_value_get(option_next, &option_value);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_value, "eth0");
				break;

			case UNT_LIST:
				error = uci2_node_list_name_get(option_next, &option_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_name, "items");

				error = uci2_node_iterator_new(option_next, &list_element_iterator);
				assert_int_equal(error, UE_NONE);

				list_element_index = 0;
				while (uci2_node_iterator_next(list_element_iterator, &list_element_next) == UE_NONE) {
					error = uci2_node_list_element_value_get(list_element_next, &list_element_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(list_element_value, list_element_value_match[list_element_index]);
					list_element_index++;
				}

				uci2_node_iterator_destroy(&list_element_iterator);
				break;

			default:
				abort();
		}
	}

	uci2_node_iterator_destroy(&option_iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_iterator(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_iterator_t *section_iterator = NULL;
	uci2_node_t *section_next = NULL;
	size_t section_index = 0;
	uci2_node_type_e section_node_type = UNT_ROOT;
	const char *section_type = NULL;
	const char *section_name = NULL;
	const char *section_type_match[] = {
		"rule",
		"rule",
		"rule",
		"rule",
		"rule2",
		"rule2",
	};
	const char *section_name_match[] = {
		"@rule[0]",
		"@rule[1]",
		"rule_X",
		"@rule[2]",
		"@rule2[0]",
		"@rule2[1]",
	};
	uci2_node_iterator_t *option_iterator = NULL;
	uci2_node_t *option_next = NULL;
	size_t option_index = 0;
	uci2_node_type_e option_node_type = UNT_ROOT;
	const char *option_name = NULL;
	const char *option_value = NULL;
	const char *option_0_name_match[] = {
		"name",
		"src",
		"proto",
		"dest_port",
		"target",
		"family",
	};
	const char *option_0_value_match[] = {
		"Allow-DHCP-Renew",
		"wan",
		"udp",
		"68",
		"ACCEPT",
		"ipv4",
	};
	const char *option_1_name_match[] = {
		"name",
		"src",
		"proto",
		"icmp_type",
		"family",
		"target",
	};
	const char *option_1_value_match[] = {
		"Allow-Ping",
		"wan",
		"icmp",
		"echo-request",
		"ipv4",
		"ACCEPT",
	};
	const char *option_2_name_match[] = {
		"name",
		"src",
		"proto",
		"family",
		"target",
	};
	const char *option_2_value_match[] = {
		"Allow-IGMP",
		"wan",
		"igmp",
		"ipv4",
		"ACCEPT",
	};
	const char *option_3_name_match[] = {
		"src",
		"dest_port",
		"target",
		"proto",
	};
	const char *option_3_value_match[] = {
		"wan",
		"51820",
		"ACCEPT",
		"udp",
	};
	const char *option_4_name_match[] = {
		"name",
		"src",
		"proto",
		"src_ip",
		"dest_ip",
		"dest_port",
		"family",
		"target",
	};
	const char *option_4_value_match[] = {
		"Allow-DHCPv6",
		"wan",
		"udp",
		"fc00::/6",
		"fc00::/6",
		"546",
		"ipv6",
		"ACCEPT",
	};
	const char *option_5_name_match[] = {
		"name",
		"src",
		"proto",
		"src_ip",
		"icmp_type",
		"family",
		"target",
	};
	const char *option_5_value_match[] = {
		"Allow-MLD",
		"wan",
		"icmp",
		"fe80::/10",
		"",
		"ipv6",
		"ACCEPT",
	};
	const char *list_name = NULL;
	uci2_node_iterator_t *list_element_iterator = NULL;
	uci2_node_t *list_element_next = NULL;
	size_t list_element_index = 0;
	uci2_node_type_e list_element_node_type = UNT_ROOT;
	const char *list_element_value = NULL;
	const char *list_element_value_match[] = {
		"130/0",
		"131/0",
		"132/0",
		"143/0",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_iterator", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(uci2_ast, NULL);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_iterator_new(root_node, &section_iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_iterator, NULL);

	section_index = 0;
	while (uci2_node_iterator_next(section_iterator, &section_next) == UE_NONE) {
		error = uci2_node_type_get(section_next, &section_node_type);
		assert_int_equal(error, UE_NONE);
		assert_int_equal(section_node_type, UNT_SECTION);
		error = uci2_node_section_type_get(section_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_type, section_type_match[section_index]);

		error = uci2_node_section_name_get(section_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(section_name, section_name_match[section_index]);

		error = uci2_node_iterator_new(section_next, &option_iterator);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(option_iterator, NULL);

		option_index = 0;
		while (uci2_node_iterator_next(option_iterator, &option_next) == UE_NONE) {
			switch (section_index) {
				case 0:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					assert_int_equal(option_node_type, UNT_OPTION);

					error = uci2_node_option_name_get(option_next, &option_name);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_name, option_0_name_match[option_index]);

					error = uci2_node_option_value_get(option_next, &option_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_value, option_0_value_match[option_index]);
					break;

				case 1:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					assert_int_equal(option_node_type, UNT_OPTION);

					error = uci2_node_option_name_get(option_next, &option_name);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_name, option_1_name_match[option_index]);

					error = uci2_node_option_value_get(option_next, &option_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_value, option_1_value_match[option_index]);
					break;

				case 2:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					assert_int_equal(option_node_type, UNT_OPTION);

					error = uci2_node_option_name_get(option_next, &option_name);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_name, option_2_name_match[option_index]);

					error = uci2_node_option_value_get(option_next, &option_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_value, option_2_value_match[option_index]);
					break;

				case 3:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					assert_int_equal(option_node_type, UNT_OPTION);

					error = uci2_node_option_name_get(option_next, &option_name);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_name, option_3_name_match[option_index]);

					error = uci2_node_option_value_get(option_next, &option_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_value, option_3_value_match[option_index]);
					break;

				case 4:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					assert_int_equal(option_node_type, UNT_OPTION);

					error = uci2_node_option_name_get(option_next, &option_name);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_name, option_4_name_match[option_index]);

					error = uci2_node_option_value_get(option_next, &option_value);
					assert_int_equal(error, UE_NONE);
					assert_string_equal(option_value, option_4_value_match[option_index]);
					break;

				case 5:
					error = uci2_node_type_get(option_next, &option_node_type);
					assert_int_equal(error, UE_NONE);
					switch (option_node_type) {
						case UNT_OPTION:
							error = uci2_node_option_name_get(option_next, &option_name);
							assert_int_equal(error, UE_NONE);
							assert_string_equal(option_name, option_5_name_match[option_index]);

							error = uci2_node_option_value_get(option_next, &option_value);
							assert_int_equal(error, UE_NONE);
							assert_string_equal(option_value, option_5_value_match[option_index]);
							break;

						case UNT_LIST:
							error = uci2_node_list_name_get(option_next, &list_name);
							assert_int_equal(error, UE_NONE);
							assert_string_equal(list_name, option_5_name_match[option_index]);

							error = uci2_node_iterator_new(option_next, &list_element_iterator);
							assert_int_equal(error, UE_NONE);

							list_element_index = 0;
							while (uci2_node_iterator_next(list_element_iterator, &list_element_next) == UE_NONE) {
								error = uci2_node_type_get(list_element_next, &list_element_node_type);
								assert_int_equal(error, UE_NONE);
								assert_int_equal(list_element_node_type, UNT_LIST_ELEMENT);

								error = uci2_node_list_element_value_get(list_element_next, &list_element_value);
								assert_int_equal(error, UE_NONE);
								assert_string_equal(list_element_value, list_element_value_match[list_element_index]);
								list_element_index++;
							}

							uci2_node_iterator_destroy(&list_element_iterator);

							break;

						default:
							abort();
					}

					break;

				default:
					abort();
			}

			option_index++;
		}
		uci2_node_iterator_destroy(&option_iterator);

		section_index++;
	}

	uci2_node_iterator_destroy(&section_iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_config_firewall(void **state)
{
	uci2_error_e error = UE_NONE;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_iterator_t *section_iterator = NULL;
	size_t index = 0;
	uci2_node_t *node_next = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	const char *section_type = NULL;
	const char *section_name = NULL;
	const char *section_type_match[] = {
		"defaults",
		"zone",
		"zone",
		"forwarding",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"include",
		"redirect",
		"redirect",
		"redirect",
	};
	const char *section_name_match[] = {
		"@defaults[0]",
		"@zone[0]",
		"@zone[1]",
		"@forwarding[0]",
		"@rule[0]",
		"@rule[1]",
		"@rule[2]",
		"@rule[3]",
		"@rule[4]",
		"@rule[5]",
		"@rule[6]",
		"@rule[7]",
		"@rule[8]",
		"@rule[9]",
		"@rule[10]",
		"@rule[11]",
		"@rule[12]",
		"@rule[13]",
		"@include[0]",
		"@redirect[0]",
		"@redirect[1]",
		"@redirect[2]",
	};
	uci2_node_t *section_node = NULL;
	uci2_node_iterator_t *option_iterator = NULL;
	size_t option_index = 0;
	size_t list_element_index = 0;
	uci2_node_t *option_node_next = NULL;
	const char *option_name = NULL;
	const char *option_value = NULL;
	const char *option_name_match[] = {
		"name",
		"src",
		"dest",
		"proto",
		"limit",
		"family",
		"target",
	};
	const char *option_value_match[] = {
		"Allow-ICMPv6-Forward",
		"wan",
		"*",
		"icmp",
		"1000/sec",
		"ipv6",
		"ACCEPT",
	};
	const char *list_name = NULL;
	uci2_node_iterator_t *list_element_iterator = NULL;
	uci2_node_t *list_element_node_next = NULL;
	const char *list_element_value = NULL;
	const char *list_element_value_match[] = {
		"echo-request",
		"echo-reply",
		"destination-unreachable",
		"packet-too-big",
		"time-exceeded",
		"bad-header",
		"unknown-header-type",
	};
	uci2_node_t *option_node = NULL;
	const char *section_type_after_delete_match[] = {
		"defaults",
		"zone",
		"zone",
		"forwarding",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"rule",
		"include",
		"redirect",
		"redirect",
	};
	const char *section_name_after_delete_match[] = {
		"@defaults[0]",
		"@zone[0]",
		"@zone[1]",
		"@forwarding[0]",
		"@rule[0]",
		"@rule[1]",
		"@rule[2]",
		"@rule[3]",
		"@rule[4]",
		"@rule[5]",
		"@rule[6]",
		"@rule[7]",
		"@rule[8]",
		"@rule[9]",
		"@rule[10]",
		"@rule[11]",
		"@rule[12]",
		"@rule[13]",
		"@include[0]",
		"@redirect[0]",
		"@redirect[2]",
	};

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_firewall", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(uci2_ast, NULL);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_iterator_new(root_node, &section_iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(section_iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		assert_int_equal(node_type, UNT_SECTION);

		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_type, NULL);
		assert_string_equal(section_type, section_type_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_name, NULL);
		assert_string_equal(section_name, section_name_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&section_iterator);

	error = uci2_node_get(uci2_ast, "@rule[6]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_node, NULL);

	error = uci2_node_iterator_new(section_node, &option_iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_iterator, NULL);

	option_index = 0;
	while (uci2_node_iterator_next(option_iterator, &option_node_next) == UE_NONE) {
		assert_ptr_not_equal(option_node_next, NULL);
		error = uci2_node_type_get(option_node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		switch (node_type) {
			case UNT_OPTION:
				error = uci2_node_option_name_get(option_node_next, &option_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_name, option_name_match[option_index]);

				error = uci2_node_option_value_get(option_node_next, &option_value);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(option_value, option_value_match[option_index]);

				option_index++;
				break;

			case UNT_LIST:
				error = uci2_node_list_name_get(option_node_next, &list_name);
				assert_int_equal(error, UE_NONE);
				assert_string_equal(list_name, "icmp_type");

				error = uci2_node_iterator_new(option_node_next, &list_element_iterator);
				assert_int_equal(error, UE_NONE);
				assert_ptr_not_equal(list_element_iterator, NULL);

				list_element_index = 0;
				while (uci2_node_iterator_next(list_element_iterator, &list_element_node_next) == UE_NONE) {
					error = uci2_node_list_element_value_get(list_element_node_next, &list_element_value);
					assert_int_equal(error, UE_NONE);
					assert_ptr_not_equal(list_element_value, NULL);
					assert_string_equal(list_element_value, list_element_value_match[list_element_index]);
					list_element_index++;
				}

				uci2_node_iterator_destroy(&list_element_iterator);

				break;

			default:
				abort();
		}

		uci2_node_iterator_destroy(&option_iterator);
	}

	error = uci2_node_get(uci2_ast, "@redirect[1]", "src_dport", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_equal(option_value, "22001");

	error = uci2_node_get(uci2_ast, "@redirect[2]", "src_dport", &option_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_node, NULL);

	error = uci2_node_option_value_get(option_node, &option_value);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(option_value, NULL);
	assert_string_not_equal(option_value, "22001");
	assert_string_equal(option_value, "80");

	uci2_node_remove(option_node);

	error = uci2_node_get(uci2_ast, "@redirect[2]", "src_dport", &option_node);
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_get(uci2_ast, "@redirect[1]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_node, NULL);

	uci2_node_remove(section_node);

	error = uci2_node_section_type_get(section_node, &section_type);
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_section_type_set(section_node, "test_type");
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_section_name_get(section_node, &section_type);
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_section_name_set(section_node, "test_name");
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_get(uci2_ast, "@redirect[1]", NULL, &section_node);
	assert_int_equal(error, UE_NODE_NOT_FOUND);

	error = uci2_node_get(uci2_ast, "@redirect[2]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_node, NULL);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(root_node, NULL);

	error = uci2_node_iterator_new(root_node, &section_iterator);
	assert_int_equal(error, UE_NONE);
	assert_ptr_not_equal(section_iterator, NULL);

	index = 0;
	while (uci2_node_iterator_next(section_iterator, &node_next) == UE_NONE) {
		assert_ptr_not_equal(node_next, NULL);
		error = uci2_node_type_get(node_next, &node_type);
		assert_int_equal(error, UE_NONE);
		assert_int_equal(node_type, UNT_SECTION);

		error = uci2_node_section_type_get(node_next, &section_type);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_type, NULL);
		assert_string_equal(section_type, section_type_after_delete_match[index]);

		error = uci2_node_section_name_get(node_next, &section_name);
		assert_int_equal(error, UE_NONE);
		assert_ptr_not_equal(section_name, NULL);
		assert_string_equal(section_name, section_name_after_delete_match[index]);

		index++;
	}

	uci2_node_iterator_destroy(&section_iterator);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_value_escaping(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;
	char name[16] = {0};
	FILE *file = NULL;
	const char *values[] = {
		"it's",
		"a\"b",
		"back\\slash",
		"tab\there",
		"multi\nline",
		"'''",
		"",
	};

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "test_type", "test_name", &section_node);
	assert_int_equal(error, UE_NONE);

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		snprintf(name, sizeof(name), "option_%zu", i);
		error = uci2_node_option_add(uci2_ast, section_node, name, values[i], &node);
		assert_int_equal(error, UE_NONE);
	}

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_escaping");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_escaping", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		snprintf(name, sizeof(name), "option_%zu", i);
		error = uci2_node_get(uci2_ast, "test_name", name, &node);
		if (values[i][0] == '\0') {
			// like libuci, an empty value means the option is not set, so it is not written
			assert_int_equal(error, UE_NODE_NOT_FOUND);
			continue;
		}

		assert_int_equal(error, UE_NONE);

		error = uci2_node_option_value_get(node, &value);
		assert_int_equal(error, UE_NONE);
		assert_string_equal(value, values[i]);
	}

	uci2_ast_destroy(&uci2_ast);

	// values escaped by libuci
	file = fopen(CONFIG_DIRECTORY_PATH_TMP "test_config_escaping", "w");
	assert_ptr_not_equal(file, NULL);
	fputs("config test_type 'test_name'\n"
		  "\toption single 'it'\\''s'\n"
		  "\toption double \"a\\\"b\"\n"
		  "\toption unquoted a\\ b\n",
		  file);
	fclose(file);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_escaping", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "test_name", "single", &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "it's");

	error = uci2_node_get(uci2_ast, "test_name", "double", &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "a\"b");

	error = uci2_node_get(uci2_ast, "test_name", "unquoted", &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "a b");

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_invalid_name(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *node = NULL;
	FILE *file = NULL;

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "test type", NULL, &section_node);
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	error = uci2_node_section_add(uci2_ast, root_node, "test_type", "test'name", &section_node);
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	error = uci2_node_section_add(uci2_ast, root_node, "test.type", "test_name", &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_name_set(section_node, "@test");
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	error = uci2_node_option_add(uci2_ast, section_node, "", "value", &node);
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	error = uci2_node_list_add(uci2_ast, section_node, "a b", &node);
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	error = uci2_node_option_add(uci2_ast, section_node, "test_option", "value", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_name_set(node, "test-option");
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	uci2_ast_destroy(&uci2_ast);

	// a parsed invalid name can't be written, and the file is left untouched
	file = fopen(CONFIG_DIRECTORY_PATH_TMP "test_config_invalid_name", "w");
	assert_ptr_not_equal(file, NULL);
	fputs("config test_type 'test_name'\n\toption 'test option' 'value'\n", file);
	fclose(file);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_invalid_name", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_invalid_name");
	assert_int_equal(error, UE_INVALID_ARGUMENT);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_invalid_name", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "test_name", "test option", &node);
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_ast_sync_file(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	struct stat stat_buffer = {0};

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	// new files are created with mode 0600, like libuci does
	umask(022);
	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_new");
	assert_int_equal(error, UE_NONE);
	assert_int_equal(stat(CONFIG_DIRECTORY_PATH_TMP "test_config_new", &stat_buffer), 0);
	assert_int_equal(stat_buffer.st_mode & 07777, 0600);

	// existing files keep their mode
	assert_int_equal(chmod(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", 0640), 0);
	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);
	assert_int_equal(stat(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &stat_buffer), 0);
	assert_int_equal(stat_buffer.st_mode & 07777, 0640);

	// a failed save does not leave a file behind
	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "missing/test_config");
	assert_int_equal(error, UE_FILE_IO);
	assert_int_not_equal(stat(CONFIG_DIRECTORY_PATH_TMP "missing/test_config", &stat_buffer), 0);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_ast_compact(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *list_node = NULL;
	uci2_node_t *node = NULL;
	size_t pool_size = 0;

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_correct", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	uci2_ast_compact(uci2_ast);
	pool_size = uci2_ast->pool->children_number;

	for (int i = 0; i < 100; i++) {
		error = uci2_node_section_add(uci2_ast, root_node, "system", "test_name", &section_node);
		assert_int_equal(error, UE_NONE);

		error = uci2_node_list_add(uci2_ast, section_node, "test_list", &list_node);
		assert_int_equal(error, UE_NONE);

		error = uci2_node_list_element_add(uci2_ast, list_node, "test_value", &node);
		assert_int_equal(error, UE_NONE);

		uci2_node_remove(section_node);
		uci2_ast_compact(uci2_ast);
	}

	assert_int_equal(uci2_ast->pool->children_number, pool_size);

	// remaining nodes are still usable
	error = uci2_node_get(uci2_ast, "@system[0]", NULL, &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "ntp", "server", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_correct");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_section_type_set_single(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *lan_node = NULL;
	uci2_node_t *wan_node = NULL;
	uci2_node_t *unnamed_node = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "interface", "lan", &lan_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "interface", "wan", &wan_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "interface", NULL, &unnamed_node);
	assert_int_equal(error, UE_NONE);

	// like libuci, only the type of this section changes
	error = uci2_node_section_type_set(lan_node, "device");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_get(lan_node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "device");

	error = uci2_node_section_type_get(wan_node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "interface");

	// moving into an existing type, and setting the same type again
	error = uci2_node_section_type_set(wan_node, "device");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_type_set(wan_node, "device");
	assert_int_equal(error, UE_NONE);

	// unnamed sections get a name of their new type
	error = uci2_node_section_type_set(unnamed_node, "switch.vlan");
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_name_get(unnamed_node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "@switch.vlan[0]");

	error = uci2_ast_sync(uci2_ast, CONFIG_DIRECTORY_PATH_TMP "test_config_type_set");
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_type_set", &uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "lan", NULL, &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_section_type_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "device");

	error = uci2_node_get(uci2_ast, "wan", NULL, &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_section_type_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "device");

	error = uci2_node_get(uci2_ast, "@switch.vlan[0]", NULL, &node);
	assert_int_equal(error, UE_NONE);

	uci2_ast_destroy(&uci2_ast);
}

static void test_uci2_node_get_after_remove(void **state)
{
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *root_node = NULL;
	uci2_node_t *section_node = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;

	error = uci2_ast_create(&uci2_ast);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, NULL, NULL, &root_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_section_add(uci2_ast, root_node, "interface", "wan", &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_option_add(uci2_ast, section_node, "proto", "dhcp", &node);
	assert_int_equal(error, UE_NONE);
	uci2_node_remove(node);

	error = uci2_node_option_add(uci2_ast, section_node, "proto", "static", &node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "wan", "proto", &node);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_option_value_get(node, &value);
	assert_int_equal(error, UE_NONE);
	assert_string_equal(value, "static");

	uci2_node_remove(section_node);

	error = uci2_node_section_add(uci2_ast, root_node, "device", "wan", &section_node);
	assert_int_equal(error, UE_NONE);

	error = uci2_node_get(uci2_ast, "wan", NULL, &node);
	assert_int_equal(error, UE_NONE);
	assert_ptr_equal(node, section_node);

	uci2_ast_destroy(&uci2_ast);
}

// expected results follow libuci's parser (file.c: parse_str, parse_backslash, uci_parse_line)
static void test_uci2_config_parse_syntax(void **state)
{
	static const char missing[] = "<missing>";
	uci2_error_e error = 0;
	uci2_ast_t *uci2_ast = NULL;
	uci2_node_t *node = NULL;
	const char *value = NULL;
	uci2_node_type_e node_type = UNT_ROOT;
	FILE *file = NULL;
	struct {
		const char *content;
		const char *section;
		const char *option;
		const char *value; // NULL: parsing fails, missing: the option doesn't exist
	} cases[] = {
		// '#' starts a comment unless quoted or escaped
		{"config t 's'\n\toption a foo#bar\n", "s", "a", "foo"},
		{"config t 's'\n\toption a 'x'#comment\n", "s", "a", "x"},
		{"config t 's' # comment\n\toption a 'x#y'\n", "s", "a", "x#y"},
		{"config t 's'\n\toption a x\\#y\n", "s", "a", "x#y"},
		// ';' separates statements unless quoted
		{"config t 's'; option a 'b'\n", "s", "a", "b"},
		{"config t 's'\n\toption a b; option c d\n", "s", "c", "d"},
		{"config t 's'\n\toption a 'x;y'\n", "s", "a", "x;y"},
		{"config t 's'\n\toption a x;y\n", "s", "a", NULL},
		// a backslash at the end of a line continues the value
		{"config t 's'\n\toption a ab\\\ncd\n", "s", "a", "abcd"},
		{"config t 's'\n\toption a \"ab\\\n  cd\"\n", "s", "a", "ab  cd"},
		{"config t 's'\n\toption a ab\\\r\ncd\n", "s", "a", "abcd"},
		// single quoted values may span lines
		{"config t 's'\n\toption a 'ab\ncd'\n", "s", "a", "ab\ncd"},
		// CRLF line endings
		{"config t 's'\r\n\toption a 'b'\r\n", "s", "a", "b"},
		{"config t 's'\r\n\toption a b\r\n", "s", "a", "b"},
		// keywords are only keywords at the start of a statement
		{"config t 's'\n\toption proto list\n", "s", "proto", "list"},
		{"config t 's'\n\toption config option\n", "s", "config", "option"},
		{"config t 's'\n\toption a b option c d\n", "s", "a", NULL},
		{"config t 's'\n\toptionx a b\n", "s", "a", NULL},
		// keywords can be abbreviated to their first letter
		{"c t 's'\n\to a 'b'\n", "s", "a", "b"},
		{"c t 's'\n\tl a 'b'\n", "s", "a", "b"},
		// a backslash at the end of the file is dropped
		{"config t 's'\n\toption a ab\\", "s", "a", "ab"},
		// options and lists are applied in order
		{"config t 's'\n\toption a '1'\n\toption a '2'\n", "s", "a", "2"},
		{"config t 's'\n\toption a '1'\n\tlist a '2'\n", "s", "a", "1"},
		{"config t 's'\n\tlist a '1'\n\toption a '2'\n", "s", "a", "2"},
		// an empty or missing value removes the option, a list gets an empty element
		{"config t 's'\n\toption a '1'\n\toption a ''\n", "s", "a", missing},
		{"config t 's'\n\toption a\n", "s", "a", missing},
		{"config t 's'\n\toption a # comment\n", "s", "a", missing},
		{"config t 's'\n\tlist a\n", "s", "a", ""},
		// sections with the same name are merged, but can't have different types
		{"config t 's'\n\toption a '1'\nconfig t 's'\n\toption b '2'\n", "s", "a", "1"},
		{"config t 's'\n\toption a '1'\nconfig t 's'\n\toption b '2'\n", "s", "b", "2"},
		{"config t 's'\nconfig u 's'\n", "s", "a", NULL},
		// an empty section name means unnamed
		{"config t ''\n\toption a 'b'\n", "@t[0]", "a", "b"},
	};

	// a file with only comments is an empty package
	file = fopen(CONFIG_DIRECTORY_PATH_TMP "test_config_syntax", "w");
	assert_ptr_not_equal(file, NULL);
	fputs("# comment\n  # comment\n", file);
	fclose(file);

	error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_syntax", &uci2_ast);
	assert_int_equal(error, UE_NONE);
	error = uci2_node_get(uci2_ast, NULL, NULL, &node);
	assert_int_equal(error, UE_NONE);
	uci2_ast_destroy(&uci2_ast);

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		file = fopen(CONFIG_DIRECTORY_PATH_TMP "test_config_syntax", "w");
		assert_ptr_not_equal(file, NULL);
		fputs(cases[i].content, file);
		fclose(file);

		error = uci2_config_parse(CONFIG_DIRECTORY_PATH_TMP "test_config_syntax", &uci2_ast);
		if (cases[i].value == NULL) {
			assert_int_equal(error, UE_PARSER);
			continue;
		}

		assert_int_equal(error, UE_NONE);

		error = uci2_node_get(uci2_ast, cases[i].section, cases[i].option, &node);
		if (cases[i].value == missing) {
			assert_int_equal(error, UE_NODE_NOT_FOUND);
			uci2_ast_destroy(&uci2_ast);
			continue;
		}

		assert_int_equal(error, UE_NONE);

		error = uci2_node_type_get(node, &node_type);
		assert_int_equal(error, UE_NONE);

		if (node_type == UNT_OPTION) {
			error = uci2_node_option_value_get(node, &value);
		} else {
			// list: check the first element
			uci2_node_iterator_t *iterator = NULL;

			error = uci2_node_iterator_new(node, &iterator);
			assert_int_equal(error, UE_NONE);
			error = uci2_node_iterator_next(iterator, &node);
			assert_int_equal(error, UE_NONE);
			uci2_node_iterator_destroy(&iterator);

			error = uci2_node_list_element_value_get(node, &value);
		}

		assert_int_equal(error, UE_NONE);
		assert_string_equal(value, cases[i].value);

		uci2_ast_destroy(&uci2_ast);
	}
}
