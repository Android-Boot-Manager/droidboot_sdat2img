#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define BLOCK_SIZE 4096
#define MAX_LINE_LENGTH 1000

typedef struct {
    uint32_t version;
    uint32_t new_blocks;
} TransferHeader;

typedef struct {
    uint32_t begin;
    uint32_t end;
} TransferCommand;

void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void parse_transfer_list_file(const char *filename, TransferHeader *header, TransferCommand **commands, uint32_t *num_commands) {
    FILE *file = fopen(filename, "r");
    char line[256];
    if (!file) {
        die("Error opening transfer list file");
    }

    fgets(line, MAX_LINE_LENGTH, file);
    header->version=atoi(line);
    fgets(line, MAX_LINE_LENGTH, file);
    header->new_blocks=atoi(line);

    // Skip useless fieleds present in version >=2
    if(header->version>=2){
        fgets(line, MAX_LINE_LENGTH, file);
        fgets(line, MAX_LINE_LENGTH, file);
    }

    char *token;
    int count=0;
    int ii=0;

    // count block adresses
    while(fgets(line, sizeof(line), file)) {
        TransferCommand *cmd = &((*commands)[ii]);
        ii++;

         // Get the first token
        token = strtok(line, " ");
        // Check if the first token is "new"
        if (token != NULL && strcmp(token, "new") == 0) {
            // Ignore the "new" token
            token = strtok(NULL, ",");

            // Parse the count
            if (token != NULL) {
                count += atoi(token);
            }

        }
    }
    fprintf(stderr, "Found: %d block adresses\n", count);
    *commands = (TransferCommand **)malloc(sizeof(TransferCommand *) * count);
    *num_commands=count/2;

    // rewind back to the begining of file
    rewind(file);

    ii=0;
    // parse block adresses
    while(fgets(line, sizeof(line), file)) {
        TransferCommand *cmd = &((*commands)[ii]);

        // Get the first token
        token = strtok(line, " ");
        // Check if the first token is "new"
        if (token != NULL && strcmp(token, "new") == 0) {
            // Ignore the "new" token
            token = strtok(NULL, ",");

            // Parse the count
            if (token != NULL) {
                count = atoi(token);
            }

            // Parse the remaining numbers
            for (int i = 0; i < count; i+=2) {
                TransferCommand *cmd = &((*commands)[ii]);
                token = strtok(NULL, ",");
                if (token != NULL) {
                    cmd->begin=atoi(token);
                    token = strtok(NULL, ",");
                    cmd->end=atoi(token);
                    ii++;
                } else {
                    // Handle the case where there are not enough numbers
                    fprintf(stderr, "Error: Not enough numbers provided.\n");
                    return ;
                }
            }
        }
    }

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "\nUsage: %s <transfer_list> <system_new_file> <system_img>\n\n", argv[0]);
        fprintf(stderr, "    <transfer_list>: transfer list file\n");
        fprintf(stderr, "    <system_new_file>: system new dat file\n");
        fprintf(stderr, "    <system_img>: output system image\n\n");
        return EXIT_FAILURE;
    }

    const char *transfer_list_file = argv[1];
    const char *new_data_file = argv[2];
    const char *output_image_file = argv[3];

    TransferHeader header;
    TransferCommand *commands;
    uint32_t num_commands;

    parse_transfer_list_file(transfer_list_file, &header, &commands, &num_commands);
    fprintf(stderr, "Done parsind, found commands: %d\n", num_commands);

    uint32_t max_file_size = 0;
    fprintf(stderr, "Found: %d commands\n", num_commands);
    FILE *src_file = fopen(new_data_file, "rb");
    if (!src_file) {
        die("Error opening source file");
    }

    FILE *dest_file = fopen(output_image_file, "r+b");
    if (!dest_file) {
        die("Error opening destination file");
    }
    for (uint32_t i = 0; i < num_commands; i++) {
        TransferCommand *cmd = &commands[i];
        max_file_size = cmd->end * BLOCK_SIZE;
        fseek(dest_file, cmd->begin * BLOCK_SIZE, SEEK_SET);
        size_t num_blocks = cmd->end - cmd->begin;
        fprintf(stderr, "Going to copy block from: begin: %d, end: %d\n", cmd->begin, cmd->end);
        for (size_t i = 0; i < num_blocks; i++) {
            uint8_t buffer[BLOCK_SIZE];
            size_t bytes_read = fread(buffer, sizeof(uint8_t), BLOCK_SIZE, src_file);
            if (bytes_read != BLOCK_SIZE) {
                die("Error reading from source file");
            }

            size_t bytes_written = fwrite(buffer, sizeof(uint8_t), BLOCK_SIZE, dest_file);
            if (bytes_written != BLOCK_SIZE) {
                die("Error writing to destination file");
            }
        }
    }

    fclose(src_file);
    fclose(dest_file);

    printf("Done! Output image: %s\n", output_image_file);

    free(commands);

    return EXIT_SUCCESS;
}
