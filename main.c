#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define Tamanho_pagina 8          // cada página contém 2 blocos de 4 palavras cada
#define Tamanho_Bloco 4          // cada bloco contém 4 palavras de 32 bits cada
#define Tamanho_palavra 32      // cada palavra contém 32 bits
#define via 4                 //numero de vias
#define conjuntos 8           //numero de conjuntos


typedef struct cache {
	int dirty;                     // write-back
	int validade;                  // bit de validade da entrada da cache
	int tag;                       // tag da entrada da cache
	char bloco[Tamanho_Bloco][11]; // dados da entrada da cache
} cache_entry;

typedef struct Tabeladepaginas {
	char paginaFisica[Tamanho_pagina*11];
	char palavras[Tamanho_pagina][11];
	int validade;
} tabelaPagina;

typedef struct mainMemory {
	char palavra[Tamanho_pagina][11];
	char pagina[87];
} main_memory;

typedef struct TLB {
	int tag;
	int validade;
	int paginaFisica;
} tlb;

typedef struct contador {
	int miss_cache;
	int miss_ram;
	int acessos_ram;
	int acessos_cache;
} Cont;

typedef struct lru {
	int topo_TLB;
	int topo;
	int cont;
} LRU;


void geracao_disco() {
	FILE* file = fopen("disco.txt", "w");
	int cont = 0, numero;
	srand(time(NULL));
	for (int i = 0; i < 8000; i++) {
		numero = rand() % 4294967295;
		fprintf(file, "%d ", numero);
		cont ++;
		if(cont==8) {
			fprintf(file, "\n");
			cont = 0;
		}
	}
	fclose(file);
}


void cache_hit(cache_entry** cache,int paginaFisica,int offsetpalavra,int enderecoVirtual,char operation,char* valorStore) {
	int mascaraindice = 7;             //  pega 3 bits
	int tag = paginaFisica >> 3;
	int indice = paginaFisica & mascaraindice;
	int qualvia = 0;

	char valor[11];
	char aux[Tamanho_Bloco][11];
	// achar o bloco correspondente
	int hit = -1;

	for (int i = 0; i < via; i++) {
		if (cache[indice][i].validade == 1 && cache[indice][i].tag == tag) {
			hit = 1;
			qualvia=i;
			break;
		}
	}
	if (hit==1) {

		//mantem sempre o ultimo endereco acessado como mais atualizado
		if(qualvia != 0) {

			for(int i=0; i<Tamanho_Bloco; i++) {
				strncpy(aux[i],cache[indice][qualvia].bloco[i],11);
			}
			for(int i=0; i<Tamanho_Bloco; i++)
				strncpy(cache[indice][qualvia].bloco[i],cache[indice][0].bloco[i],11);
			for(int i=0; i<Tamanho_Bloco; i++)
				strncpy(cache[indice][0].bloco[i],aux[i],11);

			if(cache[indice][0].dirty >= 0) {
				cache[indice][qualvia].dirty = cache[indice][0].dirty;
				cache[indice][0].dirty = -1;
			}
		}
		if (operation == 'l') {
			strncpy(valor,cache[indice][0].bloco[offsetpalavra],11);
			printf("\n\nCache hit:\n endereço %d -> Palavra: %s\n\n", enderecoVirtual, valor);

		} else if (operation == 's') { //store word
			strncpy(cache[indice][0].bloco[offsetpalavra],valorStore,11);
			cache[indice][0].dirty = paginaFisica;
			printf("\n\nCache hit:\n Palavra: %s alocado para o bloco %i da cache, \n\n", valorStore, indice);
		}
	}
}


