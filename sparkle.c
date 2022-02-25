/*
 * sparkle.c
 * =========
 * 
 * Main program for the Sparkle renderer.
 * 
 * This module interprets the Shastina script and dispatches the calls
 * to the skvm module to do the actual rendering.
 * 
 * The Shastina script is read from standard input.  Error and log
 * messages written to standard error.  Standard output is unused.
 * 
 * Module registration:
 * 
 * The actual handlers for the different operators in the script are not
 * defined in this module.  Instead, operators are defined in different
 * modules and then registered with this module.  The operator
 * implementations can make use of the skvm module to perform the
 * rendering, and they can call the public interface of this main module
 * defined in sparkle.h to interact with the script interpreter.
 * 
 * To register an operator module, include its header in the module
 * include list near the top of this source file.  Then, invoke its
 * registration function in the register_modules() function within this
 * module (see the documentation there for further information).  The
 * registration function should use the register_operator() function
 * defined by this module to register each of its operators.  Then, the
 * operators will be invoked when encountered during interpretation.
 * 
 * Compilation:
 * 
 *   - Module(s) defining operators
 *   - Recommended: 64-bit file mode with _FILE_OFFSET_BITS=64
 *   - May require the math library -lm on some platforms
 *   - Requires the skvm.c module
 *   - Requires librfdict beta 0.3.0 or compatible
 *   - Requires libshastina beta 0.9.3 or compatible
 *   - Requires libsophistry
 *   - Requires libsophistry-jpeg
 *   - Depends on libjpeg 6B or compatible (via libsophistry-jpeg)
 *   - Depends on libpng (via libsophistry)
 *   - Indirectly depends on zlib (via libpng)
 */

#include "sparkle.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "skvm.h"

#include "rfdict.h"
#include "shastina.h"

/*
 * Operator modules
 * ================
 * 
 * #include headers of operator modules here.
 */

#include "skcore.h"

/*
 * Constants
 * =========
 */

/*
 * The maximum interpreter stack height.
 */
#define STACK_HEIGHT (32)

/*
 * The maximum number of operators that may be registered.
 */
#define MAX_OPERATORS (1024)

/*
 * The maximum length of literal strings within the scripts, not
 * including terminating nul.
 */
#define MAX_STRING_LEN (255)

/*
 * Header state constants, used when parsing the header.
 */
#define HEADSTATE_INITIAL   (0) /* Initial state */
#define HEADSTATE_BEGIN     (1) /* Just read BEGIN_META */
#define HEADSTATE_BUFC      (2) /* Just read %bufcount */
#define HEADSTATE_MATC      (3) /* Just read %matcount */
#define HEADSTATE_SET       (4) /* Just read arg, expecting END_META */

/*
 * Type declarations
 * =================
 */

/*
 * CELL structure that can store any of the interpreted types.
 * 
 * Use the cell_ functions to manipulate.
 * 
 * Prototype defined in the header file.
 */
struct CELL_TAG {
  
  /*
   * The type stored in this cell.
   * 
   * This is one of the CELLTYPE_ constants.
   */
  uint8_t ctype;
  
  /*
   * Union storing the specific data, depending on the type selected by
   * ctype.
   * 
   * For NULL type, this stores an integer value set to zero.
   */
  union {
    int32_t   i;
    double    d;
    char    * pstr;
  } val;
  
};

/*
 * Static data
 * ===========
 */

/*
 * The name of the executable module, for use in diagnostic messages.
 * 
 * This is set at the start of the program entrypoint.
 */
static const char *pModule = NULL;

/*
 * The interpreter stack.
 * 
 * m_stack_init tracks whether this has been initialized yet.
 * 
 * If initialized, m_stack_count is how many items are currently pushed
 * onto the stack, which is in range [0, STACK_HEIGHT].
 * 
 * m_stack stores the actual stack.  When the stack is not empty, the
 * top element of the stack is (m_stack_count - 1).  Unused cells are
 * set to the special "NULL" type.
 * 
 * Use the stack_ functions to manipulate.
 */
static int m_stack_init = 0;
static int32_t m_stack_count;
static CELL m_stack[STACK_HEIGHT];

/*
 * The operator dispatch table.
 * 
 * m_op_init tracks whether this has been initialized yet.
 * 
 * If initialized, then the m_op table stores function pointers to each
 * of the operator implementation functions, m_op_count stores how many
 * operators have been registered, and m_op_map is a mapping of operator
 * names to indices within the m_op table.  Operator names are case
 * sensitive.
 */
