#include "compilalinb.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
//#define DEBUG

typedef struct {
    char linha[256];
    int index; // Número da linha atual começando em 0
    int bytes; // Número de bytes utilizados até esta linha
} Linha;

// alocacaoInfo[0] = totalBytes
// alocacaoInfo[1] = numVariaveis
// alocacaoInfo[2] = numParametros

// casoAtual[0] = tipoInstrucao (1: atribuicao, 2: desvio, 3: retorno)
// casoAtual[1] = tipo1 (0: variavel, 1: parametro, 2: constante)
// casoAtual[2] = tipo2 
// casoAtual[3] = tipo3 
// casoAtual[4] = indice1 (indice da variavel/parâmetro)
// casoAtual[5] = indice-valor1 (indice da variavel/parâmetro ou valor da constante)
// casoAtual[6] = indice-valor2 
// casoAtual[7] = operacao (0: +, 1: -, 2: *)

void identificaCasoLinha(const char* linha, int casoAtual[8]);
void calculaAlocacaoBytes(Linha linhas[], int numLinhas, int alocacaoInfo[3]);
void alocarParametros(unsigned char** ptrCodigo, int numParametros);
void alocarVariaveisLocais(unsigned char** ptrCodigo, int numVariaveis);
void fazDesvio(int casoAtual[8], unsigned char** ptrCodigo, Linha linhas[], int line);
void fazAtribuicao(int casoAtual[8], unsigned char** ptrCodigo);
void fazOperacao(int casoAtual[8], unsigned char** ptrCodigo);
void armazenaConstanteLittleEndian(unsigned char** ptrCodigo, int constante);

void primeiraPassagem(Linha linhas[], int numLinhas, int alocacaoInfo[3]) {
    int line = 0;
    int casoAtual[8] = { 0 };

    linhas[0].bytes = 4; // Início
    linhas[0].bytes += alocacaoInfo[1] * 7; // Qtd de parâmetros
    linhas[0].bytes += alocacaoInfo[2] * 3; // Qtd de variáveis

    while (line < numLinhas) {
        identificaCasoLinha(linhas[line].linha, casoAtual);

        switch (casoAtual[0]) {
        case 1: // Atribuição
            linhas[line + 1].bytes = linhas[line].bytes + 3; // A ultima linha é sempre ocupa 3 bytes

            //Se con então + 5, se var/par então +3, pras duas primeiras linhas
            if (casoAtual[2] == 2) {
                linhas[line + 1].bytes += 5; // Segundo valor é constante, ocupa 5 bytes
            }
            else {
                linhas[line + 1].bytes += 3; // Segundo valor é variável, ocupa 3 bytes
            }

            if (casoAtual[3] == 2) {
                linhas[line + 1].bytes += 5; // Terceiro valor é constante, ocupa 5 bytes
            }
            else {
                if(casoAtual[7] == 2){
                    linhas[line + 1].bytes += 4; // Terceiro valor é variável e mul, ocupa 4 bytes
                }
                else{
                    linhas[line + 1].bytes += 3; // Terceiro valor é variável e add/sub, ocupa 3 bytes
                }
                
            }

            break;
        case 2: // Desvio
            linhas[line + 1].bytes = linhas[line].bytes + 10; // Sempre 10

            break;
        case 3: // Retorno
            linhas[line + 1].bytes = linhas[line].bytes + 5; // Sempre 5

            break;
        default:

            #ifdef DEBUG
            printf("Erro: Instrução desconhecida\n");
            #endif 

            return;
        }

        line++;
    }
}

