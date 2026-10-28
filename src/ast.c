/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2024, Sartura d.d.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "utils/memory.h"

#include "ast.h"

void ast_init(ast_t *ast)
{
	assert(ast);

	ast->root = NULL;
	ast->pool = xcalloc(1, sizeof(ast_node_t));
}

ast_node_t *ast_node_new(ast_t *ast, enum ast_node_type type, char *name, char *value)
{
	ast_node_t *node = NULL;

	assert(ast);

	node = xcalloc(1, sizeof(ast_node_t));
	node->type = type;
	node->name = name;
	node->value = value;

	ast_node_add(ast->pool, node);

	return node;
}

void ast_node_add(ast_node_t *parent, ast_node_t *node)
{
	assert(parent);
	assert(node);

	node->parent = parent;
	parent->children_number++;
	parent->children = xrealloc(parent->children, parent->children_number * sizeof(ast_node_t *));
	parent->children[parent->children_number - 1] = node;
}

void ast_destroy(ast_t *ast)
{
	if (ast) {
		if (ast->pool) {
			for (size_t i = 0; i < ast->pool->children_number; i++) {
				XFREE(ast->pool->children[i]->name);
				XFREE(ast->pool->children[i]->value);
				XFREE(ast->pool->children[i]->children);
				XFREE(ast->pool->children[i]);
			}

			XFREE(ast->pool->children);
			XFREE(ast->pool);
		}

		XFREE(ast);
	}
}

// remove `node` from the children of its parent
void ast_node_detach(ast_node_t *node)
{
	ast_node_t *parent = NULL;
	size_t children_number = 0;

	assert(node);
	assert(node->parent);

	parent = node->parent;
	for (size_t i = 0; i < parent->children_number; i++) {
		if (parent->children[i] != node) {
			parent->children[children_number++] = parent->children[i];
		}
	}

	parent->children_number = children_number;
	node->parent = NULL;
}

// get the pool node of the AST a (not removed) node belongs to
ast_node_t *ast_node_pool_get(ast_node_t *node)
{
	assert(node);

	while (node->parent) {
		node = node->parent;
	}

	return node;
}

// mark all nodes reachable from `node` and drop removed children from the children arrays
static void ast_node_mark(ast_node_t *node)
{
	size_t children_number = 0;

	node->reachable = true;
	for (size_t i = 0; i < node->children_number; i++) {
		if (node->children[i] && node->children[i]->parent == node) {
			ast_node_mark(node->children[i]);
			node->children[children_number++] = node->children[i];
		}
	}

	node->children_number = children_number;
}

void ast_compact(ast_t *ast)
{
	ast_node_t *node = NULL;
	size_t pool_number = 0;

	if (ast == NULL || ast->pool == NULL || ast->root == NULL) {
		return;
	}

	for (size_t i = 0; i < ast->pool->children_number; i++) {
		ast->pool->children[i]->reachable = false;
	}

	ast_node_mark(ast->root);

	// free every node that is no longer part of the tree
	for (size_t i = 0; i < ast->pool->children_number; i++) {
		node = ast->pool->children[i];
		if (node->reachable) {
			ast->pool->children[pool_number++] = node;
		} else {
			XFREE(node->name);
			XFREE(node->value);
			XFREE(node->children);
			XFREE(node);
		}
	}

	ast->pool->children_number = pool_number;
}

void ast_node_move(ast_node_t *destination, ast_node_t *source)
{
	assert(destination);
	assert(destination->parent);
	assert(source);
	assert(source->parent);

	destination->children = source->children;
	destination->children_number = source->children_number;
	destination->unnamed_children_number = source->unnamed_children_number;
	source->children = NULL;
	source->children_number = 0;
	source->unnamed_children_number = 0;
	for (size_t i = 0; i < destination->children_number; i++) {
		destination->children[i]->parent = destination;
	}
}

