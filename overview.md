# Overview of C
## Programs
- A program in C consists of _function definitions_ and _structure definitions_.
- A source file is, therefore, simply a list of _functions_ and _types_.
- The execution of a C program always starts in the `main` function. From here it abstracts away processes by calling other functions.
## Variables
- Items of data which we give a name to.
- Every variable in C has an explicit _type_. These types are declared by ourselves or built into the language.
- You declare a new variable by writing the name of its type, followed by its name, and optionally setting it to some value using `=`.
- Common built in types:
  - `void` - Empty Type
  - `char` - Single Character/Byte
  - `int` - integer
  - `long` - Integer that can hold larger values
  - `float` Decimal Number
  - `double` Decimal Number with more precision
## Function Declarations
- A function is a computation that manipulates variables. It can change the state of the program. It takes input and returns a single variable as output.
- You declare a function by writing the type of the return, the name of the function, and then in parenthesis its parameters.
- A return statement must be used to let the function finish and output a variable.
For example:
```c
int add_together (int x, int y) {
  int result = x + y;
  return result;
}
```
## Structure Declarations
- Structures are used to declare new _types_. Structures are several variables bundled into a single package.
- These are typically used to represent more complex data types.
- These definitions should be above any function that uses them.
Heres how you can create a `struct` that holds the values of a point in space:
```c
typedef struct {
  float x;
  float y;
} point;
```
To access an individual field you can use a dot `.`, followed by the name of the field:
```c
point p;
p.x = 0.1;
p.y = 10.0;

float length = sqrt(p.x * p.x. + p.y * p.y);
```
## Pointers
- A pointer is a variation of a normal type where the type name is suffixed with an asterisk. For example you can declare a pointer to an integer with `int* foo`.
- You can declare a pointer to a pointer with two asterisks, e.g., `char** bar` would declare a variable `bar` that points to a pointer that points to a `char`
## Strings
- Represented by the pointer type `char*`
- Stored as a list of characters, where the final character is a special character called the _null terminator_.
## Conditionals
- These are `if` statements, you know how these work. In C they look like this:
```c
if (x > 10 && x < 100) {
  puts ("x is greater than 10 and less than 100!");
} else {
  puts ("x is less than 11 or greater than 99!");
}
```
## Loops
- There are two main loops in C:
  - `while` loop - Executes a block of code until some condition becomes false
```c
int i = 10;
while (i > 0) {
  puts("Loop Iteration");
  i = i - 1;
}
```
  - `for` loop - Executes a block of code for until the condition check is true. Declares variable value internally.
```c
for (int i = 0; i < 10; 1++) {
  puts("Loop Iteration");
}
```