void segundaPassagem(Linha linhas[], int numLinhas, unsigned char* codigo, int alocacaoInfo[3]) {
    unsigned char* ptrCodigo = codigo;
    int line = 0;
    int casoAtual[8] = { 0 };

    #ifdef DEBUG
    printf("\n[Localizacao: 0x00000000]\n"); // Imprime a localização inicial (0)
    #endif 

    // Código inicial
    *ptrCodigo++ = 0x55; // push %rbp

    #ifdef DEBUG
    printf("55 - push %%rbp\n");
    #endif 

    *ptrCodigo++ = 0x48; *ptrCodigo++ = 0x89; *ptrCodigo++ = 0xe5; // mov %rsp,%rbp

    #ifdef DEBUG
    printf("48 89 e5 - mov %%rsp,%%rbp\n");
    #endif 

    // Alocar parâmetros
    alocarParametros(&ptrCodigo, alocacaoInfo[2]);

    // Alocar variáveis locais
    alocarVariaveisLocais(&ptrCodigo, alocacaoInfo[1]);

    while (line < numLinhas) {

        #ifdef DEBUG
        printf("\n[Localizacao: 0x%08lx]\n", ptrCodigo - codigo); // Imprime a localização em hexadecimal
        #endif 

        identificaCasoLinha(linhas[line].linha, casoAtual);

        switch (casoAtual[0]) {
        case 1: // Atribuição   

            fazAtribuicao(casoAtual, &ptrCodigo);        
            break;
        case 2: // Desvio

            fazDesvio(casoAtual, &ptrCodigo, linhas, line);
            break;
        case 3: // Retorno

            *ptrCodigo++ = 0x8b; *ptrCodigo++ = 0x45; *ptrCodigo++ = 0xfc; // mov -0x4(%rbp),%eax

            #ifdef DEBUG
            printf("8b 45 fc - mov -0x4(%%rbp),%%eax\n");
            #endif 

            *ptrCodigo++ = 0x5d; // pop %rbp

            #ifdef DEBUG
            printf("5d - pop %%rbp\n");
            #endif 

            *ptrCodigo++ = 0xc3; // ret

            #ifdef DEBUG
            printf("c3 - ret\n");
            #endif 

            break;
        default:

            #ifdef DEBUG
            printf("Erro: Instrução desconhecida\n");
            #endif 

            return;
        }

        line++;
    }
}

funcp compilaLinB(FILE* f, unsigned char codigo[]) {
    Linha linhas[50];
    int numLinhas = 0;
    char linha[256];

    // Ler todas as linhas do arquivo
    while (fgets(linha, sizeof(linha), f) != NULL) {
        size_t tamanho = strlen(linha);
        if (linha[tamanho - 1] == '\n') {
            linha[tamanho - 1] = '\0';
        }
        strcpy(linhas[numLinhas].linha, linha);
        linhas[numLinhas].index = numLinhas;
        numLinhas++;

        #ifdef DEBUG
        printf("Linha %d: %s\n", linhas[numLinhas - 1].index, linhas[numLinhas - 1].linha);
        #endif 

    }

    #ifdef DEBUG
    printf("\n");
    #endif 

    // Calcular alocação
    int alocacaoInfo[3] = { 0 };
    calculaAlocacaoBytes(linhas, numLinhas, alocacaoInfo);

    // Imprimir alocação inicial
    #ifdef DEBUG
    printf("Total de bytes alocados: %d\n", alocacaoInfo[0]);
    #endif 

    #ifdef DEBUG
    printf("Numero de variaveis: %d\n", alocacaoInfo[1]);
    #endif 

    #ifdef DEBUG
    printf("Numero de parametros: %d\n", alocacaoInfo[2]);
    #endif 

    // Primeira Passagem: Calcular bytes e registrar saltos
    primeiraPassagem(linhas, numLinhas, alocacaoInfo);

    // Segunda Passagem: Gerar código de máquina
    segundaPassagem(linhas, numLinhas, codigo, alocacaoInfo);

    return (funcp)codigo;
}

