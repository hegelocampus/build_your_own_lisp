#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "mpc/mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>
static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* Otherwise include the editline headers */
#else
#include <editline/readline.h>
#endif

/* Declare New lval (Lisp Value) Struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* Create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create a new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* Print an lval */
void lval_print(lval v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;

    case LVAL_ERR:
      /*Check what type of error it is and print */
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Cannot Divide by Zero!");
      } else if (v.err == LERR_BAD_OP) {
        printf("Error: Invalid Operator!");
      } else if (v.err == LERR_BAD_NUM) {
        printf("Error: Invalid Number!");
      }
      break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

/* Use operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {
  if (x.type == LVAL_ERR) {
    return x;
  } else if (y.type == LVAL_ERR) {
    return y;
  }

  long res;
  if (strcmp(op, "min") == 0) {
    res = fminl(x.num, y.num);
  } else if (strcmp(op, "max") == 0) {
    res = fmaxl(x.num, y.num);
  } else {
    /* First char of string will always be math op */
    switch(op[0]) {
      case '+':
        res = x.num + y.num;
        break;
      case '-':
        res = x.num - y.num;
        break;
      case '*':
        res = x.num * y.num;
        break;
      case '/':
        if (y.num == 0) {
          return lval_err(LERR_DIV_ZERO);
        } else {
          res = x.num / y.num;
        }
        break;
      case '%':
        res = x.num % y.num;
        break;
      case '^':
        res = pow(x.num, y.num);
        break;
      default:
        return lval_err(LERR_BAD_OP);
    }
  }

  /* Convert long to lval and return */
  return lval_num(res);
}

lval eval(mpc_ast_t* t) {
  /* If tagged as number return it directly */
  if (strstr(t->tag, "number")) {
    /* Check if there is some error in conversion */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  /* The operator is always the second child. */
  char* op = t->children[1]->contents;

  /* Store the thirld child */
  lval x = eval(t-> children[2]);

  /* Iterate the remaining children and add them to the result */
  for (int i = 3; strstr(t->children[i]->tag, "expr"); i++) {
    x = eval_op(x, op, eval(t->children[i]));
  }

  return x;
}


int main(int argc, char** argv) {
  /* Create Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Operator = mpc_new("operator");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");

  /* Define them within the following language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                    \
      number   : /-?[0-9]+/ ;                                            \
      operator : '+' | '-' | '*' | '/' | '%' | '^' | \"max\" | \"min\" ; \
      expr     : <number> | '(' <operator> <expr>+ ')' ;                 \
      lispy    : /^/ <operator> <expr>+ /$/ ;                            \
    ",
    Number, Operator, Expr, Lispy
  );

  /* Print Version and Exit Information */
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c or input :q to Exit\n");

  /* In a never ending loop */
  while (1) {
    /* Output our prompt and get input */
    char* input = readline("blisp> ");

    /* Add input to history */
    add_history(input);

    /* Check input for :q */
    if (!strncmp(input, ":q", 2)) {
      break;
    }

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* Evaluate input, print result, then cleanup */
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  /* Undefine and Delete Parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}

