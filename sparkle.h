#ifndef SPARKLE_H_INCLUDED
#define SPARKLE_H_INCLUDED

/*
 * sparkle.h
 * =========
 * 
 * Public interface of the main sparkle module.
 * 
 * See the documentation in the source file sparkle.c for further
 * information.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Constants
 * =========
 */

/*
 * The maximum length of an operator name, not including the terminating
 * nul.
 */
#define MAX_OP_NAME (255)

/*
 * Type constants for use with CELL structure.
 */
#define CELLTYPE_NULL     (0)
#define CELLTYPE_INTEGER  (1)
#define CELLTYPE_FLOAT    (2)
#define CELLTYPE_STRING   (3)

/*
 * Type declarations
 * =================
 */

/*
 * Function pointer type for operator implementation functions.
 * 
 * pModule is the name of the executable module, for use in any
 * diagnostic messages.  line_num is the current line number in the
 * script file, for use in diagnostic messages.
 * 
 * If there is a failure in the operator, the operator should print some
 * reasonable error message to standard error.
 * 
 * Use the interpreter stack functions provided in this header to get at
 * parameters necessary for functions, remove parameters from the stack,
 * and push any necessary results.
 * 
 * Parameters:
 * 
 *   pModule - the name of this executable
 * 
 *   line_num - the current line number
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
typedef int (*fp_op)(const char *pModule, long line_num);

/*
 * Prototype for the CELL structure.
 * 
 * The actual structure is defined in the implementation file.
 */
struct CELL_TAG;
typedef struct CELL_TAG CELL;

/*
 * Public functions
 * ================
 */

/*
 * Register an operator implementation.
 * 
 * pOpName is the case-sensitive name of the operator to register.  It
 * must be a string of one or more US-ASCII alphanumeric or underscore
 * characters.  The first character must be alphabetic.  The maximum
 * length is MAX_OP_NAME.
 * 
 * pFunc is the implementation function to register for this operator.
 * See the declaration of the fp_op type for further information.
 * 
 * If the operator name has already been declared or if there are too
 * many operators being registered, an error message will be printed and
 * then a fault will occur.
 * 
 * In order to add a module of operators, you must define a registration
 * function within that module that invokes this register_operator()
 * function to register each of the operators contained within that
 * module.  Then, within the sparkle.c implementation file, #include the
 * header of the operator module at the indicated place near the top of
 * the source file and invoke the module's registration function within
 * the register_modules() function.
 * 
 * Parameters:
 * 
 *   pOpName - the (case-sensitive) name of the operator to register
 * 
 *   pFunc - the implementation function of the operator
 */
void register_operator(const char *pOpName, fp_op pFunc);

/*
 * Return the type of value stored in the given cell.
 * 
 * The cell must already be initialized.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 * Return:
 * 
 *   one of the CELLTYPE_ constants
 */
int cell_type(const CELL *pc);

/*
 * Check whether a given cell has a value that can act as a float.
 * 
 * The cell must already be initialized.  This function returns non-zero
 * if the cell contains an integer or a float.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 * Return:
 * 
 *   non-zero if float-compatible value is stored, zero if not
 */
int cell_canfloat(const CELL *pc);

/*
 * Get the integer value stored in a cell.
 * 
 * The cell must be initialized and must store an integer value.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 * Return:
 * 
 *   the stored integer value
 */
int32_t cell_get_int(const CELL *pc);

/*
 * Get the float value stored in a cell.
 * 
 * The cell must be initialized and must store either an integer or a
 * float.  Integer values will be cast to float values, but the cell
 * will remain storing an integer type.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 * Return:
 * 
 *   the stored value as a float
 */
double cell_get_float(const CELL *pc);

/*
 * Get a pointer to the string value stored in a cell.
 * 
 * The cell must be initialized and must store a string.  The returned
 * pointer is valid until the cell is changed.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 * Return:
 * 
 *   the string value contained within
 */
const char *cell_string_ptr(const CELL *pc);

/*
 * Push an integer value on top of the interpreter stack.
 * 
 * The function fails if the interpreter stack is full.
 * 
 * Parameters:
 * 
 *   v - the integer value to push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack is full
 */
int stack_push_int(int32_t v);

/*
 * Push a float value on top of the interpreter stack.
 * 
 * The function fails if the interpreter stack is full.  A fault occurs
 * if the passed value is not finite.
 * 
 * Parameters:
 * 
 *   v - the float value to push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack is full
 */
int stack_push_float(double v);

/*
 * Push a string value on top of the interpreter stack.
 * 
 * The function fails if the interpreter stack is full.  A fault occurs
 * if the passed string contains anything other than visible, printing
 * US-ASCII characters and space.  A copy is made of the string.
 * 
 * Parameters:
 * 
 *   pstr - the string to copy and push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack is full
 */
int stack_push_string(const char *pstr);

/*
 * Return how many values are currently on the interpreter stack.
 * 
 * Return:
 * 
 *   the current height of the interpreter stack
 */
int32_t stack_count(void);

/*
 * Return a specific element on the stack.
 * 
 * i is the index of the element from the top of the stack.  A value of
 * zero means the element on top of the stack.  A value of one means the
 * value below it, and so forth.  i must be greater than or equal to
 * zero and less than the stack_count() or a fault occurs.
 * 
 * Parameters:
 * 
 *   i - the index of the element
 * 
 * Return:
 * 
 *   the requested stack element
 */
const CELL *stack_index(int32_t i);

/*
 * Remove elements from the top of the stack.
 * 
 * count is the number of elements to remove from the top of the stack.
 * This must be greater than or equal to zero and less than or equal to
 * stack_count() or a fault occurs.  If zero is passed, nothing is done.
 * 
 * Parameters:
 * 
 *   count - the number of elements to remove from the stack
 */
void stack_pop(int32_t count);

#endif
