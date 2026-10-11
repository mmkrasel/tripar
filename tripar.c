//COMPILE: gcc tripar.c -g -pg -o tripar -O2 -lrt -pthread -lc 
//RUN: ./tripar

#include <stdio.h>
#include "tripar.h"

void create_undirected_graph(){
	int graph[24][7] = {
		{5, 1, 3, 4, 7, 16},
		{4, 0, 4, 15, 19},
		{3, 3, 6, 23},
		{3, 0, 2, 7},
		{3, 0, 1, 20},
		{2, 7, 10},
		{4, 2, 7, 9, 17},
		{6, 0, 3, 5, 6, 9, 10},
		{2, 9, 12},
		{4, 6, 7, 8, 18},
		{3, 5, 7, 11},
		{2, 10, 13},
		{4, 8, 13, 14, 21},
		{3, 11, 12, 14},
		{3, 12, 13, 21},
		{2, 1, 16},
		{3, 0, 15, 18},
		{3, 6, 18, 19},
		{3, 9, 16, 17},
		{2, 1, 17},
		{4, 4, 21, 22, 23},
		{4, 12, 14, 20, 22},
		{2, 20, 21},
		{2, 2, 20}
	};
	FILE* fprDeg = fopen("/data/deg.bin", "wb");
    FILE* fprAdj = fopen("/data/adj.bin", "wb");
	for(int i=0; i<24; i++){
		fwrite(&i, 4, 1, fprDeg);
		fwrite(&graph[i][0], 4, 1, fprDeg);
		// printf("%d ", i);
		// printf("%d ", graph[i][0]);
		for(int j=1; j<=graph[i][0]; j++){
			fwrite(&graph[i][j], 4, 1, fprAdj);
			//printf("%d ", graph[i][j]);
		}
		//printf("\n");
	}
	// printf("\n");
	fflush(fprDeg);
	fclose(fprDeg);
	fflush(fprAdj);
	fclose(fprAdj);
}


int main() {

	/* You may test the implementation by creating customized graph */
	create_undirected_graph();

   	char* dataset_dir  = "/data";
	char* degree_path = "deg.bin";	// contains pairs of vertices and their degrees
	char* adjlist_path = "adj.bin";	// contains only neighors of each vertex
    int albSize = 8; 				// number of adjacency list in each batch. Change it for larger datasets
    int chunkSize = 128;			// number of bytes in each chanunk/page. Change it for larger datasets
	int nt_mem = 1;					// #threads to join adj. list that are already in memory
	int nt_disk = 1;				// #threads to join adj. list at memory with indexed adj. list at disk
	init_tripar(dataset_dir, adjlist_path, degree_path, albSize, chunkSize, nt_mem, nt_disk);

	//printf("Start Orienting and Indexing Graph...\n");
   	graph_orientation_and_indexing();
   	//printf("Graph orientation DONE...\n");
	printf("\nListing triangles...\n");
	int output_type = 0; // 0: console only, 1: file only; 2: both console & file
	list_all_triangles(output_type);
	
    return 1;
}