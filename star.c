#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 32 // Tamaño de bloque en KB
#define TABLE_SIZE 100 // Tamaño máximo de la tabla FAT

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

int main() {
    FATTable fatTable;
    int currentBlock = 0; // Bloque actual
    int totalBlocks = 0; // Número total de bloques utilizados

    // Inicializar la tabla FAT
    fatTable.free_blocks_count = TABLE_SIZE;
    for (int i = 0; i < TABLE_SIZE; i++) {
        fatTable.entries[i].allocated = 0;
        fatTable.entries[i].block.next_block = -1;
        fatTable.free_blocks[i] = i;
    }

    // Simulación de asignación de bloques
    int previousBlock = -1;
    for (int i = 0; i < 10; i++) {
        // Buscar un bloque libre en la tabla FAT
        int freeBlock = getFreeBlock(&fatTable);
        if (freeBlock == -1) {
            printf("Error: No hay bloques disponibles.\n");
            return 1;
        }

        // Mostrar información del bloque asignado
        printf("Bloque asignado: %d\n", freeBlock);
        printf("Número total de bloques utilizados: %d\n", ++totalBlocks);

        // Enlazar el bloque asignado con el bloque anterior
        if (previousBlock != -1) {
            fatTable.entries[previousBlock].block.next_block = freeBlock;
        }

        // Simular escritura de datos en el bloque
        // Aquí puedes implementar tu lógica para escribir los datos en el bloque actual
        // Puedes acceder a los datos a través de fatTable.entries[freeBlock].block.data

        // Actualizar el bloque anterior y pasar al siguiente bloque
        previousBlock = freeBlock;
    }
    for (int i = 0; i < TABLE_SIZE; i++) {
       if (fatTable.entries[i].allocated) {
           releaseBlock(&fatTable, i);
        }
    }

return 0;

}
