#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#pragma pack(1)

#define LEITURA "rb"
#define ESCRITA "wb"

typedef struct cabecalho {
    unsigned short int tipo;
    int tamanho;
    unsigned short int reservado1;
    unsigned short int reservado2;
    unsigned int offset;
    unsigned int tamanhoCabecalho;
    unsigned int largura;
    unsigned int altura;
    unsigned short int planos;
    unsigned short int numBits;
    unsigned int comp;
    unsigned int imageSize; // tamanho sem o cabeçalho
    unsigned int yRes;
    unsigned int xRes;
    unsigned int usedColors;
    unsigned int sigCores;
} Cabecalho;

typedef struct pixel {
    unsigned char b;
    unsigned char g;
    unsigned char r;
} Pixel;

bool validarArgumentos(int argc, char **argv){
    if(argc != 4) {
        printf("Argumentos aceitos: \n");
        printf("nome da imagem a ser processada (formato bmp)\n");
        printf("nome da imagem de saída\n");
        printf("tamanho do filtro\n");
        return false;
    }
    return true;
}

/* Função que abre o arquivo e popula o cabeçalho
    Parâmetros:
    - fileName: nome do arquivo que será aberto
    - modo: operação que será realizada no arquivo (leitura/escrita)
    - cab: cabeçalho do arquivo    
*/
FILE *abrirArquivo(char *fileName, char *modo, Cabecalho *cab) {
    printf("Abrindo %s como %s... \n", fileName, modo);
    FILE *file = fopen(fileName, modo);
    if(file == NULL){
        printf("Erro ao abrir arquivo %s\n", fileName);
        exit(0);
    }
    if(modo == LEITURA) {
        fread(cab, sizeof(Cabecalho), 1, file);
    } else {
        fwrite(cab, sizeof(Cabecalho), 1, file); 
    }

    return file;
}

/* Função que realiza a operação de leitura/geração de arquivo
    Parâmetros:
    - file: arquivo que será lido/escrito
    - operacao: operação que será realizada (leitura/escrita)
    - nRow: número de linhas da matriz
    - nCow: número de colunas da matriz
    - matrizImg: matriz da imagem que será usada para realizar a opreação do arquivo
*/
void operarArquivo(FILE *file, char *operacao, int nRow, int nCol, Pixel matrizImg[nRow][nCol]){
    unsigned char aux;
    int i, j;
    for(i = 0; i < nRow; i++){
        int ali = (nCol * 3) % 4;

  		if (ali != 0){
   			ali = 4 - ali;
   		}

        for(j = 0; j < nCol; j++){
            if(operacao == LEITURA){
                fread(&matrizImg[i][j], sizeof(Pixel), 1, file);
            } else {
                fwrite(&matrizImg[i][j], sizeof(Pixel), 1, file);
            }
        }
         
        for(j=0; j<ali; j++){
            if(operacao == LEITURA){
   			    fread(&aux, sizeof(unsigned char), 1, file);
            } else {
                fwrite(&aux, sizeof(unsigned char), 1, file);
            }
        }
    }
}

void ordenar(int vetor[], int vSize) {
    int k, j, aux;
    for (k = 1; k < vSize; k++) {
        for (j = 0; j < vSize - k; j++) {
            if (vetor[j] > vetor[j + 1]) {
                aux = vetor[j];
                vetor[j] = vetor[j + 1];
                vetor[j + 1] = aux;
            }
        }
    }
}

void aplicarFiltro(int nRow, int nCol, Pixel imgInput[nRow][nCol], Pixel imgOutput[nRow][nCol], int filterSize) {
    int i, j, k, l;
    int vSize = filterSize * filterSize;
    int vetorR[vSize];
    int vetorG[vSize];
    int vetorB[vSize];
    int vPos;
    
    for(i = 0; i < nRow; i++){
        for(j = 0; j < nCol; j++){
            vPos = 0;
            for(k = -filterSize/2;  k <= filterSize/2; k++){
                for(l = -filterSize/2; l <= filterSize/2; l++){
                    if(i + k >= 0 && i + k < nRow && j + l >= 0 && j + l < nCol){
                        vetorR[vPos] = imgInput[i+k][j+l].r;
                        vetorG[vPos] = imgInput[i+k][j+l].g;
                        vetorB[vPos] = imgInput[i+k][j+l].b;
                        vPos++;
                    }
                }
            }

            ordenar(vetorR, vSize);
            ordenar(vetorG, vSize);
            ordenar(vetorB, vSize);

            // atribui a mediana do vetor ao seu canal
            imgOutput[i][j].r = vetorR[vPos/2];
            imgOutput[i][j].g = vetorG[vPos/2];
            imgOutput[i][j].b = vetorB[vPos/2];
        }
    }
}

int main(int argc, char **argv) {
    if(!validarArgumentos(argc, argv)) {
        printf("argumentos invalidos");
        exit(0);
    }

    Cabecalho cab;

    FILE *f1 = (FILE *) abrirArquivo(argv[1], LEITURA, &cab);
    Pixel imgInput[cab.altura][cab.largura];
    Pixel imgOutput[cab.altura][cab.largura];

    operarArquivo(f1, LEITURA, cab.altura, cab.largura, imgInput);

    aplicarFiltro(cab.altura, cab.largura, imgInput, imgOutput, atoi(argv[3]));

    FILE *f2 = (FILE *) abrirArquivo(argv[2], ESCRITA, &cab);
    operarArquivo(f2, ESCRITA, cab.altura, cab.largura, imgOutput);

    fclose(f1);
    fclose(f2);
}

