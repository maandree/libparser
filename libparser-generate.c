/* See LICENSE file for copyright and license details. */
#include <libsimple.h>
#include <libsimple-arg.h>

USAGE("main-rule");


struct token {
	size_t lineno;
	size_t column;
	size_t character;
	char s[];
};

struct node {
	struct token *token;
	struct node *parent;
	struct node *next;
	struct node *data;
	struct node **head;
};


static char **rule_names = NULL;
static size_t nrule_names = 0;
static size_t rule_names_size = 0;

static char **want_rules = NULL;
static size_t nwant_rules = 0;
static size_t want_rules_size = 0;


static int
strpcmp(const void *av, const void *bv)
{
	const char *const *a = av;
	const char *const *b = bv;
	return strcmp(*a, *b);
}


static int
isidentifier(char c)
{
	return isalpha(c) || isdigit(c) || !isascii(c) || c == '_';
}


static int
check_utf8(char *buf, size_t *ip, size_t len)
{
	size_t req, i;
	uint32_t cp;
	if ((buf[*ip] & 0xE0) == 0xC0) {
		cp = (uint32_t)(unsigned char)(buf[*ip] ^ 0xC0);
		req = 2;
	} else if ((buf[*ip] & 0xF0) == 0xE0) {
		cp = (uint32_t)(unsigned char)(buf[*ip] ^ 0xE0);
		req = 3;
	} else if ((buf[*ip] & 0xF8) == 0xF0) {
		cp = (uint32_t)(unsigned char)(buf[*ip] ^ 0xF0);
		req = 4;
	} else {
		return 0;
	}
	if (req > len - *ip)
		return 0;
	for (i = 1; i < req; i++) {
		cp <<= 6;
		if ((buf[*ip + i] & 0xC0) != 0x80)
			return 0;
		cp |= (uint32_t)(unsigned char)(buf[*ip + i] ^ 0x80);
	}
	*ip += req;
	if ((cp & UINT32_C(0xD8000)) == UINT32_C(0xD8000))
		return 0;
	if (cp < (uint32_t)1 << (7 + 0 * 6))
		return 0;
	if (cp < (uint32_t)1 << (5 + 1 * 6))
		return req == 2;
	if (cp < (uint32_t)1 << (4 + 2 * 6))
		return req == 3;
	if (cp <= UINT32_C(0x10FFFF))
		return req == 4;
	return 0;
}


static char *
readall_and_validate(int fd, const char *fname)
{
	size_t lineno = 1, column = 0, character = 0;
	size_t size = 0, len = 0, i;
	char *buf = NULL;
	ssize_t r;

	for (;; len += (size_t)r) {
		if (len == size)
			buf = erealloc(buf, size += 1024);
		r = read(fd, &buf[len], size - len);
		if (r <= 0) {
			if (!r)
				break;
			eprintf("read %s:", fname);
		}
	}

	for (i = 0; i < len; i++) {
		if (buf[i] == '\n') {
			lineno += 1;
			column = 0;
			character = 0;
		} else if (buf[i] == '\t') {
			column += 8 - column % 8;
			character += 1;
		} else if (buf[i] == '\r') {
			eprintf("%s contains a CR character on line %zu at column %zu (character %zu)\n",
			        fname, lineno, column, character);
		} else if ((0 < buf[i] && buf[i] < ' ') || buf[i] == 0x7F) {
			eprintf("%s contains a illegal character on line %zu at column %zu (character %zu)\n",
			        fname, lineno, column, character);
		} else if (buf[i] == '\0') {
			eprintf("%s contains a NUL byte on line %zu at column %zu (character %zu)\n",
			        fname, lineno, column, character);
		} else if (!(buf[i] & 0x80)) {
			character += 1;
			column += 1;
		} else if ((buf[i] & 0xC0) == 0x80) {
			eprintf("%s contains a illegal byte on line %zu at column %zu (character %zu)\n",
			        fname, lineno, column, character);
		} else {
			if (!check_utf8(buf, &i, len)) {
				eprintf("%s contains a illegal byte sequence on line %zu at column %zu (character %zu)\n",
				        fname, lineno, column, character);
			}
			i--;
			character += 1;
			column += 1;
		}
	}

	buf = erealloc(buf, len + 1);
	buf[len] = '\0';

	return buf;
}


