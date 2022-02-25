#ifndef SKCORE_H_INCLUDED
#define SKCORE_H_INCLUDED

/*
 * skcore.h
 * ========
 * 
 * Core operator module for Sparkle renderer.
 * 
 * To install:
 * 
 *   (1) #include this header in the sparkle.c source file near the top
 *       in the section "Operator modules"
 * 
 *   (2) Invoke skcore_register(); in the register_modules() function in
 *       the sparkle.c source file.
 * 
 *   (3) Compile skcore.c together with the rest of the renderer.
 */

/*
 * The registration function.
 * 
 * Call exactly once from the register_modules() function in the
 * sparkle.c source file.
 */
void skcore_register(void);

#endif
