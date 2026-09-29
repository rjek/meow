#include "bench.h"

/* floating point through the library: a series for pi in double and a
 * small Mandelbrot set in float */
int main(void)
{
    double pi = 0.0, sign = 1.0;
    float cx, cy;
    unsigned sum = 0;
    int i, x, y;

    for (i = 0; i < 400; i++) { pi += sign / (2 * i + 1); sign = -sign; }
    pi *= 4.0;
    sum = (unsigned)(pi * 1000000.0);
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 16; x++) {
            float zx = 0.0f, zy = 0.0f;
            int n = 0;
            cx = -2.0f + x * 0.1875f; cy = -1.0f + y * 0.28125f;
            while (n < 30 && zx * zx + zy * zy < 4.0f) {
                float t = zx * zx - zy * zy + cx;
                zy = 2.0f * zx * zy + cy;
                zx = t;
                n++;
            }
            sum = sum * 3 + n;
        }
    }
    report("softfp", sum);
    return 0;
}
