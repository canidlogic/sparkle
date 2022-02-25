/*
 * skvm.c
 * ======
 * 
 * Implementation of skvm.h
 * 
 * See the header for further information.
 */

#include "skvm.h"
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
 * Structure used to represent a 3x3 matrix:
 * 
 * | v11 v12 v13 |
 * | v21 v22 v23 |
 * | v31 v32 v33 |
 */
typedef struct {
  
  double v11;
  double v12;
  double v13;
  
  double v21;
  double v22;
  double v23;
  
  double v31;
  double v32;
  double v33;
  
} SKMAT;

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
  
  /* Initialize all matrices to identity */
  for(i = 0; i < matc; i++) {
    pm = &(m_pmat[i]);
    
    pm->v11 = 1.0;
    pm->v12 = 0.0;
    pm->v13 = 0.0;
    
    pm->v21 = 0.0;
    pm->v22 = 1.0;
    pm->v23 = 0.0;
    
    pm->v31 = 0.0;
    pm->v32 = 0.0;
    pm->v33 = 1.0;
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
