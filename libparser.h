/* See LICENSE file for copyright and license details. */
#ifndef LIBPARSER_H
#define LIBPARSER_H

#include <stddef.h>

union libparser_sentence;

enum libparser_sentence_type {
	LIBPARSER_SENTENCE_TYPE_CONCATENATION, /* .binary */
	LIBPARSER_SENTENCE_TYPE_ALTERNATION,   /* .binary */
	LIBPARSER_SENTENCE_TYPE_OPTIONAL,      /* .unary */
	LIBPARSER_SENTENCE_TYPE_REPEATED,      /* .unary */
	LIBPARSER_SENTENCE_TYPE_STRING,        /* .string */
	LIBPARSER_SENTENCE_TYPE_CHAR_RANGE,    /* .char_range */ /* TODO not supported yet: <low, high> */
	LIBPARSER_SENTENCE_TYPE_RULE,          /* .rule */
	LIBPARSER_SENTENCE_TYPE_EXCEPTION,     /* (none) */
	LIBPARSER_SENTENCE_TYPE_EOF            /* (none) */
};

struct libparser_sentence_binary {
	enum libparser_sentence_type type;
	const union libparser_sentence *left;
	const union libparser_sentence *right;
};

struct libparser_sentence_unary {
	enum libparser_sentence_type type;
	const union libparser_sentence *sentence;
};

struct libparser_sentence_string {
	enum libparser_sentence_type type;
	const char *string;
	size_t length;
};

struct libparser_sentence_char_range {
	enum libparser_sentence_type type;
	unsigned char low;
	unsigned char high;
};

struct libparser_sentence_rule {
	enum libparser_sentence_type type;
	const char *rule;
};

union libparser_sentence { 
	enum libparser_sentence_type type;
	struct libparser_sentence_binary binary;
	struct libparser_sentence_unary unary;
	struct libparser_sentence_string string;
	struct libparser_sentence_char_range char_range;
	struct libparser_sentence_rule rule;
};

struct libparser_rule {
	const char *name;
	union libparser_sentence *sentence;
};

struct libparser_unit {
	const char *rule;
	struct libparser_unit *in;
	struct libparser_unit *next;
	size_t start;
	size_t end;
};


extern const struct libparser_rule *const libparser_rule_table[];


struct libparser_unit *libparser_parse_file(const struct libparser_rule *const rules[],
                                            const char *data, size_t length, int *exceptionp);

#endif
