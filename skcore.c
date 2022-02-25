/*
 * skcore.c
 * ========
 * 
 * Implementation of skcore.h
 * 
 * See the header for further information.
 */

#include "sparkle.h"
#include <stdio.h>

/*
 * Operator functions
 * ==================
 */

/*
 * [message : string] print -
 */
static int op_print(const char *pModule, long line_num) {
  
  int status = 1;
  
  /* Check at least one parameter on stack */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on print!\n",
      pModule, line_num);
  }
  
  /* Check parameter type */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_STRING) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] print expecting string!\n",
        pModule, line_num);
    }
  }
  
  /* Print message */
  if (status) {
    fprintf(stderr, "%s: [Script at line %ld] %s\n",
      pModule, line_num, cell_string_ptr(stack_index(0)));
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * Registration function
 * =====================
 */

void skcore_register(void) {
  register_operator("print", &op_print);
}
