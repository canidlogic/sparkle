#ifndef SKSAMPLE_H_INCLUDED
#define SKSAMPLE_H_INCLUDED

/*
 * sksample.h
 * ==========
 * 
 * Sample operator module for Sparkle renderer.
 * 
 * To install:
 * 
 *   (1) #include this header in the sparkle.c source file near the top
 *       in the section "Operator modules"
 * 
 *   (2) Invoke sksample_register(); in the register_modules() function
 *       in the sparkle.c source file.
 * 
 *   (3) Compile sksample.c together with the rest of the renderer.
 */

/*
 * The registration function.
 * 
 * Call exactly once from the register_modules() function in the
 * sparkle.c source file.
 */
void sksample_register(void);

#endif