void Coloca_na_cache(cache_entry** cache,tlb* t,main_memory* ram,LRU* Lru,int paginaFisica,int offsetpalavra,int enderecoVirtual,char operation,char* valorStore,Cont* c) {
	int mascaraindice = 7;             //  2 bits / 11

	int mascarabloco_memoria = 1;      //  1 bit
	int mascarapagina_memoria = 254;   //  7 bits / 11111110

	int indice = paginaFisica & mascaraindice;
	int tag = paginaFisica  >> 3;
	int bloco_memoria = (paginaFisica & mascarabloco_memoria);
	int pagina_memoria = (paginaFisica & mascarapagina_memoria)>>1;

	char aux[Tamanho_Bloco][11];

	int hit = 0;
	for (int i = 0; i < via; i++) {
		if (cache[indice][i].validade == 1 && cache[indice][i].tag == tag) {
			hit = 1;
			break;
		}
	}
	if (hit==1) {
		cache_hit(cache,paginaFisica,offsetpalavra,enderecoVirtual,operation,valorStore);

	} else {
		printf("\nMiss na cache: endereço %d\n", enderecoVirtual);
		c->miss_cache++;

		if(Lru[indice].topo >= (via-1)) {
			Lru[indice].topo=0;
			cache[indice][Lru[indice].topo].validade = 0;
		}
		Lru[indice].topo++;

		//writeback
		if(cache[indice][1].dirty>=0) {
			int write_back_bloco_memoria = (cache[indice][Lru[indice].topo].dirty & mascarabloco_memoria);
			int write_back_pagina_memoria = (cache[indice][Lru[indice].topo].dirty & mascarapagina_memoria)>>1;
			printf("\npagina %d atualizada a ram: \n", write_back_pagina_memoria);
			if(write_back_bloco_memoria == 0) {
				for(int i=0; i<4; i++) {
					strncpy(ram[write_back_pagina_memoria].palavra[i],cache[indice][Lru[indice].topo].bloco[i],11);
					printf("%s ",ram[write_back_pagina_memoria].palavra[i]);
				}
			}
			if(write_back_bloco_memoria == 1) {
				int j=0;
				for(int i=4; i<8; i++) {
					strncpy(ram[write_back_pagina_memoria].palavra[i],cache[indice][Lru[indice].topo].bloco[j],11);
					j++;
					printf("%s ",ram[write_back_pagina_memoria].palavra[i]);
				}
			}
			printf("\n\n");
			cache[indice][Lru[indice].topo].dirty= -1;
		}


		//mantem sempre o ultimo endereco acessado como mais atualizado
		if(Lru[indice].topo>0) {
			cache[indice][Lru[indice].topo].tag = cache[indice][0].tag;
			for(int i=0; i<Tamanho_Bloco; i++) {
				strncpy(cache[indice][Lru[indice].topo].bloco[i],cache[indice][0].bloco[i],11);
			}
			cache[indice][Lru[indice].topo].validade = 1;
			cache[indice][0].validade = 0;
			if(cache[indice][0].dirty >= 0) {
				cache[indice][Lru[indice].topo].dirty = cache[indice][0].dirty;
				cache[indice][0].dirty = -1;
			}
		}



		if(bloco_memoria == 0) {

			for(int i=0; i<4; i++)
				strncpy(cache[indice][0].bloco[i],ram[pagina_memoria].palavra[i],11);

		}
		if(bloco_memoria == 1) {
			int j=0;
			for(int i=4; i<8; i++) {
				strncpy(cache[indice][0].bloco[j],ram[pagina_memoria].palavra[i],11);
				j++;
			}
		}
		cache[indice][0].tag = tag;
		cache[indice][0].validade = 1;
		if(Lru[0].topo_TLB>=2) {
			Lru[0].topo_TLB=0;
			t[Lru[0].topo_TLB].validade = 0;
		}
		t[Lru[0].topo_TLB].validade = 1;
		printf("Carregando bloco da ram -> carregado para entrada %i da cache\n",indice);
		for(int i = 0; i<4; i++) {
			printf("[%s] ", cache[indice][0].bloco[i]);
		}
		printf("\n");
		cache_hit(cache,paginaFisica,offsetpalavra,enderecoVirtual,operation,valorStore);
	}
}

