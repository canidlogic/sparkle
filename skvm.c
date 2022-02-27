/*
 * skvm.c
 * ======
 * 
 * Implementation of skvm.h
 * 
 * See the header for further information.
 */

#include "skvm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sophistry.h"
#include "sophistry_jpeg.h"

/*
 * Type declarations
 * =================
 */

/*
 * Structure used to represent a buffer register.
 */
typedef struct {
  
  /*
   * Pointer to the dynamically allocated data buffer.
   * 
   * If this buffer is not loaded, this pointer will be NULL.
   * 
   * Otherwise, the format of the data varies depending on how many
   * channels there are.  For one-channel, each pixel is one byte.  For
   * three-channel, each pixel is three bytes in RGB order.  For
   * four-channel, each pixel is four bytes in ARGB order.
   * 
   * Scanlines are left to right, and scanlines are stored top to
   * bottom.  There is no padding.
   */
  uint8_t *pData;
  
  /*
   * The width of the buffer in pixels.
   * 
   * In range [1, SKVM_MAX_DIM].
   */
  int16_t w;
  
  /*
   * The height of the buffer in pixels.
   * 
   * In range [1, SKVM_MAX_DIM].
   */
  int16_t h;
  
  /*
   * The total number of color channels.
   * 
   * Must be 1 (grayscale), 3 (RGB), or 4 (ARGB).
   */
  uint8_t c;
  
} SKBUF;

/*
 * Structure used to represent a matrix:
 * 
 * | a b c |
 * | d e f |
 * | 0 0 1 |
 * 
 * The third row is always 0 0 1 so it is not stored in the structure.
 * 
 * Additionally, this structure can cache the inversion of the matrix.
 * If the inversion is available, the "cached" member will be set to
 * a non-zero value, and then the following members will also be
 * available:
 * 
 * | iva ivb ivc |
 * | ivd ive ivf |
 * |  0   0   1  |
 */
typedef struct {
  
  /*
   * The matrix values.
   */
  double a;
  double b;
  double c;
  double d;
  double e;
  double f;
  
  /*
   * The cached inverse matrix values, only if "cached" is non-zero.
   */
  double iva;
  double ivb;
  double ivc;
  double ivd;
  double ive;
  double ivf;
  
  /*
   * Flag that is set to non-zero if the inverse matrix is cached.
   */
  uint8_t cached;
  
} SKMAT;

/*
 * Structure used to represent an (x,y) coordinate.
 */
typedef struct {
  
  double x;
  double y;
  
} SKPOINT;

/*
 * Structure used to represent floating-point **PREMULTIPLIED** ARGB.
 */
typedef struct {
  
  double a;
  double r;
  double g;
  double b;
  
} SKARGB;

/*
 * Static data
 * ===========
 */

/*
 * The error message from the most recent operation that can fail, or
 * NULL if there was no error.
 */
static const char *m_perr = NULL;

/*
 * Flag indicating whether this module has been initialized yet.
 */
static int m_init = 0;

/*
 * The number of buffer and matrix registers declared.
 * 
 * Only if m_init.
 * 
 * Range of bufc is [0, SKVM_MAX_BUFC].
 * Range of matc is [0, SKVM_MAX_MATC].
 */
static int32_t m_bufc;
static int32_t m_matc;

/*
 * The pointers to the buffer and matrix register arrays.
 * 
 * Only if m_init.
 * 
 * Size of arrays indicated by m_bufc and m_matc above.  For zero-size,
 * a NULL pointer is given.
 */
static SKBUF *m_pbuf;
static SKMAT *m_pmat;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void source2target(SKMAT *pt, SKPOINT *pp);
static void target2source(SKMAT *pt, SKPOINT *pp);

static void sample_nearest(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr);
static void sample_bilinear(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr);
static void sample_bicubic(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr);
    
static void matrix_mul(SKMAT *pm, const SKMAT *pa, const SKMAT *pb);

/*
 * Given a transformation matrix and a point, convert the point from
 * source space to target space.
 * 
 * Parameters:
 * 
 *   pt - the transformation matrix
 * 
 *   pp - the point to convert
 */
static void source2target(SKMAT *pt, SKPOINT *pp) {
  
  double rx = 0.0;
  double ry = 0.0;
  
  /* Check parameters */
  if ((pt == NULL) || (pp == NULL)) {
    abort();
  }
  
  /* Multiply the matrix by the expanded vector to get the result */
  rx = (pt->a * pp->x) + (pt->b * pp->y) + pt->c;
  ry = (pt->d * pp->x) + (pt->e * pp->y) + pt->f;
  
  /* Write result to point */
  pp->x = rx;
  pp->y = ry;
}

/*
 * Given a transformation matrix and a point, convert the point from
 * target space to source space.
 * 
 * Parameters:
 * 
 *   pt - the transformation matrix
 * 
 *   pp - the point to convert
 */
static void target2source(SKMAT *pt, SKPOINT *pp) {
  
  double denom = 0.0;
  double rx = 0.0;
  double ry = 0.0;
  
  /* Check parameters */
  if ((pt == NULL) || (pp == NULL)) {
    abort();
  }
  
  /* If inversion is not yet cached, compute it and cache it */
  if (!(pt->cached)) {
    denom = (pt->a * pt->e) - (pt->b * pt->d);
    
    pt->iva = pt->e / denom;
    pt->ivb = -(pt->b / denom);
    pt->ivc = ((pt->b * pt->f) - (pt->c * pt->e)) / denom;
    
    pt->ivd = -(pt->d / denom);
    pt->ive = pt->a / denom;
    pt->ivf = ((pt->c * pt->d) - (pt->a * pt->f)) / denom;
    
    pt->cached = (uint8_t) 1;
  }
  
  /* Multiply the inverted matrix by the expanded vector to get the
   * result */
  rx = (pt->iva * pp->x) + (pt->ivb * pp->y) + pt->ivc;
  ry = (pt->ivd * pp->x) + (pt->ive * pp->y) + pt->ivf;
  
  /* Write result to point */
  pp->x = rx;
  pp->y = ry;
}

/*
 * Perform nearest-neighbor interpolation.
 * 
 * pb is the buffer to sample from.  It must be loaded.
 * 
 * pp is the point within the buffer to sample.  It must be somewhere
 * within the boundaries of the buffer.
 * 
 * pr receives the sampled, premultiplied ARGB value.
 * 
 * Parameters:
 * 
 *   pb - the buffer
 * 
 *   pp - the point
 * 
 *   pr - receives the sampled color
 */
static void sample_nearest(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr) {
  
  int32_t x = 0;
  int32_t y = 0;
  uint8_t *pt = NULL;
  
  /* Check parameters */
  if ((pb == NULL) || (pp == NULL) || (pr == NULL)) {
    abort();
  }
  
  /* Make sure that buffer is loaded */
  if (pb->pData == NULL) {
    abort();
  }
  
  /* For nearest neighbor, we just have to floor both coordinates and
   * then clamp them to the allowable range */
  x = (int32_t) floor(pp->x);
  y = (int32_t) floor(pp->y);
  
  if (x < 0) {
    x = 0;
  } else if (x > pb->w - 1) {
    x = pb->w - 1;
  }
  
  if (y < 0) {
    y = 0;
  } else if (y > pb->h - 1) {
    y = pb->h - 1;
  }
  
  /* Now seek to the pixel within the source data */
  pt = pb->pData;
  pt += (y * (pb->w * pb->c));
  pt += (x * pb->c);
  
  /* Load depending on number of channels */
  if (pb->c == 1) {
    /* Grayscale */
    pr->a = 1.0;
    pr->r = ((double) *pt) / 255.0;
    pr->g = pr->r;
    pr->b = pr->r;
    
  } else if (pb->c == 3) {
    /* RGB */
    pr->a = 1.0;
    pr->r = ((double) pt[0]) / 255.0;
    pr->g = ((double) pt[1]) / 255.0;
    pr->b = ((double) pt[2]) / 255.0;
    
  } else if (pb->c == 4) {
    /* ARGB, so first load non-premultiplied */
    pr->a = ((double) pt[0]) / 255.0;
    pr->r = ((double) pt[1]) / 255.0;
    pr->g = ((double) pt[2]) / 255.0;
    pr->b = ((double) pt[3]) / 255.0;
    
    /* Now perform premultiplication */
    pr->r = pr->r * pr->a;
    pr->g = pr->g * pr->a;
    pr->b = pr->b * pr->a;
    
  } else {
    /* Shouldn't happen */
    abort();
  }
}

