#ifdef __cplusplus
#include <cstdint>  // C++ header
#else
#include <stdint.h> // C header
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>

FILE *input_file = NULL;
FILE *output_file_reader = NULL;
FILE *output_file_writer = NULL;
void *source_buffer = NULL;
void *destination_buffer = NULL;

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s if=<input file> of=<output file> bs=<block size[1k,1M,2M,etc]> count=<number of blocks[optional]>\n", prog_name);
}

void print_error(const char *prefix, const char *file_name) {
    char error_message[256];
    snprintf(error_message, sizeof(error_message), "%s %s", prefix, file_name);
    fprintf(stderr, "%s\n", error_message);
}

void cleanup() {
    if (input_file) fclose(input_file);
    if (output_file_reader) fclose(output_file_reader);
    if (output_file_writer) fclose(output_file_writer);
    if (source_buffer) free(source_buffer);
    if (destination_buffer) free(destination_buffer);
}

void signal_handler(int signal) {
    printf("\nReceived signal %d, cleaning up...\n", signal);
    cleanup();
    printf("\nExiting...\n");
    exit(EXIT_FAILURE);
}

uint64_t parse_block_size(const char *arg) {
    char *endptr;
    uint64_t size = strtoull(arg, &endptr, 10);

    if (*endptr == 'K' || *endptr == 'k') {
        size *= 1024;
    } else if (*endptr == 'M' || *endptr == 'm') {
        size *= 1024 * 1024;
    } else if (*endptr == 'G' || *endptr == 'g') {
        size *= 1024 * 1024 * 1024;
    } else if (*endptr != '\0') {
        fprintf(stderr, "Invalid block size: %s\n", arg);
        exit(EXIT_FAILURE);
    }
    return size;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler); // Set up signal handler
    uint64_t block_size = 1048576;
    uint64_t count = 0; // default count (0 means copy until EOF)
    char *input_file_name = NULL;
    char *output_file_name = NULL;
    char *block_size_arg = NULL;
    bool encountered_error = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "if=", 3) == 0) {
            input_file_name = argv[i] + 3;
            input_file = fopen(input_file_name, "rb");
            if (!input_file) {
                print_error("Error opening: ", input_file_name);
                break;
            }
        } else if (strncmp(argv[i], "of=", 3) == 0) {
            output_file_name = argv[i] + 3;
            output_file_reader = fopen(output_file_name, "rb");
            if (!output_file_reader) {
                print_error("Error opening[R]: ", output_file_name);
                break;
            }
            output_file_writer = fopen(output_file_name, "r+b");
            if (!output_file_writer) {
                print_error("Error opening[W]: ", output_file_name);
                break;
            }
        } else if (strncmp(argv[i], "bs=", 3) == 0) {
            block_size_arg = argv[i] + 3;
        } else if (strncmp(argv[i], "count=", 6) == 0) {
            count = atoi(argv[i] + 6);
        } else {
            print_usage(argv[0]);
            encountered_error = true;
            break;
        }
    }

    if (!input_file || !output_file_reader|!output_file_writer || !block_size_arg) {
        print_usage(argv[0]);
        encountered_error = true;
    }
    if (encountered_error) {
        //exit if there was ecounter error
        cleanup();
        return EXIT_FAILURE;
    }

    block_size = parse_block_size(block_size_arg);
    printf("\nBlock size: %s", block_size_arg);          // Print the original block size argument
    printf("\nBlock size [bytes]: %" PRIu64 "\n", block_size); // Print the converted block size

    // lets check if destination file sizer is bigger or same as source
    bool flag_valid_to_copy = true;
    // check the file size of source and destination
    struct stat inputfile_stat, outputfile_stat;
    // Get the status of the first file
    if (stat(input_file_name, &inputfile_stat) != 0) {
        print_error("Error getting file status for :", input_file_name);
        encountered_error = true;
    }
    if (stat(output_file_name, &outputfile_stat) != 0) {
        print_error("Error getting file status for :", output_file_name);
        encountered_error = true;
    }

    long inputfile_block_size = inputfile_stat.st_size / block_size;
    if (inputfile_block_size == 0) {
        inputfile_block_size = 1; // so that we dont devide by zero when caculating pc
        printf("input block count: unknown\n");
    }
    long outputfile_block_size = outputfile_stat.st_size / block_size;

    if (outputfile_stat.st_size == 0) {
        printf("output block count: unknown\n");
    } else {
        if (inputfile_stat.st_size > outputfile_stat.st_size) {
            fprintf(stderr, "Error: input file larger than destination file\n");
            encountered_error = true;
        }
    }

    printf("input block count: %lu  |  output block count: %lu\n", inputfile_block_size, outputfile_block_size);
    void *source_buffer = malloc(block_size);

    if (!source_buffer) {
        fprintf(stderr, "Error allocating buffer\n");
        encountered_error = true;
    }
    void *destination_buffer = malloc(block_size);

    if (!destination_buffer) {
        fprintf(stderr, "Error allocating buffer\n");
        encountered_error = true;
    }

    if (encountered_error) {
        flag_valid_to_copy = false;
    }
    //Lets start coppyitn the file

    if (flag_valid_to_copy) {
        // initialize the buffer to store the remporary readed files
        struct timeval start, end;
        gettimeofday(&start, NULL);
        long total_read = 0;
        long progress_count = 0;
        long last_write_position = 0;
        long current_seek_location = 0;

        //for printing progress
        size_t printed_count = 0;
        int write_not_write_decider = 0;
        int progress_dot_print=(10000*512)/block_size;
        if (progress_dot_print<1){
            progress_dot_print=1;
        }


        while (count == 0 || total_read < count) {
            current_seek_location = total_read * (long) block_size;
            uint64_t bytes_read_from_src = fread(source_buffer, 1, block_size, input_file);
            // check if the reading is completed
            if (!(bytes_read_from_src > 0)) {
                printf("\nFinish reading source file\n");
                break;
            }
            // Move the file pointer to the specified start position for reading

            uint64_t bytes_read_from_dest = fread(destination_buffer, 1, block_size, output_file_reader);
            // it taskes times
            if (!(bytes_read_from_dest > 0)) {
                printf("Should not reach: Destination file may be short");
                break;
            }
            // lets compare the two buffer
            int result = memcmp(source_buffer, destination_buffer, block_size);
            if (result == 0) {
                // write_not_write_decider--;
            } else {
                // re seek to write
                long delta_location = current_seek_location - last_write_position;
                if (fseek(output_file_writer, delta_location, SEEK_CUR) != 0) {
                    perror("Error seeking in file[Writing]");
                    break;
                }
                last_write_position = current_seek_location + (long) block_size;
                write_not_write_decider++;
                fwrite(source_buffer, 1, bytes_read_from_src, output_file_writer);
            }
            total_read++;
            progress_count++;
            // print the progress
            if (progress_count > progress_dot_print) {
                progress_count=0;
                if (write_not_write_decider > 0) {
                    printf("#");
                } else {
                    printf(".");
                }
                write_not_write_decider = 0;
                printed_count++;
                fflush(stdout);
            }

            if (printed_count > 80) {
                printed_count = 0;
                double percentage = ((double) total_read / (double) inputfile_block_size) * 100;
                printf(" [%lu/%lu] (%.2f%%)\n", total_read, inputfile_block_size, percentage);
                fflush(stdout);
            }
        }
        // free the buffer
        free(source_buffer);
        free(destination_buffer);

        gettimeofday(&end, NULL);
        double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
        int hours = (int) (elapsed_time / 3600);
        int minutes = (int) ((elapsed_time - hours * 3600) / 60);
        int seconds = (int) (elapsed_time - hours * 3600 - minutes * 60);
        printf("Time consumed: %dh %dm %ds\n", hours, minutes, seconds);
    }
    // Clean up
    cleanup();
    if (encountered_error) {
        //exit if there was ecounter error
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
