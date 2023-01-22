jpegtran element applies [tjTransform()](https://rawcdn.githack.com/libjpeg-turbo/libjpeg-turbo/2.1.x/doc/html/group___turbo_j_p_e_g.html#ga9cb8abf4cc91881e04a0329b2270be25) from [libjpeg-turbo](https://libjpeg-turbo.org) on jpeg images.

Lossless transforms work by moving the raw DCT coefficients from one JPEG image structure to another without altering the values of the coefficients.
This is typically faster than decompressing the image, transforming it, and re-compressing it.

Currently proper error handling is essentially missing.


