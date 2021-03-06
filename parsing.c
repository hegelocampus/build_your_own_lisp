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

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create enumeration of possible lval types */
enum {
  LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR
};

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare New lval (Lisp Value) Struct */
struct lval {
  int type;

  long num;
  /* Error and Symbol types have some string data */
  char* err;
  char* sym;
  lbuiltin fun;

  /* Pointer to list of "lval*" and length of pointers arr */
  struct lval** cell;
  int count;
};

struct lenv {
  int count;
  char** syms;
  lval** vals;
};

/* Create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

/* Create a pointer to a new Number lval */
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_fun(lbuiltin func) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
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
    case LVAL_FUN:
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

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
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

lval* builtin_op(lenv* e, lval* a, char* op) {
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

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

#define LASSERT(args, cond, err) \
  if (!(cond)) {                 \
    lval_del(args);              \
    return lval_err(err);        \
  }

lval* builtin_head(lenv* e, lval* a) {
  /* Check Error Conditions */
  LASSERT(a, a->count == 1,
      "Function 'head' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
      "Function 'head' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0,
      "Function 'head' passed {}!");

  /* Take first arg */
  lval* v = lval_take(a, 0);

  /* Delete all elements that are not head and return */
  while (v->count > 1) {
    lval_del(lval_pop(v, 1));
  }
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  /* Check Error Conditions */
  LASSERT(a, a->count == 1,
      "Function 'tail' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
      "Function 'tail' passed incorrect type!");
  LASSERT(a, a->cell[0]->count != 0,
      "Function 'tail' passed {}!");

  /* Take first arg */
  lval* v = lval_take(a, 0);

  /* Delete first element and return */
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  a->type = LVAL_QEXPR;
  return a;
}

/* Forward declaration */
lval* lval_eval(lenv* e, lval* v);

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
      "Function 'eval' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
      "Function 'eval' passed incorrect type!");

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {
  /* For each cell in 'y' add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  /* Delete the empty 'y' */
  lval_del(y);
  return x;
}

lval* builtin_join(lenv* e, lval* a) {
  for (int i = 0; i< a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
        "Function 'join' passed incorrect type!");
  }

  lval* x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin(lenv* e, lval* a, char* func) {
  lval* res;
  if (strcmp("list", func) == 0) {
    res = builtin_list(e, a);
  } else if (strcmp("head", func) == 0) {
    res = builtin_head(e, a);
  } else if (strcmp("tail", func) == 0) {
    res = builtin_tail(e, a);
  } else if (strcmp("join", func) == 0) {
    res = builtin_join(e, a);
  } else if (strcmp("eval", func) == 0) {
    res = builtin_eval(e, a);
  } else if (strstr("+-/*", func)) {
    res = builtin_op(e, a, func);
  } else {
    lval_del(a);
    res = lval_err("Unknown Function!");
  }
  return res;
}

lval* lval_eval(lenv* e, lval* v);

lval* lval_eval_sexpr(lenv* e, lval* v) {
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) {
      return lval_take(v, i);
    }
  }

  if (v->count ==0) {
    return v;
  } else if (v->count == 1) {
    return lval_take(v, 0);
  } else {
    /* Ensure first ele is a function after evaluation */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
      lval_del(v);
      lval_del(f);
      return lval_err("First element is not a function!");
    }

    /* Call function to get result */
    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
  }
}

lval* lenv_get(lenv* e, lval* v);

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  } else if (v->type == LVAL_SEXPR) {
    return lval_eval_sexpr(e, v);
  } else {
    return v;
  }
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

lval* lval_copy(lval* old) {
  lval* new = malloc(sizeof(lval));
  new->type = old->type;

  switch (old->type) {
    /* Copy Functions and Numbers Directly */
    case LVAL_FUN:
      new->fun = old->fun;
      break;
    case LVAL_NUM:
      new->num = old->num;
      break;
    /* Copy Strings using malloc and strcpy */
    case LVAL_ERR:
      new->err = malloc(strlen(old->sym) + 1);
      strcpy(new->err, old->err);
      break;
    case LVAL_SYM:
      new->sym = malloc(strlen(old->sym) + 1);
      strcpy(new->sym, old->sym);
      break;
    /* Copy Lists by coping each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
      new->count = old->count;
      new->cell = malloc(sizeof(lval*) * new->count);
      for (int i = 0; i< new->count; i++) {
        new->cell[i] = lval_copy(old->cell[i]);
      }
      break;
  }
  return new;
}

lval* lenv_get(lenv* e, lval* k) {
  /* Iterate over all items in environment */
  for (int i = 0; i < e->count; i++) {
    /* Check if the stored string matches the symbol string */
    /* If it does, return a copy of the value */
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  /* If no symbol found return error */
  return lval_err("unbound symbol!");
}

void lenv_put(lenv* e, lval* var_name, lval* new_val) {
  /* Iterate over all items in environment */
  /* This is to see if variable already exists */
  for (int i = 0; i < e->count; i++) {
    /* If the variable is found, delete item at that position */
    if (strcmp(e->syms[i], var_name->sym) == 0) {
      lval_del(e->vals[i]);
      /* Then replate with variable supplied by user */
      e->vals[i] = lval_copy(new_val);
      return;
    }
  }

  /* If no existing entry is found allocate space for a new entry */
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  /* Copy contents of lval and symbol string into new location */
  e->vals[e->count-1] = lval_copy(new_val);
  e->syms[e->count-1] = malloc(strlen(var_name->sym)+1);
  strcpy(e->syms[e->count-1], var_name->sym);
}

lval* builtin_def(lenv* e, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
      "Function 'def' passed incorrect type!");

  /* First argument is symbol list */
  lval* syms = a->cell[0];

  /* Ensure all elements of first list are symbols */
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
        "Function 'def' cannot define non-symbol");
  }

  /* Check correct number of symbols and values */
  LASSERT(a, syms->count == a->count-1,
      "Function 'def' cannot define incorrect "
      "number of values to symbols");

  /* Assign copies of values to symbols */
  for (int i = 0; i< syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* List functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "++", builtin_join);

  /* Variable Functions */
  lenv_add_builtin(e, "muta", builtin_def);

  /* Math functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
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
    case LVAL_FUN:
      printf("<function>");
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
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;        \
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

  /* Create variables and add core functions */
  lenv* e = lenv_new();
  lenv_add_builtins(e);

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
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);

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
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  /* Clear stored variables */
  lenv_del(e);

  return 0;
}

