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
typedef struct lval {
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
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

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

/* Create a pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
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
    /* If Qexpr or Sexpr then delete all elements inside */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* Free the memory allocated to contain the pointers */
      free(v->cell);
      break;
  }

  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}

lval* lval_pop(lval* v, int i) {
  /* Find the item at "i" */
  lval* x = v->cell[i];

  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
      sizeof(lval*) * (v->count-i-1));

  /* Decrease the count of items in the list */
  v->count--;

  /* Reallocate the count of items in the list */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);

  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* builtin_op(lval* a, char* op) {
  /* Ensure all arguments are numbers */
  for (int i =0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }

  /* Pop the first element */
  lval* x = lval_pop(a, 0);

  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  /* While there are still elemets remaining */
  while (a->count > 0) {
    /* Pop the next ele */
    lval* y = lval_pop(a, 0);

    /* Handle special div case */
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division By Zero!");
        break;
      } else {
        x->num /= y->num;
      }
    }

    switch (op[0]) {
      case '+':
        x->num += y->num;
        break;
      case '-':
        x->num -= y->num;
        break;
      case '*':
        x->num *= y->num;
        break;
    }

    lval_del(y);
  }

  lval_del(a);
  return x;
}

/* Forward declaration */
lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
  /*Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
    /* Error Checking */
    if (v->cell[i] == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  /* Handle Empty Expression */
  if (v->count == 0) {
    return v;
  }

  /* Handle Single Experssion */
  if (v->count == 1) {
    return lval_take(v, 0);
  }

  /* Ensure First ele is Symbol */
  lval * f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_del(f);
    lval_del(v);
    return lval_err("S-expression Does not start with symbol!");
  } else {
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
  }
}

lval* lval_eval(lval* v) {
  /* Eval Sexpressions */
  return (v->type == LVAL_SEXPR) ? lval_eval_sexpr(v) : v;
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

lval* builtin_head(lval* a) {
  /* Check Error Conditions */
  if (a->count != 1) {
    lval_del(a);
    return lval_err("Function 'head' passed too many arguments!");
  } else if (a->cell[0]->type != LVAL_QEXPR) {
    lval_del(a);
    return lval_err("Function 'head' passed incorrect type!");
  } else if (a->cell[0]->count == 0) {
    lval_del(a);
    return lval_err("Function 'head' passed {}!");
  }

  /* Otherwise take first arg */
  lval* v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

lval* builtin_tail(lval* a) {
  /* Check Error Conditions */
  if (a->count != 1) {
    lval_del(a);
    return lval_err("Function 'head' passed too many arguments!");
  } else if (a->cell[0]->type != LVAL_QEXPR) {
    lval_del(a);
    return lval_err("Function 'head' passed incorrect type!");
  } else if (a->cell[0]->count == 0) {
    lval_del(a);
    return lval_err("Function 'head' passed {}!");
  }

  /* Take first arg */
  lval* v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
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
    if (strstr(t->tag, "qexpr")) {
      x = lval_qexpr();
    }
    if (strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) {
      x = lval_sexpr();
    }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
      if (strcmp(t->children[i]->contents, "(") == 0 ||
          strcmp(t->children[i]->contents, ")") == 0 ||
          strcmp(t->children[i]->contents, "{") == 0 ||
          strcmp(t->children[i]->contents, "}") == 0 ||
          strcmp(t->children[i]->tag, "regex")  == 0
      ) {
        continue;
      }
      x = lval_add(x, lval_read(t->children[i]));
    }
  }
  return x;
}

/* Forward declaration to resolve dependency */
void lval_print(lval* v);

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
  switch (v->type) {
    case LVAL_NUM:
      printf("%li", v->num);
      break;
    case LVAL_ERR:
      printf("Error: %s", v->err);
      break;
    case LVAL_SYM:
      printf("%s", v->sym);
      break;
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;
    case LVAL_QEXPR:
      lval_expr_print(v, '{', '}');
      break;
  }
}

void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

int main(int argc, char** argv) {
  /* Create Parsers */
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Qexpr  = mpc_new("qexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");

  /* Define them within the following language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number : /-?[0-9]+/ ;                              \
      symbol : \"list\" | \"head\" | \"tail\" | \"join\" \
             | \"eval\" | '+' | '-' | '*' | '/' ;        \
      sexpr  : '(' <expr>* ')' ;                         \
      qexpr  : '{' <expr>* '}' ;                         \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
      lispy  : /^/ <expr>* /$/ ;                         \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy
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
      lval* x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_del(x);
    } else {
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  /* Undefine and Delete Parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}

