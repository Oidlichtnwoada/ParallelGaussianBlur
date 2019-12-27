#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "image.h"
#include "kernels.h"
#include "loadbmp.h"
#include <mpi.h>

#define COMM_TAG (0)

void copy_image_part_to_buffer(image_t image, int x_start, int y_start, int x_end, int y_end, image_buffer_t buffer) {
  int current_buffer_index = 0;
  for (int y = y_start; y <= y_end; y++) {
    for (int x = x_start; x <= x_end; x++) {
      pixel_t *current_pixel = &image.data[y][x];
      buffer[current_buffer_index + 0] = current_pixel->red;
      buffer[current_buffer_index + 1] = current_pixel->green;
      buffer[current_buffer_index + 2] = current_pixel->blue;
      current_buffer_index += 3;
    }
  }
}

void apply_image_part_from_buffer(image_t image, int x_start, int y_start, int x_end, int y_end, image_buffer_t buffer) {
  int current_buffer_index = 0;
  for (int y = y_start; y <= y_end; y++) {
    for (int x = x_start; x <= x_end; x++) {
      pixel_t *current_pixel = &image.data[y][x];
      current_pixel->red = buffer[current_buffer_index + 0];
      current_pixel->green = buffer[current_buffer_index + 1];
      current_pixel->blue = buffer[current_buffer_index + 2];
      current_buffer_index += 3;
    }
  }
}

int get_other_rank(int maximum_x, int maximum_y, int current_x, int current_y, int offset_x, int offset_y) {
  int ret = MPI_PROC_NULL;
  int new_x = current_x + offset_x;
  int new_y = current_y + offset_y;
  if (new_x <= maximum_x && new_x >= 0 && new_y <= maximum_y && new_y >= 0) {
    ret = new_x * (maximum_y + 1) + new_y;
  }
  return ret;
}