void Coloca_na_ram(int paginaFisica,tabelaPagina* tabela,main_memory*ram,int pagina_virtual, cache_entry** cache,int offsetpalavra,LRU* Lru,int enderecoVirtual,char operation,char* valorStore,tlb* t,Cont* c) {
	int mascarapagina_memoria = 254;  //7 bits
	int pagina_memoria = ((paginaFisica & mascarapagina_memoria)>>1);
	strcpy(ram[pagina_memoria].pagina ,tabela[pagina_virtual].paginaFisica);
	tabela[pagina_virtual].validade = 1;
	for(int i=0; i<8; i++) {
		strncpy(ram[pagina_memoria].palavra[i],tabela[pagina_virtual].palavras[i],11);

	}
	printf("\n\nCarregando pagina %d do disco->ram\n",pagina_virtual);
	printf("pagina %d: ", pagina_memoria);
	printf("%s \n", ram[pagina_memoria].pagina);
	printf("Foi alocada a memoria principal\n");
	printf("\n\n");
	Coloca_na_cache(cache,t,ram,Lru,paginaFisica,offsetpalavra, enderecoVirtual,operation,valorStore,c);
}

void Busca_na_TP(tlb* t,tabelaPagina* tabela,main_memory* ram,int pagina_virtual,LRU* Lru,cache_entry** cache,int offsetpalavra,int enderecoVirtual,char operation,char* valorStore,Cont* c) {
	Lru[0].topo_TLB++;
	if(Lru[0].topo_TLB>=2) {
		Lru[0].topo_TLB=0;
		t[Lru[0].topo_TLB].validade = 0;
	}
	t[Lru[0].topo_TLB].tag = pagina_virtual;
	int paginaFisica=pagina_virtual; //1:1
	t[Lru[0].topo_TLB].paginaFisica = paginaFisica;  //pegando o endereco da pagina fisica colocando na tlb
	if(tabela[pagina_virtual].validade == 1) {
		printf("Hit na ram: endereço %d\n", enderecoVirtual);
		Coloca_na_cache(cache,t,ram,Lru,paginaFisica,offsetpalavra, enderecoVirtual,operation,valorStore,c);
	}
	if(tabela[pagina_virtual].validade == 0) {
		c->miss_ram++;
		printf("Miss na ram: endereço %d\n", enderecoVirtual);
		Coloca_na_ram(paginaFisica,tabela,ram,pagina_virtual,cache,offsetpalavra,Lru, enderecoVirtual,operation,valorStore,t,c);
	}

}

void Busca_na_TLB(tlb* t,tabelaPagina* tabela,main_memory* ram , int enderecoVirtual,LRU* Lru,cache_entry** cache,int offsetpalavra,char operation,char* valorStore,Cont* c) {
	c->acessos_cache++;//ant
	int pagina_virtual = enderecoVirtual >> 4; //4 bits de offset 1 bit de bloco
	int paginaFisica,visitado=0;
	for(int i=0; i<2; i++) {
		if(t[i].tag == pagina_virtual) {
			printf("\nHit TLB\n");
			paginaFisica = t[i].paginaFisica;
			if(t[i].validade == 1) {
				//hit na cache,
				printf("\nHit na cache: endereço %d\n", enderecoVirtual);
				cache_hit(cache,paginaFisica,offsetpalavra,enderecoVirtual,operation,valorStore);
			} else {
				//miss na cache ta na ram (hit ram)
				//acesso cache
				c->acessos_ram++;
				printf("Hit na ram: endereço %d\n", enderecoVirtual);
				Coloca_na_cache(cache,t,ram,Lru,paginaFisica,offsetpalavra, enderecoVirtual,operation,valorStore,c);
			}
			visitado=1;
		}
	}
	if(visitado==0) {
		printf("\nMiss TLB\n");
		c->acessos_ram++;
		Busca_na_TP(t, tabela, ram,pagina_virtual,Lru,cache,offsetpalavra, enderecoVirtual,operation,valorStore,c);
	}
}