static int m_op_init = 0;
static int32_t m_op_count;
static RFDICT *m_op_map;
static fp_op m_op[MAX_OPERATORS];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int parseInt(const char *pstr, int32_t *pv);

static void cell_init(CELL *pc);
static void cell_clear(CELL *pc);

static void cell_set_int(CELL *pc, int32_t v);
static void cell_set_float(CELL *pc, double v);
static void cell_set_string(CELL *pc, const char *pstr);

static void stack_init(void);
static void op_init(void);
static int op_invoke(const char *pOpName, long line_num);

/*
 * Parse the given string as a signed integer.
 * 
 * pstr is the string to parse.
 * 
 * pv points to the integer value to use to return the parsed numeric
 * value if the function is successful.
 * 
 * In two's complement, this function will not successfully parse the
 * least negative value.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to the return numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int parseInt(const char *pstr, int32_t *pv) {
  
  int negflag = 0;
  int32_t result = 0;
  int status = 1;
  int32_t d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* If first character is a sign character, set negflag appropriately
   * and skip it */
  if (*pstr == '+') {
    negflag = 0;
    pstr++;
  } else if (*pstr == '-') {
    negflag = 1;
    pstr++;
  } else {
    negflag = 0;
  }
  
  /* Make sure we have at least one digit */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
    
      /* Make sure in range of digits */
      if ((*pstr < '0') || (*pstr > '9')) {
        status = 0;
      }
    
      /* Get numeric value of digit */
      if (status) {
        d = (int32_t) (*pstr - '0');
      }
      
      /* Multiply result by 10, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result = result * 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add in digit value, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - d) {
          result = result + d;
        } else {
          status = 0; /* overflow */
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Invert result if negative mode */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Write result if successful */
  if (status) {
    *pv = result;
  }
  
  /* Return status */
  return status;
}

/*
 * Initialize a CELL to a NULL type.
 * 
 * CAUTION:  Do not use this on a cell that has already been
 * initialized, or a memory leak may occur!
 * 
 * Parameters:
 * 
 *   pc - the uninitialized cell
 */
static void cell_init(CELL *pc) {
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* Initialize */
  memset(pc, 0, sizeof(CELL));
  pc->ctype = CELLTYPE_NULL;
  (pc->val).i = 0;
}

/*
 * Clear a CELL back to a NULL type.
 * 
 * The cell must already be initialized.  To clear an uninitialized
 * cell, use cell_init() instead.
 * 
 * This function must be called on CELL structures before they are
 * released or else a memory leak may occur.
 * 
 * Parameters:
 * 
 *   pc - the cell to clear
 */
static void cell_clear(CELL *pc) {
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* If it's a string, release the string */
  if (pc->ctype == CELLTYPE_STRING) {
    free((pc->val).pstr);
  }
  
  /* Clear to NULL */
  memset(pc, 0, sizeof(CELL));
  pc->ctype = CELLTYPE_NULL;
  (pc->val).i = 0;
}

/*
 * Store an integer value in a cell, overwriting anything that is there.
 * 
 * The cell must already be initialized.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 *   v - the integer value
 */
static void cell_set_int(CELL *pc, int32_t v) {
  
  /* Check parameters */
  if (pc == NULL) {
    abort();
  }
  
  /* Clear cell */
  cell_clear(pc);
  
  /* Store integer */
  pc->ctype = CELLTYPE_INTEGER;
  (pc->val).i = v;
}

/*
 * Store a float value in a cell, overwriting anything that is there.
 * 
 * The cell must already be initialized.  The float value must be
 * finite.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 *   v - the float value
 */
static void cell_set_float(CELL *pc, double v) {
  
  /* Check parameters */
  if ((pc == NULL) || (!isfinite(v))) {
    abort();
  }
  
  /* Clear cell */
  cell_clear(pc);
  
  /* Store float */
  pc->ctype = CELLTYPE_FLOAT;
  (pc->val).d = v;
}

/*
 * Store a string value in a cell, overwriting anything that is there.
 * 
 * The cell must already be initialized.  A copy is made of the string,
 * so the given string is not referred to again after being passed to
 * this function.  No escaping is performed on the string.
 * 
 * The string is verified to contain only US-ASCII visible printing
 * characters and space.
 * 
 * Parameters:
 * 
 *   pc - the cell
 * 
 *   pstr - the string to copy
 */
