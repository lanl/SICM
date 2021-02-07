# Programming Practices
1. All blocks use curly braces
   - Even one-line blocks
2. Constants on the left side of `==`
   - `if(NULL == foo) { ...`
3. Functions with no arguments are `(void)`
4. No C++-style comments in C code
5. No GCC extensions except in GCC-only code
6. No C++ code in libraries
   - Discouraged in components
7. Always define preprocessor macros
   - Define logicals to 0 or 1 (vs. define or not define)
   - Use `#if FOO`, not `#ifdef FOO`
