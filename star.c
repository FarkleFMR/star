#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE 32 // Tamaño de bloque en KB
#define MAX_FILE_NAME_LENGTH 256 // Longitud máxima del nombre de archivo

#define TABLE_SIZE 100 // Tamaño máximo de la tabla FAT
#define MAX_FILE_COUNT 100 // Número máximo de archivos en un archivo de almacenamiento

typedef struct {
    int next_block; // Siguiente bloque
    char filename[256]; // Nombre del archivo
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

FATTable fatTable; // Tabla FAT global

void initializeFAT() {
    // Inicializa la tabla FAT
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
    // Obtiene un bloque libre de la tabla FAT
    if (fatTable.free_blocks_count == 0) {
        return -1; // No hay bloques libres disponibles
    }

    int freeBlockIndex = fatTable.free_blocks[--fatTable.free_blocks_count];
    fatTable.entries[freeBlockIndex].allocated = 1; // Marca el bloque como asignado
    return freeBlockIndex;
}

void releaseBlock(int blockIndex) {
    // Libera un bloque y lo marca como libre en la tabla FAT
    fatTable.entries[blockIndex].allocated = 0; // Marca el bloque como libre
    fatTable.free_blocks[fatTable.free_blocks_count++] = blockIndex;
}


int createArchive(const char* archiveName, const char* fileNames[], int fileCount) {
    // Crea un archivo de almacenamiento y guarda los archivos proporcionados en él
    FILE* archive = fopen(archiveName, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Error creating archive\n");
        return 1;
    }

    fwrite(&fileCount, sizeof(int), 1, archive); // Escribe la cantidad de archivos

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copia el nombre del archivo
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        fwrite(&block, sizeof(Block), 1, archive);

        fclose(file);
    }

    fclose(archive);
    return 0;
}

int listArchive(const char* archiveName) {
    // Lista el contenido del archivo de almacenamiento
    FILE* archive = fopen(archiveName, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Lee la cantidad de archivos

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
    // Actualiza el archivo de almacenamiento agregando los archivos proporcionados
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    // Lee la cantidad de archivos existente
    int existingFileCount;
    fread(&existingFileCount, sizeof(int), 1, archive);

    // Actualiza la cantidad de archivos en el archivo
    fseek(archive, 0, SEEK_SET);
    int newFileCount = existingFileCount + fileCount;
    fwrite(&newFileCount, sizeof(int), 1, archive);

    // Busca el final del archivo
    fseek(archive, 0, SEEK_END);

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]);  // Copia el nombre del archivo
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        fwrite(&block, sizeof(Block), 1, archive);

        fclose(file);
    }

    fclose(archive);
    return 0;
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <command> <-f|--file> <archive_name> [files]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];
    const char* option = argv[2];
    const char* archiveName = argv[3];
    const char** fileList = (const char**)&argv[4];
    int fileCount = argc - 4;

    if (strcmp(option, "-f") != 0 && strcmp(option, "--file") != 0) {
        fprintf(stderr, "Error: Missing -f or --file option\n");
        return 1;
    }

    if (strcmp(command, "-c") == 0 || strcmp(command, "--create") == 0) {
        initializeFAT();
        return createArchive(archiveName, fileList, fileCount);
    } else if (strcmp(command, "-t") == 0 || strcmp(command, "--list") == 0) {
        return listArchive(archiveName);
    } else if (strcmp(command, "-u") == 0 || strcmp(command, "--update") == 0) {
        return updateArchive(archiveName, fileList, fileCount);
    } else {
        fprintf(stderr, "Invalid command: %s\n", command);
        return 1;
    }

    return 0;
}
