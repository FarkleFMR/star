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

    // Marcar todos los bloques como no asignados y sin bloque siguiente
    for (int i = 0; i < TABLE_SIZE; i++) {
        fatTable.entries[i].allocated = 0;
        fatTable.entries[i].block.next_block = -1;
    }

    // Inicializar la lista de bloques libres
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

    // Obtener el índice del bloque libre
    int freeBlockIndex = fatTable.free_blocks[--fatTable.free_blocks_count];

    // Marcar el bloque como asignado en la tabla FAT
    fatTable.entries[freeBlockIndex].allocated = 1;

    return freeBlockIndex;
}

void releaseBlock(int blockIndex) {
    // Libera un bloque y lo marca como libre en la tabla FAT

    // Marcar el bloque como libre en la tabla FAT
    fatTable.entries[blockIndex].allocated = 0;

    // Agregar el bloque liberado a la lista de bloques libres
    fatTable.free_blocks[fatTable.free_blocks_count++] = blockIndex;
}

int createArchive(const char* archiveName, const char* fileNames[], int fileCount, int verbose) {
    FILE* archive = fopen(archiveName, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Error creating archive\n");
        return 1;
    }

    fwrite(&fileCount, sizeof(int), 1, archive); // Escribir el número de archivos en el archivo de almacenamiento

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copiar el nombre del archivo al bloque
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file); // Leer los datos del archivo y almacenarlos en el bloque
        block.next_block = -1; // Establecer el siguiente bloque como -1 (indicando el final del archivo)

        fwrite(&block, sizeof(Block), 1, archive); // Escribir el bloque en el archivo de almacenamiento

        if (verbose) {
            printf("Added file: %s\n", block.filename); // Imprimir el nombre del archivo agregado
            printf("File size: %zu bytes\n", strlen(block.data)); // Imprimir el tamaño del archivo en bytes
            printf("\n");
        }

        fclose(file); // Cerrar el archivo original
    }

    fclose(archive); // Cerrar el archivo de almacenamiento
    return 0;
}

int listArchive(const char* archiveName, int verbose) {
    // Lista el contenido del archivo de almacenamiento
    FILE* archive = fopen(archiveName, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error opening archive\n");
        return 1;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Lee la cantidad de archivos almacenados en el archivo

    printf("Archive Contents:\n");

    for (int i = 0; i < fileCount; i++) {
        Block block;
        fread(&block, sizeof(Block), 1, archive); // Lee un bloque del archivo de almacenamiento

        printf("File Name: %s\n", block.filename); // Imprime el nombre del archivo contenido en el bloque
        printf("File Size: %zu bytes\n", strlen(block.data)); // Imprime el tamaño del archivo contenido en el bloque

        if (verbose) {
            printf("File Data:\n%s\n", block.data); // Imprime los datos del archivo contenido en el bloque
        }

        printf("\n");
    }

    fclose(archive); // Cierra el archivo de almacenamiento
    return 0;
}


int updateArchive(const char* archiveName, const char* fileNames[], int fileCount, int verbose) {
    // Actualiza el archivo de almacenamiento actualizando o agregando archivos según sea necesario
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo de almacenamiento\n");
        return 1;
    }

    // Lee la cantidad de archivos existentes en el archivo de almacenamiento
    int existingFileCount;
    fread(&existingFileCount, sizeof(int), 1, archive);

    // Busca el final del archivo
    fseek(archive, 0, SEEK_END);

    // Lee todos los bloques existentes y almacena su información
    Block existingBlocks[MAX_FILE_COUNT];
    for (int i = 0; i < existingFileCount; i++) {
        fseek(archive, sizeof(int) + (sizeof(Block) * i), SEEK_SET);
        fread(&existingBlocks[i], sizeof(Block), 1, archive);
    }

    // Actualiza la cantidad de archivos en el archivo de almacenamiento
    fseek(archive, 0, SEEK_SET);
    int newFileCount = existingFileCount;
    fwrite(&newFileCount, sizeof(int), 1, archive);

    // Agrega o actualiza archivos en el archivo de almacenamiento
    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error al abrir el archivo: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copia el nombre del archivo
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        // Verifica si el archivo ya existe en el archivo de almacenamiento
        int fileExists = 0;
        for (int j = 0; j < existingFileCount; j++) {
            if (strcmp(existingBlocks[j].filename, block.filename) == 0) {
                if (verbose) {
                    printf("Actualizando bloque existente:\n");
                    printf("Nombre del archivo: %s\n", existingBlocks[j].filename);
                    printf("Tamaño del archivo: %zu bytes\n", strlen(existingBlocks[j].data));
                    printf("\n");
                }

                // Actualiza el archivo existente
                fseek(archive, sizeof(int) + (sizeof(Block) * j), SEEK_SET);
                fwrite(&block, sizeof(Block), 1, archive);
                fileExists = 1;
                break;
            }
        }

        if (!fileExists) {
            // Agrega un nuevo archivo al final del archivo de almacenamiento
            fseek(archive, 0, SEEK_END);
            fwrite(&block, sizeof(Block), 1, archive);
            newFileCount++;
        }

        fclose(file);
    }

    // Actualiza la cantidad de archivos después de agregar o actualizar los archivos
    fseek(archive, 0, SEEK_SET);
    fwrite(&newFileCount, sizeof(int), 1, archive);

    fclose(archive);
    return 0;
}


