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
