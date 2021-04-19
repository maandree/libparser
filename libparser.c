/* See LICENSE file for copyright and license details. */
#include "libparser.h"
#include <stdlib.h>
#include <string.h>


struct context {
	const struct libparser_rule *const *rules;
	struct libparser_unit *cache;
	const char *data;
	size_t length;
	size_t position;
	char done;
	char exception;
	char error;
};


static void
free_unit(struct libparser_unit *unit, struct context *ctx)
{
	struct libparser_unit *prev;
	while (unit) {
		free_unit(unit->in, ctx);
		prev = unit;
		unit = unit->next;
		prev->next = ctx->cache;
		ctx->cache = prev;
	}
}


static void
dealloc_unit(struct libparser_unit *unit)
{
	struct libparser_unit *next;
	for (; unit; unit = next) {
		dealloc_unit(unit->in);
		next = unit->next;
		free(unit);
	}
}


static struct libparser_unit *
try_match(const char *rule, const union libparser_sentence *sentence, struct context *ctx)
{
	struct libparser_unit *unit, *next;
	struct libparser_unit **head;
	unsigned char c;
	size_t i;

	if (!ctx->cache) {
		unit = calloc(1, sizeof(*unit));
		if (!unit) {
			ctx->done = 1;
			ctx->error = 1;
			return NULL;
		}
	} else {
		unit = ctx->cache;
		ctx->cache = unit->next;
		unit->in = unit->next = NULL;
	}

	unit->rule = rule;
	unit->start = ctx->position;

	switch (sentence->type) {
	case LIBPARSER_SENTENCE_TYPE_CONCATENATION:
		unit->in = try_match(NULL, sentence->binary.left, ctx);
		if (!unit->in)
			goto mismatch;
		if (ctx->done)
			break;
		unit->in->next = try_match(NULL, sentence->binary.right, ctx);
		if (!unit->in->next) {
			free_unit(unit->in, ctx);
			goto mismatch;
		}
		if (!unit->in->next->rule || unit->in->next->rule[0] == '_') {
			unit->in->next->next = ctx->cache;
			ctx->cache = unit->in->next;
			unit->in->next = unit->in->next->in;
		}
		if (!unit->in->rule || unit->in->rule[0] == '_') {
			next = unit->in->next;
			unit->in->next = ctx->cache;
			ctx->cache = unit->in;
			unit->in = unit->in->in;
			if (unit->in) {
				for (head = &unit->in->next; *head; head = &(*head)->next);
				*head = next;
			} else {
				unit->in = next;
			}
		}
		break;

	case LIBPARSER_SENTENCE_TYPE_ALTERNATION:
		unit->in = try_match(NULL, sentence->binary.left, ctx);
		if (!unit->in) {
			unit->in = try_match(NULL, sentence->binary.right, ctx);
			if (!unit->in)
				goto mismatch;
		}
	prone:
		if (unit->in && (!unit->in->rule || unit->in->rule[0] == '_')) {
			unit->in->next = ctx->cache;
			ctx->cache = unit->in;
			unit->in = unit->in->in;
		}
		break;

	case LIBPARSER_SENTENCE_TYPE_REJECTION:
		unit->in = try_match(NULL, sentence->unary.sentence, ctx);
		if (unit->in) {
			free_unit(unit->in, ctx);
			if (!ctx->exception)
				goto mismatch;
			ctx->exception = 0;
		}
		ctx->position = unit->start;
		unit->rule = NULL;
		break;

	case LIBPARSER_SENTENCE_TYPE_OPTIONAL:
		unit->in = try_match(NULL, sentence->unary.sentence, ctx);
		goto prone;

	case LIBPARSER_SENTENCE_TYPE_REPEATED:
		head = &unit->in;
		while ((*head = try_match(NULL, sentence->unary.sentence, ctx))) {
			if (!(*head)->rule || (*head)->rule[0] == '_') {
				(*head)->next = ctx->cache;
				ctx->cache = *head;
				*head = (*head)->in;
				while (*head)
					head = &(*head)->next;
			} else {
				head = &(*head)->next;
			}
			if (ctx->done)
				break;
		}
		break;

	case LIBPARSER_SENTENCE_TYPE_STRING:
		if (sentence->string.length > ctx->length - ctx->position)
			goto mismatch;
		if (memcmp(&ctx->data[ctx->position], sentence->string.string, sentence->string.length))
			goto mismatch;
		ctx->position += sentence->string.length;
		break;

	case LIBPARSER_SENTENCE_TYPE_CHAR_RANGE:
		if (ctx->position == ctx->length)
			goto mismatch;
		c = ((const unsigned char *)ctx->data)[ctx->position];
		if (sentence->char_range.low > c || c > sentence->char_range.high)
			goto mismatch;
		ctx->position += 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_RULE:
		for (i = 0; ctx->rules[i]; i++)
			if (!strcmp(ctx->rules[i]->name, sentence->rule.rule))
				break;
		if (!ctx->rules[i])
			abort();
		unit->in = try_match(ctx->rules[i]->name, ctx->rules[i]->sentence, ctx);
		if (!unit->in)
			goto mismatch;
		goto prone;

	case LIBPARSER_SENTENCE_TYPE_EXCEPTION:
		ctx->done = 1;
		ctx->exception = 1;
		break;

	case LIBPARSER_SENTENCE_TYPE_EOF:
		if (ctx->position != ctx->length)
			goto mismatch;
		ctx->done = 1;
		break;

	default:
		abort();
	}

	unit->end = ctx->position;
	return unit;

mismatch:
	ctx->position = unit->start;
	unit->next = ctx->cache;
	ctx->cache = unit;
	return NULL;
}


int
libparser_parse_file(const struct libparser_rule *const rules[], const char *data, size_t length, struct libparser_unit **rootp)
{
	struct libparser_unit *ret, *t;
	struct context ctx;
	size_t i;

	ctx.rules = rules;
	ctx.cache = NULL;
	ctx.data = data;
	ctx.length = length;
	ctx.position = 0;
	ctx.done = 0;
	ctx.error = 0;
	ctx.exception = 0;

	for (i = 0; rules[i]; i++)
		if (!strcmp(rules[i]->name, "@start"))
			break;
	if (!rules[i])
		abort();

	ret = try_match(rules[i]->name, rules[i]->sentence, &ctx);

	while (ctx.cache) {
		t = ctx.cache;
		ctx.cache = t->next;
		free(t);
	}

	if (ctx.error) {
		dealloc_unit(ret);
		*rootp = NULL;
		return -1;
	}

	*rootp = ret;
	return !ctx.exception;
}
