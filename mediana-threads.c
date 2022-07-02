#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

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

typedef struct parametros { 
    int nRow;
    int nCol;
    Pixel **imgInput;	
    Pixel **imgOutput;
    int filterSize;
    int seqThread;
    int nThreads;
} Parametros;

bool validarArgumentos(int argc, char **argv){
    if(argc != 5) {
        printf("Argumentos aceitos: \n");
        printf("nome da imagem a ser processada (formato bmp)\n");
        printf("nome da imagem de saída\n");
        printf("tamanho do filtro\n");
        printf("número de threads\n");
        return false;
    }
    return true;
}

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

void operarArquivo(FILE *file, char *operacao, int nRow, int nCol, Pixel **matrizImg){
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

void * aplicarFiltro(void *args) {
    Parametros *param = (Parametros *)args;
    int nRow = param -> nRow;
    int nCol = param -> nCol;
    Pixel **imgInput = param->imgInput;
    Pixel **imgOutput = param->imgOutput;
    int filterSize = param -> filterSize;
    int seqThread = param -> seqThread;
    int nThreads = param -> nThreads;

    int r, c, rr, cc;
    int vSize = filterSize * filterSize;
    int vetorR[vSize];
    int vetorG[vSize];
    int vetorB[vSize];
    int vPos;
    
    for(r = seqThread; r < nRow; r += nThreads) {
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
            imgOutput[r][c].r = vetorR[vPos/2];
            imgOutput[r][c].g = vetorG[vPos/2];
            imgOutput[r][c].b = vetorB[vPos/2];
        }
    }
}

/*---------------------------------------------------------------------------------------*/
Pixel **alocaMatriz(int altura, int largura){
    int i;
	Pixel **m = (Pixel **)malloc(altura * sizeof(Pixel *));
	for(i=0; i<altura; i++){
		m[i] = (Pixel *)malloc(largura * sizeof(Pixel));
	}

	return m;
}
/*---------------------------------------------------------------------------------------*/
void desalocaMatriz(Pixel **m, int altura){
	printf("Desalocando matriz\n");
    int i;
	for(i=0; i<altura; i++){
		free(m[i]);
	}
	free(m);
}

/*---------------------------------------------------------------------------------------*/
int main(int argc, char **argv) {
    if(!validarArgumentos(argc, argv)) {
        printf("argumentos invalidos\n");
        exit(0);
    }

    Cabecalho cab;

    FILE *f1 = (FILE *) abrirArquivo(argv[1], LEITURA, &cab);
    
    Pixel **imgInput = alocaMatriz(cab.altura, cab.largura); 
    Pixel **imgOutput = alocaMatriz(cab.altura, cab.largura); 
	
    operarArquivo(f1, LEITURA, cab.altura, cab.largura, imgInput);
    
    int filterSize = atoi(argv[3]);
    int nThreads = atoi(argv[4]);
    int i;

    pthread_t *tid = (pthread_t *)malloc(nThreads * sizeof(pthread_t));
    Parametros *param = (Parametros *)malloc(nThreads * sizeof(Parametros));
    
    for(i = 0; i < nThreads; i++) {
        param[i].nRow = cab.altura;
        param[i].nCol = cab.largura;
	    param[i].imgInput = imgInput;
        param[i].imgOutput = imgOutput;
        param[i].filterSize = filterSize;
        param[i].seqThread = i;
        param[i].nThreads = nThreads;
	    pthread_create(&tid[i], NULL, aplicarFiltro, (void *) &param[i]);
    }

    for(i = 0; i < nThreads; i++){
		pthread_join(tid[i], NULL);
    }
    
    FILE *f2 = (FILE *) abrirArquivo(argv[2], ESCRITA, &cab);
    operarArquivo(f2, ESCRITA, cab.altura, cab.largura, imgOutput);
    	
    desalocaMatriz(imgInput, cab.altura);
    desalocaMatriz(imgOutput, cab.altura);  

    fclose(f1);
    fclose(f2);

    printf("finalizou :)\n");
}

