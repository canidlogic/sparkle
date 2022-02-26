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

#endif
