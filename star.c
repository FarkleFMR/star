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

void createArchive(){}
void listArchive(){}
void updateArchive(){}

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