void ast_node_merge(ast_node_t *node, enum ast_node_type type)
{
	assert(node);
	assert(node->parent);

	for (size_t i = 0; i < node->children_number; i++) {
		if (node->children[i] == NULL &&
			node->children[i]->parent == NULL &&
			node->children[i]->type != type &&
			node->children[i]->name == NULL) {
			continue;
		}

		for (size_t j = i + 1; j < node->children_number; j++) {
			if (node->children[j]->name &&
				node->children[i]->name &&
				strcmp(node->children[j]->name, node->children[i]->name) == 0) {
				for (size_t k = 0; k < node->children[j]->children_number; k++) {
					ast_node_add(node->children[i], node->children[j]->children[k]);
				}

				node->children[j]->children_number = 0;
				node->children[j]->unnamed_children_number = 0;
				node->children[j]->parent = NULL;
			}
		}
	}
}

void unnamed_section_name_set(ast_node_t *config_node)
{
	ast_node_t *section_type_node = NULL;
	ast_node_t *section_name_node = NULL;
	char unnamed_section_name[UNNAMED_SECTION_NAME_BUFFER_SIZE_MAX + 1] = {0};

	assert(config_node);
	assert(config_node->parent);

	for (size_t i = 0; i < config_node->children_number; i++) {
		section_type_node = config_node->children[i];
		if (section_type_node &&
			section_type_node->parent &&
			section_type_node->name) {
			for (size_t j = 0; j < config_node->children[i]->children_number; j++) {
				section_name_node = section_type_node->children[j];
				if (section_name_node &&
					section_name_node->parent &&
					section_name_node->type == ANT_SECTION_NAME &&
					section_name_node->name &&
					strcmp(section_name_node->name, UNNAMED_SECTION_NAME_PLACEHOLDER) == 0) {
					XFREE(section_name_node->name);
					snprintf(unnamed_section_name, sizeof(unnamed_section_name), "@%s[%zu]", section_type_node->name, section_type_node->unnamed_children_number);
					section_name_node->name = xstrdup(unnamed_section_name);
					section_type_node->unnamed_children_number++;
				}
			}
		}
	}
}

// create a section type node with one section, `name` NULL or empty creates an unnamed section
ast_node_t *ast_section_new(ast_t *ast, char *type, char *name, ast_node_t *options)
{
	ast_node_t *type_node = NULL;
	ast_node_t *section_node = NULL;

	assert(ast);
	assert(type);

	// like libuci, an empty name means an unnamed section
	if (name == NULL || name[0] == '\0') {
		XFREE(name);
		name = xstrdup(UNNAMED_SECTION_NAME_PLACEHOLDER);
	}

	type_node = ast_node_new(ast, ANT_SECTION_TYPE, type, NULL);
	section_node = ast_node_new(ast, ANT_SECTION_NAME, name, NULL);
	ast_node_add(type_node, section_node);

	if (options) {
		ast_node_move(section_node, options);
	}

	return type_node;
}

static ast_node_t *ast_section_child_find(ast_node_t *section_node, size_t children_number, const char *name)
{
	for (size_t i = 0; i < children_number; i++) {
		if (strcmp(section_node->children[i]->name, name) == 0) {
			return section_node->children[i];
		}
	}

	return NULL;
}

// apply the options and lists of a section in order, the same way libuci does:
// - "option" sets the value, an empty value removes the option or list
// - "list" appends to a list, an existing option becomes the first element of the list
static void ast_section_children_merge(ast_t *ast, ast_node_t *section_node)
{
	ast_node_t *node = NULL;
	ast_node_t *existing = NULL;
	ast_node_t *item = NULL;
	size_t children_number = 0;

	for (size_t i = 0; i < section_node->children_number; i++) {
		node = section_node->children[i];
		if (node->parent != section_node || node->name == NULL) {
			continue;
		}

		existing = ast_section_child_find(section_node, children_number, node->name);

		if (node->type == ANT_OPTION && (node->value == NULL || node->value[0] == '\0')) {
			if (existing) {
				existing->parent = NULL;
				for (size_t j = 0; j < children_number; j++) {
					if (section_node->children[j] == existing) {
						memmove(&section_node->children[j], &section_node->children[j + 1], (children_number - j - 1) * sizeof(ast_node_t *));
						children_number--;
						break;
					}
				}
			}

			node->parent = NULL;
			continue;
		}

		if (existing && node->type == ANT_OPTION) {
			// the option replaces whatever existed, at the position of the existing node
			for (size_t j = 0; j < existing->children_number; j++) {
				existing->children[j]->parent = NULL;
			}

			existing->children_number = 0;
			existing->type = ANT_OPTION;
			XFREE(existing->value);
			existing->value = node->value;
			node->value = NULL;
			node->parent = NULL;
			continue;
		}

		if (existing && node->type == ANT_LIST) {
			if (existing->type == ANT_OPTION) {
				item = ast_node_new(ast, ANT_LIST_ITEM, existing->value, NULL);
				existing->value = NULL;
				existing->type = ANT_LIST;
				ast_node_add(existing, item);
			}

			for (size_t j = 0; j < node->children_number; j++) {
				ast_node_add(existing, node->children[j]);
			}

			node->children_number = 0;
			node->parent = NULL;
			continue;
		}

		section_node->children[children_number++] = node;
	}

	section_node->children_number = children_number;
}

