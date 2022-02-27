#ifndef SKVM_H_INCLUDED
#define SKVM_H_INCLUDED

/*
 * skvm.h
 * ======
 * 
 * Sparkle virtual machine.
 * 
 * See sparkle.c for compilation requirements.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * The maximum number of buffers and matrices that can be declared to
 * skvm_init().
 */
#define SKVM_MAX_BUFC   (4096)
#define SKVM_MAX_MATC   (4096)

/*
 * The maximum number of pixels that may be declared for width or height
 * with skvm_reset().
 */
#define SKVM_MAX_DIM    (16384)

/*
 * Constants for selecting a sampling algorithm.
 */
#define SKVM_ALG_NEAREST  (1)   /* Nearest neighbor */
#define SKVM_ALG_BILINEAR (2)   /* Bilinear */
#define SKVM_ALG_BICUBIC  (3)   /* Bicubic */

/*
 * Flags for the sample operation.
 */
#define SKVM_FLAG_SUBAREA     (1)
#define SKVM_FLAG_PROCMASK    (2)
#define SKVM_FLAG_RASTERMASK  (4)
#define SKVM_FLAG_LEFTMODE    (8)
#define SKVM_FLAG_RIGHTMODE   (16)
#define SKVM_FLAG_ABOVEMODE   (32)
#define SKVM_FLAG_BELOWMODE   (64)

/*
 * Structure storing all the parameters necessary for a sampling
 * operation.
 */
typedef struct {
  
  /*
   * The index of the source buffer to sample from.
   * 
   * Must not be the same as target_buf.  If raster masking is in
   * effect, must not be the same as mask_buf either.
   */
  int32_t src_buf;
  
  /*
   * The index of the target buffer to draw into.
   * 
   * Must not be the same as src_buf.  If raster masking is in effect,
   * must not be the same as mask_buf either.
   */
  int32_t target_buf;
  
  /*
   * The index of the raster masking buffer.
   * 
   * Only relevant if raster masking is in effect, otherwise ignored.
   * If in effect, may not be the same as src_buf or target_buf.
   */
  int32_t mask_buf;
  
  /*
   * The subarea of the source buffer to sample.
   * 
   * Only relevant if subarea mode is in effect, otherwise these will be
   * set at the start of the sampling operation to encompass the whole
   * source buffer.
   */
  int32_t src_x;
  int32_t src_y;
  int32_t src_w;
  int32_t src_h;
  
  /*
   * The index of the transformation matrix.
   * 
   * This matrix, when premultiplied to coordinates in the source area,
   * maps the coordinates to the target space.  The inverse matrix maps
   * from target space to source space.
   */
  int32_t t_matrix;
  
  /*
   * The X boundary line, if procedural masking is in effect.
   * 
   * Must be in normalized range [0.0, 1.0].  Ignored if raster masking
   * is in effect.
   */
  double x_boundary;
  
  /*
   * The Y boundary line, if procedural masking is in effect.
   * 
   * Must be in normalized range [0.0, 1.0].  Ignored if raster masking
   * is in effect.
   */
  double y_boundary;
  
  /*
   * The sampling algorithm to use.
   * 
   * Must be one of the SKVM_ALG_ constants.
   */
  int sample_alg;
  
  /*
   * Various flags, this is a combination of SKVM_FLAG_ constants,
   * combined together with |
   * 
   * If you want to enable subarea mode, use SKVM_FLAG_SUBAREA.  If this
   * flag is enabled, the src_ fields in this structure specify a
   * subarea of the source buffer to sample.  Otherwise, the whole area
   * within the source buffer will be used.
   * 
   * If you are using procedural masking, use the SKVM_FLAG_PROCMASK
   * flag, and combine it with either SKVM_FLAG_LEFTMODE or
   * SKVM_FLAG_RIGHTMODE, and either SKVM_FLAG_ABOVEMODE or
   * SKVM_FLAG_BELOWMODE.  You should have three flags specified, one
   * selecting procedural masking mode, one selecting either left or
   * right mode, and one selecting either above or below mode.  The
   * x_boundary and y_boundary fields will in this case determine where
   * the boundary lines are for the procedural mask.
   * 
   * If you are using raster masking, use the SKVM_FLAG_RASTERMASK flag.
   * In this case, mask_buf selects the buffer that will be used as a
   * raster mask.
   * 
   * You must use either procedural masking or raster masking.  If you
   * don't want any masking, use:
   * 
   *   SKVM_FLAG_PROCMASK | SKVM_FLAG_LEFTMODE | SKVM_FLAG_ABOVEMODE
   * 
   * and then set both x_boundary and y_boundary to 0.0.
   */
  int flags;
  
} SKVM_SAMPLE_PARAM;

