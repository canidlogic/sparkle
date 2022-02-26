# Sparkle Specification

The Sparkle renderer is an interpreter for a scripted dialect of Shastina.  This document describes the format of the script that is interpreted by Sparkle.

## Shastina subset

Sparkle is based on Shastina beta 0.9.3 or compatible.  Except for the header, only the following Shastina entities are supported:

- `EOF`
- `STRING` (quoted only, no prefixes)
- `NUMERIC`
- `OPERATION`

Any other entity encountered anywhere except the header will cause parsing to fail with an unsupported Shastina entity error.

## Header

Sparkle scripts begin with a header.  The header has the following format:

    %sparkle;
    %bufcount 25;
    %matcount 2;

The `%sparkle;` metacommand must always be the first thing in a Sparkle script, or the script is not a Sparkle script.  The other two metacommands are optional and may occur in any order, though each may occur only once.  Both `%bufcount` and `%matcount` take a unsigned decimal integer parameter.  If not specified, they default to zero.

The `%bufcount` indicates how many buffer registers will be allocated, and the `%matcount` indicates how many matrix registers will be allocated.  These values are passed through to the `skvm_init()` function defined in `skvm.h`.  Their maximum values are determined by the `SKVM_MAX_BUFC` and `SKVM_MAX_MATC` constants defined in that header.

The header ends when the first entity is encountered that is _not_ one of the following:

- `BEGIN_META`
- `END_META`
- `META_TOKEN`

## Body

The body of a Sparkle script encodes a sequence of function calls into the `skvm.h` module, which performs all the Sparkle rendering operations.  The body ends with the `EOF` entity `|;` which must be the last thing in the file (except for optional trailing whitespace and line breaks).  Within the body, there are four types of elements that may be encountered:

1. Integer literal
2. Float literal
3. String literal
4. Operation

The three literal elements push a value onto the interpreter stack.  The operation element pops arguments off the interpreter stack, runs an operation, and optionally pushes results back onto the interpreter stack.  At the end of interpretation, the interpreter stack must be empty or an error occurs.

An __integer literal__ is a `NUMERIC` Shastina entity that only contains decimal digits and sign characters `+` and `-`.  All other `NUMERIC` Shastina entities are __float literals__ which should contain either a decimal point `.` or an exponent `e` or `E` or both a decimal point and an exponent.  Integer literals must have the format of an optional sign followed by a sequence of one or more decimal digits.  Float literals are parsed with the standard library `strtod()` function and must resolve to a finite value.

Integer literals are automatically promoted to floats if an operation is expecting a float argument but is given an integer argument.  However, providing a float when an integer is expected by an operation will result in an error.

A __string literal__ is a `STRING` Shastina entity that must be double-quoted without any prefix.  The string may only contain US-ASCII visible, printing characters and space characters.  The `\` character is an escape character, with two escapes supported:  `\\` is an escape for a literal `\` character, and `\"` is an escape for a literal double quote.  Empty string literals are supported, which push an empty string onto the interpreter stack.

The __operation__ entities represent rendering operations that will be performed by the Sparkle renderer.  The supported operations are documented in the next section.

## Operations

This section describes all the supported Sparkle operations, categorized by function.

### Diagnostic operations

For diagnostic purposes, you may log messages to the console with the following operation:

    [msg] print -

The `[msg]` parameter must be a string.  It is written to standard error, prefixed by an indication that this message was logged by the script.  At the end of the message, a line break is inserted.  The message is logged the moment the operation is encountered while interpreting the script.

### Load and store operations

The _load and store_ operations are responsible for reading data from external image files into memory, and writing data from memory into external image files.  PNG and JPEG image files are supported.  Raw Motion-JPEG (M-JPEG) files are also supported for both reading and writing, but note that Sparkle only supports _raw_ streams; M-JPEG encapsulated within a container such as AVI is _not_ supported.  Note that only the PNG format allows for transparency, so if you want to load or store an alpha channel, you must use PNG.

When read into memory, image data is stored within _buffer_ registers.  The total number of buffer registers available during interpretation is determined by the `%bufcount` declaration within the header.  Buffer registers numbered zero up to one less than the `%bufcount` value will be available.

Buffer registers always store image dimensions and a channel count.  The image dimensions are the width and height of the image stored within that buffer, counted in pixels.  Both width and height must be at least one, and both dimensions may not exceed the `SKVM_MAX_DIM` value defined in the `skvm.h` header.  The channel count must be either 1, 3, or 4.  A channel count of one means a grayscale image.  A channel count of three means an RGB image.  A channel count of four means an ARGB image (with an alpha channel).

