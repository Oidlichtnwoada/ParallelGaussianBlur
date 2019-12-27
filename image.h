#ifndef SRC_IMAGE_H_
#define SRC_IMAGE_H_

#include "common.h"

typedef struct {
  int border;     // size of the border
  int width;      // width of the overall image
  int height;     // height of the overall iamge
  pixel_t **data; // image data, including the border
} image_t;


typedef unsigned char *image_buffer_t;

/**
 * convert a buffer into an image.
 *
 * The image has to be allocated before calling this function.
 * The offset parameters are used to load a specific region into the image.
 * The border parameter specifies the number of pixels that are repeated around
 * the edge of the image
 */
void image_from_buffer(image_buffer_t buffer, int width, int height,
                       int offset_x, int offset_y, image_t target_image);

/**
 * copy the contents of given image into a target buffer.
 *
 * The buffer has to be allocated before calling this function and has to be
 * large enough. The three color-values red, green and blue are stored directly
 * after each other for each pixel. The width, height and offset parameters
 * specify the dimensions if the (sub-)image that is loaded into the buffer.
 */
void buffer_from_image(image_t image, int width, int height, int offset_x,
                       int offset_y, image_buffer_t target_buffer);

/**
 * allocates memory for an image with a border.
 * @param width real width of the image
 * @param height real height of the image
 * @param border size of the border to add
 * @return image that needs to be freed by @free_image@
 */
image_t malloc_image_uninitialized(int width, int height, int border);

/**
 * free an image
 */
void free_image(image_t image);

/**
 * access the image at the coordinate (x,y)
 */
pixel_t *img(image_t image, int x, int y);

/**
 * Print the red channel of a buffer.
 */
void print_buffer(image_buffer_t buffer, int width, int height);

/**
 * Print image red channel.
 */
void print_image(image_t image);

#endif /* SRC_IMAGE_H_ */
