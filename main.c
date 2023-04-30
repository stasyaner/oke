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
#define CHILDREN_CHUNK_START "children:"
#define CHILDREN_CHUNK_START_LENGTH 9
#define CHILDREN_ARRAY_CHUNK_START "children:["
#define CHILDREN_ARRAY_CHUNK_START_LENGTH 10
#define CHILDREN_CHUNK_END ","
#define CHILDREN_CHUNK_END_LENGTH 1
#define CHILDREN_ARRAY_CHUNK_END "],"
#define CHILDREN_ARRAY_CHUNK_END_LENGTH 2
#define JSX_END_CHUNK "})"
#define JSX_END_CHUNK_LENGTH 2
#define FRAGMENT_CHUNK "_Fragment,{"
#define FRAGMENT_CHUNK_LENGTH 11
#define TRUE_CHUNK "true"
#define TRUE_CHUNK_LENGTH 4

#define transpile_nodes(nodes) transpile_nodes_base(nodes, 0)
#define transpile_nodes_csv(nodes) transpile_nodes_base(nodes, 1)
static char *transpile_nodes_base(
	const Node **nodes,
	char should_comma_separate
);

static long array_length(const void **array_pointer);

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

static char is_intrinsic(char *element_name) {
	/* JSX elements starting with a lowercase letter are considered
	   intrinsic. */
	return element_name[0] >= 'a' && element_name[0] <= 'z';
}