// merge sections with the same name, returns false if they have different types
static bool ast_sections_merge(ast_node_t *config_node)
{
	ast_node_t *type_node = NULL;
	ast_node_t *section_node = NULL;
	ast_node_t *other_type_node = NULL;
	ast_node_t *other_section_node = NULL;

	for (size_t i = 0; i < config_node->children_number; i++) {
		type_node = config_node->children[i];
		if (type_node->parent != config_node) {
			continue;
		}

		for (size_t j = 0; j < type_node->children_number; j++) {
			section_node = type_node->children[j];
			if (section_node->parent != type_node || strcmp(section_node->name, UNNAMED_SECTION_NAME_PLACEHOLDER) == 0) {
				continue;
			}

			for (size_t k = i; k < config_node->children_number; k++) {
				other_type_node = config_node->children[k];
				if (other_type_node->parent != config_node) {
					continue;
				}

				for (size_t l = (k == i) ? j + 1 : 0; l < other_type_node->children_number; l++) {
					other_section_node = other_type_node->children[l];
					if (other_section_node->parent != other_type_node || strcmp(other_section_node->name, section_node->name) != 0) {
						continue;
					}

					// like libuci, a section can't be redefined with a different type
					if (other_type_node != type_node) {
						return false;
					}

					for (size_t m = 0; m < other_section_node->children_number; m++) {
						if (other_section_node->children[m]->parent == other_section_node) {
							ast_node_add(section_node, other_section_node->children[m]);
						}
					}

					other_section_node->children_number = 0;
					other_section_node->parent = NULL;
				}
			}
		}
	}

	return true;
}

// create the root node from the parsed package and lines, returns NULL if the content is invalid
ast_node_t *ast_root_new(ast_t *ast, ast_node_t *package, ast_node_t *lines)
{
	ast_node_t *root_node = NULL;
	ast_node_t *config_node = NULL;
	ast_node_t *type_node = NULL;

	assert(ast);

	root_node = ast_node_new(ast, ANT_ROOT, xstrdup(AST_NODE_ROOT_NAME), NULL);
	ast->root = root_node;

	if (package) {
		ast_node_add(root_node, package);
	}

	config_node = ast_node_new(ast, ANT_CONFIG, xstrdup(AST_NODE_CONFIG_NAME), NULL);
	ast_node_add(root_node, config_node);

	if (lines == NULL) {
		return root_node;
	}

	// use children from lines
	ast_node_move(config_node, lines);
	// merge section type nodes with the same name into a single node
	ast_node_merge(config_node, ANT_SECTION_TYPE);

	if (ast_sections_merge(config_node) == false) {
		return NULL;
	}

	for (size_t i = 0; i < config_node->children_number; i++) {
		type_node = config_node->children[i];
		if (type_node->parent != config_node) {
			continue;
		}

		for (size_t j = 0; j < type_node->children_number; j++) {
			if (type_node->children[j]->parent == type_node) {
				ast_section_children_merge(ast, type_node->children[j]);
			}
		}
	}

	// set correct names for unnamed section nodes
	unnamed_section_name_set(config_node);

	return root_node;
}

