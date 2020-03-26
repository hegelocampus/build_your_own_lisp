# S-Expressions
## Lists and Lisps
- Lisps are famous for having little distinction between data and code. Both are represented using the same structure.
- In order to achieve this we will need to create an internal list structure that is built up recursively using numbers, symbols, and other lists.
  - This is known as an **S-Expression**, meaning _Symbolic Expression_.
  - This will be represented in our project using the `lval` structure
## Pointers
- Lists require pointers in C.
- When you call a function in C the arguments are always passed _by value_. This means a _copy_ of them is passed to the function you call.
  - This is true for `int`, `long`, `char`, and user-defined `struct` types.
  - A common problem than arises here is when we have a large struct containing many other sub-structs that need to be passed around. Each time you call a new function a new copy of that struct would be created. This can lead to a huge amount of data being used.
  - An additional problem is that the `struct` is always a fixed size. Because of this if you wanted to call a function with a mutable list of things you couldn't use a `struct`.
- A pointer is just a number that represents the starting index of a memory-address.
- You can declare a pointer through adding a `*` suffix to any existing data type. For example: `char*`, `int*`, `long*`
  - To create a pointer to some data, you need to get its index. You can do this using the _address of_ operator `&`
  - To actually get the data at an address of a pointer, known as _dereferencing_, you can use the `*` operator on the left-hand side of a variable (`*foo`). To get data at the field of a pointer to a struct you use the arrow (`foo->bar`).
## The Stack & The Heap
There are two general sections in memory: The Stack and The Heap
### The Stack
- The Stack is the memory where your program lives. It holds all the temporary variables and data structures that exist as you manipulate and edit them.
- Each time you call a function a new area of the stack is put aside for the function to use.
- Once the function has finished the area it used in the call stack is unallocated.
### The Heap
- The Heap is a section of memory put aside for storage of objects with a longer lifespan.
- **Memory is this area has to be manually allocated and deallocated**
  - To allocate new memory `malloc` is used. This function takes the number of bytes required, and returns a pointer to a new block of memory with that many bytes.
  - To deallocate used memory you must pass the memory pointer to the `free` function
- The Heap is trickier to work with than the Stack because it requires that you **always free used memory** when you are done with it. If you don't do this (or don't do it correctly) the program will continue to allocate more and more memory. This is called a _memory leak_.
  - A good rule of thumb is to have one `free` call for each `malloc` call.