void identificaCasoLinha(const char* linha, int casoAtual[8]) {
    memset(casoAtual, 0, 8 * sizeof(int));

    while (*linha == ' ' || *linha == '\t') {
        linha++;
    }

    if (strstr(linha, "<=") != NULL) {
        casoAtual[0] = 1; // Atribuição

        if (linha[0] == 'v') {
            casoAtual[1] = 0; // Variável
        } else if (linha[0] == 'p') {
            casoAtual[1] = 1; // Parâmetro
        }
        casoAtual[4] = linha[1] - '0';

        const char* p = linha;
        while (*p != '<') p++;
        p += 3;

        if (p[0] == 'v') {
            casoAtual[2] = 0; // Variável
            casoAtual[5] = p[1] - '0';
        } else if (p[0] == 'p') {
            casoAtual[2] = 1; // Parâmetro
            casoAtual[5] = p[1] - '0';
        } else if (p[0] == '$') {
            casoAtual[2] = 2; // Constante
            casoAtual[5] = atoi(p + 1);
        }

        p++; p++; p++;

        if (*p == '+') {
            casoAtual[7] = 0;
        } else if (*p == '-') {
            casoAtual[7] = 1;
        } else if (*p == '*') {
            casoAtual[7] = 2;
        }

        p++; p++;

        if (p[0] == 'v') {
            casoAtual[3] = 0; // Variável
            casoAtual[6] = p[1] - '0';
        } else if (p[0] == 'p') {
            casoAtual[3] = 1; // Parâmetro
            casoAtual[6] = p[1] - '0';
        } else if (p[0] == '$') {
            casoAtual[3] = 2; // Constante
            casoAtual[6] = atoi(p + 1);
        }
    } else if (strncmp(linha, "if", 2) == 0) {
        casoAtual[0] = 2; // Desvio

        const char* p = linha;
        p += 3;

        if (p[0] == 'v') {
            casoAtual[1] = 0;
        } else if (p[0] == 'p') {
            casoAtual[1] = 1;
        }

        casoAtual[4] = p[1] - '0';

        p += 3;

        casoAtual[5] = atoi(p);
    } else if (strncmp(linha, "ret", 3) == 0) {
        casoAtual[0] = 3; // Retorno
    }
}
// Calcula quantos Bytes vão ser necessários e também as informações de quantas variáveis/parâmetros tem ao longo da função
void calculaAlocacaoBytes(Linha linhas[], int numLinhas, int alocacaoInfo[3]) {
    int usoVariaveis[4] = { 0, 0, 0, 0 };
    int usoParametros[2] = { 0, 0 };

    for (int i = 0; i < numLinhas; i++) {
        const char* linha = linhas[i].linha;

        if (strstr(linha, "v1") != NULL) usoVariaveis[0] = 1;
        if (strstr(linha, "v2") != NULL) usoVariaveis[1] = 1;
        if (strstr(linha, "v3") != NULL) usoVariaveis[2] = 1;
        if (strstr(linha, "v4") != NULL) usoVariaveis[3] = 1;
        if (strstr(linha, "p1") != NULL) usoParametros[0] = 1;
        if (strstr(linha, "p2") != NULL) usoParametros[1] = 1;
    }

    int numVariaveis = usoVariaveis[0] + usoVariaveis[1] + usoVariaveis[2] + usoVariaveis[3];
    int numParametros = usoParametros[0] + usoParametros[1];
    int totalAlocacao = numVariaveis * 4 + numParametros * 4;

    alocacaoInfo[0] = totalAlocacao;
    alocacaoInfo[1] = numVariaveis;
    alocacaoInfo[2] = numParametros;
}

// Alocar parâmetros nos 16-32 bytes se houver (em teoria até no máximo 24, mas se necessitasse de puxar o rbp pra outra função 32)
void alocarParametros(unsigned char** ptrCodigo, int numParametros) {
    if (numParametros > 0) {
        // Alocar o primeiro parâmetro no EDI
        *(*ptrCodigo)++ = 0x89; *(*ptrCodigo)++ = 0x7d; *(*ptrCodigo)++ = 0xec; // mov %edi, -0x14(%rbp)

        #ifdef DEBUG
        printf("89 7d ec - mov %%edi,-0x14(%%rbp)\n");
        #endif 
    }
    if (numParametros > 1) {
        // Alocar o segundo parâmetro no ESI
        *(*ptrCodigo)++ = 0x89; *(*ptrCodigo)++ = 0x75; *(*ptrCodigo)++ = 0xe8; // mov %esi, -0x18(%rbp)

        #ifdef DEBUG
        printf("89 75 e8 - mov %%esi,-0x18(%%rbp)\n");
        #endif 
    }
}


// Alocar variáveis Locais nos 0-16 bytes se houver
void alocarVariaveisLocais(unsigned char** ptrCodigo, int numVariaveis) {
    for (int i = 0; i < numVariaveis; i++) {
        *(*ptrCodigo)++ = 0xc7; *(*ptrCodigo)++ = 0x45; *(*ptrCodigo)++ = (unsigned char)(0xfc - i * 4);
        *(*ptrCodigo)++ = 0x00; *(*ptrCodigo)++ = 0x00; *(*ptrCodigo)++ = 0x00; *(*ptrCodigo)++ = 0x00;

        #ifdef DEBUG
        printf("c7 45 %02x 00 00 00 00 - movl $0x0,-0x%x(%%rbp)\n", 0xfc - i * 4, 0x4 + i * 4);
        #endif 
    }
}