static void cell_set_string(CELL *pc, const char *pstr) {
  
  const char *ps = NULL;
  
  /* Check parameters */
  if ((pc == NULL) || (pstr == NULL)) {
    abort();
  }
  for(ps = pstr; *ps != 0; ps++) {
    if ((*ps < 0x20) || (*ps > 0x7e)) {
      abort();
    }
  }
  
  /* Clear cell */
  cell_clear(pc);
  
  /* Make a dynamic copy of the string in the cell */
  pc->ctype = CELLTYPE_STRING;
  (pc->val).pstr = (char *) malloc(strlen(pstr) + 1);
  if ((pc->val).pstr == NULL) {
    abort();
  }
  strcpy((pc->val).pstr, pstr);
}

/*
 * Initialize the interpreter stack if not already initialized.
 * 
 * The other stack functions will call this automatically.
 */
static void stack_init(void) {
  
  int32_t i = 0;
  
  if (!m_stack_init) {
  
    m_stack_count = 0;
    memset(m_stack, 0, ((size_t) STACK_HEIGHT) * sizeof(CELL));
    for(i = 0; i < STACK_HEIGHT; i++) {
      cell_init(&(m_stack[i]));
    }
    m_stack_init = 1;
  }
}

/*
 * Initialize the operators data if not already initialized.
 * 
 * This is called automatically at the start of operator table
 * functions.
 */
static void op_init(void) {
  
  int32_t i = 0;
  
  if (!m_op_init) {
    
    m_op_count = 0;
    m_op_map = rfdict_alloc(1);
    memset(m_op, 0, ((size_t) MAX_OPERATORS) * sizeof(fp_op));
    for(i = 0; i < MAX_OPERATORS; i++) {
      m_op[i] = NULL;
    }
    m_op_init = 1;
  }
}

/*
 * Use the operator registration table to invoke a named operator.
 * 
 * pOpName is the name of the operator to invoke.  line_num is the line
 * number in the script, used for diagnostic messages, which is also
 * passed through to the operator implementation.
 * 
 * If an error occurs, printing the error message will be handled by
 * this function and should also be handled by invoked operator
 * functions.
 * 
 * Calling an operator name that hasn't been registered is treated as an
 * error.
 * 
 * Parameters:
 * 
 *   pOpName - the operator name
 * 
 *   line_num - the line number in the script
 * 
 * Return:
 * 
 *   non-zero if successful, zero if operator invocation failed or
 *   operator failed
 */