/*
 * Perform bilinear interpolation.
 * 
 * pb is the buffer to sample from.  It must be loaded.
 * 
 * pp is the point within the buffer to sample.  It must be somewhere
 * within the boundaries of the buffer.
 * 
 * pr receives the sampled, premultiplied ARGB value.
 * 
 * Parameters:
 * 
 *   pb - the buffer
 * 
 *   pp - the point
 * 
 *   pr - receives the sampled color
 */
static void sample_bilinear(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr) {
  /* @@TODO: */
  fprintf(stderr, "Bilinear interpolation not implemented yet!\n");
  abort();
}

/*
 * Perform bicubic interpolation.
 * 
 * pb is the buffer to sample from.  It must be loaded.
 * 
 * pp is the point within the buffer to sample.  It must be somewhere
 * within the boundaries of the buffer.
 * 
 * pr receives the sampled, premultiplied ARGB value.
 * 
 * Parameters:
 * 
 *   pb - the buffer
 * 
 *   pp - the point
 * 
 *   pr - receives the sampled color
 */
static void sample_bicubic(
    const SKBUF   * pb,
    const SKPOINT * pp,
          SKARGB  * pr) {
  /* @@TODO: */
  fprintf(stderr, "Bicubic interpolation not implemented yet!\n");
  abort();
}

/*
 * Multiply two matrices together and store their result in a third
 * matrix.
 * 
 * The operation performed is:
 * 
 *   (*pm) = (*pa) * (*pb)
 * 
 * pm may not point to the same matrix structure as pa or pb, but pa and
 * pb may point to the same structure.
 * 
 * This does not check that the results are finite, nor does it cache an
 * inverse matrix.
 * 
 * Parameters:
 * 
 *   pm - the matrix to store the result in
 * 
 *   pa - the first matrix operand
 * 
 *   pb - the second matrix operand
 */
static void matrix_mul(SKMAT *pm, const SKMAT *pa, const SKMAT *pb) {
  
  /* Check parameters */
  if ((pm == NULL) || (pa == NULL) || (pb == NULL)) {
    abort();
  }
  if ((pm == pa) || (pm == pb)) {
    abort();
  }
  
  /* Perform multiplication */
  pm->a = (pa->a * pb->a) + (pa->b * pb->d);
  pm->b = (pa->a * pb->b) + (pa->b * pb->e);
  pm->c = (pa->a * pb->c) + (pa->b * pb->f) + pa->c;
  
  pm->d = (pa->d * pb->a) + (pa->e * pb->d);
  pm->e = (pa->d * pb->b) + (pa->e * pb->e);
  pm->f = (pa->d * pb->c) + (pa->e * pb->f) + pa->f;
  
  /* Clear the cache flag on the result */
  pm->cached = (uint8_t) 0;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * skvm_init function.
 */
void skvm_init(int32_t bufc, int32_t matc) {
  
  int32_t i = 0;
  SKBUF *ps = NULL;
  SKMAT *pm = NULL;
  
  /* Check state */
  if (m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((bufc < 0) || (bufc > SKVM_MAX_BUFC) ||
      (matc < 0) || (matc > SKVM_MAX_MATC)) {
    abort();
  }
  
  /* Store counts */
  m_bufc = bufc;
  m_matc = matc;
  
  /* Allocate registers */
  if (bufc > 0) {
    m_pbuf = (SKBUF *) calloc(bufc, sizeof(SKBUF));
    if (m_pbuf == NULL) {
      abort();
    }
    
  } else {
    m_pbuf = NULL;
  }
  
  if (matc > 0) {
    m_pmat = (SKMAT *) calloc(matc, sizeof(SKMAT));
    if (m_pmat == NULL) {
      abort();
    }
    
  } else {
    m_pmat = NULL;
  }
  
  /* Initialize all buffer registers to 1x1 grayscale, unloaded */
  for(i = 0; i < bufc; i++) {
    ps = &(m_pbuf[i]);
    
    ps->pData = NULL;
    ps->w = 1;
    ps->h = 1;
    ps->c = (uint8_t) 1;
  }
  
  /* Initialize all matrices to identity with cached inverse, which is
   * also the identity */
  for(i = 0; i < matc; i++) {
    pm = &(m_pmat[i]);
    
    pm->a = 1.0;    pm->b = 0.0;    pm->c = 0.0;
    pm->d = 0.0;    pm->e = 1.0;    pm->f = 0.0;
    
    pm->cached = (uint8_t) 1;
    
    pm->iva = 1.0;  pm->ivb = 0.0;  pm->ivc = 0.0;
    pm->ivd = 0.0;  pm->ive = 1.0;  pm->ivf = 0.0;
  }
  
  /* Set the initialized flag */
  m_init = 1;
}

/*
 * skvm_reason function.
 */
const char *skvm_reason(void) {
  
  const char *pResult = NULL;
  
  if (m_perr != NULL) {
    pResult = m_perr;
  } else {
    pResult = "No error";
  }
  
  return pResult;
}

/*
 * skvm_bufc function.
 */
int32_t skvm_bufc(void) {
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Return value */
  return m_bufc;
}

/*
 * skvm_matc function.
 */
int32_t skvm_matc(void) {
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Return value */
  return m_matc;
}

/*
 * skvm_get_dim function.
 */
void skvm_get_dim(int32_t i, int32_t *pw, int32_t *ph) {
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) || (pw == NULL) || (ph == NULL)) {
    abort();
  }
  
  /* Return the requested information */
  *pw = (m_pbuf[i]).w;
  *ph = (m_pbuf[i]).h;
}

/*
 * skvm_get_channels function.
 */
int skvm_get_channels(int32_t i) {
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc)) {
    abort();
  }
  
  /* Return the requested information */
  return (m_pbuf[i]).c;
}

/*
 * skvm_is_loaded function.
 */
int skvm_is_loaded(int32_t i) {
  
  int result = 0;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc)) {
    abort();
  }
  
  /* Determine if loaded */
  if ((m_pbuf[i]).pData != NULL) {
    result = 1;
  } else {
    result = 0;
  }
  
  /* Return the requested information */
  return result;
}

/*
 * skvm_reset function.
 */
void skvm_reset(int32_t i, int32_t w, int32_t h, int c) {
  
  SKBUF *ps = NULL;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) ||
      (w < 1) || (w > SKVM_MAX_DIM) ||
      (h < 1) || (h > SKVM_MAX_DIM) ||
      ((c != 1) && (c != 3) && (c != 4))) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* If buffer currently loaded, release it */
  if (ps->pData != NULL) {
    free(ps->pData);
    ps->pData = NULL;
  }
  
  /* Load buffer dimensions */
  ps->w = w;
  ps->h = h;
  ps->c = (uint8_t) c;
}

/*
 * skvm_load_png function.
 */