// Realiza a atribuição
void fazAtribuicao(int casoAtual[8], unsigned char** ptrCodigo) {
    int localVarPar = (casoAtual[1] * 16) + (casoAtual[4] * 4); // Valor do local da variável

    // Chama a operação aritmética
    fazOperacao(casoAtual, ptrCodigo);

    // Armazena o resultado na memória
    unsigned char instr_mov[] = { 0x89, 0x45, (unsigned char)(0x0 - localVarPar) }; // mov %eax, -0xN(%rbp)
    for (int i = 0; i < sizeof(instr_mov); i++) {
        *(*ptrCodigo)++ = instr_mov[i];
    }

    #ifdef DEBUG
    printf("89 45 %02x - mov %%eax, -0x%x(%%rbp)\n", (unsigned char)(0x0 - localVarPar), 0x00 + localVarPar);
    #endif 

}

// Realiza operação aritmética
void fazOperacao(int casoAtual[8], unsigned char** ptrCodigo) {
    // Carregar a primeira variável/paramêtro/constante em %eax
    if (casoAtual[2] == 2) { // Se o primeiro operando for constante
        // Opcode para mov $const, %eax
        *(*ptrCodigo)++ = 0xb8;

        // Imprime o começo da instrução gerada

        #ifdef DEBUG
        printf("b8");
        #endif 

        armazenaConstanteLittleEndian(ptrCodigo, casoAtual[5]); //casoAtual[5] = Constante

        #ifdef DEBUG
        printf(" mov $0x%x,%%eax\n", casoAtual[5]);
        #endif 
    }
    else { // Se o primeiro operando for variável/parâmetro
        int localVarParCon = (casoAtual[2] * 16) + (casoAtual[5] * 4); // Valor do local da variável/parâmetro
        unsigned char instr_mov1[] = { 0x8b, 0x45, (unsigned char)((0x00 - localVarParCon)) }; // mov -0xN(%rbp),%eax
        for (int i = 0; i < sizeof(instr_mov1); i++) {
            *(*ptrCodigo)++ = instr_mov1[i];
        }

        #ifdef DEBUG
        printf("8b 45 %02x - mov -0x%x(%%rbp),%%eax\n", (unsigned char)((0x00 - localVarParCon)), 0x00 + localVarParCon);
        #endif 
    }

    // Realizar a operação aritmética   
    switch (casoAtual[7]) { //casoAtual[7] = Operador
    case 0: // Adição
        if (casoAtual[3] == 2) { // Se o segundo operando for constante
            // Instrução inicial
            *(*ptrCodigo)++ = 0x05; // add $const, %eax

            #ifdef DEBUG
            printf("05");
            #endif 

            armazenaConstanteLittleEndian(ptrCodigo, casoAtual[6]); // Adiciona a constante

            #ifdef DEBUG
            printf(" add $0x%x,%%eax\n", casoAtual[6]);
            #endif 
        }
        else { // Se o segundo operando for variável/parâmetro
            int localVarParCon = (casoAtual[3] * 16) + (casoAtual[6] * 4); // Valor do local da variável/parâmetro
            unsigned char instr_add[] = { 0x03, 0x45, (unsigned char)((0x00 - localVarParCon)) }; // add -0xN(%rbp),%eax
            for (int i = 0; i < sizeof(instr_add); i++) {
                *(*ptrCodigo)++ = instr_add[i];
            }

            #ifdef DEBUG
            printf("03 45 %02x - add -0x%x(%%rbp),%%eax\n", (unsigned char)((0x00 - localVarParCon)), 0x00 + localVarParCon);
            #endif 
        }
        break;
    case 1: // Subtração
        if (casoAtual[3] == 2) { // Se o segundo operando for constante
            // Instrução inicial
            *(*ptrCodigo)++ = 0x2d; // sub $const, %eax

            #ifdef DEBUG
            printf("2d");
            #endif 

            armazenaConstanteLittleEndian(ptrCodigo, casoAtual[6]); // Adiciona a constante

            #ifdef DEBUG
            printf(" sub $0x%x,%%eax\n", casoAtual[6]);
            #endif 
        }
        else { // Se o segundo operando for variável/parâmetro
            int localVarParCon = (casoAtual[3] * 16) + (casoAtual[6] * 4); // Valor do local da variável/parâmetro
            unsigned char instr_sub[] = { 0x2b, 0x45, (unsigned char)((0x00 - localVarParCon)) }; // sub -0xN(%rbp),%eax
            for (int i = 0; i < sizeof(instr_sub); i++) {
                *(*ptrCodigo)++ = instr_sub[i];
            }

            #ifdef DEBUG
            printf("2b 45 %02x - sub -0x%x(%%rbp),%%eax\n", (unsigned char)((0x00 - localVarParCon)), 0x00 + localVarParCon);
            #endif 
        }
        break;
    case 2: // Multiplicação
        if (casoAtual[3] == 2) { // Se o segundo operando for constante
            // Instrução inicial
            *(*ptrCodigo)++ = 0x69; // imul $const, %eax

            #ifdef DEBUG
            printf("69");
            #endif 
            
            armazenaConstanteLittleEndian(ptrCodigo, casoAtual[6]); // Adiciona a constante

            #ifdef DEBUG
            printf(" imul $0x%x,%%eax\n", casoAtual[6]);
            #endif 
        }
        else { // Se o segundo operando for variável/parâmetro
            int localVarParCon = (casoAtual[3] * 16) + (casoAtual[6] * 4); // Valor do local da variável/parâmetro
            unsigned char instr_imul[] = { 0x0f, 0xaf, 0x45, (unsigned char)((0x00 - localVarParCon)) }; // imul -0xN(%rbp),%eax
            for (int i = 0; i < sizeof(instr_imul); i++) {
                *(*ptrCodigo)++ = instr_imul[i];
            }

            #ifdef DEBUG
            printf("0f af 45 %02x - imul -0x%x(%%rbp),%%eax\n", (unsigned char)((0x00 - localVarParCon)), 0x00 + localVarParCon);
            #endif 

        }
        break;
    default:

        #ifdef DEBUG
        printf("Operação desconhecida\n");
        #endif 

    }
}

