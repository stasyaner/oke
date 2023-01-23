#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../mes/parser.h"

#define BUF_SIZE 20000
#define JSX_START_CHUNK "_jsx("
#define JSX_START_CHUNK_LENGTH 5
#define JSXS_START_CHUNK "_jsxs("
#define JSXS_START_CHUNK_LENGTH 6
#define JSX_ARGUMENT_START_CHUNK ",{"
#define JSX_ARGUMENT_START_CHUNK_LENGTH 2
#define CHILDREN_CHUNK_START "children:["
#define CHILDREN_CHUNK_START_LENGTH 10
#define CHILDREN_CHUNK_END "],"
#define CHILDREN_CHUNK_END_LENGTH 2
#define JSX_END_CHUNK "})"
#define JSX_END_CHUNK_LENGTH 2
#define FRAGMENT_FUNC "_Fragment"

#define transpile_nodes(nodes) transpile_nodes_base(nodes, 0)
#define transpile_nodes_csv(nodes) transpile_nodes_base(nodes, 1)
static char *transpile_nodes_base(
	const Node **nodes,
	char should_comma_separate
);

static char *get_input(void) {
	char *buf = NULL;
	long i = 0;
	char c = EOF;

	while((c = getchar()) != EOF) {
		if(!buf) {
			buf = malloc(BUF_SIZE);
		}

		if(i == (BUF_SIZE - 2)) {
			fprintf(stderr, "Buffer is too small.\n");
			exit(1);
		}

		buf[i] = c;
		i++;
	}

	buf[i] = '\0';

	return buf;
}

static char *compose_string_from_chunks(const char **chunks) {
	char *output = NULL;
	long cursor = 0;
	const char **chunk_pointer = chunks;
	const char *chunk;
	long chunk_length = 0;
	char *output_realloc;
	long output_size_to_alloc;

	while((chunk = *chunk_pointer)) {
		chunk_length = strlen(chunk);

		if(output) {
			/* cursor + chunk + \0 */
			output_size_to_alloc = cursor + chunk_length + 1;
			output_realloc = realloc(output, output_size_to_alloc);

			if(output_realloc) {
				output = output_realloc;
			} else {
				fprintf(stderr, "Failed to realloc.\n");
				exit(1);
			}
		} else {
			output = malloc(chunk_length);
		}

		memcpy(output + cursor, chunk, chunk_length);
		cursor += chunk_length;
		chunk_pointer++;
	}

	output[cursor] = '\0';

	if(!chunks) {
		return NULL;
	}

	return output;
}

static char *transpile_node(const Node *node) {
	char *output = NULL;
	long output_length;

	if(!node) {
		return NULL;
	}

	if(node->type == file_node) {
		return transpile_node(node->child);
	} else if(node->type == statement_list_node) {
		return transpile_nodes((const Node **)node->children);
	} else if(node->type == jsx_element_node) {
		char *opening_element_transpiled;
		char *children_transpiled;
		char *chunks[7];

		opening_element_transpiled = transpile_node(node->opening_element);

		if(!node->opening_element->is_self_closing) {
			if(node->opening_element->child) {
				/* if(!node->closing_element->child ||
					node->closing_element->child->type != identifier_node
				) {
					fprintf(stderr, "Identifier expected.");
					exit(1);
				} */
				if(!node->closing_element->child) {
					fprintf(stderr, "Missing closing element.");
					exit(1);
				}
				if(strcmp(
					node->opening_element->child->value,
					node->closing_element->child->value
				)) {
					fprintf(stderr, "Closing element doesn't match.");
					exit(1);
				}
			}
		}

		chunks[0] = JSX_START_CHUNK;
		chunks[1] = opening_element_transpiled;
		chunks[2] = JSX_ARGUMENT_START_CHUNK;

		if(node->children) {
			children_transpiled = transpile_nodes_csv(
				(const Node **)node->children
			);
		}

		if(children_transpiled) {
			chunks[0] = JSXS_START_CHUNK;
			chunks[3] = CHILDREN_CHUNK_START;
			chunks[4] = children_transpiled;
			chunks[5] = CHILDREN_CHUNK_END JSX_END_CHUNK;
			chunks[6] = NULL;
		} else {
			chunks[3] = JSX_END_CHUNK;
			chunks[4] = NULL;
		}

		output = compose_string_from_chunks((const char **)chunks);
		free(opening_element_transpiled);
		/* TODO: remove strlen when we stop returning empty string
			as a fallback */
		if(children_transpiled && strlen(children_transpiled)) {
			free(children_transpiled);
		}
	} else if(node->type == jsx_opening_element_node) {
		if(node->child) {
			/* if(node->child->type != identifier_node) {
				fprintf(stderr, "Identifier expected.");
				exit(1);
			} */

			/* JSX elements starting with a lowercase letter are considered
			   intrinsic. */
			if(node->child->value[0] >= 'a' && node->child->value[0] <= 'z') {
				/* " + element_name + " + \0 */
				output_length = 3 + strlen(node->child->value);
				output = malloc(output_length);
				output[0] = '"';
				strcpy(output + 1, node->child->value);
				output[output_length - 2] = '"';
				output[output_length - 1] = '\0';
			} else {
				output = node->child->value;
			}
		} else {
			output = FRAGMENT_FUNC;
		}
	} else if(node->type == jsx_expression_node) {
		return "";
		/* transpile_nodes((const Node **)node->children); */
	} else if(node->type == jsx_text_node) {
		return "";
		/* transpile_nodes((const Node **)node->children); */
	} else {
		fprintf(stderr, "Unexpected node type.\n");
		exit(1);
	}

	return output;
}