int main() {
	// Inicializa o disco
	geracao_disco();
	//inicializa a Tlb
	tlb* t = malloc(2*sizeof(tlb));
	memset(t, 0, 2*sizeof(tlb)); //tlb zerada
	for(int i=0; i<2; i++) {
		t[i].validade= 0;
		t[i].tag = -1;
	}
	// Inicializa a TP
	tabelaPagina* tabela = malloc(1000 * sizeof(tabelaPagina));
	FILE* arquivo = fopen("disco.txt", "r");
	char linha[1000];

	for (int i = 0; i < 1000; i++) {
		fgets(linha, sizeof(linha), arquivo);
		strcpy(tabela[i].paginaFisica, linha);

		// Separar as palavras da linha usando strtok
		char* token = strtok(linha, " ");
		int j = 0;
		while (token != NULL && j < Tamanho_pagina) {
			strncpy(tabela[i].palavras[j], token,11);
			token = strtok(NULL, " ");
			j++;
		}
		tabela[i].validade = 0;
	}

	fclose(arquivo);
	// Inicializa a ram
	main_memory* ram = malloc(100 * sizeof(main_memory));

	memset(ram, 0, 100*sizeof(main_memory)); // Preenche a memória com zeros
	// Inicializa a cache
	cache_entry** cache = malloc(conjuntos * sizeof(cache_entry));
	memset(cache, 0,conjuntos*sizeof(cache_entry)); // Memória zerada
	for (int i = 0; i < conjuntos; i++) {
		cache[i] = malloc(via * sizeof(cache_entry));
		memset(cache[i], 0,via*sizeof(cache_entry));
	}
	for (int i = 0; i < conjuntos; i++) {
		for(int j = 0; j< via; j++) {
			cache[i][j].dirty = -1;
			cache[i][j].validade = 0;
			cache[i][j].tag = -1;
		}
	}
	//Inicializa LRU
	LRU* Lru = malloc(conjuntos*sizeof(LRU));
	for(int i=0; i<conjuntos; i++) {
		Lru[i].topo=-1;
		Lru[i].topo_TLB=-1;
	}
	//Inicializa o contador
	Cont cont;
	cont.miss_cache=0;
	cont.miss_ram=0;
	cont.acessos_ram=0;
	cont.acessos_cache=0;

	FILE* file = fopen("prog.txt", "r");
	if (file == NULL) {
		printf("Erro: não foi possível abrir o arquivo.\n");
		return 1;
	}
	int n=0;

	char endereco[100];

	//cache
	int mascaraoffsetbyte = 3;                  //  Seleciona Os Dois Primeiros Bits
	int mascaraoffsetpalavra = 12;             //  Seleciona o bit 3 e 4

	// Lendo o endereço
	while (n < 100 && fgets(endereco, sizeof(endereco), file) != NULL) {
		char *token = strtok(endereco, " "); // Separar os valores usando o espaço como delimitador
		int numValoresLidos = 0;

		char operation;
		int enderecoVirtual;
		char valorStore[50];

		while (token != NULL) {
			if (numValoresLidos == 0) {
				sscanf(token, "%c", &operation);
			} else if (numValoresLidos == 1) {
				sscanf(token, "%d", &enderecoVirtual);
			} else if (numValoresLidos == 2) {
				sscanf(token, "%s", valorStore);
			}

			token = strtok(NULL, " ");
			numValoresLidos++;
		}

		if (numValoresLidos == 2 || numValoresLidos == 3) {
			printf("Instrucao %d:\n",n+1);
		} else {
			printf("Erro ao ler a linha %d.\n", n + 1);
		}
		if(enderecoVirtual>16383) {
			break;
		}
		//separa os offset
		int offsetbyte = enderecoVirtual & mascaraoffsetbyte;
		int offsetpalavra = (enderecoVirtual & mascaraoffsetpalavra) >> 2;
		// Realiza a tradução de endereços

		Busca_na_TLB(t,tabela,ram,enderecoVirtual,Lru,cache,offsetpalavra,operation,valorStore,&cont);
		n++;
	}
	// Fecha o arquivo
	fclose(file);
	printf("Taxa de falhas da cache:%.2f%% \n",(((float)cont.miss_cache/(float)cont.acessos_cache)*100.0));

	printf("Taxa de falhas da ram:%.2f%% \n",(((float)cont.miss_ram/(float)cont.acessos_ram)*100.0));

	free(tabela);
	free(t);
	free(cache);
	free(ram);
	return 0;
}

