/*
 * sksample.c
 * ==========
 * 
 * Implementation of sksample.h
 * 
 * See the header for further information.
 */

#include "sksample.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sparkle.h"
#include "skvm.h"

/*
 * Local data
 * ==========
 */

/*
 * The sample source.
 * 
 * If not yet configured, m_src will be -1 and all other fields are
 * invalid.
 * 
 * Otherwise, m_src is the buffer index that is used as the source to
 * sample from.
 * 
 * If the m_src_subarea flag is set to zero, then the rest of the fields
 * are ignored and the full source buffer will be used as the source
 * area.
 * 
 * If the m_src_subarea flag is set to non-zero, then the fields
 * m_src_buf_w and m_src_buf_h store the buffer width and the buffer
 * height at the time that the source subarea was defined, and
 * m_src_x, m_src_y, m_src_w, and m_src_h define the source subarea.
 * 
 * If a source subarea is defined, when the sample operation occurs, the
 * m_src_buf_w and m_src_buf_h values must match the current dimensions
 * of the source buffer.
 */
static int32_t m_src = -1;

static int m_src_subarea;
static int32_t m_src_buf_w;
static int32_t m_src_buf_h;

static int32_t m_src_x;
static int32_t m_src_y;
static int32_t m_src_w;
static int32_t m_src_h;

/*
 * The sample target.
 * 
 * If not yet configured, m_target will be -1.  Otherwise, it is the
 * buffer index that is used as the target to draw into.
 */
static int32_t m_target = -1;

/*
 * The transformation matrix.
 * 
 * If not yet configured, m_matrix will be -1.  Otherwise, it is the
 * matrix index that selects the transformation matrix.
 */
static int32_t m_matrix = -1;

/*
 * The masking state.
 * 
 * If m_mask_buf is -1 then procedural masking is in effect and the
 * other fields define the procedural mask.  Otherwise, m_mask_buf is
 * the buffer index for the raster mask and the other fields are
 * ignored.
 * 
 * For procedural masking, m_x_boundary and m_y_boundary store the
 * normalized X and Y boundary coordinates.  m_right is non-zero for
 * right mode and zero for left mode.  m_below is non-zero for below
 * mode and zero and above mode.
 * 
 * The default is a procedural mask that doesn't mask anything.
 */
static int32_t m_mask_buf = -1;
static double m_x_boundary = 0.0;
static double m_y_boundary = 0.0;
static int m_right = 0;
static int m_below = 0;

/*
 * The sampling algorithm.
 * 
 * This is one of the SKVM_ALG_ constants, by default selecting bilinear
 * interpolation.
 */
static int m_alg = SKVM_ALG_BILINEAR;

/*
 * Operator functions
 * ==================
 */

/*
 * - sample -
 */
