This is a hobby project, aiming to implement a MPEG-1 Video bitstream decoder, according to ISO/IEC 11172. It is intended as self-educational project and is currently under development.

The decoding process includes the following steps:
 - Parsing bitstream (read GOP, picture, slice and macroblock headers)
 - Run-level (Huffman-based) decoding on coded blocks
 - Dequantization
 - Inverse Discrete Cosine Transform (iDCT)
 - Macroblock predictions 
 - Motion compensation
 - Convert YUV image to RGB or other color space (optional)

So far it supports I-frames and has partial support for P frames. B and D frames are not supported at this point. The internal pixel format used is NV12.

