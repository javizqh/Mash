// Copyright 2023 Javier Izquierdo Hernández
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "builtin/export.h"
#include "builtin/mash_math.h"

char *math_use = "math expression";
char *math_description = "Write result of the arithmetic expression.";
char *math_help =
    "    Display the result of the arithmetic expression, followed by a\n"
    "    newline, on the standard output.\n\n"
    "    Exit Status:\n"
    "    Returns success unless an error in the expression is found.\n";

static int
help()
{
	printf("math: %s\n", math_use);
	printf("    %s\n\n%s", math_description, math_help);
	return EXIT_SUCCESS;
}

static int
usage()
{
	fprintf(stderr, "Usage: %s\n", math_use);
	return EXIT_FAILURE;
}

// STATIC FUNCTIONS FOR BUILTIN
static int error_in_operations = 0;

enum lexer_type {
	MATH_SYMBOL = 1,
	MATH_NUMBER,
	MATH_VARIABLE
};

enum math_size {
	MAX_OPERAND_SIZE = 32
};

struct token {
	char data[MAX_OPERAND_SIZE];
	double value;
	int type;
	int priority;
	int symbol_priority;
	struct token *next;
};

typedef struct token Token;

Token *
newToken()
{
	Token *token = (Token *)malloc(sizeof(Token));

	// Check if malloc failed
	if (token == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(token, 0, sizeof(Token));
	token->value = 1;
	token->type = 0;
	token->priority = 0;
	token->symbol_priority = 0;
	token->next = NULL;

	return token;
}

Token *
add_token(Token *prev_token, int priority)
{
	Token *new_token = newToken();

	prev_token->next = new_token;
	new_token->priority = priority;
	return new_token;
}

void
free_all_tokens(Token *first_token)
{
	Token *token = first_token;
	Token *to_free = token;

	while (to_free) {
		token = token->next;
		free(to_free);
		to_free = token;
	}
	return;
}

static int
is_symbol(char c)
{
	return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

static double
get_number(char *number)
{
	double num = 0;

	for (; *number != '\0'; number++) {
		if (*number >= '0' && *number <= '9') {
			num = num * 10 + (*number - '0');
		} else {
			return -1;
		}
	}
	return num;
}

static Token *
tokenize(char *expression)
{
	Token *first_token = newToken();
	Token *current_token = first_token;
	int total_priority = 0;
	char *line = expression;

	for (; *expression != '\0'; expression++) {
		if (*expression >= '0' && *expression <= '9') {
			if (current_token->type != MATH_NUMBER
			    && current_token->type) {
				current_token =
				    add_token(current_token, total_priority);
				current_token->type = MATH_NUMBER;
			}
			strncat(current_token->data, expression, 1);
			current_token->type = MATH_NUMBER;
		} else if ((*expression >= 'a' && *expression <= 'z') ||
			   (*expression >= 'A' && *expression <= 'Z')) {
			if (current_token->type != MATH_VARIABLE
			    && current_token->type) {
				current_token =
				    add_token(current_token, total_priority);
				current_token->type = MATH_VARIABLE;
			}
			strncat(current_token->data, expression, 1);
			current_token->type = MATH_VARIABLE;
		} else if (is_symbol(*expression)) {
			if (!current_token->type) {
				if (*expression != '-' && *expression != '+') {
					fprintf(stderr,
						"mash: error: math: incorrect character '%c' at the beginning of expression '%s'\n",
						*expression, line);
					free_all_tokens(first_token);
					return NULL;
				}
				current_token->type = MATH_NUMBER;
			}
			current_token =
			    add_token(current_token, total_priority);
			current_token->type = MATH_SYMBOL;
			strncat(current_token->data, expression, 1);
		} else if (*expression == '(') {
			if (!current_token->type) {
				current_token->priority++;
			}
			total_priority++;
		} else if (*expression == ')') {
			if (!current_token->type) {
				fprintf(stderr,
					"mash: error: math: incorrect character '%c' in expression '%s'\n",
					*expression, line);
				free_all_tokens(first_token);
				return NULL;
			}
			total_priority--;
		} else if (*expression != ' ' && *expression != '\t') {
			fprintf(stderr,
				"mash: error: math: incorrect character '%c' in expression '%s'\n",
				*expression, line);
			free_all_tokens(first_token);
			return NULL;
		}
	}
	if (total_priority != 0) {
		fprintf(stderr,
			"mash: error: math: incorrect expression '%s'\n", line);
		free_all_tokens(first_token);
		return NULL;
	}
	if (current_token->type == MATH_SYMBOL) {
		fprintf(stderr,
			"mash: error: math: incorrect symbol '%s' at the end of expression\n",
			line);
		free_all_tokens(first_token);
		return NULL;
	}
	return first_token;
}

static int
substitute_values(Token *first_token)
{
	Token *token, *prev_token, *to_free;
	char *variable;
	double value;
	int prev_is_symbol = -1;

	for (token = first_token; token; token = token->next) {
		switch (token->type) {
		case MATH_NUMBER:
			if (!prev_is_symbol) {
				fprintf(stderr, "mash: error:");
				return -1;
			}
			prev_is_symbol = 0;
			if ((value = get_number(token->data)) < 0) {
				return -1;
			}
			token->value *= value;
			break;
		case MATH_VARIABLE:
			if (!prev_is_symbol) {
				return -1;
			}
			prev_is_symbol = 0;
			variable = get_env_by_name(token->data);
			if (variable == NULL) {
				fprintf(stderr,
					"mash: error: var %s does not exist\n",
					token->data);
				return -1;
			}

			if (*variable == '-') {
				token->value *= -1;
				variable++;
				strcpy(token->data, variable);
				variable--;
			} else if (*variable == '+') {
				variable++;
				strcpy(token->data, variable);
				variable--;
			} else {
				strcpy(token->data, variable);
			}

			free(variable);
			if ((value = get_number(token->data)) < 0) {
				return -1;
			}
			token->value *= value;
			token->type = MATH_NUMBER;
			break;
		default:
			if (prev_is_symbol > 0) {
				prev_is_symbol = 1;
				if (*token->data == '-') {
					to_free = token;
					token->next->value *= -1;
					prev_token->next = token->next;
					token = prev_token;
					free(to_free);
				} else if (*token->data == '+') {
					to_free = token;
					prev_token->next = token->next;
					token = prev_token;
					free(to_free);
				} else {
					return -1;
				}
				break;
			}
			prev_is_symbol = 1;
			if (strcmp(token->data, "*") == 0
			    || strcmp(token->data, "/") == 0) {
				token->symbol_priority = 1;
			} else if (strcmp(token->data, "^") == 0) {
				token->symbol_priority = 2;
			}
			break;
		}
		prev_token = token;
	}
	return 0;
}

static double
calculate(char symbol, double op_1, double op_2)
{
	switch (symbol) {
	case '*':
		return op_1 * op_2;
		break;
	case '+':
		return op_1 + op_2;
		break;
	case '-':
		return op_1 - op_2;
		break;
	case '/':
		if (op_2 == 0) {
			error_in_operations = 1;
			return 0;
		}
		return op_1 / op_2;
		break;
	case '^':
		return pow(op_1, op_2);
		break;
	}
	return 0;
}

static double
do_operations(Token *start_token)
{
	// num sim1 num2 sim2 num3 sim3 num4
	// If sim2 == * and sim1 == 1 replace num2 with (num2 sim2 num3)
	// Do the operations here
	// Symbol priority ^; * /; + -
	Token *op_1, *op_2, *symbol;
	Token *prev_symbol = NULL;
	Token *token = start_token;
	Token *prev_op = token;
	int symbol_p = -1;
	double result = 0;

	if (start_token->next == NULL) {
		result = start_token->value;
		free(start_token);
		return result;
	}

	for (; token; token = token->next) {
		if (token->type != MATH_SYMBOL) {
			if (token->next == NULL) {
				if (prev_symbol) {
					token = add_token(prev_symbol, 0);
				} else {
					token = newToken();
				}
				token->next = NULL;
				token->value =
				    calculate(*symbol->data, op_1->value,
					      op_2->value);
				if (start_token == op_1) {
					free(symbol);
					free(op_1);
					free(op_2);
					break;
				}
				token = start_token;
				free(symbol);
				free(op_1);
				free(op_2);
				symbol_p = -1;
			}
			prev_op = token;
			continue;
		}
		if (symbol_p == -1) {
			symbol_p = token->symbol_priority;
			symbol = token;
			op_1 = prev_op;
			op_2 = token->next;
			continue;
		}

		if (token->symbol_priority > symbol_p) {
			prev_symbol = symbol;
			symbol_p = token->symbol_priority;
			symbol = token;
			op_1 = prev_op;
			op_2 = token->next;
		} else {
			// Do operation
			if (prev_symbol) {
				token = add_token(prev_symbol, 0);
			} else {
				token = newToken();
			}
			token->next = op_2->next;
			token->value =
			    calculate(*symbol->data, op_1->value, op_2->value);
			if (start_token != op_1) {
				token = start_token;
				prev_op = start_token;
			} else {
				start_token = token;
				prev_op = token;
			}
			free(symbol);
			free(op_1);
			free(op_2);
			symbol_p = -1;
			prev_symbol = NULL;
		}
	}
	result = token->value;
	free(token);
	return result;
}

static double
operate(Token *start_token)
{
	// Operate on the highest priority token
	Token *high_priority_token;
	Token *sp_token;
	Token *token = start_token;
	Token *prev_token = token;
	int priority = token->priority;

	for (; token; token = token->next) {
		if (token->priority > priority) {
			high_priority_token = add_token(prev_token, priority);
			for (sp_token = token; sp_token;
			     sp_token = sp_token->next) {
				if (sp_token->priority < token->priority) {
					break;
				}
			}
			high_priority_token->type = MATH_NUMBER;
			high_priority_token->value = operate(token);
			high_priority_token->next = sp_token;
			token = high_priority_token;
		} else if (token->priority < priority) {
			prev_token->next = NULL;
			return do_operations(start_token);
		}
		prev_token = token;
	}
	return do_operations(start_token);
}

int
math(int argc, char *argv[])
{
	argc--;
	argv++;
	double result;

	if (argc != 1) {
		return usage();
	}

	if (strcmp(argv[0], "--help") == 0) {
		return help();
	}

	Token *first_token = tokenize(argv[0]);

	if (first_token == NULL) {
		return EXIT_FAILURE;
	}

	if (substitute_values(first_token) != 0) {
		fprintf(stderr,
			"mash: error: math: incorrect expression '%s'\n",
			argv[0]);
		free_all_tokens(first_token);
		return EXIT_FAILURE;
	}

	result = operate(first_token);

	if (error_in_operations) {
		error_in_operations = 0;
		fprintf(stderr, "mash: error: math: division by 0 in '%s'\n",
			argv[0]);
		return EXIT_FAILURE;
	}

	printf("%d\n", (int)result);
	return EXIT_SUCCESS;
}