static int op_sample(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t w = 0;
  int32_t h = 0;
  int32_t w2 = 0;
  int32_t h2 = 0;
  
  SKVM_SAMPLE_PARAM sp;
  
  /* Initialize structures */
  memset(&sp, 0, sizeof(SKVM_SAMPLE_PARAM));
  
  /* Make sure that source, target, and matrix have been configured */
  if (m_src < 0) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] source must be configured before sample!\n",
      pModule, line_num);
  }
  
  if (status && (m_target < 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] target must be configured before sample!\n",
      pModule, line_num);
  }
  
  if (status && (m_matrix < 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] matrix must be configured before sample!\n",
      pModule, line_num);
  }
  
  /* Make sure that source and target are not the same */
  if (status && (m_src == m_target)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Sample source and target must be different!\n",
      pModule, line_num);
  }
  
  /* If raster mask defined, make sure not same as source nor target */
  if (status && (m_mask_buf >= 0)) {
    if (status && (m_src == m_mask_buf)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Sample source and mask must be different!\n",
        pModule, line_num);
    }
    
    if (status && (m_target == m_mask_buf)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Sample target and mask must be different!\n",
        pModule, line_num);
    }
  }
  
  /* Make sure source and target are loaded */
  if (status) {
    if (!skvm_is_loaded(m_src)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Sample source buffer is not loaded!\n",
        pModule, line_num);
    }
  }
  
  if (status) {
    if (!skvm_is_loaded(m_target)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Sample target buffer is not loaded!\n",
        pModule, line_num);
    }
  }
  
  /* If raster mask is configured, make sure loaded, grayscale, and
   * same dimensions as target */
  if (status && (m_mask_buf >= 0)) {
    if (!skvm_is_loaded(m_mask_buf)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Mask buffer is not loaded!\n",
        pModule, line_num);
    }
    
    if (status && (skvm_get_channels(m_mask_buf) != 1)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Mask buffer must be grayscale!\n",
        pModule, line_num);
    }
    
    if (status) {
      skvm_get_dim(m_mask_buf, &w, &h);
      skvm_get_dim(m_target, &w2, &h2);
      if ((w != w2) || (h != h2)) {
        status = 0;
        fprintf(stderr,
          "%s: [Line %ld] Mask buffer must match target dimensions!\n",
          pModule, line_num);
      }
    }
  }
  
  /* If subarea, make sure source still same size */
  if (status && m_src_subarea) {
    skvm_get_dim(m_src, &w, &h);
    if ((w != m_src_buf_w) || (h != m_src_buf_h)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Subarea no longer valid for source!\n",
        pModule, line_num);
    }
  }
  
  /* Now fill in the sample operation structure */
  if (status) {
    sp.src_buf = m_src;
    sp.target_buf = m_target;
    
    if (m_mask_buf >= 0) {
      sp.mask_buf = m_mask_buf;
    }
    
    if (m_src_subarea) {
      sp.src_x = m_src_x;
      sp.src_y = m_src_y;
      sp.src_w = m_src_w;
      sp.src_h = m_src_h;
    }
    
    sp.t_matrix = m_matrix;
    
    if (m_mask_buf < 0) {
      sp.x_boundary = m_x_boundary;
      sp.y_boundary = m_y_boundary;
    }
    
    sp.sample_alg = m_alg;
    
    sp.flags = 0;
    if (m_src_subarea) {
      sp.flags |= SKVM_FLAG_SUBAREA;
    }
    if (m_mask_buf >= 0) {
      sp.flags |= SKVM_FLAG_RASTERMASK;
    } else {
      sp.flags |= SKVM_FLAG_PROCMASK;
      if (m_right) {
        sp.flags |= SKVM_FLAG_RIGHTMODE;
      } else {
        sp.flags |= SKVM_FLAG_LEFTMODE;
      }
      if (m_below) {
        sp.flags |= SKVM_FLAG_BELOWMODE;
      } else {
        sp.flags |= SKVM_FLAG_ABOVEMODE;
      }
    }
  }
  
  /* Finally, invoke the sample operation */
  if (status) {
    skvm_sample(&sp);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] sample_source -
 */
static int op_sample_source(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t i = 0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_source!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_source!\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    i = cell_get_int(stack_index(0));
  }
  
  /* Check that register is valid */
  if (status && ((i < 0) || (i >= skvm_bufc()))) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Invalid buffer index!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_src = i;
    m_src_subarea = 0;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] [x] [y] [width] [height] sample_source_area -
 */