int appendArchive(const char* archiveName, const char* fileNames[], int fileCount, int verbose) {
    // Actualiza el archivo de almacenamiento agregando los archivos proporcionados
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo de almacenamiento\n");
        return 1;
    }

    int existingFileCount;
    fread(&existingFileCount, sizeof(int), 1, archive); // Lee la cantidad de archivos existentes en el archivo de almacenamiento

    fseek(archive, 0, SEEK_SET);
    int newFileCount = existingFileCount + fileCount;
    fwrite(&newFileCount, sizeof(int), 1, archive); // Actualiza la cantidad de archivos en el archivo de almacenamiento

    // Busca el final del archivo
    fseek(archive, 0, SEEK_END);

    for (int i = 0; i < fileCount; i++) {
        FILE* file = fopen(fileNames[i], "rb");
        if (file == NULL) {
            fprintf(stderr, "Error al abrir el archivo: %s\n", fileNames[i]);
            fclose(archive);
            return 1;
        }

        Block block;
        strcpy(block.filename, fileNames[i]); // Copia el nombre del archivo
        size_t bytesRead = fread(block.data, sizeof(char), sizeof(block.data), file);
        block.next_block = -1;

        fwrite(&block, sizeof(Block), 1, archive); // Escribe el bloque en el archivo de almacenamiento

        fclose(file);

        if (verbose) {
            printf("Archivo agregado:\n");
            printf("Nombre del archivo: %s\n", block.filename);
            printf("Tamaño del archivo: %zu bytes\n", bytesRead);
            printf("\n");
        }
    }

    fclose(archive);
    return 0;
}


