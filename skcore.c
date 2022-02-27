/*
 * skcore.c
 * ========
 * 
 * Implementation of skcore.h
 * 
 * See the header for further information.
 */

#include "skcore.h"

#include <stdio.h>

#include "sparkle.h"
#include "skvm.h"

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
 * [i] [w] [h] [c] reset -
 */
static int op_reset(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  int32_t w = 0;
  int32_t h = 0;
  int32_t c = 0;
  
  /* Check at least four parameters on stack */
  if (stack_count() < 4) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on reset!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(3)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_INTEGER)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Wrong param types for reset!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(3));
    w = cell_get_int(stack_index(2));
    h = cell_get_int(stack_index(1));
    c = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check dimensions */
  if (status) {
    if ((w < 1) || (w > SKVM_MAX_DIM) ||
        (h < 1) || (h > SKVM_MAX_DIM)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Dimensions out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check channels */
  if (status) {
    if ((c != 1) && (c != 3) && (c != 4)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Invalid channel count!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_reset(i, w, h, c);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(4);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] load_png -
 */
static int op_load_png(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  const char *pPath = NULL;
  
  /* Check at least two parameters on stack */
  if (stack_count() < 2) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on load_png!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_STRING)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for load_png!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(1));
    pPath = cell_string_ptr(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_load_png(i, pPath)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] load_png fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(2);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] load_jpeg -
 */
static int op_load_jpeg(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  const char *pPath = NULL;
  
  /* Check at least two parameters on stack */
  if (stack_count() < 2) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on load_jpeg!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_STRING)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for load_jpeg!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(1));
    pPath = cell_string_ptr(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_load_jpeg(i, pPath)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] load_jpeg fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(2);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] load_frame -
 */
static int op_load_frame(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  int32_t f = 0;
  const char *pPath = NULL;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on load_frame!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_STRING)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for load_frame!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(2));
    f = cell_get_int(stack_index(1));
    pPath = cell_string_ptr(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_load_mjpg(i, f, pPath)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] load_frame fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [a] [r] [g] [b] fill -
 */
static int op_fill(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  int32_t a = 0;
  int32_t r = 0;
  int32_t g = 0;
  int32_t b = 0;
  
  /* Check at least five parameters on stack */
  if (stack_count() < 5) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on fill!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(4)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(3)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_INTEGER)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Wrong param types for fill!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(4));
    a = cell_get_int(stack_index(3));
    r = cell_get_int(stack_index(2));
    g = cell_get_int(stack_index(1));
    b = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check channel ranges */
  if (status) {
    if ((a < 0) || (a > 255) ||
        (r < 0) || (r > 255) ||
        (g < 0) || (g > 255) ||
        (b < 0) || (b > 255)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Channel values out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_load_fill(i, (int) a, (int) r, (int) g, (int) b);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(5);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] store_png -
 */
static int op_store_png(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  const char *pPath = NULL;
  
  /* Check at least two parameters on stack */
  if (stack_count() < 2) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on store_png!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_STRING)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for store_png!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(1));
    pPath = cell_string_ptr(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_store_png(i, pPath)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] store_png fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(2);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] [q] store_jpeg -
 */
static int op_store_jpeg(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  const char *pPath = NULL;
  int32_t q = 0;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on store_jpeg!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_STRING) ||
        (cell_type(stack_index(0)) != CELLTYPE_INTEGER)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for store_jpeg!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(2));
    pPath = cell_string_ptr(stack_index(1));
    q = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Clamp quality */
  if (status) {
    if (q < 0) {
      q = 0;
    } else if (q > 100) {
      q = 100;
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_store_jpeg(i, pPath, 0, q)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] store_jpeg fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [path] [q] store_mjpg -
 */
static int op_store_mjpg(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t i = 0;
  const char *pPath = NULL;
  int32_t q = 0;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on store_mjpg!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_STRING) ||
        (cell_type(stack_index(0)) != CELLTYPE_INTEGER)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for store_mjpg!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(2));
    pPath = cell_string_ptr(stack_index(1));
    q = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Clamp quality */
  if (status) {
    if (q < 0) {
      q = 0;
    } else if (q > 100) {
      q = 100;
    }
  }
  
  /* Perform operation */
  if (status) {
    if (!skvm_store_jpeg(i, pPath, 1, q)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] store_mjpg fail: %s\n",
        pModule, line_num,
        skvm_reason());
    }
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] identity -
 */