static int op_sample_source_area(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t i = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t w = 0;
  int32_t h = 0;
  int32_t buf_w = 0;
  int32_t buf_h = 0;
  
  /* Check that at least five parameters */
  if (stack_count() < 5) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_source_area!\n",
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
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_source_area!\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    i = cell_get_int(stack_index(4));
    x = cell_get_int(stack_index(3));
    y = cell_get_int(stack_index(2));
    w = cell_get_int(stack_index(1));
    h = cell_get_int(stack_index(0));
  }
  
  /* Check that register is valid */
  if (status && ((i < 0) || (i >= skvm_bufc()))) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Invalid buffer index!\n",
      pModule, line_num);
  }
  
  /* Check that width and height are at least one */
  if (status) {
    if ((w < 1) || (h < 1)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Subarea dimensions must be positive!\n",
        pModule, line_num);
    }
  }
  
  /* Get dimensions of buffer register */
  if (status) {
    skvm_get_dim(i, &buf_w, &buf_h);
  }
  
  /* Check that (x, y) is within buffer register area */
  if (status) {
    if ((x < 0) || (x >= buf_w) || (y < 0) || (y >= buf_h)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Subarea outside of buffer boundaries!\n",
        pModule, line_num);
    }
  }
  
  /* Check that full extent of subarea is within buffer register area */
  if (status) {
    if ((x > buf_w - w) || (y > buf_h - h)) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Subarea extent outside of buffer boundaries!\n",
        pModule, line_num);
    }
  }
  
  /* Update state */
  if (status) {
    m_src = i;
    m_src_subarea = 1;
    m_src_buf_w = buf_w;
    m_src_buf_h = buf_h;
    m_src_x = x;
    m_src_y = y;
    m_src_w = w;
    m_src_h = h;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(5);
  }
  
  /* Return status */
  return status;
}

/*
 * [i] sample_target -
 */
static int op_sample_target(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t i = 0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_target!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_target!\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    i = cell_get_int(stack_index(0));
  }
  
  /* Check that register is valid */
  if (status && ((i < 0) || (i >= skvm_bufc()))) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Invalid buffer index!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_target = i;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * [m] sample_matrix -
 */
static int op_sample_matrix(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t m = 0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_matrix!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_matrix!\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    m = cell_get_int(stack_index(0));
  }
  
  /* Check that register is valid */
  if (status && ((m < 0) || (m >= skvm_matc()))) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Invalid matrix index!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_matrix = m;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * - sample_mask_none -
 */
static int op_sample_mask_none(const char *pModule, long line_num) {
  
  /* Ignore parameters */
  (void) pModule;
  (void) line_num;
  
  /* Update state */
  m_mask_buf = -1;
  m_x_boundary = 0.0;
  m_y_boundary = 0.0;
  m_right = 0;
  m_below = 0;

  /* Return successful */
  return 1;
}

/*
 * [x] sample_mask_x -
 */
static int op_sample_mask_x(const char *pModule, long line_num) {
  
  int status = 1;
  double v = 0.0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_mask_x!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_canfloat(stack_index(0))) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_mask_x\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    v = cell_get_float(stack_index(0));
  }
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Check that parameter value is finite and in range [0.0, 1.0] */
  if (status && (!isfinite(v))) {
    status = 0;
    fprintf(stderr,
    "%s: [Line %ld] Procedural mask boundary must be in [0.0, 1.0]!\n",
      pModule, line_num);
  }
  if (status && (!((v >= 0.0) && (v <= 1.0)))) {
    status = 0;
    fprintf(stderr,
    "%s: [Line %ld] Procedural mask boundary must be in [0.0, 1.0]!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_x_boundary = v;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * [x] sample_mask_y -
 */
static int op_sample_mask_y(const char *pModule, long line_num) {
  
  int status = 1;
  double v = 0.0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_mask_y!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_canfloat(stack_index(0))) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_mask_y\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    v = cell_get_float(stack_index(0));
  }
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Check that parameter value is finite and in range [0.0, 1.0] */
  if (status && (!isfinite(v))) {
    status = 0;
    fprintf(stderr,
    "%s: [Line %ld] Procedural mask boundary must be in [0.0, 1.0]!\n",
      pModule, line_num);
  }
  if (status && (!((v >= 0.0) && (v <= 1.0)))) {
    status = 0;
    fprintf(stderr,
    "%s: [Line %ld] Procedural mask boundary must be in [0.0, 1.0]!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_y_boundary = v;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * - sample_mask_left -
 */
static int op_sample_mask_left(const char *pModule, long line_num) {
  
  int status = 1;
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_right = 0;
  }

  /* Return status */
  return status;
}

/*
 * - sample_mask_right -
 */
static int op_sample_mask_right(const char *pModule, long line_num) {
  
  int status = 1;
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_right = 1;
  }

  /* Return status */
  return status;
}

/*
 * - sample_mask_above -
 */
static int op_sample_mask_above(const char *pModule, long line_num) {
  
  int status = 1;
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_below = 0;
  }

  /* Return status */
  return status;
}

