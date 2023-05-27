#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE 32 // Tamaño de bloque en KB
#define MAX_FILE_NAME_LENGTH 256 // Tamaño máximo del nombre del archivo
#define TABLE_SIZE 100 // Tamaño máximo de la tabla FAT
#define MAX_FILE_COUNT 100 // Máxima cantidad de documentos en  un archivo


typedef struct {
    int next_block; // Bloque siguiente
    char data[BLOCK_SIZE * 1024]; // Datos del bloque
} Block;

typedef struct {
    int allocated; // Indica si el bloque está asignado (1) o libre (0)
    Block block; // Bloque de datos
} FATEntry;

typedef struct {
    FATEntry entries[TABLE_SIZE]; // Tabla FAT
    int free_blocks[TABLE_SIZE]; // Índices de bloques libres
    int free_blocks_count; // Número de bloques libres
} FATTable;

FATTable fatTable; // FAT global

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


int getFreeBlock(FATTable *fatTable) {
    if (fatTable->free_blocks_count == 0) {
        return -1; // No hay bloques libres disponibles
    }

    int freeBlockIndex = fatTable->free_blocks[--fatTable->free_blocks_count];
    fatTable->entries[freeBlockIndex].allocated = 1; // Marcar el bloque como asignado
    return freeBlockIndex;
}

void releaseBlock(FATTable *fatTable, int blockIndex) {
    fatTable->entries[blockIndex].allocated = 0; // Marcar el bloque como libre
    fatTable->free_blocks[fatTable->free_blocks_count++] = blockIndex;
}

void createArchive(){}int createArchive(const char* archiveName, const char* fileList[], int fileCount) {
    // Create a new archive
    FILE* archive = fopen(archiveName, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Error creating archive\n");
        return 1;
    }

    // Write file count to the archive
    fwrite(&fileCount, sizeof(int), 1, archive);

    // Process each file and add it to the archive
    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileList[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileList[i]);
            continue;
        }

        // Get the file size
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Write file name length and file name to the archive
        int fileNameLength = strlen(fileList[i]);
        fwrite(&fileNameLength, sizeof(int), 1, archive);
        fwrite(fileList[i], sizeof(char), fileNameLength, archive);

        // Write file size to the archive
        fwrite(&fileSize, sizeof(long), 1, archive);

        // Write file data to the archive
        int remainingBytes = fileSize;
        while (remainingBytes > 0) {
            int blockIndex = getFreeBlock();
            if (blockIndex == -1) {
                fprintf(stderr, "Insufficient space in archive\n");
                fclose(file);
                fclose(archive);
                return 1;
            }

            Block block;
            block.next_block = -1;

            int bytesRead = fread(block.data, sizeof(char), BLOCK_SIZE * 1024, file);
            if (bytesRead <= 0) {
                break;
            }

            fwrite(&block, sizeof(Block), 1, archive);

            if (bytesRead < BLOCK_SIZE * 1024) {
                remainingBytes -= bytesRead;
                releaseBlock(blockIndex);
                break;
            }

            remainingBytes -= bytesRead;
            fatTable.entries[blockIndex].block.next_block = getFreeBlock();
        }

        fclose(file);
    }

    fclose(archive);
    return 0;
}
int listArchive(const char* archiveName) {
    // List the contents of an archive
    FILE* archive = fopen(archiveName, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive);

    printf("Archive Contents:\n");

    for (int i = 0; i < fileCount; i++) {
        int fileNameLength;
        fread(&fileNameLength, sizeof(int), 1, archive);

        char fileName[MAX_FILE_NAME_LENGTH];
        fread(fileName, sizeof(char), fileNameLength, archive);
        fileName[fileNameLength] = '\0';

        long fileSize;
        fread(&fileSize, sizeof(long), 1, archive);

        printf("File Name: %s\n", fileName);
        printf("File Size: %ld bytes\n", fileSize);
        printf("\n");
    }

    fclose(archive);
    return 0;
}
int updateArchive(const char* archiveName, const char* fileList[], int fileCount) {
    // Update the content of an existing archive
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    // Read the existing file count from the archive
    int existingFileCount;
    fread(&existingFileCount, sizeof(int), 1, archive);

    // Calculate the new file count
    int newFileCount = existingFileCount + fileCount;

    // Update the file count in the archive
    fseek(archive, 0, SEEK_SET);
    fwrite(&newFileCount, sizeof(int), 1, archive);

    // Seek to the end of the archive
    fseek(archive, 0, SEEK_END);

    // Process each file and update it in the archive
    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileList[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileList[i]);
            continue;
        }

        // Get the file size
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Write file name length and file name to the archive
        int fileNameLength = strlen(fileList[i]);
        fwrite(&fileNameLength, sizeof(int), 1, archive);
        fwrite(fileList[i], sizeof(char), fileNameLength, archive);

        // Write file size to the archive
        fwrite(&fileSize, sizeof(long), 1, archive);

        // Write file data to the archive
        int remainingBytes = fileSize;
        while (remainingBytes > 0) {
            int blockIndex = getFreeBlock();
            if (blockIndex == -1) {
                fprintf(stderr, "Insufficient space in archive\n");
                fclose(file);
                fclose(archive);
                return 1;
            }

            Block block;
            block.next_block = -1;

            int bytesRead = fread(block.data, sizeof(char), BLOCK_SIZE * 1024, file);
            if (bytesRead <= 0) {
                break;
            }

            fwrite(&block, sizeof(Block), 1, archive);

            if (bytesRead < BLOCK_SIZE * 1024) {
                remainingBytes -= bytesRead;
                releaseBlock(blockIndex);
                break;
            }

            remainingBytes -= bytesRead;
            fatTable.entries[blockIndex].block.next_block = getFreeBlock();
            fatTable.entries[blockIndex].block.next_block = getFreeBlock();
        }

        fclose(file);
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
    } else {
        fprintf(stderr, "Invalid option\n");
        return 1;
    }

    return 0;
}