static char *transpile_nodes_base(
	const Node **nodes,
	char should_comma_separate
) {
	const Node **p = nodes;
	const Node *node;
	char *node_transpiled;
	long node_transpiled_length;
	char *output = NULL;
	long cursor = 0;
	char *output_realloc;
	long output_to_malloc_size = 0;

	if(!p) {
		return NULL;
	}

	while((node = *p)) {
		node_transpiled = transpile_node(node);

		if(!node_transpiled) {
			fprintf(stderr, "Couldn't transpile node.\n");
		}

		node_transpiled_length = strlen(node_transpiled);
		/* TODO: remove when we stop returning empty string */
		if(!node_transpiled_length) {
			p++;
			continue;
		}

		output_to_malloc_size = node_transpiled_length;
		if(should_comma_separate) {
			output_to_malloc_size++;
		}

		if(output) {
			/* node + cursor + \0 */
			output_to_malloc_size += cursor + 1;
			output_realloc = realloc(output, output_to_malloc_size);

			if(output_realloc) {
				output = output_realloc;
			} else {
				fprintf(stderr, "Failed to realloc.\n");
			}
		} else {
			output = malloc(output_to_malloc_size);
		}

		memcpy(output + cursor, node_transpiled, node_transpiled_length);
		cursor += node_transpiled_length;
		if(should_comma_separate) {
			output[cursor] = ',';
			cursor++;
		}
		p++;
	}

	if (output) {
		output[cursor] = '\0';
	}

	return output;
}

/* Count length of a NULL terinated array of pointers */
static long array_length(const void **array_pointer) {
	long i = 0;
	const void **p = array_pointer;

	while(*p) {
		i++;
		p++;
	}

	return i;
}

static char **transpile_nodes2(const Node **nodes) {
	const Node **p = nodes;
	char **output = NULL;
	long length = array_length((const void **)nodes);
	const Node *node;
	char *node_transpiled;
	char **pp;

	if(!p) {
		return NULL;
	}

	output = malloc(length);
	pp = output;

	while((node = *p)) {
		node_transpiled = transpile_node(node);
		*pp = node_transpiled;
		p++;
		pp++;
	}

	return output;
}

static char *do_nodes(
	char *input,
	Node **nodes,
	char **compiled_ast
) {
	Node **node_pointer = nodes;
	char **node_compiled_pointer = compiled_ast;
	long shift = 0;
	long input_length = strlen(input);
	char *output = NULL;
	long output_length = 0;
	long prev_node_end = 0;
	long allocated_size = 0;
	Node *node;

	while((node = *node_pointer)) {
		long node_length = node->end - node->start;
		long node_compiled_length = strlen(*node_compiled_pointer);
		long node_start_shifted = node->start - shift;

		if(output) {
			allocated_size = input_length - node_length + node_compiled_length;
			if(output_length > allocated_size) {
				output = realloc(output, allocated_size);
			}
		} else {
			allocated_size = input_length;
			output = malloc(allocated_size);
		}

		strcpy(output + output_length, input + prev_node_end);
		strcpy(output + node_start_shifted, *node_compiled_pointer);
		/* strcpy(output + node_compiled_length, input + node->end); */

		prev_node_end = node->end;
		shift = node_length - node_compiled_length;
		output_length = strlen(output);

		free(node);
		free(*node_compiled_pointer);

		node_pointer++;
		node_compiled_pointer++;
	}

	strcpy(output + output_length, input + prev_node_end);

	return output;
}

int main() {
	char *input = NULL;
	char *output = NULL;
	char **output2 = NULL;
	Node *ast = NULL;

	input = get_input();
	ast = parse(input);

	/* output = transpile_node(ast); */
	output2 = transpile_nodes2((const Node**)ast->child->children);
	/* output = malloc(strlen(input));
	strcpy(output, input); */
	output = do_nodes(
		input,
		ast->child->children,
		output2
	);
	/* printf("%ld\n", array_length((const void **)ast->child->children)); */
	printf("%s\n", output);
	/* print_ast(ast); */

	return 0;
}