// Realiza o desvio
void fazDesvio(int casoAtual[8], unsigned char** ptrCodigo, Linha linhas[], int line) {
    int localVarPar = (casoAtual[1] * 16) + (casoAtual[4] * 4); // Valor do local da variável/parâmetro

    // Comparar o valor na memória diretamente
    unsigned char instr_cmp[] = { 0x83, 0x7d, (unsigned char)(0x00 - localVarPar), 0x00}; // Instrução cmp
    for (int i = 0; i < sizeof(instr_cmp); i++) {
        *(*ptrCodigo)++ = instr_cmp[i];
    }

    #ifdef DEBUG
    printf("83 7d %02x 00 - cmp $0, -0x%x(%%rbp)\n", (unsigned char)(0x00 - localVarPar), 0x00 + localVarPar);
    #endif 

    int moduloDeslocamentoBytesDecimal = abs(linhas[line].bytes - linhas[casoAtual[5] - 1].bytes + 10); // (-1 no caso atual pq a linha começa em 0), (+10 pra considerar os bytes do proprio comando)
    

    // Instrução jne (near jump -- salto longo)
    *(*ptrCodigo)++ = 0x0f; 
    *(*ptrCodigo)++ = 0x85; 

    #ifdef DEBUG
    printf("0f 85");
    #endif 

    if ((line + 1) > casoAtual[5]) { //Linha atual maior que linha de destino, subtrai       
        armazenaConstanteLittleEndian(ptrCodigo, -1 * moduloDeslocamentoBytesDecimal); // Adiciona a constante negativa (4 bytes)

        #ifdef DEBUG
        printf(" jne %d\n", -1 * moduloDeslocamentoBytesDecimal);
        #endif

    }
    else { //Linha de destino maior que linha atual, adiciona
        armazenaConstanteLittleEndian(ptrCodigo, moduloDeslocamentoBytesDecimal); // Adiciona a constante positiva (4 bytes)

        #ifdef DEBUG
        printf(" jne %d\n", moduloDeslocamentoBytesDecimal);
        #endif

    }

}

void armazenaConstanteLittleEndian(unsigned char** ptrCodigo, int constante) {
    // Converte a constante para little-endian
    unsigned char bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = (unsigned char)((constante >> (i * 8)) & 0xFF);
    }

    // Armazena os bytes no ponteiro de código
    for (int i = 0; i < 4; i++) {
        *(*ptrCodigo)++ = bytes[i];
    }

    // Imprime os bytes na ordem correta (little-endian)

    #ifdef DEBUG
    printf(" %02x %02x %02x %02x -", bytes[0], bytes[1], bytes[2], bytes[3]);
    #endif 
}
