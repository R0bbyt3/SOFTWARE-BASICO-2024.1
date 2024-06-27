/* Robbie (Giovana) Carvalho 2311833 3WB (?) */ 

#include <stdio.h>
#include <stdlib.h>

// Estrutura para cada caractere e seu c�digo Huffman
struct compactadora {
    char simbolo;             // O caractere real
    unsigned int codigo;      // O c�digo Huffman como um n�mero inteiro
    int tamanho;              // Quantos bits s�o usados no c�digo
};

// Fun��o para visualizar os bits (legal para debugging)
void print_bits(unsigned int codigo, int tamanho) {
    for (int i = tamanho - 1; i >= 0; i--) {
        printf("%d", (codigo >> i) & 1);
    }
}

void compacta(FILE* arqTexto, FILE* arqBin, struct compactadora* v) {
    char ch;
    unsigned char buffer = 0;
    int bit_count = 0;

    printf("MENSAGEM ORIGINAL: ");
    while (fread(&ch, sizeof(char), 1, arqTexto) == 1) {
        printf("%c", ch); 
        for (int i = 0; i < 32; i++) { 
            if (v[i].simbolo == ch) {
                for (int j = v[i].tamanho - 1; j >= 0; j--) {
                    unsigned char bit = (v[i].codigo >> j) & 1;
                    buffer = (buffer << 1) | bit;
                    bit_count++;

                    if (bit_count == 8) {
                        fwrite(&buffer, sizeof(unsigned char), 1, arqBin);
                        buffer = 0;
                        bit_count = 0;
                    }
                }
                break;
            }
        }
    }

    for (int i = 0; i < 32; i++) {
        if (v[i].simbolo == 0x04) { 
            for (int j = v[i].tamanho - 1; j >= 0; j--) {
                unsigned char bit = (v[i].codigo >> j) & 1;
                buffer = (buffer << 1) | bit;
                bit_count++;

                if (bit_count == 8) {
                    fwrite(&buffer, sizeof(unsigned char), 1, arqBin);
                    buffer = 0;
                    bit_count = 0;
                }
            }
            break;
        }
    }

    if (bit_count > 0) {
        buffer = buffer << (8 - bit_count);
        fwrite(&buffer, sizeof(unsigned char), 1, arqBin);
    }

    printf("\n");
}

void descompacta(FILE* arqBin, FILE* arqTexto, struct compactadora* v) {
    unsigned int current_bits = 0;
    int bit_count = 0;
    unsigned char byte;

    printf("MENSAGEM DESCOMPACTADA: ");
    while (fread(&byte, sizeof(unsigned char), 1, arqBin) == 1) {
        for (int i = 7; i >= 0; i--) {
            unsigned char bit = (byte >> i) & 1;
            current_bits = (current_bits << 1) | bit;
            bit_count++;

            for (int j = 0; j < 32; j++) {
                if (bit_count == v[j].tamanho && current_bits == v[j].codigo) {
                    if (v[j].simbolo == 0x04) { 
                        printf("\n");
                        return;
                    }
                    printf("%c", v[j].simbolo); 
                    fwrite(&(v[j].simbolo), sizeof(char), 1, arqTexto);
                    current_bits = 0;
                    bit_count = 0;
                    break;
                }
            }
        }
    }
    printf("\n");
}

