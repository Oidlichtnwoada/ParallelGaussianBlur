
#include "image.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

void image_from_buffer(image_buffer_t buffer, int width, int height,
                       int offset_x, int offset_y, image_t target_image) {
  int targetHeight = offset_y + target_image.border;
  int targetWidth  = offset_x + target_image.border;
  int size = height * width * 3;
  int offset_width = 0;
  int offset_height = 0;
  for (int i = 0; i < size; i += 3) {
    char red = buffer[i];
    char green = buffer[i + 1];
    char blue = buffer[i + 2];
    pixel_t pixel = {.red = red, .green = green, .blue = blue};
    target_image
        .data[targetHeight + offset_height][targetWidth + offset_width] = pixel;

    // jump three values due to RGB are next together
    if (offset_width + 1 >= width) {
      offset_width = 0;
      offset_height++;
    } else {
      offset_width++;
    }
  }
}

void buffer_from_image(image_t image, int width, int height, int offset_x,
                       int offset_y, image_buffer_t target_buffer) {
  int targetHeight = offset_y + image.border;
  int targetWidth = offset_x + image.border;
  int size = height * width * 3;
  int offset_width = 0;
  int offset_height = 0;
  for (int i = 0; i < size; i += 3) {
    pixel_t pixel =
        image.data[targetHeight + offset_height][targetWidth + offset_width];
    target_buffer[i] = pixel.red;
    target_buffer[i + 1] = pixel.green;
    target_buffer[i + 2] = pixel.blue;
    // jump three values due to RGB are next together
    if (offset_width + 1 >= width) {
      offset_width = 0;
      offset_height++;
    } else {
      offset_width++;
    }
  }
}


image_t malloc_image_uninitialized(int width, int height, int border) {
  image_t image;
  image.border = border;
  image.height = height;
  image.width = width;
  int total_height = height + 2 * border;
  int total_width = width + 2 * border;

  image.data = (pixel_t **)malloc(sizeof(pixel_t *) * total_height);
  for (int y = 0; y < total_height; ++y) {
    image.data[y] = (pixel_t *)malloc(sizeof(pixel_t) * total_width);
    for (int x = 0; x < total_width; ++x) {
      pixel_t pixel = {.red = 0, .green = 0, .blue = 0};
      image.data[y][x] = pixel;
    }
  }
  return image;
}

void free_image(image_t image) {
  int total_height = image.height + 2 * image.border;
  for (int y = 0; y < total_height; ++y) {
    free(image.data[y]);
  }
  free(image.data);
}

pixel_t *img(image_t image, int x, int y) {
  return &image.data[x + image.border][y + image.border];
}

void print_buffer(image_buffer_t buffer, int width, int height) {
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      printf("%3d ", *(buffer + (y * width + x) * 3));
    }
    printf("\n");
  }
}

void print_image(image_t image) {
  for (int y = 0; y < image.height + 2 * image.border; y++) {
    for (int x = 0; x < image.width + 2 * image.border; x++) {
      printf("%3d ", image.data[y][x].blue);
    }
    printf("\n");
  }
}