static char *transpile_node(const Node *node) {
	char *output = NULL;
	long output_length;

	if(!node) {
		return NULL;
	}

	if(node->type == file_node) {
		output = transpile_node(node->child);
	} else if(node->type == statement_list_node) {
		output = transpile_nodes((const Node **)node->children);
	} else if(node->type == jsx_element_node) {
		char *opening_element_transpiled;
		char *children_transpiled = NULL;
		char is_children_array = 1;
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

		if(node->children) {
			long children_count = array_length((const void **)node->children);
			if(children_count == 1) {
				is_children_array = 0;
				children_transpiled = transpile_node(*node->children);
			} else {
				children_transpiled = transpile_nodes_csv(
					(const Node **)node->children
				);
			}
		}

		if(children_transpiled) {
			chunks[3] = children_transpiled;
			if(is_children_array) {
				chunks[0] = JSXS_START_CHUNK;
				chunks[2] = CHILDREN_ARRAY_CHUNK_START;
				chunks[4] = CHILDREN_ARRAY_CHUNK_END JSX_END_CHUNK;
			} else {
				chunks[2] = CHILDREN_CHUNK_START;
				chunks[4] = CHILDREN_CHUNK_END JSX_END_CHUNK;
			}
			chunks[5] = NULL;
		} else {
			chunks[2] = JSX_END_CHUNK;
			chunks[3] = NULL;
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
			char *element_name = transpile_node(node->child);
			long element_name_length = node->child->end - node->child->start;
			char *atrributes_transpiled;
			const char *chunks[4];

			if(is_intrinsic(element_name)) {
				/* " + element_name + " + \0 */
				char *element_name_enclosed = malloc(element_name_length + 3);
				element_name_enclosed[0] = '"';
				memcpy(
					element_name_enclosed + 1,
					element_name,
					element_name_length
				);
				element_name_enclosed[element_name_length + 1] = '"';
				element_name_enclosed[element_name_length + 2] = '\0';
				free(element_name);
				element_name = element_name_enclosed;
			}

			chunks[0] = element_name;
			chunks[1] = JSX_ARGUMENT_START_CHUNK;

			if(node->children) {
				atrributes_transpiled =
					transpile_nodes_csv((const Node **)node->children);
				chunks[2] = atrributes_transpiled;
				chunks[3] = NULL;
			} else {
				chunks[2] = NULL;
			}

			output = compose_string_from_chunks(chunks);
		} else {
			/* fragment_function + \0 */
			output = malloc(FRAGMENT_CHUNK_LENGTH + 1);
			strcpy(output, FRAGMENT_CHUNK);
		}
	} else if(
		node->type == jsx_text_node ||
		node->type == string_literal_node
	) {
		/* we need strlen here as node length (node->end - node->start)
		   doesn't represent the real length of the string in node->value */
		long node_length = strlen(node->value);
		/* node->raw could be of use here, not to mess around with quotes */
		/* " + text + " + \0 */
		output_length = 3 + node_length;
		output = malloc(output_length);
		output[0] = '"';
		memcpy(output + 1, node->value, node_length);
		output[output_length - 2] = '"';
		output[output_length - 1] = '\0';
	} else if(node->type == jsx_attribute_node) {
		char *attribute_name_transpiled = transpile_node(node->left);
		long attribute_name_transpiled_length =
			strlen(attribute_name_transpiled);
		char *attribute_value_transpiled;
		long attribute_value_transpiled_length;

		if(node->right) {
			attribute_value_transpiled = transpile_node(node->right);
			attribute_value_transpiled_length =
				strlen(attribute_value_transpiled);
		} else {
			attribute_value_transpiled = TRUE_CHUNK;
			attribute_value_transpiled_length = TRUE_CHUNK_LENGTH;
		}

		/* identifier + ':' + value + '\0' */
		output_length =
			attribute_name_transpiled_length +
			attribute_value_transpiled_length + 2;
		output = malloc(output_length);
		memcpy(
			output,
			attribute_name_transpiled,
			attribute_name_transpiled_length)
		;
		output[attribute_name_transpiled_length] = ':';
		memcpy(
			output + attribute_name_transpiled_length + 1,
			attribute_value_transpiled,
			attribute_value_transpiled_length
		);
		output[output_length - 1] = '\0';
	} else if(
		node->type == identifier_node ||
		node->type == jsx_expression_text_node
	) {
		long node_length = node->end - node->start;
		output = malloc(node_length);
		memcpy(output, node->value, node_length);
		output[node_length] = '\0';
	} else if(node->type == jsx_expression_node) {
		output = transpile_nodes((const Node **)node->children);
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

/* Count length of a NULL terminated array of pointers */
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

static char *do_nodes(char *input, Node **nodes, char **compiled_ast) {
	Node **node_pointer = nodes;
	char **node_compiled_pointer = compiled_ast;
	long node_compiled_length;
	long input_length = strlen(input);
	char *output = NULL;
	char *output_realloc = NULL;
	long cursor = 0;
	long prev_node_end = 0;
	long size_to_alloc = 0;
	long space_between_nodes_length = 0;
	Node *node;
	char is_first_node = 1;

	for(;;) {
		node = *node_pointer;

		if(node) {
			space_between_nodes_length = node->start - prev_node_end;
			node_compiled_length = strlen(*node_compiled_pointer);
			size_to_alloc = node_compiled_length;
		} else {
			if(is_first_node) {
				break;
			}
			space_between_nodes_length = input_length - prev_node_end;
			/* \0 */
			size_to_alloc = 1;
		}

		size_to_alloc += cursor + space_between_nodes_length;
		if(output) {
			output_realloc = realloc(output, size_to_alloc);

			if(output_realloc) {
				output = output_realloc;
			} else {
				fprintf(stderr, "Failed to realloc.\n");
				exit(1);
			}
		} else {
			output = malloc(size_to_alloc);
		}

		memcpy(
			output + cursor,
			input + prev_node_end,
			space_between_nodes_length
		);
		cursor += space_between_nodes_length;

		if(node) {
			memcpy(
				output + cursor,
				*node_compiled_pointer,
				node_compiled_length
			);

			cursor += node_compiled_length;
			prev_node_end = node->end;

			free(node);
			free(*node_compiled_pointer);

			node_pointer++;
			node_compiled_pointer++;
			is_first_node = 0;
		} else {
			output[cursor] = '\0';
			break;
		}
	}

	free(nodes);
	free(compiled_ast);

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
	printf("%s", output);
	/* print_ast(ast); */

	return 0;
}
