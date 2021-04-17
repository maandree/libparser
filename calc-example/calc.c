/* See LICENSE file for copyright and license details. */
#include <libparser.h>
#include <libsimple.h>
#include <libsimple-arg.h>

USAGE("");


static void
free_input(struct libparser_unit *node)
{
	struct libparser_unit *next;
	for (; node; next = node->next, free(node), node = next)
		free_input(node->in);
}


static intmax_t
calculate(struct libparser_unit *node, const char *line)
{
	struct libparser_unit *next;
	intmax_t value = 0;
	int op;
	if (!node->rule) {
		next = node->in->next;
		value = calculate(node->in, line);
		free_input(next);
	} else if (!strcmp(node->rule, "DIGIT")) {
		value = (intmax_t)(line[node->start] - '0');
	} else if (!strcmp(node->rule, "sign")) {
		value = !strcmp(node->in->rule, "SUB") ? -1 : +1;
		free(node->in);
	} else if (!strcmp(node->rule, "unsigned")) {
		value = 0;
		next = node->in;
		free(node);
		for (node = next; node; node = next) {
			next = node->next;
			value *= 10;
			value += calculate(node, line);
		}
	} else if (!strcmp(node->rule, "number")) {
		next = node->in->next;
		value = calculate(node->in, line);
		free(node);
		for (node = next; node; node = next) {
			next = node->next;
			value *= calculate(node, line);
		}
	} else if (!strcmp(node->rule, "value")) {
		next = node->in->next;
		value = calculate(node->in, line);
		if (next)
			value *= calculate(next, line);
	} else if (!strcmp(node->rule, "hyper1")) {
		next = node->in->next;
		value = calculate(node->in, line);
		free(node);
		node = next;
		while (node) {
			next = node->next;
			op = !strcmp(node->rule, "SUB") ? -1 : +1;
			free(node);
			node = next;
			next = node->next;
			if (op < 0)
				value -= calculate(node, line);
			else
				value += calculate(node, line);
			node = next;
		}
	} else if (!strcmp(node->rule, "hyper2")) {
		next = node->in->next;
		value = calculate(node->in, line);
		free(node);
		node = next;
		while (node) {
			next = node->next;
			op = !strcmp(node->rule, "DIV") ? -1 : +1;
			free(node);
			node = next;
			next = node->next;
			if (op < 0)
				value /= calculate(node, line);
			else
				value *= calculate(node, line);
			node = next;
		}
	} else if (node->rule[0] != '@') {
		abort();
	} else if (node->in) {
		next = node->in->next;
		value = calculate(node->in, line);
		if (next)
			free_input(next);
	}
	free(node);
	return value;
}


int
main(int argc, char *argv[])
{
	struct libparser_unit *input;
	char *line = NULL;
	size_t size = 0;
	ssize_t len;
	intmax_t res;
	int exception;

	ARGBEGIN {
	default:
		usage();
	} ARGEND;
	if (argc)
		usage();

	while ((len = getline(&line, &size, stdin)) >= 0) {
		if (len && line[len - 1] == '\n')
			line[--len] = '\0';
		input = libparser_parse_file(libparser_rule_table, line, (size_t)len, &exception);
		if (!input) {
			weprintf("didn't find anything to parse\n");
			free_input(input);
			continue;
		} else if (input->end != (size_t)len) {
			weprintf("line could not be parsed, stopped at column %zu\n", input->end);
			free_input(input);
			continue;
		} else if (exception) {
			weprintf("premature end of line\n");
			free_input(input);
			continue;
		}
		res = calculate(input, line);
		printf("%ji\n", res);
	}

	free(line);
	return 0;
}
