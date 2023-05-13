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
#include "builtin/mash_math.h"

char * math_use = "math [-n] [arg ...]";
char * math_description = "Write arguments to the standard output.";
char * math_help = 
"    Display the ARGs, separated by a single space character and followed by a\n"
"    newline, on the standard output.\n\n"
"    Options:\n"
"      -n    do not append a newline\n"
"    Exit Status:\n"
"    Returns success unless a write error occurs.\n";

static int help() {
	printf("math: %s\n", math_use);
	printf("    %s\n\n%s", math_description, math_help);
	return EXIT_SUCCESS;
}

static int usage() {
  fprintf(stderr,"Usage: %s\n",math_use);
	return EXIT_FAILURE;
}

// STATIC FUNCTIONS FOR BUILTIN
typedef struct token {
  char data[MAX_OPERAND_SIZE];
  double value;
  int type;
  int priority;
  int symbol_priority;
  struct token * next;
} Token;

Token * newToken() {
  Token *token = (Token *) malloc(sizeof(Token));

	// Check if malloc failed
	if (token == NULL) {
		err(EXIT_FAILURE, "malloc failed");
	}
	memset(token, 0, sizeof(Token));
  token->value = 0;
  token->type = 0;
  token->priority = 0;
  token->symbol_priority = 0;
  token->next = NULL;

	return token;
}

Token * add_token(Token * prev_token, int priority) {
  Token *new_token = newToken();
  prev_token->next = new_token;
  new_token->priority = priority;
	return new_token;
}

static int is_symbol(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' || c == '^'; 
}

static double get_number(char *number) {
  double num = 0;
  for (; *number != '\0'; number++)
  {
    if (*number >= '0' && *number <= '9')
    {
      num = num * 10 + (*number - '0');
    } else {
      return -1;
    }
  }
  return num;
}

static Token * tokenize(char * expression) {
  Token * first_token = newToken();
  Token * current_token = first_token;
  int total_priority = 0;

  for (; *expression != '\0' ; expression++)
  {
    if (*expression >= '0' && *expression <= '9') {
      if (current_token->type != MATH_NUMBER && current_token->type) {
        current_token = add_token(current_token, total_priority);
        current_token->type = MATH_NUMBER;
      }
      strncat(current_token->data, expression, 1);
      current_token->type = MATH_NUMBER;
    } else if ((*expression >= 'a' && *expression <= 'z') ||
      (*expression >= 'A' && *expression <= 'Z')) 
    {
      if (current_token->type != MATH_VARIABLE && current_token->type) {
        current_token = add_token(current_token, total_priority);
        current_token->type = MATH_VARIABLE;
      }
      strncat(current_token->data, expression, 1);
      current_token->type = MATH_VARIABLE;
    } else if (is_symbol(*expression)) {
      current_token = add_token(current_token, total_priority);
      current_token->type = MATH_SYMBOL;
      strncat(current_token->data, expression, 1);
    } else if (*expression == '(') {
      total_priority++;
    } else if (*expression == ')') {
      total_priority--;
    } else {
      // TODO: handle error token
    }
  }
  if (total_priority != 0) {
    // TODO: handle error token
  }
  if (current_token->type == MATH_SYMBOL) {
    // TODO: handle error token
  }
  return first_token;
}

static int substitute_values(Token * first_token) {
  Token * token;
  int prev_is_symbol = 0;
  for (token = first_token; token; token = token->next)
  {
    switch (token->type)
    {
    case MATH_NUMBER:
      if (!prev_is_symbol) {
        // TODO: error bad number
      }
      prev_is_symbol = 0;
      if ((token->value = get_number(token->data)) < 0) {
        // TODO: error bad number
        return -1;
      }
      break;
    case MATH_VARIABLE:
      if (!prev_is_symbol) {
        // TODO: error bad number
      }
      prev_is_symbol = 0;
      /* code */
      break;
    default:
      if (prev_is_symbol) {
        // TODO: error bad number
      }
      prev_is_symbol = 1;
      if (strcmp(token->data, "*") == 0 || strcmp(token->data, "/") == 0) {
        token->symbol_priority = 1;
      } else if (strcmp(token->data, "^") == 0) {
        token->symbol_priority = 2;
      }
      break;
    }
  }
  return 0;
}

static double calculate(char symbol, double op_1, double op_2) {
  switch (symbol)
  {
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
    return op_1 / op_2;
    break;
  case '^':
    return pow(op_1, op_2);
    break;
  }
  return 0;
}

static double do_operations(Token * start_token) {
  // num sim1 num2 sim2 num3 sim3 num4
  // If sim2 == * and sim1 == 1 replace num2 with (num2 sim2 num3)
  // Do the operations here
  // Symbol priority ^; * /; + -
  Token * op_1, * op_2, * symbol;
  Token * prev_symbol = NULL;
  Token * token = start_token;
  Token * prev_op = token;
  int symbol_p = -1;
  double result = 0;

  if (start_token->next == NULL) {
    result = start_token->value;
    free(start_token);
    return result;
  }

  for (; token; token = token->next) {
    if (token->type != MATH_SYMBOL) {
      if (token->next == NULL)
      {
        if (prev_symbol) {
          token = add_token(prev_symbol, 0);
        } else {
          token = newToken();
        }
        token->next = NULL;
        token->value = calculate(*symbol->data,op_1->value,op_2->value);
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
      token->value = calculate(*symbol->data,op_1->value,op_2->value);
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

static double operate(Token * start_token) {
  // Operate on the highest priority token
  Token * high_priority_token;
  Token * sp_token;
  Token * token = start_token;
  Token * prev_token = token;
  int priority = token->priority;
  for (; token; token = token->next) {
    if (token->priority > priority) {
      high_priority_token = add_token(prev_token, priority);
      for ( sp_token = token; sp_token; sp_token = sp_token->next) {
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
  // TODO: free memory
  return do_operations(start_token);
}

int math(int argc, char* argv[]) {
  argc--; argv++;
  // First divide by symbols, numbers and variables
  // Then substitute variables with values
  // Create order of operations
  if (argc != 1) {
    return usage();
  }

  if (strcmp(argv[0], "--help") == 0) {
    return help();
  }

  Token * first_token = tokenize(argv[0]);
  if (first_token == NULL) {
    return EXIT_FAILURE;
  }

  if (substitute_values(first_token) != 0) {
    return EXIT_FAILURE;
  }
  printf("%f\n",operate(first_token));
  return EXIT_SUCCESS;
}