Each buffer register may be either in an _unloaded_ or _loaded_ state.  In an unloaded state, the buffer register stores the image dimensions and the channel count, but no actual image data is loaded.  In a loaded state, the buffer register has the image dimensions, a channel count, and an actual memory buffer filled with image data.  Naturally, an unloaded buffer register consumes much less memory than a loaded buffer register.

Upon initialization, all buffer registers are set to image dimensions of one by one, channel counts of one, and are in unloaded state.  You can change the image dimension and channel count of a register by using the `reset` operation:

    [i] [w] [h] [c] reset -

This operation takes a buffer register index parameter `[i]`, an image width `[w]`, an image height `[h]`, and a channel count `[c]`.  Each of these parameters must be integers.  The `reset` operation will unload the buffer register if it is loaded, and then change the image dimensions and channel count.  The buffer register is left in unloaded state.  In addition to preparing a buffer register for loading, this operation can also be used simply to unload data from a buffer register.

You can load image data from a PNG or JPEG file into a buffer using the following operations:

    [i] [path] load_png -
    [i] [path] load_jpeg -

Prior to using these load operations, you must use `reset` on the buffer register to establish the image dimensions and the color channels.  The image dimensions set on the buffer register must exactly match the image dimensions of the file being read or the operation fails on an error.  However, the channel count of the buffer register does _not_ need to match the channel count within the image file.  Colors are converted automatically to the channel count selected by the buffer object.  If color conversion needs to remove an alpha channel to fit into the buffer object, then the transparent colors are mixed against an opaque white background.  If the buffer register is unloaded, it will be unloaded by these operations.  If the buffer register is already loaded, its contents will be overwritten.

The `[i]` parameter is the buffer register index and the `[path]` parameter is the path to the image file.  Note that the underlying Sophistry library requires PNG files to have a `.png` extension or loading will fail.  Note also that the whole image is read into memory, so you may run out of memory if you try to load multiple high-resolution image files at the same time.

