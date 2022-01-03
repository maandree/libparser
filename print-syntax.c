/* See LICENSE file for copyright and license details. */

/* This file exist to help debugging */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libparser.h"


static int
print_sentence(const union libparser_sentence *sentence, int indent)
{
	int len;

	switch (sentence->type) {
	case LIBPARSER_SENTENCE_TYPE_CONCATENATION:
		printf("(");
		indent = print_sentence(sentence->binary.left, indent + 1);
		printf(", ");
		indent = print_sentence(sentence->binary.right, indent + 2);
		printf(")");
		indent + 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_ALTERNATION:
		printf("(");
		print_sentence(sentence->binary.left, indent + 1);
		printf(" | \n%*.s", indent + 1, "");
		indent = print_sentence(sentence->binary.right, indent + 1);
		printf(")");
		indent + 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_REJECTION:
		printf("!(");
		indent = print_sentence(sentence->unary.sentence, indent + 2);
		printf(")");
		indent + 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_OPTIONAL:
		printf("[");
		indent = print_sentence(sentence->unary.sentence, indent + 1);
		printf("]");
		indent + 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_REPEATED:
		printf("{");
		indent = print_sentence(sentence->unary.sentence, indent + 1);
		printf("}");
		indent += 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_STRING:
		printf("\"%.*s\"%n", (int)sentence->string.length, sentence->string.string, &len);
		indent += len;
		break;

	case LIBPARSER_SENTENCE_TYPE_CHAR_RANGE:
		if (isprint(sentence->char_range.low) && isprint(sentence->char_range.high))
			printf("<\"%c\", \"%c\">%n", sentence->char_range.low, sentence->char_range.high, &len);
		else if (isprint(sentence->char_range.low))
			printf("<\"%c\", 0x%02x>%n", sentence->char_range.low, sentence->char_range.high, &len);
		else if (isprint(sentence->char_range.high))
			printf("<0x%02x, \"%c\">%n", sentence->char_range.low, sentence->char_range.high, &len);
		else
			printf("<0x%02x, 0x%02x>%n", sentence->char_range.low, sentence->char_range.high, &len);
		indent += len;
		break;

	case LIBPARSER_SENTENCE_TYPE_RULE:
		printf("%s%n", sentence->rule.rule, &len);
		indent += len;
		break;

	case LIBPARSER_SENTENCE_TYPE_EXCEPTION:
		printf("-");
		indent += 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_EOF:
		printf("%s%n", "!<0x00, 0xFF>", &len);
		indent += len;
		break;

	default:
		abort();
	}

	return indent;
}


int
main(int argc, char *argv[0])
{
	size_t i;
	int indent, first = 1;

	if (argc != 1) {
		fprintf(stderr, "usage: %s\n", argv[0]);
		return 1;
	}

	for (i = 0; libparser_rule_table[i]; i++) {
		if (libparser_rule_table[i]->name[0] == '@')
			continue;

		if (!first) {
			printf("\n");
		} else {
			first = 0;
		}

		printf("%s = %n", libparser_rule_table[i]->name, &indent);
		print_sentence(libparser_rule_table[i]->sentence, indent);
		printf(";\n");

	}

	if (fflush(stdout) || ferror(stdout) || fclose(stdout)) {
		fprintf(stderr, "%s: printf: %s\n", argv[0], strerror(errno));
		return 1;
	}
	return 0;
}