static int op_identity(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t m = 0;
  
  /* Check at least one parameter on stack */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on identity!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for identity!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    m = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((m < 0) || (m >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_matrix_reset(m);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] [a] [b] multiply -
 */
static int op_multiply(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t m = 0;
  int32_t a = 0;
  int32_t b = 0;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on multiply!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (cell_type(stack_index(0)) != CELLTYPE_INTEGER)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for multiply!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    m = cell_get_int(stack_index(2));
    a = cell_get_int(stack_index(1));
    b = cell_get_int(stack_index(0));
  }
  
  /* Check register ranges */
  if (status) {
    if ((m < 0) || (m >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  if (status) {
    if ((a < 0) || (a >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  if (status) {
    if ((b < 0) || (b >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check that result matrix is unique */
  if (status) {
    if ((m == a) || (m == b)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Result matrix may not overlap operands!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_matrix_multiply(m, a, b);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] [tx] [ty] translate -
 */
static int op_translate(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t m = 0;
  double tx = 0.0;
  double ty = 0.0;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on translate!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (!cell_canfloat(stack_index(1))) ||
        (!cell_canfloat(stack_index(0)))) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for translate!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    m = cell_get_int(stack_index(2));
    tx = cell_get_float(stack_index(1));
    ty = cell_get_float(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((m < 0) || (m >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_matrix_translate(m, tx, ty);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] [sx] [sy] scale -
 */
static int op_scale(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t m = 0;
  double sx = 0.0;
  double sy = 0.0;
  
  /* Check at least three parameters on stack */
  if (stack_count() < 3) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on scale!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(2)) != CELLTYPE_INTEGER) ||
        (!cell_canfloat(stack_index(1))) ||
        (!cell_canfloat(stack_index(0)))) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for scale!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    m = cell_get_int(stack_index(2));
    sx = cell_get_float(stack_index(1));
    sy = cell_get_float(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((m < 0) || (m >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check that scaling values are non-zero */
  if (status && ((sx == 0.0) || (sy == 0.0))) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Scaling value may not be zero!\n",
      pModule, line_num);
  }
  
  /* Perform operation */
  if (status) {
    skvm_matrix_scale(m, sx, sy);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(3);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] [deg] rotate -
 */
static int op_rotate(const char *pModule, long line_num) {
  
  int status = 1;
  
  int32_t m = 0;
  double deg = 0.0;
  
  /* Check at least two parameters on stack */
  if (stack_count() < 2) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on rotate!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if ((cell_type(stack_index(1)) != CELLTYPE_INTEGER) ||
        (!cell_canfloat(stack_index(0)))) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for rotate!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    m = cell_get_int(stack_index(1));
    deg = cell_get_float(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((m < 0) || (m >= skvm_matc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Matrix index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_matrix_rotate(m, deg);
  }
  
  /* Remove arguments from stack */
  if (status) {
    stack_pop(2);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] color_invert -
 */
static int op_color_invert(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t i = 0;
  
  /* Check at least one parameter on stack */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Stack underflow on invert!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong param types for invert!\n",
        pModule, line_num);
    }
  }
  
  /* Get the parameters */
  if (status) {
    i = cell_get_int(stack_index(0));
  }
  
  /* Check register range */
  if (status) {
    if ((i < 0) || (i >= skvm_bufc())) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register index out of range!\n",
        pModule, line_num);
    }
  }
  
  /* Check that register is loaded */
  if (status) {
    if (!skvm_is_loaded(i)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Register is not loaded!\n",
        pModule, line_num);
    }
  }
  
  /* Perform operation */
  if (status) {
    skvm_color_invert(i);
  }
  
  /* Remove arguments from stack */
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
  /* Diagnostic ops */
  register_operator("print", &op_print);
  
  /* Load/store ops */
  register_operator("reset", &op_reset);
  register_operator("load_png", &op_load_png);
  register_operator("load_jpeg", &op_load_jpeg);
  register_operator("load_frame", &op_load_frame);
  register_operator("fill", &op_fill);
  register_operator("store_png", &op_store_png);
  register_operator("store_jpeg", &op_store_jpeg);
  register_operator("store_mjpg", &op_store_mjpg);
  
  /* Matrix ops */
  register_operator("identity", &op_identity);
  register_operator("multiply", &op_multiply);
  register_operator("translate", &op_translate);
  register_operator("scale", &op_scale);
  register_operator("rotate", &op_rotate);
  
  /* Color ops */
  register_operator("color_invert", &op_color_invert);
}