You can also load individual frames from a raw Motion-JPEG sequence.  To do this, you must first build an index file of the M-JPEG sequence.  An index is constructed using the `mjpg_index` program from the [mjpg_tools](https://github.com/canidlogic/mjpg-tools) project.  This index file must be in the same directory as the raw Motion-JPEG sequence, and it must have the same name as the raw Motion-JPEG sequence, except for an additional file extension added to the end.  (The `mjpg_index` program will automatically add a `.index` extension.)  You can then use the following operation to load a Motion-JPEG frame:

    [i] [f] [index_path] load_frame -

The `[i]` parameter is the buffer register index.  The `[f]` parameter is the frame index within the M-JPEG sequence, where zero is the first frame.  `[index_path]` is the path to the _index_ file (__not__ the M-JPEG file!)

It is also possible to load a buffer register simply by filling it with a solid color.  The following operation does that:

    [i] [a] [r] [g] [b] fill -

The `[i]` parameter is the buffer register index.  The other parameters give the alpha, red, green, and blue channel values.  Each must be integers in range [0, 255].  If the buffer has fewer than four channels, then the ARGB color will be down-converted before filling.  If the buffer is already loaded, its contents will be blanked to the given color.

You can also store buffers to image files on disk.  The buffer registers in this case must already be loaded or an error occurs.  The following operation stores a buffer register to a PNG file:

    [i] [path] store_png -

The given `[path]` must have a `.png` extension for sake of the underlying Sophistry library.  If the path already exists, it will be overwritten.

JPEG storage is more complicated:

    [i] [path] [q] store_jpeg -
    [i] [path] [q] store_mjpg -

In both of these operations, `[i]` is the buffer register to read from, `[path]` is the path to the file to store to, and `[q]` is the compression quality, which must be an integer (90 is a sensible default value).  The only difference between these two operations is what happens if a file at `[path]` already exists.  The `store_jpeg` operation will overwrite the file if it already exists.  The `store_mjpg` operation, on the other hand, will append the new JPEG file to the end of the file that already exists at that location.  The `store_mjpg` when run multiple times on the same file will thus generate a raw Motion-JPEG (MJPG) sequence.

### Matrix operations

Sparkle has a set of _matrix registers_.  The number of matrix registers available is declared in the header with the `%matcount` directive.

All matrices are 3x3.  Initially, all matrix registers are initialized to the identity matrix:

    | 1 0 0 |
    | 0 1 0 |
    | 0 0 1 |

At any time, you can reset a matrix register back to this identity state by using the following command:

    [m] identity -

The matrix register to reset is identified by `[m]`.

Matrix registers are used as transformation matrices in sampling operations (see later).  The matrices are intended to transform coordinates from the space of a source buffer to the coordinate space of a target register like so:

    | x'|   | t11 t12 t13 |   | x |
    | y'| = | t21 t22 t23 | * | y |
    | 1 |   |  0   0   1  |   | 1 |

Here, (`x`, `y`) is the coordinate in the source buffer space, and (`x'`, `y'`) is the transformed coordinate in the target buffer space.  The third row of the transformation matrix is always `0 0 1` and the inversion of the transformation matrix also always has `0 0 1` in the third row.  Therefore, only the first two rows of the matrix are actually stored.

The inversion of transformation matrices also needs to be available when sampling operations are performed.  Each matrix register has space for a cached inversion, so that the inversion is only computed once.  All the matrix operations provided by Sparkle are invertible, so the inversion will always be available.  The inversion of a transformation matrix is computed as follows:

           | a b c |
       A = | d e f |
           | 0 0 1 |


           |    e       -b     bf - ce |
           | -------  -------  ------- |
           | ae - bd  ae - bd  ae - bd |
           |                           |
           |   -d        a     cd - af |
    A^-1 = | -------  -------  ------- |
           | ae - bd  ae - bd  ae - bd |
           |                           |
           |                           |
           |    0        0        1    |
           |                           |

You can combine matrices using the following operation:

    [m] [a] [b] multiply -

The `[m]` matrix register is set to the result of multiplying the matrix in register `[a]` with the matrix in register `[b]`.  The `[m]` register must be different from the `[a]` and `[b]` registers, but the `[a]` and `[b]` registers are allowed to be the same register.  Note that since coordinate transformation is done by pre-multiplication, matrix `[a]` is the transformation that will be done _after_ the transformation described by matrix `[b]`.  (Matrix multiplication is _not_ commutative, so the order of operands matters.)

The rest of the matrix operations describe specific transformations.  All of these matrix operations take a single matrix register argument, which is then modified by the transformation.  Specifically, the matrix held in the register is _pre-multiplied_ by the matrix describing the transformation, such that the new transformation will be performed _after_ the transformations already encoded in the transformation matrix.

The first transformation available is __translation__ which is invoked by the following operator and pre-multiplies the following matrix to the matrix register:

    [m] [tx] [ty] translate -

    | 1  0  tx |
    | 0  1  ty |
    | 0  0  1  |

This has the effect of adding `[tx]` to each X coordinate, and adding `[ty]` to each Y coordinate.  Both `[tx]` and `[ty]` may have any finite value.

The second transformation available is __scaling__ which is invoked by the following operator and pre-multiplies the following matrix to the matrix register:

    [m] [sx] [sy] scale -

    | sx  0   0 |
    | 0   sy  0 |
    | 0   0   1 |

Setting both scaling values to 1.0 is equivalent to an identity matrix (which does nothing).  Both scaling values may have any finite value except for zero.  Values greater than 1.0 expand coordinates outwards from the axis while values less than 1.0 compress coordinates towards the axis.  If you make a certain scaling value negative, it has the effect of performing scaling according to the absolute value _and_ reflecting coordinates across the axis.  For example, using an `[sx]` value of -1.0 and a `[sy]` value of 1.0 has the effect of reflecting all X coordinates across the Y axis.

The third transformation available is __rotation__ which is invoked by the following operator and pre-multiplies the following matrix to the matrix register:

    [m] [deg] rotate -

    | cos(deg) -sin(deg)  0 |
    | sin(deg)  cos(deg)  0 |
    |    0         0      1 |

The rotation `[deg]` is specified in degrees.  All coordinates are rotated clockwise around the origin by this transformation.  Any finite value can be used for `[deg]`.  A value of zero does nothing.  Values outside the range (-360.0, 360.0) are collapsed into that range with `fmod()` by this function, so you can pass any degree value and it will be reduced appropriately.

### Sampling operations

_Sampling_ is a sophisticated way of drawing one buffer into another buffer, optionally applying effects such as translation, rotation, scaling, and masking along the way.  Because of its complexity, there is not just one sampling operation, but rather a whole group of sampling operations.  The operation `sample` is used to perform an actual sampling operation, while all the other operations are used to configure the various parameters of the sampling.  Parameters are "sticky" so they remain in effect until they are changed and can therefore be reused in subsequent sampling operations.

#### Sampling source

The first parameter in a sampling operation is the _source_ which indicates what will be sampled.  This can either be a whole buffer register or a subarea selected from a buffer register.  The following operations are used to configure the sampling source:

    [i] sample_source -
    [i] [x] [y] [width] [height] sample_source_area -

Initially, the sampling source is set to a special _not configured_ state, and sampling operations will fail if they are attempted when the sampling source is in that state.  You can use the `sample_source` operation to indicate that a whole buffer register should be sampled in sampling operations.  The `[i]` is the buffer register that will be sampled.  With this invocation, `[i]` must merely be a valid buffer register.  It does not have to be loaded, and its dimensions and color channels are irrelevant.  The buffer register is only accessed when the actual `sample` operation takes place, at which point the buffer must be loaded or the sampling operation will fail.  The sampling source established by `sample_source` is sticky and remains until changed by `sample_source` or `sample_source_area`.  The dimensions and color channels of the selected source buffer may change between different sampling operations.

You can also select only a subarea of a specific buffer register as the sampling source.  To do this, use the `sample_source_area` operation, specifying the buffer register, the `[x]` and `[y]` of the top-left corner of the subarea, and the `[width]` and `[height]` of the subarea in pixels.  Each of these parameters must be integers.  `[x]` and `[y]` must both be at least zero and at most one less than the width and height of the buffer register, respectively.  `[width]` and `[height]` must both be at least one, and also such that when added to `[x]` and `[y]` respectively, the result does not exceed the width and height respectively of the buffer register.  In other words, the selected subarea can't have any partial pixels (coordinates must be integers), and the subarea must be contained entirely within the dimensions of the buffer register.

The buffer register selected by `sample_source_area` does not need to be loaded.  However, in contrast to `sample_source`, the dimensions of the buffer register _are_ relevant.  In addition to using the buffer register dimensions to check its subarea parameter values, `sample_source_area` will also record the buffer register dimensions that were in place when it was called.  Then, when the `sample` operation takes place, a check will be made that the source buffer register is both loaded and has the exact same dimensions as when the `sample_source_area` operation was called, with the sampling operation failing if this is not the case.  (The color channels do not need to be the same, though.)  In other words, the subarea established by `sample_source_area` is also sticky, but whenever `sample` is invoked, the source buffer must have the same dimensions as when the subarea was established, in contrast to `sample_source` for which this constraint does not apply.

#### Sampling target

The second parameter in a sampling operation is the _target_ which indicates where the sampled image will be drawn.  You can set the target buffer register with the following operation:

    [i] sample_target -

Initially, the sampling target is set to a special _not configured_ state, and sampling operations will fail if they are attempted when the sampling target is in that state.  You can use this `sample_target` operation to indicate that `[i]` is the buffer register that will be drawn into.  `[i]` must merely be a valid buffer register.  It does not have to be loaded, and its dimensions and color channels are irrelevant.  The buffer register is only accessed when the actual `sample` operation takes place, at which point the buffer must be loaded or the sampling operation will fail.  The sampling target established by `sample_target` is sticky and remains until changed by `sample_target`.  The dimensions and color channels of the selected target buffer may change between different sampling operations.

You are allowed to use `sample_target` to set the same buffer register as is specified for the sampling source.  However, when the actual `sample` operation occurs, the source buffer register and target buffer register must be different &mdash; you are not allowed to sample a buffer register into itself.

Unlike for the sampling source, the sampling target does not allow the specification of subareas.  Instead, you use transformation matrices and masking (described in later subsections), which are much more powerful.

#### Transformation matrix

The way in which the source buffer is sampled and drawn into the target buffer is controlled by a _transformation matrix._  This is a 3x3, two-dimensional matrix that can transform coordinates from the source buffer into coordinates within the target buffer.  The matrix allows for translation, rotation, mirroring, and scaling.

Matrix operations are described separately in the previous section about matrix operations.  Within the sampling module, you simply get an operation that indicates which matrix register holds the transformation matrix:

    [m] sample_matrix -

Initially, the sampling matrix is set to a special _not configured_ state, and sampling operations will fail if they are attempted when the sampling matrix is in that state.  You use this `sample_matrix` operation to indicate that matrix register `[m]` holds the transformation matrix.  The matrix register established by this operation is sticky and remains until changed by `sample_matrix`.

The specific interpretation of the transformation matrix is as follows:

    | x'|   | t11 t12 t13 |   | x |
    | y'| = | t21 t22 t23 | * | y |
    | 1 |   |  0   0   1  |   | 1 |

Here, (`x`, `y`) is a coordinate in the source buffer, and (`x'`, `y'`) is the corresponding coordinate in the target buffer.

For pixels, the origin of the pixel is a floating-point coordinate that is in the center of the pixel area.  So, for example, the pixel (2, 3) has its pixel origin at (2.5, 3.5).

#### Target masking

Before sampled pixels are drawn into the target buffer, they are passed through a _masking layer._  The masking layer can either pass the sampled pixel through, prevent the sampled pixel from being drawn, or adjust the transparency of the sampled pixel before it is drawn.  The masking layer allows for matte effects when compositing the sampled image into the target buffer.

Initially, the masking layer is set to pass all sampled pixels through.  In other words, the default behavior of the masking layer is to have no effect whatsoever on drawing.  At any time, you can reset the masking layer to this default of doing nothing by using the following operation:

    - sample_mask_none -

This operation clears any masking layer currently in effect and replaces it with a masking layer that passes through all sampled pixels.

Two kinds of masking layers are supported:  _procedural masking_ and _raster masking._  Procedural masking is defined by: 

1. X boundary line
2. Y boundary line
3. left or right mode
4. above or below mode

Both the X and Y boundary lines are represented by a floating-point value in range [0.0, 1.0].  For the X boundary line, 0.0 is the left edge of the target and 1.0 is the right edge of the target.  For the Y boundary line, 0.0 is the top edge of the target and 1.0 is the bottom edge of the target.  Values between 0.0 and 1.0 can be used to select any horizontal or vertical position on the target frame.  Since the boundary line positions are specified in the normalized space [0.0, 1.0], the position of these lines is independent from the specific dimensions used in the target layer.

In _above_ mode, all pixels that are above the Y boundary line are prevented from being drawn, while all pixels that are on the Y boundary line or below it are unaffected.  In _below_ mode, all pixels that are below the Y boundary line are prevented from being drawn, while all pixels that are on the Y boundary line or above it are unaffected.

In _left_ mode, all pixels that are left of the X boundary line are prevented from being drawn, while all pixels that are on the X boundary line or right of it are unaffected.  In _right_ mode, all pixels that are right of the X boundary line are prevented from being drawn, while all pixels that are on the X boundary line or left of it are unaffected.

Procedural masking applies both the masking indicated by the X boundary line and left/right mode, as well as the masking indicated by the Y boundary line and above/below mode.  Pixels that are unaffected by either are passed through and drawn, while pixels that are masked out by either the X boundary line or the Y boundary line or both are prevented from being drawn.

The initial masking state selected by `sample_mask_none` is actually a procedural mask where X and Y boundary lines are both set to 0.0, and the modes are left and above.  Since the boundary lines are on the left and top edges of the image, this has the effect of passing through all pixels and masking none of them.

To adjust the procedural masking settings, you can use the following operators:

    [x] sample_mask_x -
    [y] sample_mask_y -
    - sample_mask_left -
    - sample_mask_right -
    - sample_mask_above -
    - sample_mask_below -

The `sample_mask_x` and `sample_mask_y` operators adjust the X and Y boundary lines, respectively.  Both take a floating-point parameter that must be in range [0.0, 1.0].  The remaining four operators switch between left/right modes and above/below modes.  You may not use any of these six procedural masking operators while raster masking is in effect (see below).  Instead, you must first use `sample_mask_none` to get back into procedural masking mode and reset the procedural masking state.

The other form of masking is _raster masking._  To enter raster masking mode, use the following operator:

    [i] sample_mask_raster -

The `[i]` parameter must be an integer that is a valid buffer index.  The buffer does not need to be loaded and its state is irrelevant when `sample_mask_raster` is called.  However, when the actual `sample` operation takes place, the masking buffer must satisfy the following constraints:

1. Masking buffer must not be same as source buffer
2. Masking buffer must not be same as target buffer
3. Masking buffer must be loaded
4. Masking buffer must be grayscale (1-channel)
5. Masking buffer must be exact dimensions of target

The masking buffer associates a grayscale value with each pixel of the target buffer.  If this grayscale value is full black, it means the target pixel is masked and will not be drawn to.  If this grayscale value is full white, it means the target pixel is not masked and can be drawn to.  If the grayscale value is somewhere between black and white, it is an alpha channel multiplier in range (0.0, 1.0) that is multiplied to the alpha channel value of sampled pixels before they are composited into the target buffer, thus making them more transparent to a certain degree.

All procedural masking effects can also be performed with corresponding raster masks, but procedural masking is much more efficient because it doesn't require a potentially large memory buffer for the mask, and it is also faster because certain optimizations can be applied to it that aren't possible with general raster masks.  Therefore, you should only use a raster mask if it is impossible to get the effect with a procedural mask.

To get back to procedural masking mode from raster masking mode, you must use `sample_mask_none` to reset to the initial, procedural masking state.

#### Sampling algorithm

The `sample` operation supports different sampling algorithms.  You can use the following operators to change the sampling mode:

    - sample_nearest -
    - sample_bilinear -
    - sample_bicubic -

The default sampling algorithm is `sample_bilinear`.

The `sample_nearest` mode is nearest-neighbor sampling.  This is the fastest, but it results in a blocky appearance.  However, if you are trying to get a pixel-art effect, this mode may get the best results.

The `sample_bilinear` mode uses bilinear sampling.  This default algorithm is a compromise between speed and quality.  It is much better quality that `sample_nearest` while also being faster than `sample_bicubic`.  However, `sample_bicubic` will usually give smoother results, if you can accept the slower performance.

The `sample_bicubic` mode uses bicubic sampling.  This is usually the highest-quality sampling algorithm, but it is also the slowest.

#### Sample operation

When you have configured all parameters using the operators described in the preceding sections, you can then perform the actual sampling operation using the following operator:

    - sample -

The first step in the sampling procedure is to get the boundary coordinates of the source area.  If `sample_source` was used to establish the source, the source area starts at (0, 0) and proceeds to (_w_, _h_) where _w_ and _h_ are the width and height of the buffer.  If `sample_source_area` was used to establish the source, the source area starts at (_x_, _y_) and proceeds to (_x_ + _w_, _y_ + _h_), where each of these parameters was established by the `sample_source_area` operation.

The coordinates of the four corners of this boundary source area are then projected into target space using the transformation matrix.  A bounding box of these four projected corner points is established in target space.  Then, the computed bounding box is expanded to cover whole pixels in the target space.  Next, the expanded bounding box in target space is intersected with the area of the target buffer.  Finally, if procedural masking is in effect, the intersected bounding box is once again intersected with the rectangular area of the target buffer that _isn't_ masked out.

The result of these steps is a bounding box within the target buffer area that indicates which pixels are affected by the sampling operation, taking into account procedural masking but _not_ raster masking.  This bounding box might be empty, in which case the sampling operation finishes without actually drawing or sampling anything.  Otherwise, each pixel within the bounding box is rendered using the algorithm below.

If raster masking is in effect, the first step is to check whether the raster mask at this location in the target buffer is full black.  If it is, then rendering is skipped and nothing is drawn for this pixel.  Otherwise, the algorithm proceeds as usual.

Rendering requires an inverse matrix derived from the transformation matrix.  This inverse matrix is cached within the matrix register so that it is only computed once.  The inverse matrix indicates how pixels in the target area map to pixels in the source area.  For each pixel being rendered in the target area, the inverse matrix is used to figure out where it is located in the source buffer.  If its location in the source buffer is outside the boundary coordinates of the source area that were computed earlier, then nothing is drawn for this rendered pixel.  Otherwise, the selected sampling algorithm is used to compute a sampled pixel value.

Regardless of the number of color channels used in the source buffer, sampling always works in ARGB mode.  If the sampling buffer is RGB or grayscale, pixels are upconverted to ARGB before being passed to the sampling algorithm.  Furthermore, sampling works with _premultiplied_ alpha, whereas the ARGB stored in source buffers is non-premultiplied, so even if the source buffer is ARGB, it must still be converted to premultiplied alpha.  The result of the sampling algorithm will be a (premultiplied) ARGB color value.  If raster masking is in effect and the grayscale value for this target pixel is less than full white, all of the (premultiplied) ARGB components in the sampled value are multiplied by the normalized grayscale value, which will make it more transparent.

The final step is to composite the sampled pixel into the target buffer.  The current pixel value in the target buffer is read and converted to premultiplied ARGB.  The sampled pixel value is composited over the target buffer value to get a premultiplied ARGB result.  The result is then converted to the color system used by the target buffer and written into the target buffer.