/*
 * Initialize the Sparkle virtual machine.
 * 
 * You must call this function before using any of the other functions
 * in this module.  You may only call this function once.
 * 
 * bufc must be in range [0, SKVM_MAX_BUFC] and matc must be in range
 * [0, SKVM_MAX_MATC].
 * 
 * Initially, all buffer objects are declared as 1x1 grayscale and all
 * matrices are declared as the identity matrix.
 * 
 * Parameters:
 * 
 *   bufc - the maximum number of buffer objects
 * 
 *   matc - the maximum number of matrix objects
 */
void skvm_init(int32_t bufc, int32_t matc);

/*
 * Return an error message from the last operation.
 * 
 * Only operations that can return a failure status will affect the
 * reason returned by this function.  If no such operation has yet been
 * performed, or the last such operation did not fail, "No error" is
 * returned.
 * 
 * The returned message is statically allocated and should not be freed.
 * 
 * Return:
 * 
 *   the error message from the last operation
 */
const char *skvm_reason(void);

/*
 * Return the bufc value that was used to initialize skvm_init().
 * 
 * Return:
 * 
 *   the bufc value
 */
int32_t skvm_bufc(void);

/*
 * Return the matc value that was used to initialize skvm_init().
 * 
 * Return:
 * 
 *   the matc value
 */
int32_t skvm_matc(void);

/*
 * Get the current pixel dimensions of a given buffer object.
 * 
 * i is the index of the buffer object to query.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * The width and height of the buffer is written to *pw and *ph.
 * 
 * Parameters:
 * 
 *   i - the buffer to query
 * 
 *   pw - pointer to variable to receive the width
 * 
 *   ph - pointer to variable to receive the height
 */
void skvm_get_dim(int32_t i, int32_t *pw, int32_t *ph);

/*
 * Get the current number of channels of a given buffer object.
 * 
 * i is the index of the buffer object to query.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * Parameters:
 * 
 *   i - the buffer to query
 * 
 * Return:
 * 
 *   the current number of color channels in the buffer, which is either
 *   1 (grayscale), 3 (RGB), or 4 (ARGB)
 */
int skvm_get_channels(int32_t i);

/*
 * Check whether a given buffer object is currently loaded.
 * 
 * i is the index of the buffer object to query.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * Parameters:
 * 
 *   i - the buffer to query
 * 
 * Return:
 * 
 *   non-zero if buffer is currently loaded, zero if not
 */
int skvm_is_loaded(int32_t i);

/*
 * Reset a specific buffer object.
 * 
 * i is the index of the buffer object to reset.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * w and h are the width and height of the buffer object, in pixels.
 * They must both be at least one and at most SKVM_MAX_DIM.
 * 
 * c is the number of channels.  It must be either 1 (grayscale),
 * 3 (RGB), or 4 (ARGB).
 * 
 * If the buffer currently holds something, it will be released.  The
 * buffer will be put into an unloaded state, storing the dimensions and
 * color channels, but not yet having an actual data buffer.  You can
 * also use this function just to release a buffer, in which case you
 * could just set the w, h, and c parameters all to one.
 * 
 * Parameters:
 * 
 *   i - the buffer to reset
 * 
 *   w - the width of the buffer
 * 
 *   h - the height of the buffer
 * 
 *   c - the number of channels in the buffer
 */
void skvm_reset(int32_t i, int32_t w, int32_t h, int c);