static int op_invoke(const char *pOpName, long line_num) {
  
  int status = 1;
  long oi = 0;
  
  /* Initialize operator table if necessary */
  op_init();
  
  /* Check parameters */
  if (pOpName == NULL) {
    abort();
  }
  
  /* Look up the operator function index */
  oi = rfdict_get(m_op_map, pOpName, -1);
  if (oi < 0) {
    status = 0;
    fprintf(stderr, "%s: [Line %ld] Unknown operator: %s!\n",
            pModule, line_num, pOpName);
  }
  
  /* Dispatch to operator function */
  if (status) {
    if (!(m_op[oi](pModule, line_num))) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Operator %s failed!\n",
        pModule, line_num, pOpName);
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Public functions
 * ================
 * 
 * Specifications given in header.
 */

/*
 * register_operator function.
 */
void register_operator(const char *pOpName, fp_op pFunc) {
  
  const char *pc = NULL;
  
  /* Initialize operator table if necessary */
  op_init();
  
  /* Check parameters */
  if ((pOpName == NULL) || (pFunc == NULL)) {
    abort();
  }
  if (strlen(pOpName) > MAX_OP_NAME) {
    abort();
  }
  if (((pOpName[0] < 'A') || (pOpName[0] > 'Z')) &&
      ((pOpName[0] < 'a') || (pOpName[0] > 'z'))) {
    abort();
  }
  for(pc = pOpName; *pc != 0; pc++) {
    if (((*pc < 'A') || (*pc > 'Z')) &&
        ((*pc < 'a') || (*pc > 'z')) &&
        ((*pc < '0') || (*pc > '9')) &&
        (*pc != '_')) {
      abort();
    }
  }
  
  /* Check if we have space remaining */
  if (m_op_count >= MAX_OPERATORS) {
    fprintf(stderr,
      "%s: [Initialization] Too many operator registrations!\n",
      pModule);
    abort();
  }
  
  /* Insert a mapping from this operator name to the new index in the
   * operator table */
  if (!rfdict_insert(m_op_map, pOpName, (long) m_op_count)) {
    fprintf(stderr,
      "%s: [Initialization] Operator name %s registered twice!\n",
      pModule, pOpName);
    abort();
  }
  
  /* Add the function pointer to the table */
  m_op[m_op_count] = pFunc;
  m_op_count++;
}

/*
 * cell_type function.
 */
int cell_type(const CELL *pc) {
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* Return value */
  return pc->ctype;
}

/*
 * cell_canfloat function.
 */
int cell_canfloat(const CELL *pc) {
  
  int result = 0;
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* Determine result */
  if ((pc->ctype == CELLTYPE_INTEGER) ||
        (pc->ctype == CELLTYPE_FLOAT)) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * cell_get_int function.
 */
int32_t cell_get_int(const CELL *pc) {
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  if (pc->ctype != CELLTYPE_INTEGER) {
    abort();
  }
  
  /* Return integer value */
  return (pc->val).i;
}

/*
 * cell_get_float function.
 */
double cell_get_float(const CELL *pc) {
  
  double v = 0.0;
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  
  /* Retrieve float value */
  if (pc->ctype == CELLTYPE_FLOAT) {
    v = (pc->val).d;
  
  } else if (pc->ctype == CELLTYPE_INTEGER) {
    v = (double) ((pc->val).i);
    
  } else {
    /* Wrong type stored in cell */
    abort();
  }
  
  /* Return float value */
  return v;
}

/*
 * cell_string_ptr function.
 */
const char *cell_string_ptr(const CELL *pc) {
  
  /* Check parameter */
  if (pc == NULL) {
    abort();
  }
  if (pc->ctype != CELLTYPE_STRING) {
    abort();
  }
  
  /* Return pointer */
  return (pc->val).pstr;
}

/*
 * stack_push_int function.
 */
int stack_push_int(int32_t v) {
  
  int status = 1;
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Fail if stack is full */
  if (m_stack_count >= STACK_HEIGHT) {
    status = 1;
  }
  
  /* Push the integer value */
  if (status) {
    cell_set_int(&(m_stack[m_stack_count]), v);
    m_stack_count++;
  }
  
  /* Return status */
  return status;
}

/*
 * stack_push_float function.
 */
int stack_push_float(double v) {
  
  int status = 1;
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Fail if stack is full */
  if (m_stack_count >= STACK_HEIGHT) {
    status = 1;
  }
  
  /* Push the float value */
  if (status) {
    cell_set_float(&(m_stack[m_stack_count]), v);
    m_stack_count++;
  }
  
  /* Return status */
  return status;
}

/*
 * stack_push_string function.
 */
int stack_push_string(const char *pstr) {
  
  int status = 1;
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Fail if stack is full */
  if (m_stack_count >= STACK_HEIGHT) {
    status = 1;
  }
  
  /* Push the string value */
  if (status) {
    cell_set_string(&(m_stack[m_stack_count]), pstr);
    m_stack_count++;
  }
  
  /* Return status */
  return status;
}

/*
 * stack_count function.
 */
int32_t stack_count(void) {
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Return result */
  return m_stack_count;
}

/*
 * stack_index function.
 */
const CELL *stack_index(int32_t i) {
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Check parameter */
  if ((i < 0) || (i >= m_stack_count)) {
    abort();
  }
  
  /* Return the requested element */
  return &(m_stack[m_stack_count - 1 - i]);
}

/*
 * stack_pop function.
 */
void stack_pop(int32_t count) {
  
  int i = 0;
  
  /* Initialize stack if necessary */
  stack_init();
  
  /* Check parameter */
  if ((count < 0) || (count > m_stack_count)) {
    abort();
  }
  
  /* Only proceed if count is greater than zero */
  if (count > 0) {
  
    /* Clear elements from the top of the stack */
    for(i = 0; i < count; i++) {
      cell_clear(&(m_stack[m_stack_count - 1 - i]));
    }
  
    /* Reduce the stack height */
    m_stack_count -= count;
  }
}

/*
 * Operator module registration
 * ============================
 */

static void register_modules(void) {
  /* Call the operator module registration functions here */
  skcore_register();
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  
  int read_signature = 0;
  int head_state = HEADSTATE_INITIAL;
  int32_t config_value = 0;
  int32_t bufc_value = -1;
  int32_t matc_value = -1;
  
  SNSOURCE *pin = NULL;
  SNPARSER *ps = NULL;
  
  SNENTITY ent;
  
  int has_float = 0;
  double dv = 0.0;
  char *endptr = NULL;
  int32_t iv = 0;
  
  int sbuf_len = 0;
  const char *pc = NULL;
  char sbuf[MAX_STRING_LEN + 1];
  
  /* Initialize structures */
  memset(&ent, 0, sizeof(SNENTITY));
  memset(sbuf, 0, MAX_STRING_LEN + 1);
  
  /* Set module name */
  pModule = NULL;
  if ((argc > 0) && (argv != NULL)) {
    pModule = argv[0];
  }
  if (pModule == NULL) {
    pModule = "sparkle";
  }
  
  /* No arguments expected */
  if (argc > 1) {
    status = 0;
    fprintf(stderr, "%s: Not expecting arguments!\n", pModule);
  }
  
  /* Wrap standard input in a Shastina source and allocate a parser */
  if (status) {
    pin = snsource_stream(stdin, SNSTREAM_NORMAL);
    ps = snparser_alloc();
  }
  
  /* ------------------------- */
  /*                           */
  /* HEADER AND INITIALIZATION */
  /*                           */
  /* ------------------------- */
  
  /* Parse the header */
  if (status) {
    for(
        snparser_read(ps, &ent, pin);
        (ent.status == SNENTITY_BEGIN_META) ||
          (ent.status == SNENTITY_END_META) ||
          (ent.status == SNENTITY_META_TOKEN);
        snparser_read(ps, &ent, pin)) {
      
      /* Handling depends on state */
      if (head_state == HEADSTATE_INITIAL) {
        /* We are in initial state or we just read END_META, so now
         * since we are still in the header we expect BEGIN_META */
        if (ent.status != SNENTITY_BEGIN_META) {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Header syntax error!\n",
                    pModule,
                    snparser_count(ps));
        }
        
        /* Change state to BEGIN */
        if (status) {
          head_state = HEADSTATE_BEGIN;
        }
        
      } else if (head_state == HEADSTATE_BEGIN) {
        /* We just began a header metacommand, so now we expect a
         * token */
        if (ent.status != SNENTITY_META_TOKEN) {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Header syntax error!\n",
                    pModule,
                    snparser_count(ps));
        }
        
        /* Set the next state depending on which token was read */
        if (status) {
          if (strcmp(ent.pKey, "sparkle") == 0) {
            if (!read_signature) {
              read_signature = 1;
              head_state = HEADSTATE_SET;
            } else {
              status = 0;
              fprintf(stderr,
                "%s: [Line %ld] Multiple %%sparkle; signatures!",
                pModule,
                snparser_count(ps));
            }
          
          } else if (strcmp(ent.pKey, "bufcount") == 0) {
            if (read_signature) {
              head_state = HEADSTATE_BUFC;
            } else {
              status = 0;
              fprintf(stderr,
                "%s: Failed to read %%sparkle; signature!\n",
                pModule);
            }
            
          } else if (strcmp(ent.pKey, "matcount") == 0) {
            if (read_signature) {
              head_state = HEADSTATE_MATC;
            } else {
              status = 0;
              fprintf(stderr,
                "%s: Failed to read %%sparkle; signature!\n",
                pModule);
            }
            
          } else {
            /* Unrecognized token */
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Unrecognized header token: %s!\n",
              pModule,
              snparser_count(ps),
              ent.pKey);
          }
        }
        
      } else if ((head_state == HEADSTATE_BUFC) ||
                  (head_state == HEADSTATE_MATC)) {
        /* We are now ready to read the token value */
        if (ent.status != SNENTITY_META_TOKEN) {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Header syntax error!\n",
                    pModule,
                    snparser_count(ps));
        }
        
        /* Parse the token value as an integer */
        if (status) {
          if (!parseInt(ent.pKey, &config_value)) {
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Failed to parse as integer: %s!\n",
              pModule,
              snparser_count(ps),
              ent.pKey);
          }
        }
        
        /* Check the range of the token value */
        if (status) {
          if (config_value < 0) {
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Header value may not be negative: %s!\n",
              pModule,
              snparser_count(ps),
              ent.pKey);
          }
        }
        if (status) {
          if (head_state == HEADSTATE_BUFC) {
            if (config_value > SKVM_MAX_BUFC) {
              status = 0;
              fprintf(stderr,
                "%s: Maximum value for %%bufcount is %ld!\n",
                pModule,
                (long) SKVM_MAX_BUFC);
            }
            
          } else if (head_state == HEADSTATE_MATC) {
            if (config_value > SKVM_MAX_MATC) {
              status = 0;
              fprintf(stderr,
                "%s: Maximum value for %%matcount is %ld!\n",
                pModule,
                (long) SKVM_MAX_MATC);
            }
            
          } else {
            /* Shouldn't happen */
            abort();
          }
        }
        
        /* Write the configuration value, checking that it hasn't
         * already been set */
        if (status) {
          if (head_state == HEADSTATE_BUFC) {
            if (bufc_value < 0) {
              bufc_value = config_value;
            } else {
              status = 0;
              fprintf(stderr,
                "%s: [Line %ld] %%bufcount already set!\n",
                pModule,
                snparser_count(ps));
            }
            
          } else if (head_state == HEADSTATE_MATC) {
            if (matc_value < 0) {
              matc_value = config_value;
            } else {
              status = 0;
              fprintf(stderr,
                "%s: [Line %ld] %%matcount already set!\n",
                pModule,
                snparser_count(ps));
            }
            
          } else {
            /* Shouldn't happen */
            abort();
          }
        }
        
        /* Proceed to next status */
        if (status) {
          head_state = HEADSTATE_SET;
        }
        
      } else if (head_state == HEADSTATE_SET) {
        /* We just set a configuration variable, so we should have an
         * END_META entity now */
        if (ent.status != SNENTITY_END_META) {
          status = 0;
          fprintf(stderr, "%s: [Line %ld] Header syntax error!\n",
                    pModule,
                    snparser_count(ps));
        }
        
        /* Return state back to initial state */
        if (status) {
          head_state = HEADSTATE_INITIAL;
        }
        
      } else {
        /* Shouldn't happen */
        abort();
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
    
    /* If we got here without error, check for parsing error */
    if (status && (ent.status < 0)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Shastina error: %s!\n",
                pModule,
                snparser_count(ps),
                snerror_str(ent.status));
    }
    
    /* Check that we are in appropriate header state */
    if (status && (head_state != HEADSTATE_INITIAL)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Header syntax error!\n",
                pModule,
                snparser_count(ps));
    }
    
    /* Check that we successfully read the signature */
    if (status && (!read_signature)) {
      status = 0;
      fprintf(stderr, "%s: Failed to read %%sparkle; signature!\n",
                pModule);
    }
    
    /* Set default values on configuration variables that weren't set in
     * the header */
    if (status) {
      if (bufc_value < 0) {
        bufc_value = 0;
      }
      if (matc_value < 0) {
        matc_value = 0;
      }
    }
  }
  
  /* Initialize the skvm module */
  if (status) {
    skvm_init(bufc_value, matc_value);
  }
  
  /* Register operator modules */
  if (status) {
    register_modules();
  }
  
  /* -------------- */
  /*                */
  /* INTERPRETATION */
  /*                */
  /* -------------- */
  
  /* Interpret all tokens; we've already read the first body token, so
   * loop does not start with a read */
  if (status) {
    for( ;
        ent.status > 0;
        snparser_read(ps, &ent, pin)) {
      
      /* Handle the entity types */
      if (ent.status == SNENTITY_STRING) {
        /* String -- check that string is double-quoted and that there
         * is no prefix */
        if ((ent.str_type != SNSTRING_QUOTED) ||
            ((ent.pKey)[0] != 0)) {
          status = 0;
          fprintf(stderr,
            "%s: [Line %ld] Unsupported Shastina string type!\n",
            pModule, snparser_count(ps));
        }
        
        /* Copy the string value into our local buffer, watching for
         * overflow, checking character ranges, and also interpreting
         * escape codes */
        if (status) {
          memset(sbuf, 0, MAX_STRING_LEN + 1);
          sbuf_len = 0;
          
          for(pc = ent.pValue; *pc != 0; pc++) {
            /* Check character range */
            if ((*pc < 0x20) || (*pc > 0x7e)) {
              status = 0;
              fprintf(stderr,
                "%s: [Line %ld] String contains illegal characters!\n",
                pModule, snparser_count(ps));
            }
            
            /* If this is an escape code, check that next character is
             * either a backslash or a double quote and then advance to
             * that character */
            if (status && (*pc == '\\')) {
              if ((pc[1] == '\\') || (pc[1] == '"')) {
                pc++;
              } else {
                status = 0;
                fprintf(stderr,
                  "%s: [Line %ld] String contains bad escape code!\n",
                  pModule, snparser_count(ps));
              }
            }
            
            /* Check that we have space remaining in buffer */
            if (status && (sbuf_len >= MAX_STRING_LEN)) {
              status = 0;
              fprintf(stderr, "%s: [Line %ld] String is too long!\n",
                pModule, snparser_count(ps));
            }
            
            /* Add to buffer */
            if (status) {
              sbuf[sbuf_len] = *pc;
              sbuf_len++;
            }
            
            /* Leave loop if error */
            if (!status) {
              break;
            }
          }
        }
        
        /* Push a copy of the string onto the stack */
        if (status) {
          if (!stack_push_string(sbuf)) {
            status = 0;
            fprintf(stderr, "%s: [Line %ld] Stack overflow!\n",
              pModule, snparser_count(ps));
          }
        }
        
      } else if (ent.status == SNENTITY_NUMERIC) {
        /* Numeric -- first determine whether this is a float */
        has_float = 0;
        for(pc = ent.pKey; *pc != 0; pc++) {
          if ((*pc == '.') || (*pc == 'e') || (*pc == 'E')) {
            has_float = 1;
            break;
          }
        }
        
        /* Proceed depending on whether float or not */
        if (has_float) {
          /* Float, so attempt float parsing using standard library */
          errno = 0;
          dv = strtod(ent.pKey, &endptr);
          if (errno != 0) {
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Failed to parse as float: %s\n",
              pModule, snparser_count(ps), ent.pKey);
          }
          if (status && (endptr != NULL)) {
            if (endptr[0] != 0) {
              status = 0;
              fprintf(stderr,
                "%s: [Line %ld] Failed to parse as float: %s\n",
                pModule, snparser_count(ps), ent.pKey);
            }
          }
          if (status && (!isfinite(dv))) {
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Float is not finite: %s\n",
              pModule, snparser_count(ps), ent.pKey);
          }
          
          /* Push the float onto the stack */
          if (status) {
            if (!stack_push_float(dv)) {
              status = 0;
              fprintf(stderr, "%s: [Line %ld] Stack overflow!\n",
                pModule, snparser_count(ps));
            }
          }
          
        } else {
          /* Integer, so attempt to parse it */
          if (!parseInt(ent.pKey, &iv)) {
            status = 0;
            fprintf(stderr,
              "%s: [Line %ld] Failed to parse as integer: %s\n",
              pModule, snparser_count(ps), ent.pKey);
          }
          
          /* Push the integer onto the stack */
          if (status) {
            if (!stack_push_int(iv)) {
              status = 0;
              fprintf(stderr, "%s: [Line %ld] Stack overflow!\n",
                pModule, snparser_count(ps));
            }
          }
        }
        
      } else if (ent.status == SNENTITY_OPERATION) {
        /* Operation, so dispatch operation */
        if (!op_invoke(ent.pKey, snparser_count(ps))) {
          status = 0;
        }
        
      } else {
        /* Unsupported entity type */
        status = 0;
        fprintf(stderr,
          "%s: [Line %ld] Unsupported Shastina entity type!\n",
          pModule, snparser_count(ps));
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
    
    /* If we got here without error, check for parsing error */
    if (status && (ent.status < 0)) {
      status = 0;
      fprintf(stderr, "%s: [Line %ld] Shastina error: %s!\n",
                pModule,
                snparser_count(ps),
                snerror_str(ent.status));
    }
    
    /* Consume everything after the EOF */
    if (status) {
      ent.status = snsource_consume(pin);
      if (ent.status < 0) {
        status = 0;
        fprintf(stderr, "%s: Error after EOF marker: %s!\n",
          pModule,
          snerror_str(ent.status));
      }
    }
  }
  
  /* Check that interpreter stack is empty */
  if (status) {
    if (!(stack_count() < 1)) {
      status = 0;
      fprintf(stderr, "%s: Interpreter stack not empty at EOF!\n",
        pModule);
    }
  }
  
  /* Free Shastina parser if allocated */
  snparser_free(ps);
  ps = NULL;
  
  /* Free Shastina input source if allocated */
  snsource_free(pin);
  pin = NULL;
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