static struct token **
tokenise(const char *data)
{
	enum {
		NEW_TOKEN,
		IDENTIFIER,
		STRING,
		STRING_ESC,
		SPACE
	} state = NEW_TOKEN;
	size_t lineno = 1, column = 0, character = 0;
	size_t token_lineno = 0, token_column = 0, token_character = 0;
	struct token **tokens = NULL;
	char *token = NULL;
	size_t i, ntokens = 0, tokens_size = 0;
	size_t token_len = 0, token_size = 0;

	for (i = 0; data[i]; i++) {
	again:
		switch (state) {
		case NEW_TOKEN:
			token_lineno = lineno;
			token_column = column;
			token_character = character;
			if (token_len == token_size)
				token = erealloc(token, token_size += 16);
			token[token_len++] = data[i];
			if (isidentifier(data[i])) {
				state = IDENTIFIER;
			} else if (isspace(data[i])) {
				state = SPACE;
			} else if (data[i] == '"') {
				state = STRING;
				if (data[i + 1] == '"') {
					eprintf("empty string token on line %zu at column %zu (character %zu)\n",
					        lineno, column, character);
				}
			} else {
			add_token:
				if (token_len == token_size)
					token = erealloc(token, token_size += 16);
				token[token_len++] = '\0';
				if (ntokens == tokens_size)
					tokens = ereallocn(tokens, tokens_size += 16, sizeof(*tokens), 0);
				tokens[ntokens] = emalloc(offsetof(struct token, s) + token_len);
				tokens[ntokens]->lineno = token_lineno;
				tokens[ntokens]->column = token_column;
				tokens[ntokens]->character = token_character;
				stpcpy(tokens[ntokens++]->s, token);
				token_len = 0;
				state = NEW_TOKEN;
			}
			break;

		case IDENTIFIER:
			if (isidentifier(data[i]) || data[i] == '-') {
			add_char:
				if (token_len == token_size)
					token = erealloc(token, token_size += 16);
				token[token_len++] = data[i];
			} else {
			add_token_and_do_again:
				if (token_len == token_size)
					token = erealloc(token, token_size += 16);
				token[token_len++] = '\0';
				if (ntokens == tokens_size)
					tokens = ereallocn(tokens, tokens_size += 16, sizeof(*tokens), 0);
				tokens[ntokens] = emalloc(offsetof(struct token, s) + token_len);
				tokens[ntokens]->lineno = token_lineno;
				tokens[ntokens]->column = token_column;
				tokens[ntokens]->character = token_character;
				stpcpy(tokens[ntokens++]->s, token);
				token_len = 0;
				state = NEW_TOKEN;
				goto again;
			}
			break;

		case STRING:
			if (data[i] == '\n' || data[i] == '\t') {
				eprintf("illegal whitespace on line %zu at column %zu (character %zu)\n",
				        lineno, column, character);
			} else if (data[i] == '"') {
				goto add_token;
			} else if (data[i] == '\\') {
				state = STRING_ESC;
				goto add_char;
			} else {
				goto add_char;
			}
			break;

		case STRING_ESC:
			if (data[i] == '\n' || data[i] == '\t') {
				eprintf("illegal whitespace on line %zu at column %zu (character %zu)\n",
				        lineno, column, character);
			}
			if (token_len == token_size)
				token = erealloc(token, token_size += 16);
			token[token_len++] = data[i];
			state = STRING;
			break;

		case SPACE:
			if (isspace(data[i])) {
				goto add_char;
			} else {
				goto add_token_and_do_again;
			}
			break;

		default:
			abort();
		};

		if (data[i] == '\n') {
			lineno += 1;
			column = 0;
			character = 0;
		} else if (data[i] == '\t') {
			column += 8 - column % 8;
			character += 1;
		} else {
			character += (data[i] & 0xC0) != 0x80;
			column += 1;
		}
	}
	if (state != NEW_TOKEN && state != SPACE)
		eprintf("premature end of file\n");

	tokens = ereallocn(tokens, ntokens + 1, sizeof(*tokens), 0);
	tokens[ntokens] = NULL;
	free(token);

	return tokens;
}


