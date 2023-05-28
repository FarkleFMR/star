#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE 32 // Block size in KB
#define MAX_FILE_NAME_LENGTH 256 // Maximum file name length

#define TABLE_SIZE 100 // Maximum size of the FAT table
#define MAX_FILE_COUNT 100 // Maximum number of files in an archive

typedef struct {
    int next_block; // Bloque siguiente
    char filename[256]; // Nombre del archivo
    char data[BLOCK_SIZE * 1024]; // Datos del bloque
} Block;

typedef struct {
    int allocated; // Indicates if the block is allocated (1) or free (0)
    Block block; // Data block
} FATEntry;

typedef struct {
    FATEntry entries[TABLE_SIZE]; // FAT table
    int free_blocks[TABLE_SIZE]; // Indices of free blocks
    int free_blocks_count; // Number of free blocks
} FATTable;

FATTable fatTable; // Global FAT table

void initializeFAT() {
    // Initialize the FAT table
    for (int i = 0; i < TABLE_SIZE; i++) {
        fatTable.entries[i].allocated = 0;
        fatTable.entries[i].block.next_block = -1;
    }

    for (int i = 0; i < TABLE_SIZE; i++) {
        fatTable.free_blocks[i] = i;
    }
    fatTable.free_blocks_count = TABLE_SIZE;
}


int getFreeBlock() {
    // Get a free block from the FAT table
    if (fatTable.free_blocks_count == 0) {
        return -1; // No free blocks available
    }

    int freeBlockIndex = fatTable.free_blocks[--fatTable.free_blocks_count];
    fatTable.entries[freeBlockIndex].allocated = 1; // Mark the block as allocated
    return freeBlockIndex;
}

void releaseBlock(int blockIndex) {
    // Release a block and mark it as free in the FAT table
    fatTable.entries[blockIndex].allocated = 0; // Mark the block as free
    fatTable.free_blocks[fatTable.free_blocks_count++] = blockIndex;
}


int createArchive(const char* archiveName, const char* fileNames[], int fileCount) {
    FILE* archive = fopen(archiveName, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Error creating archive\n");
        return 1;
    }

    fwrite(&fileCount, sizeof(int), 1, archive); // Write the file count

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copy the file name
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        fwrite(&block, sizeof(Block), 1, archive);

        fclose(file);
    }

    fclose(archive);
    return 0;
}

int listArchive(const char* archiveName) {
    FILE* archive = fopen(archiveName, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Read the file count

    printf("Archive Contents:\n");

    for (int i = 0; i < fileCount; i++) {
        Block block;
        fread(&block, sizeof(Block), 1, archive);

        printf("File Name: %s\n", block.filename);
        printf("File Size: %zu bytes\n", strlen(block.data));
        printf("\n");
    }

    fclose(archive);
    return 0;
}


int updateArchive(const char* archiveName, const char* fileNames[], int fileCount) {
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    // Read the existing file count
    int existingFileCount;
    fread(&existingFileCount, sizeof(int), 1, archive);

    // Update the file count in the archive
    fseek(archive, 0, SEEK_SET);
    int newFileCount = existingFileCount + fileCount;
    fwrite(&newFileCount, sizeof(int), 1, archive);

    // Seek to the end of the archive
    fseek(archive, 0, SEEK_END);

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copy the file name
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        fwrite(&block, sizeof(Block), 1, archive);

        fclose(file);
    }

    fclose(archive);
    return 0;
}


int packFromInput(const char* archiveName) {
    // Pack contents from standard input into an archive
    FILE* archive = fopen(archiveName, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    int fileCount = 0;
    fwrite(&fileCount, sizeof(int), 1, archive); // Write initial file count as 0

    char inputBuffer[BLOCK_SIZE * 1024];
    size_t bytesRead;

    while ((bytesRead = fread(inputBuffer, sizeof(char), BLOCK_SIZE * 1024, stdin)) > 0) {
        Block block;
        block.next_block = -1;
        memcpy(block.data, inputBuffer, bytesRead);

        fwrite(&block, sizeof(Block), 1, archive);
    }

    fclose(archive);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <options>\n", argv[0]);
        return 1;
    }

    const char* option = argv[1];

    if (strcmp(option, "-c") == 0 || strcmp(option, "--create") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s -c <archive_name> <file1> <file2> ... <fileN>\n", argv[0]);
            return 1;
        }

        initializeFAT();
        int fileCount = argc - 3;
        const char** fileList = (const char**)&argv[3];
        return createArchive(argv[2], fileList, fileCount);
    } else if (strcmp(option, "-t") == 0 || strcmp(option, "--list") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s -t <archive_name>\n", argv[0]);
            return 1;
        }

        return listArchive(argv[2]);
    } else if (strcmp(option, "-u") == 0 || strcmp(option, "--update") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s -u <archive_name> <file1> <file2> ... <fileN>\n", argv[0]);
            return 1;
        }

        int fileCount = argc - 3;
        const char** fileList = (const char**)&argv[3];
        return updateArchive(argv[2], fileList, fileCount);
    } else if (strcmp(option, "-f") == 0 || strcmp(option, "--file") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s -f <archive_name>\n", argv[0]);
            return 1;
        }

        return packFromInput(argv[2]);
    } else {
        fprintf(stderr, "Invalid option\n");
        return 1;
    }

    return 0;
}
