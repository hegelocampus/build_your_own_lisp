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
  /* Error and Symbol types have some string data */
  char* err;
  char* sym;
  /* Count and Pointer to list of "lval*" */
  int count;
  struct lval** cell;
} lval;

/* Create enumeration of possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

/* Create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create a pointer to a new Number lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a pointer to a new error lval */
lval* lval_err(char* m) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* Create a pointer to a new Symbol lval */
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* Create a pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval* v) {
  switch (v->type) {
    /* Do nothing for number type */
    case LVAL_NUM: 
      break;
    /* For Err or Sym free the string data */
    case LVAL_ERR:
      free(v->err);
      break;
    case LVAL_SYM:
      free(v->sym);
      break;
    /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR:
      for (int i = 0; i < v-> count; i++) {
        lval_del(v->cell[i]);
      }
      break;
  }

  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_read(mpc_ast_t* t) {
  lval* x = NULL;
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) {
    x = lval_read_num(t);
  } else if (strstr(t->tag, "symbol")) {
    x = lval_sym(t->contents);
  } else {
    if (strcmp(t->tag, ">") == 0 || strcmp(t->tag, "sexpr")) {
      x = lval_sexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
      if (strcmp(t->children[i]->contents, "(")  == 0 
        || strcmp(t->children[i]->contents, "(") == 0
        || strcmp(t->children[i]->tag, "regex")  == 0) {
        continue;
      }
      x = lval_add(x, lval_read(t->children[i]));
    }
  }
  return x;
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);

    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

/* Print an lval */
void lval_print(lval* v) {
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v->num);
      break;
    case LVAL_ERR:
      printf("%s", v->err);
      break;
    case LVAL_SYM:
      printf("%s", v->sym);
      break;
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;
  }
}

void lval_println(lval* v) {
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
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");

  /* Define them within the following language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                    \
      number   : /-?[0-9]+/ ;                                            \
      operator : '+' | '-' | '*' | '/' | '%' | '^' | \"max\" | \"min\" ; \
      sexpr    : '(' <expr>* ')' ;                                       \
      expr     : <number> | <symbol> | <sexpr> ;                         \
      lispy    : /^/ <expr>* /$/ ;                            \
    ",
    Number, Symbol, Sexpr, Expr, Lispy
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
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}