void extractArchive(const char* archiveName, int verbose) {
    // Extrae archivos de un archivo de almacenamiento
    FILE* archive = fopen(archiveName, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo de almacenamiento\n");
        return;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Lee el número de archivos

    for (int i = 0; i < fileCount; i++) {
        Block block;
        fread(&block, sizeof(Block), 1, archive); // Lee el bloque del archivo de almacenamiento

        FILE* file = fopen(block.filename, "wb");
        if (file == NULL) {
            fprintf(stderr, "Error al crear el archivo: %s\n", block.filename);
            continue;
        }

        fwrite(block.data, sizeof(char), strlen(block.data), file); // Escribe los datos del bloque en el archivo

        fclose(file);

        if (verbose) {
            printf("Archivo extraído: %s\n", block.filename);
        }
    }

    fclose(archive);
}


void packArchive(const char* archiveName, int verbose) {
    // Desfragmenta el contenido del archivo de almacenamiento
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo de almacenamiento\n");
        return;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Lee el número de archivos

    // Lee todos los bloques existentes y guarda su información
    Block existingBlocks[MAX_FILE_COUNT];
    for (int i = 0; i < fileCount; i++) {
        fseek(archive, sizeof(int) + (sizeof(Block) * i), SEEK_SET);
        fread(&existingBlocks[i], sizeof(Block), 1, archive);
    }

    // Reescribe el archivo comenzando desde el principio
    fseek(archive, 0, SEEK_SET);
    fwrite(&fileCount, sizeof(int), 1, archive); // Reescribe el número de archivos

    // Reescribe los bloques en un orden desfragmentado
    for (int i = 0; i < fileCount; i++) {
        fseek(archive, 0, SEEK_END);

        Block block = existingBlocks[i];
        fwrite(&block, sizeof(Block), 1, archive);

        if (verbose) {
            printf("Archivo empaquetado: %s\n", block.filename);
        }
    }

    fclose(archive);
}

int deleteFile(const char* archiveName, const char* fileName, int verbose) {
    // Elimina un archivo del archivo de almacenamiento
    FILE* archive = fopen(archiveName, "r+b");
    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo de almacenamiento\n");
        return 1;
    }

    int fileCount;
    fread(&fileCount, sizeof(int), 1, archive); // Lee el número de archivos

    int fileFound = 0;
    for (int i = 0; i < fileCount; i++) {
        Block block;
        fread(&block, sizeof(Block), 1, archive);

        if (strcmp(block.filename, fileName) == 0) {
            fileFound = 1;

            if (verbose) {
                printf("Archivo eliminado: %s\n", fileName);
            }

            // Mueve el último archivo a la posición actual
            fseek(archive, -sizeof(Block), SEEK_CUR);
            Block lastBlock;
            fread(&lastBlock, sizeof(Block), 1, archive);
            fseek(archive, -sizeof(Block), SEEK_CUR);
            fwrite(&lastBlock, sizeof(Block), 1, archive);

            // Acorta el archivo eliminando el último bloque
            int newFileCount = fileCount - 1;
            fseek(archive, 0, SEEK_SET);
            fwrite(&newFileCount, sizeof(int), 1, archive);

            fclose(archive);
            return 0;
        }
    }

    fclose(archive);

    if (!fileFound) {
        fprintf(stderr, "Archivo no encontrado: %s\n", fileName);
        return 1;
    }

    return 0;
}


int main(int argc, char* argv[]) {
    // Verifica la cantidad de argumentos
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <comando> <-f|--file> <nombre_archivo> [archivos]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];  // Almacena el comando especificado
    const char* option = argv[2];  // Almacena la opción especificada
    const char* archiveName = NULL;  // Almacena el nombre del archivo de almacenamiento
    const char** fileList = NULL;  // Almacena la lista de archivos
    int fileCount = 0;  // Almacena la cantidad de archivos
    int verbose = 0;  // Indica si se debe mostrar información detallada

    // Verifica la opción especificada
    if (strcmp(option, "-f") != 0 && strcmp(option, "--file") != 0) {
        fprintf(stderr, "Error: Falta la opción -f o --file\n");
        return 1;
    }

    // Obtiene el nombre del archivo de almacenamiento y la lista de archivos
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            archiveName = argv[i];
            fileList = (const char**)&argv[i + 1];
            fileCount = argc - i - 1;
            break;
        }
    }

    // Verifica si se proporcionó el nombre del archivo de almacenamiento
    if (archiveName == NULL) {
        fprintf(stderr, "Error: No se proporcionó el nombre del archivo de almacenamiento\n");
        return 1;
    }

    // Ejecuta el comando correspondiente
    if (strcmp(command, "-c") == 0 || strcmp(command, "--create") == 0) {
        initializeFAT();
        return createArchive(archiveName, fileList, fileCount, verbose);
    } else if (strcmp(command, "-t") == 0 || strcmp(command, "--list") == 0) {
        return listArchive(archiveName, verbose);
    } else if (strcmp(command, "-r") == 0 || strcmp(command, "--append") == 0) {
        return appendArchive(archiveName, fileList, fileCount, verbose);
    } else if (strcmp(command, "-u") == 0 || strcmp(command, "--update") == 0) {
        return updateArchive(archiveName, fileList, fileCount, verbose);
    } else if (strcmp(command, "-x") == 0 || strcmp(command, "--extract") == 0) {
        extractArchive(archiveName, verbose);
        return 0;
    } else if (strcmp(command, "-p") == 0 || strcmp(command, "--pack") == 0) {
        packArchive(archiveName, verbose);
        return 0;
    } else if (strcmp(command, "--delete") == 0) {
        return deleteFile(archiveName, fileList[0], verbose);
    } else {
        fprintf(stderr, "Comando inválido: %s\n", command);
        return 1;
    }

    return 0;
}