static void
emit_and_free_sentence(struct node *node, size_t *indexp)
{
	size_t index = (*indexp)++, left, right;
	struct node *next;

	for (; node->token->s[0] == '('; node = next) {
		next = node->data;
		free(node->token);
		free(node);
	}

	if (node->token->s[0] == '[' || node->token->s[0] == '{') {
		emit_and_free_sentence(node->data, indexp);
		printf("static union libparser_sentence sentence_%zu_%zu = {.unary = {"
		           ".type = LIBPARSER_SENTENCE_TYPE_%s, .sentence = &sentence_%zu_%zu"
		       "}};\n",
		       nrule_names, index, node->token->s[0] == '[' ? "OPTIONAL" : "REPEATED", nrule_names, index + 1);
	} else if (node->token->s[0] == '|' || node->token->s[0] == ',') {
		right = *indexp;
		emit_and_free_sentence(node->data->next, indexp);
		left = *indexp;
		emit_and_free_sentence(node->data, indexp);
		printf("static union libparser_sentence sentence_%zu_%zu = {.binary = {"
		           ".type = LIBPARSER_SENTENCE_TYPE_%s, "
		           ".left = &sentence_%zu_%zu, .right = &sentence_%zu_%zu"
		       "}};\n",
		       nrule_names, index, node->token->s[0] == '|' ? "ALTERNATION" : "CONCATENATION",
		       nrule_names, left, nrule_names, right);
	} else if (node->token->s[0] == '"') {
		printf("static union libparser_sentence sentence_%zu_%zu = {.string = {"
		           ".type = LIBPARSER_SENTENCE_TYPE_STRING, "
		           ".string = %s\", .length = sizeof(%s\") - 1"
		       "}};\n",
		       nrule_names, index, node->token->s, node->token->s);
	} else if (node->token->s[0] == '-') {
		printf("static union libparser_sentence sentence_%zu_%zu = {.type = LIBPARSER_SENTENCE_TYPE_EXCEPTION};\n",
		       nrule_names, index);
	} else {
		if (nwant_rules == want_rules_size)
			want_rules = ereallocn(want_rules, want_rules_size += 16, sizeof(*want_rules), 0);
		want_rules[nwant_rules++] = estrdup(node->token->s);
		printf("static union libparser_sentence sentence_%zu_%zu = {.rule = {"
		           ".type = LIBPARSER_SENTENCE_TYPE_RULE, .rule = \"%s\""
		       "}};\n",
		       nrule_names, index, node->token->s);
	}

	free(node->token);
	free(node);
}


static struct node *
order_sentences(struct node *node)
{
	struct node *tail = NULL, **head = &tail;
	struct node *stack = NULL;
	struct node *next, *prev;

	for (; node; node = next) {
		next = node->next;
		if (node->token->s[0] == '(' || node->token->s[0] == '[' || node->token->s[0] == '{') {
			node->data = order_sentences(node->data);
			*head = node;
			head = &node->next;
		} else if (node->token->s[0] == '|' || node->token->s[0] == ',') {
		again_operators:
			if (!stack) {
				node->next = stack;
				stack = node;
			} else if (node->token->s[0] == ',' && stack->token->s[0] == '|') {
				node->next = stack;
				stack = node;
			} else if (node->token->s[0] == stack->token->s[0]) {
				*head = stack;
				head = &stack->next;
				stack = stack->next;
				node->next = stack;
				stack = node;
			} else {
				*head = stack;
				head = &stack->next;
				stack = stack->next;
				goto again_operators;
			}
		} else {
			*head = node;
			head = &node->next;
		}
	}

	for (; stack; stack = next) {
		next = stack->next;
		*head = stack;
		head = &stack->next;
	}

	*head = NULL;

	for (stack = tail, prev = NULL; stack; prev = stack, stack = next) {
		next = stack->next;
		stack->next = prev;
		if (stack->token->s[0] == '|' || stack->token->s[0] == ',') {
			prev = stack->next->next->next;
			stack->data = stack->next->next;
			stack->data->next = stack->next;
			stack->next = prev;
		}
	}

	return prev;
}


static void
emit_and_free_rule(struct node *rule)
{
	size_t index = 0;

	rule->data = order_sentences(rule->data);
	emit_and_free_sentence(rule->data, &index);

	printf("static struct libparser_rule rule_%zu = {\"%s\", &sentence_%zu_0};\n", nrule_names, rule->token->s, nrule_names);

	if (nrule_names == rule_names_size)
		rule_names = ereallocn(rule_names, rule_names_size += 16, sizeof(*rule_names), 0);
	rule_names[nrule_names++] = estrdup(rule->token->s);
	free(rule->token);
	free(rule);
}