/*
 * - sample_mask_below -
 */
static int op_sample_mask_below(const char *pModule, long line_num) {
  
  int status = 1;
  
  /* Check that raster masking is not in effect */
  if (status && (m_mask_buf >= 0)) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Can't adjust procedural mask when raster mask!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_below = 1;
  }

  /* Return status */
  return status;
}

/*
 * [i] sample_mask_raster -
 */
static int op_sample_mask_raster(const char *pModule, long line_num) {
  
  int status = 1;
  int32_t i = 0;
  
  /* Check that at least one parameter */
  if (stack_count() < 1) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Stack underflow on sample_mask_raster!\n",
      pModule, line_num);
  }
  
  /* Check parameter types */
  if (status) {
    if (cell_type(stack_index(0)) != CELLTYPE_INTEGER) {
      status = 0;
      fprintf(stderr,
        "%s: [Line %ld] Wrong types for sample_mask_raster!\n",
        pModule, line_num);
    }
  }
  
  /* Get parameters */
  if (status) {
    i = cell_get_int(stack_index(0));
  }
  
  /* Check that register is valid */
  if (status && ((i < 0) || (i >= skvm_bufc()))) {
    status = 0;
    fprintf(stderr,
      "%s: [Line %ld] Invalid buffer index!\n",
      pModule, line_num);
  }
  
  /* Update state */
  if (status) {
    m_mask_buf = i;
  }
  
  /* Remove parameters from stack */
  if (status) {
    stack_pop(1);
  }
  
  /* Return status */
  return status;
}

/*
 * - sample_nearest -
 */
static int op_sample_nearest(const char *pModule, long line_num) {
  
  /* Ignore parameters */
  (void) pModule;
  (void) line_num;
  
  /* Update state */
  m_alg = SKVM_ALG_NEAREST;

  /* Return successful */
  return 1;
}

/*
 * - sample_bilinear -
 */
static int op_sample_bilinear(const char *pModule, long line_num) {
  
  /* Ignore parameters */
  (void) pModule;
  (void) line_num;
  
  /* Update state */
  m_alg = SKVM_ALG_BILINEAR;

  /* Return successful */
  return 1;
}

/*
 * - sample_bicubic -
 */
static int op_sample_bicubic(const char *pModule, long line_num) {
  
  /* Ignore parameters */
  (void) pModule;
  (void) line_num;
  
  /* Update state */
  m_alg = SKVM_ALG_BICUBIC;

  /* Return successful */
  return 1;
}

/*
 * Registration function
 * =====================
 */

void sksample_register(void) {
  register_operator("sample", &op_sample);
  register_operator("sample_source", &op_sample_source);
  register_operator("sample_source_area", &op_sample_source_area);
  register_operator("sample_target", &op_sample_target);
  register_operator("sample_matrix", &op_sample_matrix);
  register_operator("sample_mask_none", &op_sample_mask_none);
  register_operator("sample_mask_x", &op_sample_mask_x);
  register_operator("sample_mask_y", &op_sample_mask_y);
  register_operator("sample_mask_left", &op_sample_mask_left);
  register_operator("sample_mask_right", &op_sample_mask_right);
  register_operator("sample_mask_above", &op_sample_mask_above);
  register_operator("sample_mask_below", &op_sample_mask_below);
  register_operator("sample_mask_raster", &op_sample_mask_raster);
  register_operator("sample_nearest", &op_sample_nearest);
  register_operator("sample_bilinear", &op_sample_bilinear);
  register_operator("sample_bicubic", &op_sample_bilinear);
}