/*
 * Read a PNG file and load its contents into a buffer object.
 * 
 * i is the index of the buffer object to load.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * pPath is the path to the PNG file to load.
 * 
 * The buffer must have the exact same dimensions as the buffer object
 * or the operation fails.  If the buffer is already loaded, this
 * function overwrites all the data currently in the buffer.
 * 
 * If the PNG file does not have the same color channel value as the
 * buffer, the colors will automatically be converted.
 * 
 * If this operation fails, skvm_reason() can return an error message.
 * Failure leaves the buffer contents in an undefined state.
 * 
 * Parameters:
 * 
 *   i - the buffer to load
 * 
 *   pPath - the path to the PNG file to read
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int skvm_load_png(int32_t i, const char *pPath);

/*
 * Read a JPEG file and load its contents into a buffer object.
 * 
 * i is the index of the buffer object to load.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * pPath is the path to the JPEG file to load.
 * 
 * The buffer must have the exact same dimensions as the buffer object
 * or the operation fails.  If the buffer is already loaded, this
 * function overwrites all the data currently in the buffer.
 * 
 * If the JPEG file does not have the same color channel value as the
 * buffer, the colors will automatically be converted.
 * 
 * If this operation fails, skvm_reason() can return an error message.
 * Failure leaves the buffer contents in an undefined state.
 * 
 * Parameters:
 * 
 *   i - the buffer to load
 * 
 *   pPath - the path to the JPEG file to read
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int skvm_load_jpeg(int32_t i, const char *pPath);

/*
 * Read a JPEG frame from within a raw Motion-JPEG sequence and load its
 * contents into a buffer object.
 * 
 * i is the index of the buffer object to load.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * f is the frame number within the Motion-JPEG sequence, where zero is
 * the first frame.  The function will fail if the frame index is out of
 * range.
 * 
 * pIndexPath is the path to the Motion-JPEG index file.  The path to
 * the Motion-JPEG file will be derived from this path from removing the
 * last file extension, so there must be at least one file extension in
 * this path or the function will fail.  For example, if "movie.mjpg.ix"
 * is the given index file path, then "movie.mjpg" will be the actual
 * raw Motion-JPEG sequence.
 * 
 * The index file is an array of 64-bit signed integers, each of which
 * must be zero or greater, stored in big-endian order.  The first
 * integer counts the number of integers that follow it in the file
 * (excluding the first integer).  The remaining integers define the
 * locations of JPEG frames within the Motion-JPEG file, and all such
 * integers must be in strictly ascending order.  Each integer is the
 * file offset of where a frame begins.
 * 
 * The buffer must have the exact same dimensions as the buffer object
 * or the operation fails.  If the buffer is already loaded, this
 * function overwrites all the data currently in the buffer.
 * 
 * If the JPEG file does not have the same color channel value as the
 * buffer, the colors will automatically be converted.
 * 
 * If this operation fails, skvm_reason() can return an error message.
 * Failure leaves the buffer contents in an undefined state.
 * 
 * Parameters:
 * 
 *   i - the buffer to load
 * 
 *   pPath - the path to the JPEG file to read
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int skvm_load_mjpg(int32_t i, int32_t f, const char *pIndexPath);

/*
 * Load a buffer object with a solid color.
 * 
 * i is the index of the buffer object to load.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * a, r, g, b are the alpha, red, green, and blue channel values of the
 * color to use as a fill color.  All must be in the range 0-255.  If
 * the color channels in the buffer are less than four, this ARGB color
 * is automatically down-converted.  Alpha is non-premultiplied, and
 * zero means fully transparent.
 * 
 * If the buffer is already loaded, this function overwrites all the
 * data currently in the buffer.  If the buffer is not currently loaded,
 * this function will load the buffer.
 * 
 * Parameters:
 * 
 *   i - the buffer to fill
 * 
 *   a - the alpha value (0-255)
 * 
 *   r - the red value (0-255)
 * 
 *   g - the green value (0-255)
 * 
 *   b - the blue value (0-255)
 */
void skvm_load_fill(int32_t i, int a, int r, int g, int b);