int main(int argc, char *argv[]) {
  int reps = 5;
  // global vars for rank0
  image_t global_image;
  image_buffer_t global_buffer = NULL;
  int kernel_offset = 2;

  // wall clock time
  double spent_time = -1.0;

  // initialize MPI
  int success = MPI_Init(&argc, &argv);
  if (MPI_ERR_OTHER == success) {
    printf("MPI Error while initialization\n");
    exit(2);
  }
  int rank, world;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world);

  if (argc == 2) {
    if (strcmp(argv[1], "--help") != 0 && strcmp(argv[1], "-h") != 0) {
      reps = strtol(argv[1], NULL, 10);
    } else {
      if (rank == 0) {
        fprintf(stderr,
                "SYNOPSIS: %s n\n\tn - Number of repetitions, default 5\n",
                argv[0]);
      }
      MPI_Finalize();
      return 1;
    }
  } else if (argc > 2) {
    if (rank == 0) {
      fprintf(stderr, "SYNOPSIS: %s n\n\tn - Number of repetitions, default 5\n",
              argv[0]);
    }
    MPI_Finalize();
    return 1;
  }

  // load image
  if (rank == 0) {
    unsigned int width;
    unsigned int height;

    // load image
    int err = loadbmp_decode_file("MARBLES.BMP", &global_buffer, &width,
                                  &height, LOADBMP_RGB);
    if (err) {
      printf("LoadBMP Load Error: %u", err);
      return 1;
    }
    if (width < world || height < world) {
      fprintf(stderr, "Image is too small\n");
      return 1;
    }

    //printf("width*height=%d*%d=%d\n", width, height, width * height);

    // convert image
    global_image = malloc_image_uninitialized(width, height, kernel_offset);
    image_from_buffer(global_buffer, width, height, 0, 0, global_image);
  }

  // transform image
  {

    MPI_Status status;
    // setup cart-communicator
    int mpi_dims[2] = {0,0};
    MPI_Comm comm_cart;
    int cart_rank;
    MPI_Dims_create(world, 2, mpi_dims);
    MPI_Cart_create(MPI_COMM_WORLD, 2, mpi_dims, (int[2]){0, 0}, 0, &comm_cart);
    MPI_Comm_rank(MPI_COMM_WORLD, &cart_rank);
    int cart_loc[2];
    MPI_Cart_coords(comm_cart, rank, 2, cart_loc);

    // broadcast image size
    int img_dims[2];
    if (rank == 0) {
      img_dims[0] = global_image.width;
      img_dims[1] = global_image.height;
    }
    MPI_Bcast(img_dims, 2, MPI_INT, 0, comm_cart);

    // calculate local dimensions
    int local_width = img_dims[0] / mpi_dims[0];
    int local_height = img_dims[1] / mpi_dims[1];
    int rem_width = img_dims[0] % mpi_dims[0];
    int rem_height = img_dims[1] % mpi_dims[1];

    if (mpi_dims[0] == cart_loc[0]+1) {
      local_width += img_dims[0] % mpi_dims[0];
    }
    if (mpi_dims[1] == cart_loc[1]+1) {
      local_height += img_dims[1] % mpi_dims[1];
    }
    int send_width = local_width + 2 * kernel_offset;
    int send_height = local_height + 2 * kernel_offset;
    int send_count = send_width * send_height * 3;
    image_buffer_t img_buffer = (unsigned char *)malloc(send_count);
    // allocate local image
    image_t local_img =
        malloc_image_uninitialized(local_width, local_height, kernel_offset);

    // broadcast image
    if (rank == 0) {
      int max_send_width = local_width + rem_width + 2*kernel_offset;
      int max_send_height = local_height + rem_height + 2*kernel_offset;
      int max_send_count = max_send_width * max_send_height * 3;
      image_buffer_t img_send_buffer = (unsigned char *)malloc(max_send_count);

      for (int i = 0; i < mpi_dims[0]; i++) {
        for (int j = 0; j < mpi_dims[1]; j++) {
          if (i == 0 && j == 0) {
            continue;
          }
          int target_rank = i * mpi_dims[1] + j;
          int offset_x = local_width * i - kernel_offset;
          int offset_y = local_height * j - kernel_offset;
          int target_width = local_width;
          int target_height = local_height;
          if (i + 1 == mpi_dims[0]) {
            target_width += rem_width;
          }
          if (j + 1 == mpi_dims[1]) {
            target_height += rem_height;
          }
          int target_send_width = target_width + 2 * kernel_offset;
          int target_send_height = target_height + 2 * kernel_offset;
          int target_send_count = target_send_width * target_send_height * 3;

          // load the image-region into the buffer
          buffer_from_image(global_image, target_send_width, target_send_height, offset_x, offset_y, img_send_buffer);
          // send the buffer
          MPI_Send(img_send_buffer, target_send_count, MPI_UNSIGNED_CHAR, target_rank, 0, comm_cart);
        }
      }
      // extract local image for rank0
      buffer_from_image(global_image, send_width, send_height, -kernel_offset,
                        -kernel_offset, img_send_buffer);
      image_from_buffer(img_send_buffer, send_width, send_height, -kernel_offset, -kernel_offset, local_img);
      free(img_send_buffer);
    } else {
      MPI_Recv(img_buffer, send_count, MPI_UNSIGNED_CHAR,
               0, // recv from rank0
               0, comm_cart, &status);
      image_from_buffer(img_buffer, send_width, send_height, -kernel_offset,
                        -kernel_offset, local_img);
    }

    // convert buffer to image
    image_t local_img_out = malloc_image_uninitialized(
        local_img.width, local_img.height, local_img.border);

    // initialize border-send- and recv- buffers
    int border_max_send_count;
    if (local_width < local_height) {
      border_max_send_count = local_height;
    } else {
      border_max_send_count = local_width;
    }
    border_max_send_count += 2 * kernel_offset;
    border_max_send_count *= 3 * kernel_offset;
    image_buffer_t border_send_buffer =
        (unsigned char *)malloc(border_max_send_count);
    image_buffer_t border_recv_buffer =
        (unsigned char *)malloc(border_max_send_count);

    double start_time = MPI_Wtime();

    int corner_border_send_recv_count = local_img.border * local_img.border * 3;
    int upper_lower_border_send_recv_count = local_img.width * local_img.border * 3;
    int left_right_border_send_recv_count = local_img.height * local_img.border * 3;
    int maximum_x = mpi_dims[0] - 1;
    int maximum_y = mpi_dims[1] - 1;
    int current_x = cart_loc[0];
    int current_y = cart_loc[1];

    // apply filters locally and exchange borders
    for (int i = 0; i < reps; i++) {

      compute_gaussian_blur(local_img.data, local_height, local_width,
                            local_img_out.data);

      image_t tmp_img = local_img;
      local_img = local_img_out;
      local_img_out = tmp_img;

      /*
        The border pixel information for all eight neighbours must be exchanged
      */

      // upper lower border exchange
      for (int k = 0; k <= 1; k++) {
        int x_start = local_img.border;
        int x_end = local_img.border + local_img.width - 1;
        if ((current_y + k) % 2) {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, 0, -1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, x_start, local_img.border, x_end, 2 * local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, upper_lower_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, upper_lower_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, x_start, 0, x_end, local_img.border - 1, border_recv_buffer);
          }
        } else {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, 0, +1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, x_start, local_img.height, x_end, local_img.height + local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, upper_lower_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, upper_lower_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, x_start, local_img.height + local_img.border, x_end, local_img.height + 2 * local_img.border - 1, border_recv_buffer); 
          }
        }
      }

      // left right border exchange
      for (int k = 0; k <= 1; k++) {
        int y_start = local_img.border;
        int y_end = local_img.border + local_img.height - 1;
        if ((current_x + k) % 2) {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, -1, 0);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.border, y_start, 2 * local_img.border - 1, y_end, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, left_right_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, left_right_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, 0, y_start, local_img.border - 1, y_end, border_recv_buffer);
          }
        } else {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, +1, 0);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.width, y_start, local_img.width + local_img.border - 1, y_end, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, left_right_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, left_right_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, local_img.width + local_img.border, y_start, local_img.width + 2 * local_img.border - 1, y_end, border_recv_buffer);
          }
        }
      }

      // upper left right lower border exchange
      for (int k = 0; k <= 1; k++) {
        if ((current_x + k) % 2) {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, -1, -1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.border, local_img.border, 2 * local_img.border - 1, 2 * local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, 0, 0, local_img.border - 1, local_img.border - 1, border_recv_buffer);
          }
        } else {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, +1, +1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.width, local_img.height, local_img.width + local_img.border - 1, local_img.height + local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, local_img.width + local_img.border, local_img.height + local_img.border, local_img.width + 2 * local_img.border - 1, local_img.height + 2 * local_img.border - 1, border_recv_buffer);
          }
        }
      }

      // lower left right upper border exchange
      for (int k = 0; k <= 1; k++) {
        if ((current_x + k) % 2) {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, -1, +1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.border, local_img.height, 2 * local_img.border - 1, local_img.height + local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, 0, local_img.height + local_img.border, local_img.border - 1, local_img.height + 2 * local_img.border - 1, border_recv_buffer);
          }
        } else {
          int other_rank = get_other_rank(maximum_x, maximum_y, current_x, current_y, +1, -1);
          if (other_rank != MPI_PROC_NULL) {
            copy_image_part_to_buffer(local_img, local_img.width, local_img.border, local_img.width + local_img.border - 1, 2 * local_img.border - 1, border_send_buffer);
            MPI_Sendrecv(border_send_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, border_recv_buffer, corner_border_send_recv_count, MPI_UNSIGNED_CHAR, other_rank, COMM_TAG, comm_cart, MPI_STATUS_IGNORE);
            apply_image_part_from_buffer(local_img, local_img.width + local_img.border, 0, local_img.width + 2 * local_img.border - 1, local_img.border - 1, border_recv_buffer);
          }
        }
      }
    }

    double total_time = MPI_Wtime() - start_time;
    MPI_Reduce(&total_time, &spent_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm_cart);

    free(border_send_buffer);
    free(border_recv_buffer);
    free_image(local_img_out);

    // collect image parts
    if (rank == 0) {
      int max_send_width = local_width + rem_width;
      int max_send_height = local_height + rem_height;
      int max_send_count = max_send_width * max_send_height * 3;
      image_buffer_t img_send_buffer = (unsigned char *)malloc(max_send_count);

      for (int i = 0; i < mpi_dims[0]; i++) {
        for (int j = 0; j < mpi_dims[1]; j++) {
          if (i == 0 && j == 0) {
            continue;
          }
          int target_rank = i * mpi_dims[1] + j;
          int offset_x = local_width * i;
          int offset_y = local_height * j;
          int target_width = local_width;
          int target_height = local_height;
          if (i + 1 == mpi_dims[0]) {
            target_width += rem_width;
          }
          if (j + 1 == mpi_dims[1]) {
            target_height += rem_height;
          }
          int target_count = target_width * target_height * 3;

          MPI_Recv(img_send_buffer, target_count, MPI_UNSIGNED_CHAR, target_rank, 0, comm_cart, &status);
          // load the image-region into the buffer
          image_from_buffer(img_send_buffer, target_width, target_height, offset_x, offset_y, global_image);
        }
      }
      // integrate own image
      buffer_from_image(local_img, local_width, local_height, 0, 0, img_buffer);
      image_from_buffer(img_buffer, local_width, local_height, 0, 0,
                        global_image);
      free(img_send_buffer);
    } else {
      buffer_from_image(local_img, local_width, local_height, 0, 0, img_buffer);
      MPI_Send(img_buffer, local_width * local_height * 3, MPI_UNSIGNED_CHAR, 0,
               0, comm_cart);
    }

    free(img_buffer);
  }


  // save image
  if (rank == 0) {
    int width = global_image.width;
    int height = global_image.height;

    // convert image buffer
    buffer_from_image(global_image, width, height, 0, 0, global_buffer);

    // save image buffer
    int err = loadbmp_encode_file("MARBLES2.BMP", global_buffer, width, height,
                                  LOADBMP_RGB);
    if (err) {
      printf("LoadBMP Save Error: %u\n", err);
    }
    printf("%d,%d,%lf\n", world, reps, spent_time);
    free(global_buffer);
    free_image(global_image);
  }
  MPI_Finalize();
  return 0;
}
