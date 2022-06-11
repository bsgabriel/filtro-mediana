#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#pragma pack(1)

#define LEITURA "rb"
#define ESCRITA "wb"
#define arrayOutput(r,c) arrayOutput[r * nCol + c] // c = coluna, r = linha, nCol = tamanho do array?

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
    if(argc != 5) {
        printf("Argumentos aceitos: \n");
        printf("nome da imagem a ser processada (formato bmp)\n");
        printf("nome da imagem de saída\n");
        printf("tamanho do filtro\n");
        printf("número de processos\n");
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

void aplicarFiltro(int nRow, int nCol, Pixel imgInput[nRow][nCol], Pixel *arrayOutput, int filterSize, int seqProcesso, int nProcessos) {
    int r, c, rr, cc;
    int vSize = filterSize * filterSize;
    int vetorR[vSize];
    int vetorG[vSize];
    int vetorB[vSize];
    int vPos;
    
    for(r = seqProcesso; r < nRow; r += nProcessos){
        for(c = 0; c < nCol; c++){
            vPos = 0;
            for(rr = -filterSize/2;  rr <= filterSize/2; rr++){
                for(cc = -filterSize/2; cc <= filterSize/2; cc++){
                    if(r + rr >= 0 && r + rr < nRow && c + cc >= 0 && c + cc < nCol){
                        vetorR[vPos] = imgInput[r+rr][c+cc].r;
                        vetorG[vPos] = imgInput[r+rr][c+cc].g;
                        vetorB[vPos] = imgInput[r+rr][c+cc].b;
                        vPos++;
                    }
                }
            }

            ordenar(vetorR, vSize);
            ordenar(vetorG, vSize);
            ordenar(vetorB, vSize);

            // atribui a mediana do vetor ao seu canal        
            arrayOutput(r,c).r = vetorR[vPos/2];
            arrayOutput(r,c).g = vetorG[vPos/2];
            arrayOutput(r,c).b = vetorB[vPos/2];
        }
    }
}

void arrayToMatriz(int nRow, int nCol, Pixel *arrayOutput, Pixel imgOutput[nRow][nCol]) {
    printf("Transformando o array em uma matriz...\n");
    int r, c;
    for(r = 0; r < nRow; r++) {
        for(c = 0; c < nCol; c++) {
            imgOutput[r][c] = arrayOutput(r,c);
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

    int chave = 'G';
    int shmid = shmget(chave, cab.altura * cab.largura * sizeof(Pixel), 0600 | IPC_CREAT);
    Pixel *arrayOutput = shmat(shmid, 0, 0);

    operarArquivo(f1, LEITURA, cab.altura, cab.largura, imgInput);

    int filterSize = atoi(argv[3]);
    int nProcessos = atoi(argv[4]);
    int seqProcesso = 0; // processo pai tem seq = 0
    int i;

    for(i = 1; i < nProcessos; i++) {
        int pid = fork();
        if (pid == 0){
            seqProcesso = i;
            break;
        } 
    }

    aplicarFiltro(cab.altura, cab.largura, imgInput, arrayOutput, filterSize, seqProcesso, nProcessos);

    if (seqProcesso != 0){
        shmdt(arrayOutput);
    } else {
        for(i = 1; i < nProcessos; i++){
            wait(NULL);
        }
        arrayToMatriz(cab.altura, cab.largura, arrayOutput, imgOutput);
        FILE *f2 = (FILE *) abrirArquivo(argv[2], ESCRITA, &cab);
        operarArquivo(f2, ESCRITA, cab.altura, cab.largura, imgOutput);
    	
        shmdt(arrayOutput);
	    shmctl(shmid, IPC_RMID, 0);
        fclose(f1);
        fclose(f2);
        printf("finalizou :)\n");
    }
}