/*
 * Store the contents of a loaded buffer into a PNG file.
 * 
 * i is the index of the buffer object to store.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * If the buffer is not currently loaded, this function will fail.
 * 
 * pPath is the path to the PNG file to write.  If the path already
 * exists, it will be overwritten.
 * 
 * If the function fails, skvm_reason() can retrieve a reason.
 * 
 * Parameters:
 * 
 *   i - the buffer to store
 * 
 *   pPath - path to the PNG file to store to
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int skvm_store_png(int32_t i, const char *pPath);

/*
 * Store the contents of a loaded buffer into a (M-)JPEG file.
 * 
 * i is the index of the buffer object to store.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * If the buffer is not currently loaded, this function will fail.
 * 
 * pPath is the path to the (M-)JPEG file to write.
 * 
 * If mjpg is zero, then we are just writing a simple JPEG file.  If the
 * file at path pPath already exists, it will be overwritten.
 * 
 * If mjpg is non-zero, then we are in M-JPEG mode.  If the file at path
 * pPath already exists, we will append this new frame to the end of it.
 * 
 * If the function fails, skvm_reason() can retrieve a reason.
 * 
 * Parameters:
 * 
 *   i - the buffer to store
 * 
 *   pPath - path to the JPEG file to store to
 * 
 *   mjpg - non-zero to append to file if it already exists; zero to
 *   overwrite
 * 
 *   q - the compression quality
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int skvm_store_jpeg(int32_t i, const char *pPath, int mjpg, int q);

/*
 * Reset a given matrix register to the identity.
 * 
 * m is the index of the matrix register.  It must be at least zero and
 * less than the matc value passed to skvm_init().
 * 
 * Parameters:
 * 
 *   m - the matrix register
 */
void skvm_matrix_reset(int32_t m);

/*
 * Multiply two matrix registers together and store the result in a
 * third.
 * 
 * The operation performed is:
 * 
 *     m = a * b
 * 
 * All three arguments must be matrix registers that are at least zero
 * and less than the matc value passed to skvm_init().
 * 
 * In addition, the m register may not be the same as either the a or b
 * registers.  However, the a and b registers may be the same.
 * 
 * Parameters:
 * 
 *   m - the matrix register to store the result in
 * 
 *   a - the first matrix register operand
 * 
 *   b - the second matrix register operand
 */
void skvm_matrix_multiply(int32_t m, int32_t a, int32_t b);

/*
 * Premultiply a matrix register by a translation transform.
 * 
 * m is the index of the matrix register.  It must be at least zero and
 * less than the matc value passed to skvm_init().
 * 
 * tx and ty are the translations that are done on X coordinates and Y
 * coordinates, respectively.  Both values must be finite.
 * 
 * Parameters:
 * 
 *   m - the matrix register to modify
 * 
 *   tx - the X translation
 * 
 *   ty - the Y translation
 */
void skvm_matrix_translate(int32_t m, double tx, double ty);

/*
 * Premultiply a matrix register by a scaling transform.
 * 
 * m is the index of the matrix register.  It must be at least zero and
 * less than the matc value passed to skvm_init().
 * 
 * sx and sy are the scaling values for the X and Y coordinates,
 * respectively.  They may have any finite, non-zero value.
 * 
 * Parameters:
 * 
 *   m - the matrix register to modify
 * 
 *   sx - the X axis scaling value
 * 
 *   sy - the Y axis scaling value
 */
void skvm_matrix_scale(int32_t m, double sx, double sy);

/*
 * Premultiply a matrix register by a rotation transform.
 * 
 * m is the index of the matrix register.  It must be at least zero and
 * less than the matc value passed to skvm_init().
 * 
 * deg is the clockwise rotation angle in degrees.  It may be any finite
 * value.  Values outside the range (-360.0, 360.0) are automatically
 * reduced to this range by this function.
 * 
 * Parameters:
 * 
 *   m - the matrix register to modify
 * 
 *   deg - the clockwise rotation in degrees
 */
void skvm_matrix_rotate(int32_t m, double deg);

/*
 * Perform a sampling operation.
 * 
 * The parameters for the operation are given in the provided structure.
 * See the structure documentation for further information, and also see
 * SparkleSpec.md for further information about how the sampling
 * operation works.
 * 
 * The given structure may be modified by this procedure.
 * 
 * Parameters:
 * 
 *   ps - the sampling parameters
 */
void skvm_sample(SKVM_SAMPLE_PARAM *ps);

/*
 * Invert all the color channels (except alpha) in a specific buffer.
 * 
 * i is the index of the buffer object to store.  It must be at least
 * zero and less than the bufc value passed to skvm_init().
 * 
 * If the buffer is not currently loaded, a fault will occur.
 * 
 * Parameters:
 * 
 *   i - the buffer to invert
 */
void skvm_color_invert(int32_t i);

#endif
