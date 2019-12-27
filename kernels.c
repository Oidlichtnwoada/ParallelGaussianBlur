/*
 * kernels.c
 *
 *  Created on: May 6, 2019
 *      Author: sascha
 */

#include <stdio.h>

#include "kernels.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

double gaussian_kernel[5][5] =
  {{1.0 / 256.0, 4.0  / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0},
  {4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0},
  {6.0 / 256.0, 24.0 / 256.0, 36.0 / 256.0, 24.0 / 256.0, 6.0 / 256.0},
  {4.0 / 256.0, 16.0 / 256.0, 24.0 / 256.0, 16.0 / 256.0, 4.0 / 256.0},
  {1.0 / 256.0, 4.0  / 256.0,  6.0 / 256.0,  4.0 / 256.0, 1.0 / 256.0}};

int gaussian_kernel_offset = 2;


void compute_gaussian_blur(pixel_t **image_in, int height, int width, pixel_t **image_out) {
  int i, j;
  int ii, jj;
  // printf("w=%d h=%d\n", width, height);

  for(i=gaussian_kernel_offset; i<height+gaussian_kernel_offset; i++) {
    for(j=gaussian_kernel_offset; j<width+gaussian_kernel_offset; j++) {

          double c_red   = 0.0;
          double c_green = 0.0;
          double c_blue  = 0.0;

          for (ii=-2; ii<=2; ii++) {
              for(jj=-2; jj<=2; jj++) {
                c_red   += (double)image_in[i+ii][j+jj].red   * gaussian_kernel[ii+gaussian_kernel_offset][jj+gaussian_kernel_offset];
                c_green += (double)image_in[i+ii][j+jj].green * gaussian_kernel[ii+gaussian_kernel_offset][jj+gaussian_kernel_offset];
                c_blue  += (double)image_in[i+ii][j+jj].blue  * gaussian_kernel[ii+gaussian_kernel_offset][jj+gaussian_kernel_offset];
              }
          }

          image_out[i][j].red   = (unsigned char)(c_red);
          image_out[i][j].green = (unsigned char)(c_green);
          image_out[i][j].blue  = (unsigned char)(c_blue);
    }
  }


}