int
main(int argc, char *argv[])
{
	enum {
		IDENTIFIER,
		STRING,
		SYMBOL,
	} type;
	enum {
		NEW_RULE,
		EXPECT_EQUALS,
		EXPECT_OPERAND,
		EXPECT_OPERATOR
	} state = NEW_RULE;
	struct node *stack = NULL, *parent_node, *node;
	char *data;
	struct token **tokens;
	size_t i, j;
	int cmp, err;

	ARGBEGIN {
	default:
		usage();
	} ARGEND;

	if (argc != 1 || !isidentifier(argv[0][0]))
		usage();
	for (i = 0; argv[0][i]; i++)
		if (!isidentifier(argv[0][i]) && argv[0][i] != '-')
			usage();

	data = readall_and_validate(STDIN_FILENO, "<stdin>");
	tokens = tokenise(data);
	free(data);

	printf("#include <libparser.h>\n");

	i = 0;
again:
	for (; tokens[i]; i++) {
		if (tokens[i + 1] && tokens[i]->s[0] == '(' && tokens[i + 1]->s[0] == '*') {
			for (i += 2; tokens[i] && tokens[i + 1]; i++) {
				if (tokens[i]->s[0] == '*' && tokens[i + 1]->s[0] == ')') {
					i += 2;
					goto again;
				}
			}
			eprintf("premature end of file\n");
		}

		if (tokens[i]->s[0] == '"') {
			type = STRING;
		} else if (isidentifier(tokens[i]->s[0])) {
			type = IDENTIFIER;
		} else if (isspace(tokens[i]->s[0])) {
			free(tokens[i]);
			continue;
		} else {
			type = SYMBOL;
		}

		switch (state) {
		case NEW_RULE:
			if (type != IDENTIFIER) {
				eprintf("expected an identifier on line %zu at column %zu (character %zu)\n",
				        tokens[i]->lineno, tokens[i]->column, tokens[i]->character);
			}
			stack = calloc(1, sizeof(*stack));
			stack->token = tokens[i];
			stack->head = &stack->data;
			state = EXPECT_EQUALS;
			for (j = 0; j < nrule_names; j++) {
				if (!strcmp(rule_names[j], tokens[i]->s)) {
					eprintf("duplicate definition of \"%s\" on line %zu at column %zu (character %zu)\n",
					        tokens[i]->s, tokens[i]->lineno, tokens[i]->column, tokens[i]->character);
				}
			}
			break;

		case EXPECT_EQUALS:
			if (type != SYMBOL || tokens[i]->s[0] != '=') {
				eprintf("expected an '=' on line %zu at column %zu (character %zu)\n",
				        tokens[i]->lineno, tokens[i]->column, tokens[i]->character);
			}
			free(tokens[i]);
			state = EXPECT_OPERAND;
			break;

		case EXPECT_OPERAND:
			if (type == SYMBOL) {
				if (tokens[i]->s[0] == '(' || tokens[i]->s[0] == '[' || tokens[i]->s[0] == '{') {
					parent_node = stack;
					stack = ecalloc(1, sizeof(*stack));
					stack->parent = parent_node;
					stack->token = tokens[i];
					stack->head = &stack->data;
				} else if (tokens[i]->s[0] == '-') {
					goto add;
				} else {
				stray:
					eprintf("stray '%c' on line %zu at column %zu (character %zu)\n",
					        tokens[i]->s[0], tokens[i]->lineno, tokens[i]->column, tokens[i]->character);
				}
			} else {
			add:
				state = EXPECT_OPERATOR;
				goto add_singleton;
			}
			break;

		case EXPECT_OPERATOR:
			if (tokens[i]->s[0] == '|' || tokens[i]->s[0] == ',') {
				state = EXPECT_OPERAND;
			add_singleton:
				node = calloc(1, sizeof(*node));
				node->token = tokens[i];
				*stack->head = node;
				stack->head = &node->next;
			} else if (tokens[i]->s[0] == ')') {
				if (stack->token->s[0] != '(')
					goto stray;
				goto pop;
			} else if (tokens[i]->s[0] == ']') {
				if (stack->token->s[0] != '[')
					goto stray;
				goto pop;
			} else if (tokens[i]->s[0] == '}') {
				if (stack->token->s[0] != '{')
					goto stray;
			pop:
				free(tokens[i]);
				*stack->parent->head = stack;
				stack->parent->head = &stack->next;
				stack = stack->parent;
			} else if (tokens[i]->s[0] == ';') {
				if (stack->token->s[0] == ')' || stack->token->s[0] == ']' || stack->token->s[0] == '}')
					eprintf("premature end of rule on line %zu at column %zu (character %zu): "
					        "'%s' on line %zu at column %zu (character %zu) not closed\n",
					        tokens[i]->lineno, tokens[i]->column, tokens[i]->character, stack->token->s,
					        stack->token->lineno, stack->token->column, stack->token->character);
				emit_and_free_rule(stack);
				free(tokens[i]);
				state = NEW_RULE;
			} else {
				eprintf("expected a '|', ',', or '%c' on line %zu at column %zu (character %zu)\n",
				        stack->token->s[0] == '(' ? ')' :
				        stack->token->s[0] == '[' ? ']' :
				        stack->token->s[0] == '{' ? '}' : ';',
				        tokens[i]->lineno, tokens[i]->column, tokens[i]->character);
			}
			break;

		default:
			abort();
		}
	}
	free(tokens);
	if (state != NEW_RULE)
		eprintf("premature end of file\n");

	err = 0;
	qsort(rule_names, nrule_names, sizeof(*rule_names), strpcmp);
	qsort(want_rules, nwant_rules, sizeof(*want_rules), strpcmp);
	for (i = j = 0; i < nrule_names && j < nwant_rules;) {
		cmp = strcmp(rule_names[i], want_rules[j]);
		if (!cmp) {
			i++;
			for (j++; j < nwant_rules && !strcmp(want_rules[j - 1], want_rules[j]); j++);
		} else if (!strcmp(rule_names[i], argv[0])) {
			i++;
		} else if (cmp < 0) {
			weprintf("rule \"%s\" defined but not used\n", rule_names[i]);
			i++;
			err = 1;
		} else {
			weprintf("rule \"%s\" used but not defined\n", want_rules[j]);
			for (j++; j < nwant_rules && !strcmp(want_rules[j - 1], want_rules[j]); j++);
			err = 1;
		}
	}
	for (; i < nrule_names; i++) {
		if (strcmp(rule_names[i], argv[0])) {
			weprintf("rule \"%s\" defined but not used\n", rule_names[i]);
			err = 1;
		}
	}
	while (j < nwant_rules) {
		weprintf("rule \"%s\" used but not defined\n", want_rules[j]);
		for (j++; j < nwant_rules && !strcmp(want_rules[j - 1], want_rules[j]); j++);
		err = 1;
	}
	if (err)
		exit(1);

	for (i = 0; i < nrule_names; i++)
		if (!strcmp(rule_names[i], argv[0]))
			goto found_main;
	eprintf("specified main rule (\"%s\") was not defined\n", argv[0]);

found_main:
	printf("static union libparser_sentence noeof_sentence = {.type = LIBPARSER_SENTENCE_TYPE_EXCEPTION};\n");
	printf("static struct libparser_rule noeof_rule = {\"@noeof\", &noeof_sentence};\n");
	printf("static union libparser_sentence noeof_rule_sentence = {.rule = "
	           "{.type = LIBPARSER_SENTENCE_TYPE_RULE, .rule = \"@noeof\"}"
	       "};\n");

	printf("static union libparser_sentence eof_sentence = {.type = LIBPARSER_SENTENCE_TYPE_EOF};\n");
	printf("static struct libparser_rule eof_rule = {\"@eof\", &eof_sentence};\n");
	printf("static union libparser_sentence eof_rule_sentence = {.rule = "
	           "{.type = LIBPARSER_SENTENCE_TYPE_RULE, .rule = \"@eof\"}"
	       "};\n");

	printf("static union libparser_sentence end_sentence = {.binary = {"
	           ".type = LIBPARSER_SENTENCE_TYPE_ALTERNATION, "
	           ".left = &eof_rule_sentence, .right = &noeof_rule_sentence"
	       "}};\n");

	printf("static union libparser_sentence main_rule_sentence = {.rule = "
	           "{.type = LIBPARSER_SENTENCE_TYPE_RULE, .rule = \"%s\"}"
	       "};\n", argv[0]);

	printf("static union libparser_sentence main_sentence = {.binary = {"
	           ".type = LIBPARSER_SENTENCE_TYPE_CONCATENATION, "
	           ".left = &main_rule_sentence, .right = &end_sentence"
	       "}};\n");
	printf("static struct libparser_rule main_rule = {\"@start\", &main_sentence};\n");

	printf("const struct libparser_rule *const libparser_rule_table[] = {\n");
	for (i = 0; i < nrule_names; i++) {
		printf("\t&rule_%zu,\n", i);
		free(rule_names[i]);
	}
	printf("\t&eof_rule,\n");
	printf("\t&noeof_rule,\n");
	printf("\t&main_rule,\n");
	printf("\tNULL\n};\n");
	free(rule_names);
	for (i = 0; i < nwant_rules; i++)
		free(want_rules[i]);
	free(want_rules);

	if (ferror(stdout) || fflush(stdout) || fclose(stdout))
		eprintf("printf:");
	return 0;
}