int skvm_load_png(int32_t i, const char *pPath) {
  
  int status = 1;
  int errn = 0;
  int32_t x = 0;
  int32_t y = 0;
  
  SKBUF *ps = NULL;
  SPH_IMAGE_READER *pr = NULL;
  
  uint8_t  *pi = NULL;
  uint32_t *psl = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) || (pPath == NULL)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Allocate a buffer for the register, if we don't already have one */
  if (ps->pData == NULL) {
    ps->pData = (uint8_t *) malloc((size_t)
                              (ps->w * ps->h * ((int32_t) ps->c)));
    if (ps->pData == NULL) {
      abort();
    }
  }
  
  /* Allocate PNG image reader on file */
  pr = sph_image_reader_newFromPath(pPath, &errn);
  if (pr == NULL) {
    status = 0;
    m_perr = sph_image_errorString(errn);
  }
  
  /* Make sure dimensions of PNG image match dimensions of buffer */
  if (status) {
    if ((ps->w != sph_image_reader_width(pr)) ||
        (ps->h != sph_image_reader_height(pr))) {
      status = 0;
      m_perr = "PNG file mismatches dimensions of buffer";
    }
  }
  
  /* Read each scanline into the buffer */
  if (status) {
    pi = ps->pData;
    for(y = 0; y < ps->h; y++) {
      /* Read a scanline */
      psl = sph_image_reader_read(pr, &errn);
      if (psl == NULL) {
        status = 0;
        m_perr = sph_image_errorString(errn);
      }
      
      /* Transfer each pixel into the buffer */
      if (status) {
        for(x = 0; x < ps->w; x++) {
          /* Read a pixel and unpack it */
          sph_argb_unpack(psl[x], &argb);
          
          /* Transfer with possible down-conversion */
          if (ps->c == 4) {
            pi[0] = (uint8_t) argb.a;
            pi[1] = (uint8_t) argb.r;
            pi[2] = (uint8_t) argb.g;
            pi[3] = (uint8_t) argb.b;
            pi += 4;
            
          } else if (ps->c == 3) {
            sph_argb_downRGB(&argb);
            pi[0] = (uint8_t) argb.r;
            pi[1] = (uint8_t) argb.g;
            pi[2] = (uint8_t) argb.b;
            pi += 3;
            
          } else if (ps->c == 1) {
            sph_argb_downGray(&argb);
            *pi = (uint8_t) argb.g;
            pi++;
            
          } else {
            /* Shouldn't happen */
            abort();
          }          
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Release image reader object if allocated */
  sph_image_reader_close(pr);
  pr = NULL;
  
  /* If we failed, unload register if loaded */
  if (!status) {
    if (ps->pData != NULL) {
      free(ps->pData);
      ps->pData = NULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * skvm_load_jpeg function.
 */
int skvm_load_jpeg(int32_t i, const char *pPath) {
  
  int status = 1;
  int32_t x = 0;
  int32_t y = 0;
  int src_c = 0;
  
  FILE *pf = NULL;
  SKBUF *ps = NULL;
  SPH_JPEG_READER *pr = NULL;
  
  uint8_t *pi = NULL;
  uint8_t *pj = NULL;
  uint8_t *psl = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) || (pPath == NULL)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Allocate a buffer for the register, if we don't already have one */
  if (ps->pData == NULL) {
    ps->pData = (uint8_t *) malloc((size_t)
                              (ps->w * ps->h * ((int32_t) ps->c)));
    if (ps->pData == NULL) {
      abort();
    }
  }
  
  /* Open JPEG file */
  pf = fopen(pPath, "rb");
  if (pf == NULL) {
    status = 0;
    m_perr = "Failed to open JPEG file";
  }
  
  /* Allocate JPEG reader on file */
  if (status) {
    pr = sph_jpeg_reader_new(pf);
    if (sph_jpeg_reader_status(pr) != SPH_JPEG_ERR_OK) {
      status = 0;
      m_perr = sph_jpeg_errstr(sph_jpeg_reader_status(pr));
    }
  }
  
  /* Make sure dimensions of JPEG image match dimensions of buffer */
  if (status) {
    if ((ps->w != sph_jpeg_reader_width(pr)) ||
        (ps->h != sph_jpeg_reader_height(pr))) {
      status = 0;
      m_perr = "JPEG file mismatches dimensions of buffer";
    }
  }
  
  /* Get the JPEG channel count */
  if (status) {
    src_c = sph_jpeg_reader_channels(pr);
  }
  
  /* Allocate scanline buffer */
  if (status) {
    psl = (uint8_t *) calloc((size_t) ps->w, (size_t) src_c);
    if (psl == NULL) {
      abort();
    }
  }
  
  /* Read each scanline into the buffer */
  if (status) {
    pi = ps->pData;
    for(y = 0; y < ps->h; y++) {
      /* Read a scanline */
      if (!sph_jpeg_reader_get(pr, psl)) {
        status = 0;
        m_perr = sph_jpeg_errstr(sph_jpeg_reader_status(pr));
      }
      
      /* Transfer each pixel into the buffer */
      if (status) {
        pj = psl;
        for(x = 0; x < ps->w; x++) {
          /* Transfer appropriately */
          if (ps->c == 4) {
            if (src_c == 3) {
              /* Expand RGB->ARGB */
              pi[0] = (uint8_t) 255;
              pi[1] = pj[0];
              pi[2] = pj[1];
              pi[3] = pj[2];
              
              pi += 4;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Expand gray->ARGB */
              pi[0] = (uint8_t) 255;
              pi[1] = *pj;
              pi[2] = *pj;
              pi[3] = *pj;
              
              pi += 4;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else if (ps->c == 3) {
            if (src_c == 3) {
              /* RGB -> RGB */
              pi[0] = pj[0];
              pi[1] = pj[1];
              pi[2] = pj[2];
              
              pi += 3;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Expand gray->RGB */
              pi[0] = *pj;
              pi[1] = *pj;
              pi[2] = *pj;
              
              pi += 3;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else if (ps->c == 1) {
            if (src_c == 3) {
              /* Downconvert RGB->gray */
              argb.a = 255;
              argb.r = pj[0];
              argb.g = pj[1];
              argb.b = pj[2];
              sph_argb_downGray(&argb);
              
              *pi = (uint8_t) argb.g;
              
              pi++;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Gray -> gray */
              *pi = *pj;
              
              pi++;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else {
            /* Shouldn't happen */
            abort();
          }
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Release scanline buffer if allocated */
  if (psl != NULL) {
    free(psl);
    psl = NULL;
  }
  
  /* Release image reader object if allocated */
  sph_jpeg_reader_free(pr);
  pr = NULL;
  
  /* Close file handle if open */
  if (pf != NULL) {
    fclose(pf);
    pf = NULL;
  }
  
  /* If we failed, unload register if loaded */
  if (!status) {
    if (ps->pData != NULL) {
      free(ps->pData);
      ps->pData = NULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * skvm_load_mjpg function.
 */
int skvm_load_mjpg(int32_t i, int32_t f, const char *pIndexPath) {
  
  int status = 1;
  int32_t x = 0;
  int32_t y = 0;
  int src_c = 0;
  int32_t last_dot = 0;
  int32_t last_sep = 0;
  uint64_t total_frames = 0;
  uint64_t frame_offs = 0;
  
  FILE *pf = NULL;
  SKBUF *ps = NULL;
  SPH_JPEG_READER *pr = NULL;
  
  char *pJPEGPath = NULL;
  uint8_t *pi = NULL;
  uint8_t *pj = NULL;
  uint8_t *psl = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters, except f */
  if ((i < 0) || (i >= m_bufc) || (pIndexPath == NULL)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Allocate a buffer for the register, if we don't already have one */
  if (ps->pData == NULL) {
    ps->pData = (uint8_t *) malloc((size_t)
                              (ps->w * ps->h * ((int32_t) ps->c)));
    if (ps->pData == NULL) {
      abort();
    }
  }
  
  /* Find the index of the last dot and the last forward or backslash in
   * the index file path */
  last_dot = -1;
  last_sep = -1;
  for(x = 0; pIndexPath[x] != 0; x++) {
    y = pIndexPath[x];
    if (y == '.') {
      last_dot = x;
      
    } else if (y == '/') {
      last_sep = x;
      
    } else if (y == '\\') {
      last_sep = x;
    }
  }

  /* Make sure we that have a last dot, that if there is a last
   * separator it occurs before the last dot, and that the last dot is
   * not the first character */
  if (last_dot <= 0) {
    status = 0;
    m_perr = "Invalid index file path";
  }
  if (status && (last_sep >= 0)) {
    if (last_sep > last_dot) {
      status = 0;
      m_perr = "Invalid index file path";
    }
  }
  
  /* Get a copy of the index file path up to but excluding the last dot,
   * which will be the JPEG file path */
  if (status) {
    pJPEGPath = (char *) calloc((size_t) (last_dot + 1), 1);
    if (pJPEGPath == NULL) {
      abort();
    }
    memcpy(pJPEGPath, pIndexPath, (size_t) last_dot);
  }
  
  /* Open index file */
  if (status) {
    pf = fopen(pIndexPath, "rb");
    if (pf == NULL) {
      status = 0;
      m_perr = "Failed to open index file";
    }
  }
  
  /* Read first eight bytes as big-endian integer to get count of
   * frames */
  if (status) {
    for(x = 0; x < 8; x++) {
      y = getc(pf);
      if (y == EOF) {
        status = 0;
        m_perr = "Invalid index file";
        break;
      }
      total_frames = (total_frames << 8) | y;
    }
  }
  
  /* Check that given frame index is in range */
  if (status) {
    if ((f < 0) || (f >= total_frames)) {
      status = 0;
      m_perr = "Invalid frame index";
    }
  }
  
  /* Add one to the frame index, watching for overflow, so that it is
   * the integer offset within the index file */
  if (status) {
    if (f < INT32_MAX) {
      f++;
    } else {
      status = 0;
      m_perr = "Frame index overflow";
    }
  }
  
  /* Multiply the adjusted frame index by eight, watching for overflow,
   * so that it is the byte offset of the appropriate record within the
   * index file */
  if (status) {
    if (f <= INT32_MAX / 8) {
      f *= 8;
    } else {
      status = 0;
      m_perr = "Frame index overflow";
    }
  }
  
  /* Seek to the appropriate location in the index file */
  if (status) {
    if (fseeko(pf, (off_t) f, SEEK_SET)) {
      status = 0;
      m_perr = "Index seek error";
    }
  }
  
  /* Read eight bytes as big-endian integer to get offset of frame */
  if (status) {
    for(x = 0; x < 8; x++) {
      y = getc(pf);
      if (y == EOF) {
        status = 0;
        m_perr = "Invalid index file";
        break;
      }
      frame_offs = (frame_offs << 8) | y;
    }
  }
  
  /* Close index file */
  if (status) {
    fclose(pf);
    pf = NULL;
  }
  
  /* If frame offset is beyond signed 32-bit range, make sure off_t is
   * eight bytes, indicating 64-bit file mode */
  if (status && (frame_offs > INT32_MAX)) {
    if (sizeof(off_t) < 8) {
      status = 0;
      m_perr = "64-bit file mode required";
    }
  }
  
  /* Open JPEG file */
  if (status) {
    pf = fopen(pJPEGPath, "rb");
    if (pf == NULL) {
      status = 0;
      m_perr = "Failed to open JPEG file";
    }
  }
  
  /* Seek to start of frame */
  if (status) {
    if (fseeko(pf, (off_t) frame_offs, SEEK_SET)) {
      status = 0;
      m_perr = "MJPEG seek error";
    }
  }
  
  /* Allocate JPEG reader on file */
  if (status) {
    pr = sph_jpeg_reader_new(pf);
    if (sph_jpeg_reader_status(pr) != SPH_JPEG_ERR_OK) {
      status = 0;
      m_perr = sph_jpeg_errstr(sph_jpeg_reader_status(pr));
    }
  }
  
  /* Make sure dimensions of JPEG image match dimensions of buffer */
  if (status) {
    if ((ps->w != sph_jpeg_reader_width(pr)) ||
        (ps->h != sph_jpeg_reader_height(pr))) {
      status = 0;
      m_perr = "JPEG file mismatches dimensions of buffer";
    }
  }
  
  /* Get the JPEG channel count */
  if (status) {
    src_c = sph_jpeg_reader_channels(pr);
  }
  
  /* Allocate scanline buffer */
  if (status) {
    psl = (uint8_t *) calloc((size_t) ps->w, (size_t) src_c);
    if (psl == NULL) {
      abort();
    }
  }
  
  /* Read each scanline into the buffer */
  if (status) {
    pi = ps->pData;
    for(y = 0; y < ps->h; y++) {
      /* Read a scanline */
      if (!sph_jpeg_reader_get(pr, psl)) {
        status = 0;
        m_perr = sph_jpeg_errstr(sph_jpeg_reader_status(pr));
      }
      
      /* Transfer each pixel into the buffer */
      if (status) {
        pj = psl;
        for(x = 0; x < ps->w; x++) {
          /* Transfer appropriately */
          if (ps->c == 4) {
            if (src_c == 3) {
              /* Expand RGB->ARGB */
              pi[0] = (uint8_t) 255;
              pi[1] = pj[0];
              pi[2] = pj[1];
              pi[3] = pj[2];
              
              pi += 4;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Expand gray->ARGB */
              pi[0] = (uint8_t) 255;
              pi[1] = *pj;
              pi[2] = *pj;
              pi[3] = *pj;
              
              pi += 4;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else if (ps->c == 3) {
            if (src_c == 3) {
              /* RGB -> RGB */
              pi[0] = pj[0];
              pi[1] = pj[1];
              pi[2] = pj[2];
              
              pi += 3;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Expand gray->RGB */
              pi[0] = *pj;
              pi[1] = *pj;
              pi[2] = *pj;
              
              pi += 3;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else if (ps->c == 1) {
            if (src_c == 3) {
              /* Downconvert RGB->gray */
              argb.a = 255;
              argb.r = pj[0];
              argb.g = pj[1];
              argb.b = pj[2];
              sph_argb_downGray(&argb);
              
              *pi = (uint8_t) argb.g;
              
              pi++;
              pj += 3;
              
            } else if (src_c == 1) {
              /* Gray -> gray */
              *pi = *pj;
              
              pi++;
              pj++;
              
            } else {
              /* Shouldn't happen */
              abort();
            }
            
          } else {
            /* Shouldn't happen */
            abort();
          }
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Release JPEG path if allocated */
  if (pJPEGPath != NULL) {
    free(pJPEGPath);
    pJPEGPath = NULL;
  }
  
  /* Release scanline buffer if allocated */
  if (psl != NULL) {
    free(psl);
    psl = NULL;
  }
  
  /* Release image reader object if allocated */
  sph_jpeg_reader_free(pr);
  pr = NULL;
  
  /* Close file handle if open */
  if (pf != NULL) {
    fclose(pf);
    pf = NULL;
  }
  
  /* If we failed, unload register if loaded */
  if (!status) {
    if (ps->pData != NULL) {
      free(ps->pData);
      ps->pData = NULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * skvm_load_fill function.
 */
void skvm_load_fill(int32_t i, int a, int r, int g, int b) {
  
  int32_t x = 0;
  int32_t y = 0;
  
  SKBUF *ps = NULL;
  uint8_t  *pi = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) ||
      (a < 0) || (a > 255) ||
      (r < 0) || (r > 255) ||
      (g < 0) || (g > 255) ||
      (b < 0) || (b > 255)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Allocate a buffer for the register, if we don't already have one */
  if (ps->pData == NULL) {
    ps->pData = (uint8_t *) malloc((size_t)
                              (ps->w * ps->h * ((int32_t) ps->c)));
    if (ps->pData == NULL) {
      abort();
    }
  }
  
  /* Store color in structure */
  argb.a = a;
  argb.r = r;
  argb.g = g;
  argb.b = b;
  
  /* Down-convert color if necessary */
  if (ps->c == 3) {
    sph_argb_downRGB(&argb);
    
  } else if (ps->c == 1) {
    sph_argb_downGray(&argb);
    
  } else {
    /* Only other choice should be 4-channel */
    if (ps->c != 4) {
      abort();
    }
  }
  
  /* Fill the buffer with the color */
  pi = ps->pData;
  for(y = 0; y < ps->h; y++) {
    for(x = 0; x < ps->w; x++) {
      /* Copy pixel fill color */
      if (ps->c == 4) {
        pi[0] = (uint8_t) argb.a;
        pi[1] = (uint8_t) argb.r;
        pi[2] = (uint8_t) argb.g;
        pi[3] = (uint8_t) argb.b;
        pi += 4;
        
      } else if (ps->c == 3) {
        pi[0] = (uint8_t) argb.r;
        pi[1] = (uint8_t) argb.g;
        pi[2] = (uint8_t) argb.b;
        pi += 3;
        
      } else if (ps->c == 1) {
        *pi = (uint8_t) argb.g;
        pi++;
        
      } else {
        /* Shouldn't happen */
        abort();
      }          
    }
  }
}

/*
 * skvm_store_png function.
 */
int skvm_store_png(int32_t i, const char *pPath) {
  
  int status = 1;
  int errn = 0;
  int dconv = 0;
  int32_t x = 0;
  int32_t y = 0;
  
  SKBUF *ps = NULL;
  SPH_IMAGE_WRITER *pw = NULL;
  uint32_t *psl = NULL;
  uint8_t *pi = NULL;
  uint32_t *pj = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) || (pPath == NULL)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Fail if buffer is not loaded */
  if (ps->pData == NULL) {
    status = 0;
    m_perr = "Buffer must be full to store";
  }
  
  /* Based on number of channels, determine down-conversion setting */
  if (status) {
    if (ps->c == 4) {
      /* ARGB buffer, so no down-conversion */
      dconv = SPH_IMAGE_DOWN_NONE;
      
    } else if (ps->c == 3) {
      /* RGB buffer, so RGB down-conversion */
      dconv = SPH_IMAGE_DOWN_RGB;
      
    } else if (ps->c == 1) {
      /* Grayscale buffer, so grayscale down-conversion */
      dconv = SPH_IMAGE_DOWN_GRAY;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* Create the writer object */
  if (status) {
    pw = sph_image_writer_newFromPath(
          pPath, ps->w, ps->h, dconv, 0, &errn);
    if (pw == NULL) {
      status = 0;
      m_perr = sph_image_errorString(errn);
    }
  }
  
  /* Get the scanline buffer */
  if (status) {
    psl = sph_image_writer_ptr(pw);
  }
  
  /* Write each scanline */
  if (status) {
    pi = ps->pData;
    for(y = 0; y < ps->h; y++) {
      /* Write all data to the scanline */
      pj = psl;
      for(x = 0; x < ps->w; x++) {
        /* Fill the ARGB structure */
        if (ps->c == 4) {
          argb.a = pi[0];
          argb.r = pi[1];
          argb.g = pi[2];
          argb.b = pi[3];
          
          pi += 4;
          
        } else if (ps->c == 3) {
          argb.a = 255;
          argb.r = pi[0];
          argb.g = pi[1];
          argb.b = pi[2];
          
          pi += 3;
          
        } else if (ps->c == 1) {
          argb.a = 255;
          argb.r = *pi;
          argb.g = *pi;
          argb.b = *pi;
          
          pi++;
          
        } else {
          /* Shouldn't happen */
          abort();
        }
        
        /* Write the packed color */
        *pj = sph_argb_pack(&argb);
        pj++;
      }
      
      /* Write the scanline to the file */
      sph_image_writer_write(pw);
    }
  }
  
  /* Free writer if allocated */
  sph_image_writer_close(pw);
  pw = NULL;
  
  /* Return status */
  return status;
}

/*
 * skvm_store_jpeg function.
 */
int skvm_store_jpeg(int32_t i, const char *pPath, int mjpg, int q) {
  
  int status = 1;
  int chcount = 0;
  const char *pMode = NULL;
  int32_t x = 0;
  int32_t y = 0;
  
  SKBUF *ps = NULL;
  SPH_JPEG_WRITER *pw = NULL;
  FILE *pf = NULL;
  
  uint8_t *psl = NULL;
  uint8_t *pi = NULL;
  uint8_t *pj = NULL;
  
  SPH_ARGB argb;
  
  /* Initialize structures */
  memset(&argb, 0, sizeof(SPH_ARGB));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((i < 0) || (i >= m_bufc) || (pPath == NULL)) {
    abort();
  }
  
  /* Get buffer register */
  ps = &(m_pbuf[i]);
  
  /* Fail if buffer is not loaded */
  if (ps->pData == NULL) {
    status = 0;
    m_perr = "Buffer must be full to store";
  }
  
  /* Based on number of channels, determine JPEG channels */
  if (status) {
    if (ps->c == 4) {
      /* ARGB buffer, but JPEG only supports RGB, so set to three */
      chcount = 3;
      
    } else if (ps->c == 3) {
      /* RGB buffer, so three channels */
      chcount = 3;
      
    } else if (ps->c == 1) {
      /* Grayscale buffer, so one channel */
      chcount = 1;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* Based on the mjpg flag, determine opening mode for file */
  if (status) {
    if (mjpg) {
      pMode = "ab";
    } else {
      pMode = "wb";
    }
  }
  
  /* Open the file */
  if (status) {
    pf = fopen(pPath, pMode);
    if (pf == NULL) {
      status = 0;
      m_perr = "Failed to create JPEG file";
    }
  }
  
  /* Create the writer object */
  if (status) {
    pw = sph_jpeg_writer_new(pf, ps->w, ps->h, chcount, q);
  }
  
  /* Allocate the scanline buffer */
  if (status) {
    psl = (uint8_t *) calloc((size_t) ps->w, (size_t) chcount);
    if (psl == NULL) {
      abort();
    }
  }
  
  /* Write each scanline */
  if (status) {
    pi = ps->pData;
    for(y = 0; y < ps->h; y++) {
      /* Write all data to the scanline */
      pj = psl;
      for(x = 0; x < ps->w; x++) {
        /* Different handling depending on buffer channels */
        if (ps->c == 4) {
          /* ARGB, so we need to down-convert to RGB */
          argb.a = pi[0];
          argb.r = pi[1];
          argb.g = pi[2];
          argb.b = pi[3];
          
          sph_argb_downRGB(&argb);
          
          pj[0] = (uint8_t) argb.r;
          pj[1] = (uint8_t) argb.g;
          pj[2] = (uint8_t) argb.b;
          
          pi += 4;
          pj += 3;
          
        } else if (ps->c == 3) {
          /* RGB -> RGB */
          pj[0] = pi[0];
          pj[1] = pi[1];
          pj[2] = pi[2];
          
          pi += 3;
          pj += 3;
          
        } else if (ps->c == 1) {
          /* Gray -> gray */
          *pj = *pi;
          
          pi++;
          pj++;
          
        } else {
          /* Shouldn't happen */
          abort();
        }
      }
      
      /* Write the scanline to the file */
      sph_jpeg_writer_put(pw, psl);
    }
  }
  
  /* Free writer if allocated */
  sph_jpeg_writer_free(pw);
  pw = NULL;
  
  /* Free scanline buffer if allocated */
  if (psl != NULL) {
    free(psl);
    psl = NULL;
  }
  
  /* Close file if open */
  if (pf != NULL) {
    fclose(pf);
    pf = NULL;
  }
  
  /* Return status */
  return status;
}

/*
 * skvm_matrix_reset function.
 */
void skvm_matrix_reset(int32_t m) {
  
  SKMAT *pm = NULL;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((m < 0) || (m >= m_matc)) {
    abort();
  }
  
  /* Get the selected matrix */
  pm = &(m_pmat[m]);
  
  /* Reset to identity, with a cached inversion which is also the
   * identity matrix */
  memset(pm, 0, sizeof(SKMAT));
  
  pm->a = 1.0;    pm->b = 0.0;    pm->c = 0.0;
  pm->d = 0.0;    pm->e = 1.0;    pm->f = 0.0;
  
  pm->cached = (uint8_t) 1;
  
  pm->iva = 1.0;  pm->ivb = 0.0;  pm->ivc = 0.0;
  pm->ivd = 0.0;  pm->ive = 1.0;  pm->ivf = 0.0;
}

/*
 * skvm_matrix_multiply function.
 */
void skvm_matrix_multiply(int32_t m, int32_t a, int32_t b) {
  
  SKMAT *pm = NULL;
  SKMAT *pa = NULL;
  SKMAT *pb = NULL;
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((m < 0) || (m >= m_matc) ||
      (a < 0) || (a >= m_matc) ||
      (b < 0) || (b >= m_matc)) {
    abort();
  }
  if ((m == a) || (m == b)) {
    abort();
  }
  
  /* Get the selected matrices */
  pm = &(m_pmat[m]);
  pa = &(m_pmat[a]);
  pb = &(m_pmat[b]);
  
  /* Perform multiplication */
  matrix_mul(pm, pa, pb);
}

/*
 * skvm_matrix_translate function.
 */
void skvm_matrix_translate(int32_t m, double tx, double ty) {
  
  SKMAT *pm = NULL;
  SKMAT ma;
  SKMAT mb;
  
  /* Initialize structures */
  memset(&ma, 0, sizeof(SKMAT));
  memset(&mb, 0, sizeof(SKMAT));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((m < 0) || (m >= m_matc) ||
      (!isfinite(tx)) ||
      (!isfinite(ty))) {
    abort();
  }
  
  /* Only proceed if non-zero translation */
  if ((tx != 0.0) || (ty != 0.0)) {
  
    /* Get the selected matrix */
    pm = &(m_pmat[m]);
    
    /* Copy the matrix to a local variable */
    memcpy(&mb, pm, sizeof(SKMAT));
    
    /* Initialize transform matrix to identity with nothing cached */
    ma.a = 1.0; ma.b = 0.0; ma.c = 0.0;
    ma.d = 0.0; ma.e = 1.0; ma.f = 0.0;
    
    ma.cached = (uint8_t) 0;
    
    /* Set up the translation transform */
    ma.c = tx;
    ma.f = ty;
    
    /* Premultiply by transform and store result in register */
    matrix_mul(pm, &ma, &mb);
  }
}

/*
 * skvm_matrix_scale function.
 */
void skvm_matrix_scale(int32_t m, double sx, double sy) {
  
  SKMAT *pm = NULL;
  SKMAT ma;
  SKMAT mb;
  
  /* Initialize structures */
  memset(&ma, 0, sizeof(SKMAT));
  memset(&mb, 0, sizeof(SKMAT));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((m < 0) || (m >= m_matc) ||
      (!isfinite(sx)) ||
      (!isfinite(sy)) ||
      (sx == 0.0) || (sy == 0.0)) {
    abort();
  }
  
  /* Only proceed if non-trivial scaling */
  if ((sx != 1.0) || (sy != 1.0)) {
  
    /* Get the selected matrix */
    pm = &(m_pmat[m]);
    
    /* Copy the matrix to a local variable */
    memcpy(&mb, pm, sizeof(SKMAT));
    
    /* Initialize scaling matrix to identity with nothing cached */
    ma.a = 1.0; ma.b = 0.0; ma.c = 0.0;
    ma.d = 0.0; ma.e = 1.0; ma.f = 0.0;
    
    ma.cached = (uint8_t) 0;
    
    /* Set up the scaling transform */
    ma.a = sx;
    ma.e = sy;
    
    /* Premultiply by transform and store result in register */
    matrix_mul(pm, &ma, &mb);
  }
}

/*
 * skvm_matrix_rotate function.
 */
void skvm_matrix_rotate(int32_t m, double deg) {
  
  SKMAT *pm = NULL;
  SKMAT ma;
  SKMAT mb;
  
  /* Initialize structures */
  memset(&ma, 0, sizeof(SKMAT));
  memset(&mb, 0, sizeof(SKMAT));
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameters */
  if ((m < 0) || (m >= m_matc) || (!isfinite(deg))) {
    abort();
  }
  
  /* Reduce angle to range (-360.0, 360.0) and convert to radians */
  if (deg != 0.0) {
    deg = fmod(deg, 360.0);
  }
  if (deg != 0.0) {
    deg = (deg * M_PI) / 180.0;
  }
  
  /* Only proceed if rotation is not zero */
  if (deg != 0.0) {
  
    /* Get the selected matrix */
    pm = &(m_pmat[m]);
    
    /* Copy the matrix to a local variable */
    memcpy(&mb, pm, sizeof(SKMAT));
    
    /* Initialize rotation matrix to identity with nothing cached */
    ma.a = 1.0; ma.b = 0.0; ma.c = 0.0;
    ma.d = 0.0; ma.e = 1.0; ma.f = 0.0;
    
    ma.cached = (uint8_t) 0;
    
    /* Set up the rotation transform (deg has been converted to radians
     * already) */
    ma.a = cos(deg);
    ma.b = -(sin(deg));
    
    ma.d = sin(deg);
    ma.e = cos(deg);
    
    /* Premultiply by transform and store result in register */
    matrix_mul(pm, &ma, &mb);
  }
}

/*
 * skvm_sample function.
 */
void skvm_sample(SKVM_SAMPLE_PARAM *ps) {
  
  int     i = 0;
  int32_t x = 0;
  int32_t y = 0;
  int32_t stride = 0;
  double  mv = 0.0;
  
        uint8_t * pt = NULL;
  const uint8_t * pm = NULL;
        uint8_t * pscan = NULL;
  const uint8_t * pscan_m = NULL;
  
  const SKBUF * pSrc = NULL;
  const SKBUF * pMask = NULL;
        SKMAT * pMatrix = NULL;
        SKBUF * pTarget = NULL;
  
  SKPOINT pnt;
  SKARGB  rcol;
  SKARGB  tcol;
  SKARGB  fcol;
  SKPOINT corners[4];
  SPH_ARGB argb;
  
  double f_min_x = 0.0;
  double f_min_y = 0.0;
  double f_max_x = 0.0;
  double f_max_y = 0.0;
  
  int32_t bound_x = 0;
  int32_t bound_y = 0;
  int32_t min_x = 0;
  int32_t min_y = 0;
  int32_t max_x = 0;
  int32_t max_y = 0;
  
  /* Initialize arrays and structures */
  memset(&pnt, 0, sizeof(SKPOINT));
  memset(&rcol, 0, sizeof(SKARGB));
  memset(&tcol, 0, sizeof(SKARGB));
  memset(&fcol, 0, sizeof(SKARGB));
  memset(&argb, 0, sizeof(SPH_ARGB));
  memset(corners, 0, sizeof(SKPOINT) * 4);
  
  /* Check state */
  if (!m_init) {
    abort();
  }
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* ===================================== *
   *                                       *
   * INTERNAL PARAMETER CONSISTENCY CHECKS *
   *                                       *
   * ===================================== */
  
  /* Check that either procedural masking or raster masking mode
   * selected, but not both */
  if ((ps->flags & SKVM_FLAG_PROCMASK) &&
      (ps->flags & SKVM_FLAG_RASTERMASK)) {
    abort();
  }
  if (((ps->flags & SKVM_FLAG_PROCMASK) == 0) &&
      ((ps->flags & SKVM_FLAG_RASTERMASK) == 0)) {
    abort();
  }
  
  /* If procedural mode, check that exactly one flag from each mode
   * group is selected */
  if (ps->flags & SKVM_FLAG_PROCMASK) {
    if ((ps->flags & SKVM_FLAG_LEFTMODE) &&
        (ps->flags & SKVM_FLAG_RIGHTMODE)) {
      abort();
    }
    if (((ps->flags & SKVM_FLAG_LEFTMODE) == 0) &&
        ((ps->flags & SKVM_FLAG_RIGHTMODE) == 0)) {
      abort();
    }
    
    if ((ps->flags & SKVM_FLAG_ABOVEMODE) &&
        (ps->flags & SKVM_FLAG_BELOWMODE)) {
      abort();
    }
    if (((ps->flags & SKVM_FLAG_ABOVEMODE) == 0) &&
        ((ps->flags & SKVM_FLAG_BELOWMODE) == 0)) {
      abort();
    }
  }
  
  /* Check that the buffer indices are valid */
  if ((ps->src_buf < 0) || (ps->src_buf >= m_bufc) ||
      (ps->target_buf < 0) || (ps->target_buf >= m_bufc)) {
    abort();
  }
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    if ((ps->mask_buf < 0) || (ps->mask_buf >= m_bufc)) {
      abort();
    }
  }
  
  /* Check that buffer indices are unique */
  if (ps->src_buf == ps->target_buf) {
    abort();
  }
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    if ((ps->src_buf == ps->mask_buf) ||
        (ps->target_buf == ps->mask_buf)) {
      abort();
    }
  }
  
  /* Check that matrix buffer is valid */
  if ((ps->t_matrix < 0) || (ps->t_matrix >= m_matc)) {
    abort();
  }
  
  /* If procedural masking, check that boundary fields are finite and
   * in normalized range */
  if (ps->flags & SKVM_FLAG_PROCMASK) {
    if ((!isfinite(ps->x_boundary)) || (!isfinite(ps->y_boundary))) {
      abort();
    }
    if ((!((ps->x_boundary >= 0.0) && (ps->x_boundary <= 1.0))) ||
        (!((ps->y_boundary >= 0.0) && (ps->y_boundary <= 1.0)))) {
      abort();
    }
  }
  
  /* Check that sampling algorithm is valid */
  if ((ps->sample_alg != SKVM_ALG_NEAREST) &&
      (ps->sample_alg != SKVM_ALG_BILINEAR) &&
      (ps->sample_alg != SKVM_ALG_BICUBIC)) {
    abort();
  }
  
  /* ======================================= *
   *                                         *
   * GET BUFFERS AND FINISH PARAMETER CHECKS *
   *                                         *
   * ======================================= */
  
  /* Get the buffers */
  pSrc    = &(m_pbuf[ps->src_buf   ]);
  pTarget = &(m_pbuf[ps->target_buf]);
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    pMask = &(m_pbuf[ps->mask_buf  ]);
  }
  
  /* If subarea mode, check that subarea is within the source buffer,
   * else set the subarea fields to encompass the whole source buffer */
  if (ps->flags & SKVM_FLAG_SUBAREA) {
    /* Subarea specified, so first check that (x,y) coordinates are
     * within buffer */
    if ((ps->src_x < 0) || (ps->src_x >= pSrc->w)) {
      abort();
    }
    if ((ps->src_y < 0) || (ps->src_y >= pSrc->h)) {
      abort();
    }
    
    /* Next, check that width and height are greater than zero */
    if ((ps->src_w < 1) || (ps->src_h < 1)) {
      abort();
    }
    
    /* Finally, check that (x + width) <= src_width and also that
     * (y + height) <= src_height */
    if ((ps->src_x > pSrc->w - ps->src_w) ||
        (ps->src_y > pSrc->h - ps->src_h)) {
      abort();
    }
    
  } else {
    /* No subarea specified, so set to whole source buffer */
    ps->src_x = 0;
    ps->src_y = 0;
    ps->src_w = pSrc->w;
    ps->src_h = pSrc->h;
  }
  
  /* If raster masking is in effect, mask buffer must have exact same
   * dimensions as target buffer, and mask buffer must be grayscale */
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    if ((pMask->w != pTarget->w) || (pMask->h != pTarget->h)) {
      abort();
    }
    if (pMask->c != 1) {
      abort();
    }
  }
  
  /* All buffers must be loaded */
  if ((pSrc->pData == NULL) || (pTarget->pData == NULL)) {
    abort();
  }
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    if (pMask->pData == NULL) {
      abort();
    }
  }
  
  /* ========================== *
   *                            *
   * DETERMINE RENDERING BOUNDS *
   *                            *
   * ========================== */
  
  /* Get the transformation matrix */
  pMatrix = &(m_pmat[ps->t_matrix]);
  
  /* Write the corners of the source area into the corner array */
  corners[0].x = ps->src_x;
  corners[0].y = ps->src_y;
  
  corners[1].x = ps->src_x + ps->src_w;
  corners[1].y = ps->src_y;
  
  corners[2].x = ps->src_x;
  corners[2].y = ps->src_y + ps->src_h;
  
  corners[3].x = ps->src_x + ps->src_w;
  corners[3].y = ps->src_y + ps->src_h;
  
  /* Transform all the corners from source space to target space */
  source2target(pMatrix, &(corners[0]));
  source2target(pMatrix, &(corners[1]));
  source2target(pMatrix, &(corners[2]));
  source2target(pMatrix, &(corners[3]));
  
  /* Figure out the bounding box around the transformed corner
   * coordinates */
  f_min_x = corners[0].x;
  f_max_x = corners[0].x;
  
  f_min_y = corners[0].y;
  f_max_y = corners[0].y;
  
  for(i = 1; i < 4; i++) {
    if (corners[i].x > f_max_x) {
      f_max_x = corners[i].x;
    } else if (corners[i].x < f_min_x) {
      f_min_x = corners[i].x;
    }
    
    if (corners[i].y > f_max_y) {
      f_max_y = corners[i].y;
    } else if (corners[i].y < f_min_y) {
      f_min_y = corners[i].y;
    }
  }

  /* Take the floor of the minimums and the ceiling of the maximums to
   * expand the bounding box outward to encompass whole pixels */
  f_min_x = floor(f_min_x);
  f_min_y = floor(f_min_y);
  
  f_max_x = ceil(f_max_x);
  f_max_y = ceil(f_max_y);
  
  /* Make sure these boundary coordinates are finite and within range of
   * signed 32-bit integers */
  if ((!isfinite(f_min_x)) || (!isfinite(f_min_y)) ||
      (!isfinite(f_max_x)) || (!isfinite(f_max_y))) {
    fprintf(stderr, "Numeric problem during sparkle sampling!\n");
    abort();
  }
  
  if ((f_min_x < (double) INT32_MIN) ||
        (f_max_x > (double) INT32_MAX) ||
        (f_min_y < (double) INT32_MIN) ||
        (f_max_y > (double) INT32_MAX)) {
    abort();
  }
  
  /* Now convert to integers */
  min_x = (int32_t) f_min_x;
  min_y = (int32_t) f_min_y;
  
  max_x = (int32_t) f_max_x;
  max_y = (int32_t) f_max_y;

  /* Consistency check */
  if ((min_x > max_x) || (min_y > max_y)) {
    fprintf(stderr, "Numeric problem during sparkle sampling!\n");
    abort();
  }
  
  /* If the maximum X is negative or the maximum Y is negative, then
   * the intersection with the target area is empty so we can leave */
  if ((max_x < 0) || (max_y < 0)) {
    return;
  }
  
  /* If the minimum X is greater than or equal to the width or the
   * minimum Y is greater than or equal to the height, then the
   * intersection with the target area is empty so we can leave */
  if ((min_x >= pTarget->w) || (min_y >= pTarget->h)) {
    return;
  }
  
  /* If we got here, maximum X and maximum Y are at least zero, so clamp
   * them so that they are at most one less than the width or height */
  if (max_x > pTarget->w - 1) {
    max_x = pTarget->w - 1;
  }
  if (max_y > pTarget->h - 1) {
    max_y = pTarget->h - 1;
  }
  
  /* If we got here, minimum X and minimum Y are less than the width or
   * height respectively, so clamp them so that they are at least
   * zero */
  if (min_x < 0) {
    min_x = 0;
  }
  if (min_y < 0) {
    min_y = 0;
  }
  
  /* If we are in procedural masking mode, intersect the rendering area
   * with the part of the target that is not masked */
  if (ps->flags & SKVM_FLAG_PROCMASK) {
    
    /* Get integer versions of the bounds */
    if (ps->x_boundary == 0.0) {
      bound_x = 0;
      
    } else if (ps->x_boundary == 1.0) {
      bound_x = pTarget->w - 1;
      
    } else {
      bound_x = (int32_t) floor(ps->x_boundary *
                                  ((double) (pTarget->w - 1)));
    }
    
    if (ps->y_boundary == 0.0) {
      bound_y = 0;
      
    } else if (ps->y_boundary == 1.0) {
      bound_y = pTarget->h - 1;
      
    } else {
      bound_y = (int32_t) floor(ps->y_boundary *
                                  ((double) (pTarget->h - 1)));
    }

    /* Handle the X boundary based on left or right mode */
    if (ps->flags & SKVM_FLAG_LEFTMODE) {
      /* Left mode, so only draw area where x >= boundary; if max_x is
       * less than boundary, nothing to draw so leave */
      if (max_x < bound_x) {
        return;
      }
      
      /* If we got here, max_x is in bounds, so now just clamp min_x to
       * boundary */
      if (min_x < bound_x) {
        min_x = bound_x;
      }
      
    } else if (ps->flags & SKVM_FLAG_RIGHTMODE) {
      /* Right mode, so only draw area where x <= boundary; if min_x is
       * greater than boundary, nothing to draw so leave */
      if (min_x > bound_x) {
        return;
      }
      
      /* If we got here, min_x is in bounds, so now just clamp max_x to
       * boundary */
      if (max_x > bound_x) {
        max_x = bound_x;
      }
      
    } else {
      /* Shouldn't happen */
      abort();
    }
    
    /* Handle the Y boundary based on above or below mode */
    if (ps->flags & SKVM_FLAG_BELOWMODE) {
      /* Below mode, so only draw area where y <= boundary; if min_y is
       * greater than boundary, nothing to draw so leave */
      if (min_y > bound_y) {
        return;
      }
      
      /* If we got here, min_y is in bounds, so now just clamp max_y to
       * boundary */
      if (max_y > bound_y) {
        max_y = bound_y;
      }
      
    } else if (ps->flags & SKVM_FLAG_ABOVEMODE) {
      /* Above mode, so only draw area where y >= boundary; if max_y is
       * less than boundary, nothing to draw so leave */
      if (max_y < bound_y) {
        return;
      }
      
      /* If we got here, max_y is in bounds, so now just clamp min_y to
       * boundary */
      if (min_y < bound_y) {
        min_y = bound_y;
      }
      
    } else {
      /* Shouldn't happen */
      abort();
    }
  }
  
  /* ============== *
   *                *
   * RENDERING LOOP *
   *                *
   * ============== */
  
  /* Compute the stride between scanlines within the target pixel data,
   * and also establish pt as a pointer to the start of the first pixel
   * to render in the target buffer */
  stride = pTarget->w * ((int32_t) pTarget->c);
  pt = pTarget->pData;
  pt += (stride * min_y);
  pt += (min_x * ((int32_t) pTarget->c));
  
  /* Establish pm as a pointer to the first pixel mask value in the
   * raster mask, if raster masking is enabled */
  if (ps->flags & SKVM_FLAG_RASTERMASK) {
    pm = pMask->pData;
    pm += (pMask->w * min_y);
    pm += min_x;
  }
  
  /* We will now iterate over every pixel in the rendering boundaries of
   * the target, which we just computed in order to perform the
   * rendering operation */
  for(y = min_y; y <= max_y; y++) {
    /* Save the pointer to the start of this rendering scanline */
    pscan = pt;
    if (ps->flags & SKVM_FLAG_RASTERMASK) {
      pscan_m = pm;
    }
    
    for(x = min_x; x <= max_x; x++) {
      /* If raster masking mode is on, proceed to next pixel without
       * rendering if this mask value is zero */
      if (ps->flags & SKVM_FLAG_RASTERMASK) {
        if (*pm == 0) {
          /* Move to the next pixel to render */
          pt = pt + pTarget->c;
          pm++;
         
          /* Continue loop */
          continue;
        }
      }
      
      /* Project the current target location into source space */
      pnt.x = (double) x;
      pnt.y = (double) y;
      
      target2source(pMatrix, &pnt);
      
      if ((!isfinite(pnt.x)) || (!isfinite(pnt.y))) {
        fprintf(stderr, "Numeric problem during sparkle sampling!\n");
        abort();
      }
      
      /* Only proceed if projected point is within the source area */
      if ((pnt.x >= (double) ps->src_x) &&
          (pnt.x <= (double) (ps->src_x + ps->src_w)) &&
          (pnt.y >= (double) ps->src_y) &&
          (pnt.y <= (double) (ps->src_y + ps->src_h))) {
      
        /* Sample the point within the source */
        if (ps->sample_alg == SKVM_ALG_NEAREST) {
          sample_nearest(pSrc, &pnt, &rcol);
          
        } else if (ps->sample_alg == SKVM_ALG_BILINEAR) {
          sample_bilinear(pSrc, &pnt, &rcol);
          
        } else if (ps->sample_alg == SKVM_ALG_BICUBIC) {
          sample_bicubic(pSrc, &pnt, &rcol);
          
        } else {
          /* Shouldn't happen */
          abort();
        }
      
        /* If raster masking is in effect and the current mask value is
         * not full white, then multiply all of the (premultiplied)
         * channels by the normalized mask value to make them more
         * transparent */
        if (ps->flags & SKVM_FLAG_RASTERMASK) {
          if (*pm != 255) {
            mv = ((double) *pm) / 255.0;
            rcol.a *= mv;
            rcol.r *= mv;
            rcol.g *= mv;
            rcol.b *= mv;
          }
        }
      
        /* Get the current target pixel color and convert it to
         * premultiplied ARGB */
        if (pTarget->c == 1) {
          /* Grayscale conversion */
          tcol.a = 1.0;
          tcol.r = ((double) *pt) / 255.0;
          tcol.g = tcol.r;
          tcol.b = tcol.r;
          
        } else if (pTarget->c == 3) {
          /* RGB conversion */
          tcol.a = 1.0;
          tcol.r = ((double) pt[0]) / 255.0;
          tcol.g = ((double) pt[1]) / 255.0;
          tcol.b = ((double) pt[2]) / 255.0;
          
        } else if (pTarget->c == 4) {
          /* Non-premultiplied ARGB to premultiplied ARGB conversion */
          tcol.a = ((double) pt[0]) / 255.0;
          tcol.r = ((double) pt[1]) / 255.0;
          tcol.g = ((double) pt[2]) / 255.0;
          tcol.b = ((double) pt[3]) / 255.0;
          
          tcol.r = tcol.r * tcol.a;
          tcol.g = tcol.g * tcol.a;
          tcol.b = tcol.b * tcol.a;
          
        } else {
          /* Shouldn't happen */
          abort();
        }
      
        /* Composite rcol OVER tcol and store result in fcol */
        fcol.a = rcol.a + (tcol.a * (1.0 - rcol.a));
        fcol.r = rcol.r + (tcol.r * (1.0 - rcol.a));
        fcol.g = rcol.g + (tcol.g * (1.0 - rcol.a));
        fcol.b = rcol.b + (tcol.b * (1.0 - rcol.a));
      
        /* Check for finite results */
        if ((!isfinite(fcol.a)) ||
            (!isfinite(fcol.r)) ||
            (!isfinite(fcol.g)) |
            (!isfinite(fcol.b))) {
          fprintf(stderr, "Numeric problem during sparkle sampling!\n");
          abort();
        }
      
        /* Store to target buffer depending on channel count */
        if (pTarget->c == 1) {
          /* Grayscale, so we know alpha channel should be fully opaque
           * since background pixel was fully opaque; begin by writing
           * each of the color channels into the integer ARGB structure
           * with the opacity set to 255 */
          argb.a = 255;
          argb.r = (int) floor(fcol.r * 255.0);
          argb.g = (int) floor(fcol.g * 255.0);
          argb.b = (int) floor(fcol.b * 255.0);
          
          /* Clamp channels */
          if (argb.r < 0) {
            argb.r = 0;
          } else if (argb.r > 255) {
            argb.r = 255;
          }
          
          if (argb.g < 0) {
            argb.g = 0;
          } else if (argb.g > 255) {
            argb.g = 255;
          }
          
          if (argb.b < 0) {
            argb.b = 0;
          } else if (argb.b > 255) {
            argb.b = 255;
          }
          
          /* Down-convert to grayscale */
          sph_argb_downGray(&argb);
          
          /* Store the resulting grayscale value */
          *pt = (uint8_t) argb.g;
          
        } else if (pTarget->c == 3) {
          /* RGB, so we know alpha channel should be fully opaque since
           * background pixel was fully opaque; begin by writing each of
           * the color channels into the integer ARGB structure with the
           * opacity set to 255 */
          argb.a = 255;
          argb.r = (int) floor(fcol.r * 255.0);
          argb.g = (int) floor(fcol.g * 255.0);
          argb.b = (int) floor(fcol.b * 255.0);
          
          /* Clamp channels */
          if (argb.r < 0) {
            argb.r = 0;
          } else if (argb.r > 255) {
            argb.r = 255;
          }
          
          if (argb.g < 0) {
            argb.g = 0;
          } else if (argb.g > 255) {
            argb.g = 255;
          }
          
          if (argb.b < 0) {
            argb.b = 0;
          } else if (argb.b > 255) {
            argb.b = 255;
          }
          
          /* Store the RGB value */
          pt[0] = (uint8_t) argb.r;
          pt[1] = (uint8_t) argb.g;
          pt[2] = (uint8_t) argb.b;
          
        } else if (pTarget->c == 4) {
          /* ARGB, so we need to convert back to non-premultiplied
           * before storing; first of all, get the integer value for the
           * alpha channel, which is the same in both representations */
          argb.a = (int) floor(fcol.a);
          
          /* Clamp alpha */
          if (argb.a < 0) {
            argb.a = 0;
          } else if (argb.a > 255) {
            argb.a = 255;
          }
          
          /* Conversion depends on whether alpha channel is zero */
          if (argb.a < 1) {
            /* Alpha channel is zero, so store transparent black */
            pt[0] = (uint8_t) 0;
            pt[1] = (uint8_t) 0;
            pt[2] = (uint8_t) 0;
            pt[3] = (uint8_t) 0;
            
          } else {
            /* Alpha channel is non-zero, and we know it's not so close
             * to zero that it would cause numeric problems, so convert
             * the other channels to non-premultiplied */
            fcol.r = fcol.r / fcol.a;
            fcol.g = fcol.g / fcol.a;
            fcol.b = fcol.b / fcol.a;
            
            /* Finite check */
            if ((!isfinite(fcol.r)) ||
                (!isfinite(fcol.g)) ||
                (!isfinite(fcol.b))) {
              fprintf(stderr,
                "Numeric problem during sparkle sampling!\n");
              abort();
            }
            
            /* Clamp to range in float */
            if (!(fcol.r <= 1.0)) {
              fcol.r = 1.0;
            } else if (!(fcol.r >= 0.0)) {
              fcol.r = 0.0;
            }
            
            if (!(fcol.g <= 1.0)) {
              fcol.g = 1.0;
            } else if (!(fcol.g >= 0.0)) {
              fcol.g = 0.0;
            }
            
            if (!(fcol.b <= 1.0)) {
              fcol.b = 1.0;
            } else if (!(fcol.b >= 0.0)) {
              fcol.b = 0.0;
            }
            
            /* Convert to integer channels */
            argb.a = (int) floor(fcol.a * 255.0);
            argb.r = (int) floor(fcol.r * 255.0);
            argb.g = (int) floor(fcol.g * 255.0);
            argb.b = (int) floor(fcol.b * 255.0);
            
            /* Clamp channels */
            if (argb.a < 0) {
              argb.a = 0;
            } else if (argb.a > 255) {
              argb.a = 255;
            }
            
            if (argb.r < 0) {
              argb.r = 0;
            } else if (argb.r > 255) {
              argb.r = 255;
            }
            
            if (argb.g < 0) {
              argb.g = 0;
            } else if (argb.g > 255) {
              argb.g = 255;
            }
            
            if (argb.b < 0) {
              argb.b = 0;
            } else if (argb.b > 255) {
              argb.b = 255;
            }
            
            /* Store the ARGB value */
            pt[0] = (uint8_t) argb.a;
            pt[1] = (uint8_t) argb.r;
            pt[2] = (uint8_t) argb.g;
            pt[3] = (uint8_t) argb.b;
          }
          
        } else {
          /* Shouldn't happen */
          abort();
        }
      }
      
      /* Move to the next pixel to render */
      pt = pt + pTarget->c;
      if (ps->flags & SKVM_FLAG_RASTERMASK) {
        pm++;
      }
    }
    
    /* If this is not the last rendering iteration, move to the next
     * rendering scanline using the pointer we saved at the start of
     * this loop iteration */
    if (y < max_y) {
      pt = pscan + stride;
      if (ps->flags & SKVM_FLAG_RASTERMASK) {
        pm = pscan_m + pMask->w;
      }
    }
  }
}
