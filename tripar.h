#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

char path[120];
int albSize;
int chunkSize;
int NTHREADS_MEM = 1;
int NTHREADS_DISK = 1;
int nodeCount;
int vriSize;
long maxItemInMem;
int maxNumIteration;
long* vriOffset;
int iterationId = 0;
int** countElbows;
int maxItemForThread;
int** memAdj;
int* vDegrees;
long* triangleCountDisk;
long* triangleCountMem;
int** chunks;
int fprOut;
int fprAlbDsk;
int fprAlbMem;
int startMemNode = 0;
int endMemNode;
int* vriDsk;
int* vriMem;
long inDskOffset = 0;
long inMemOffset = 0;
int fpwTri;
int **writeChunk;
int *writeChunkIndex;
long *writeOffset;
int* outGraphInMem;
int output_type;

long fileWriteOffset=0;				// Shared variable
pthread_mutex_t fileWriteOffset_mutex;	// Mutex controlling access to fileWriteOffset
long getFileSize(char path[]);
void init_tripar(char* p, char* af, char* df, int alb_size, int chunk_size, int nt_mem, int nt_disk);
void graph_orientation_and_indexing();
int list_all_triangles(int ot);
int mainThread();
int readSelecetedALBs(int range, int size, long offset);
void *memThread(void *x);
void *diskThread(void *x);
void *listTriangleInDisk(void *x);
void *listTriangleInMemory(void *x);
void writeTri(int tId, int u, int v, int w);
void finalWriteTri();

void init_tripar(char* p, char* af, char* df, int alb_size, int chunk_size, int nt_mem, int nt_disk){
	sprintf(path, "%s", p);
	albSize = alb_size;
	chunkSize = chunk_size;

  	char tmpPath[120];
	sprintf(tmpPath, "%s/%s", path, df);
    long fileSize = getFileSize(tmpPath);
   	nodeCount = fileSize/8;
	vriSize = nodeCount / albSize + 2;

	sprintf(tmpPath, "%s/%s", path, af);
	fileSize = getFileSize(tmpPath);

    printf("\nDataset Size: %ld Bytes; Nodes Count: %d;\n\n", fileSize, nodeCount);

	maxItemInMem = chunkSize / 4;
    NTHREADS_MEM = nt_mem;
    NTHREADS_DISK = nt_disk;	
	
	//printf("MaxInMem	: %ld\n", maxItemInMem);
	maxNumIteration = fileSize/ 8 / maxItemInMem + 1;
	countElbows = (int**) malloc(maxNumIteration * sizeof(int*));

	maxItemForThread = maxItemInMem / NTHREADS_MEM + 1;

    int i = 0;
    memAdj = (int**) malloc(nodeCount * sizeof (int*));
    vDegrees = (int*) malloc(nodeCount * 4);
    for (i = 0; i < nodeCount; i++) {
        vDegrees[i] = 0;
    }
    triangleCountMem = (long*) malloc(NTHREADS_MEM * sizeof (long));
    for (i = 0; i < NTHREADS_MEM; i++) {
        triangleCountMem[i] = 0;
    }   
    triangleCountDisk = (long*) malloc(NTHREADS_DISK * sizeof (long));
    chunks = (int**) malloc(NTHREADS_DISK * sizeof (int*));
    for (i = 0; i < NTHREADS_DISK; i++) {
        triangleCountDisk[i] = 0;
        chunks[i] = (int*) malloc(chunkSize);
    }
    char outputPath[120];
	sprintf(outputPath, "%s/tri.bin", path);
	if ((fpwTri = open(outputPath, O_CREAT|O_RDWR, S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
	    perror("open() error");
	    exit(1);
	}
    
    writeChunk = (int**) malloc((NTHREADS_MEM+NTHREADS_DISK) * sizeof (int*));
    writeChunkIndex = (int*) malloc((NTHREADS_MEM+NTHREADS_DISK) * sizeof (int));
    writeOffset = (long*) malloc((NTHREADS_MEM+NTHREADS_DISK) * sizeof (long));
    for(i=0; i<NTHREADS_MEM+NTHREADS_DISK; i++){
		writeChunk[i] = (int*) malloc(4096);
		writeChunkIndex[i] = 0;
		writeOffset[i] = 0;
    }
}

void graph_orientation_and_indexing(){
	// Input graph is in adjacency list and in binary format
		// deg.bin contains list of (NodeId and Degree) in binary format
		// adj.bin contains list of 'd' number of neighbor id, where 'd' is the degree
		// neighbors are sorted ASC by their ids
	// This implementation considers that all adjacency list will be fit into memory after orientation
	char tmpPath[120];
	FILE *fpwOut;
	int fpwAlbDsk;
	int fpwAlbMem;
	FILE *fprDeg;
	FILE *fprAdj;
	sprintf(tmpPath, "%s/deg.bin", path);
	if((fprDeg = fopen(tmpPath, "rb"))==NULL){
		printf("file open fail 1\n");
		return;
	}
	sprintf(tmpPath, "%s/adj.bin", path);
	if((fprAdj = fopen(tmpPath, "rb"))==NULL){
		printf("file open fail 1\n");
		return;
	}
	sprintf(tmpPath, "%s/out.bin", path);
	if((fpwOut = fopen(tmpPath, "wb"))==NULL){
		printf("file open fail 2\n");
		return;
	}
	sprintf(tmpPath, "%s/albdsk.bin", path);
	//if((fpwAlbDsk = fopen(tmpPath, "wb"))==NULL){
	//	printf("file open fail 3\n");
	//	return;
	//}
	if ((fpwAlbDsk = open(tmpPath, O_CREAT|O_RDWR, S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
		printf("%s\n", tmpPath);
	    perror("open() error");
	    exit(1);
	}
	
	sprintf(tmpPath, "%s/albmem.bin", path);
	//if((fpwAlbMem = fopen(tmpPath, "wb"))==NULL){
	//	printf("file open fail 3\n");
	//	return;
	//}
	if ((fpwAlbMem = open(tmpPath, O_CREAT|O_RDWR, S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
		printf("%s\n", tmpPath);
	    perror("open() error");
	    exit(1);
	}
	
	int* degrees;
	int node;
	int adjCnt;
	int uu;
	// we can reduce access tme here by keeping only the degree
	degrees = (int*) malloc(nodeCount*4);
	//while(1){
	while(!feof(fprDeg)){
		uu=fread(&node, 4, 1, fprDeg);
		uu=fread(&adjCnt, 4, 1, fprDeg);
		degrees[node] = adjCnt;
		//printf("%d %d\n", node, adjCnt);
		if(node==nodeCount-1){
			break;
		}
	}//Read All
	fclose(fprDeg);
	//printf("%d\n", nodeCount);
	int j;
	int* adjALBs;
	adjALBs = (int*)malloc(vriSize*4);
	for(j=0; j < vriSize; j++){
		adjALBs[j] = 0;
	}
	vriOffset = (long*) malloc(vriSize * sizeof (long));
    for (j = 0; j < vriSize; j++) {
        vriOffset[j] = -1;
    }
	
	int readCount = 0;
	int adjALBsCount = 0;
	int lastVertex=0;
	int fromAlb=0; 
	int outNodeCount;
	int* outNodes;
	int v;
	int degV;
	int hid;
    long offset = 0;
	//int maxOutDegree = 0;
	//int maxOutDegreeNode = -1;
	int u = 0;
	vriDsk = (int*) malloc(maxNumIteration * vriSize * 4);
	vriMem = (int*) malloc(maxNumIteration * vriSize * 4);
	int vriDskCount = 0;
	int vriMemCount = 0;
	//while(1){
	while(!feof(fprAdj)){
		if((readCount) >= maxItemInMem){
			if(adjALBsCount>0){
				if(iterationId == maxNumIteration){
					maxNumIteration++;
					countElbows = (int**) realloc(countElbows, maxNumIteration*sizeof(int*));
				}
				countElbows[iterationId] = (int*) malloc(12);
				countElbows[iterationId][0] = lastVertex;
				countElbows[iterationId][1] = 0;
				countElbows[iterationId][2] = 0;
				//printf("%d %d: ", iterationId, lastVertex+1);
				for(j=0; j < vriSize; j++){
					if(adjALBs[j]!=0){
						if(j < fromAlb || j > lastVertex/albSize){ // outside memory
							vriDsk[vriDskCount++] = j;
							countElbows[iterationId][1]++;
						}else if(j == fromAlb || j == lastVertex/albSize){ // both outside&inside memory
							vriDsk[vriDskCount++] = j;
							countElbows[iterationId][1]++;
							vriMem[vriMemCount++] = j;
							countElbows[iterationId][2]++;
						}					
						else{ // inside memory
							vriMem[vriMemCount++] = j;
							countElbows[iterationId][2]++;
						}
					}
				}
				iterationId++;
				fromAlb = lastVertex/albSize;
			}
			for(j=0; j < vriSize; j++){
				adjALBs[j] = 0;
			}
			adjALBsCount = 0;
			readCount = 0;
		}
		outNodeCount = 0;
		outNodes = (int*)malloc(degrees[u]*4);
		for(j=0; j < degrees[u]; j++){
			uu = fread(&v, 4, 1, fprAdj);	
			degV = degrees[v];
			if( degrees[u] < degV || (degrees[u] == degV && u < v)){
				outNodes[outNodeCount++] = v;
			}else if(degV > 1 && adjALBs[v/albSize] == 0){
				adjALBs[v/albSize] = 1;
				adjALBsCount++;
			}
		}
		//printf("\n");
		if(outNodeCount > 0){
		    hid = u / albSize;
		    if (vriOffset[hid] < 0) {
		        vriOffset[hid] = offset;
		    }   
			lastVertex = u;
			fwrite(&u, 4, 1, fpwOut);	
			fwrite(&outNodeCount, 4, 1, fpwOut);
			fwrite(outNodes, 4, outNodeCount, fpwOut);			
			readCount += outNodeCount+2;
			//printf("%d %d",u, outNodeCount);
			offset += (outNodeCount + 2)*4;
		}
		free(outNodes);
		outNodes = NULL;
		u++;
		if(u==nodeCount){
			break;
		}
		//printf("%d\n", u);
	}//Read All
	//printf("%d\n", lastVertex);
	if(adjALBsCount>0){
		if(iterationId == maxNumIteration){
			maxNumIteration++;
			countElbows = (int**) realloc(countElbows, maxNumIteration*sizeof(int*));
		}
		countElbows[iterationId] = (int*) malloc(12);
		countElbows[iterationId][0] = lastVertex;
		countElbows[iterationId][1] = 0;
		countElbows[iterationId][2] = 0;
		//printf("%d %d: ", iterationId, lastVertex+1);
		for(j=0; j < vriSize; j++){
			if(adjALBs[j]!=0){
				if(j < fromAlb || j > lastVertex/albSize){ // outside memory
					vriDsk[vriDskCount++] = j;
					countElbows[iterationId][1]++;
				}else if(j == fromAlb || j == lastVertex/albSize){ // both outside&inside memory
					vriDsk[vriDskCount++] = j;
					countElbows[iterationId][1]++;
					vriMem[vriMemCount++] = j;
					countElbows[iterationId][2]++;
				}					
				else{ // inside memory
					vriMem[vriMemCount++] = j;
					countElbows[iterationId][2]++;
				}
			}
		}
		iterationId++;
	}
	if(vriDskCount>0){
		uu = pwrite(fpwAlbDsk, vriDsk, vriDskCount*4, 0);
	}
	if(vriMemCount>0){
		uu = pwrite(fpwAlbMem, vriMem, vriMemCount*4, 0);
	}
	free(vriDsk);
	free(vriMem);
	vriDsk = NULL;
	vriMem = NULL;
	
	vriOffset[hid+1] = offset;
    vriOffset[vriSize-1] = offset;
    for (hid = vriSize-1; hid > 0 ; hid--) {
    	if(vriOffset[hid] < 0){
    		vriOffset[hid] = vriOffset[hid+1];
    	}
    	//printf("%ld\n", vriOffset[hid]);
    }
    --vriSize;
    
    //printf("%ld\n", vriOffset[0]);
	//printf("Last Node = %d %d %d %d\n", u, nodeCount, countElbows[iterationId-1][0], iterationId);
	//printf("Max Out Degree : %d %d\n", maxOutDegreeNode, maxOutDegree);
	maxNumIteration = iterationId;
	iterationId = 0;
    //for (hid = 0; hid < maxNumIteration ; hid++) {
    //	printf("%d %d\n", countElbows[hid][0], countElbows[hid][1]);
    //}
	free(adjALBs);
	adjALBs = NULL;
	fclose(fpwOut);
	close(fpwAlbDsk);
	close(fpwAlbMem);
	fclose(fprAdj);	
	free(degrees);
}

int list_all_triangles(int ot){
	output_type = ot;

	char tmpPath[120];
    sprintf(tmpPath, "%s/out.bin", path);
    if ((fprOut = open(tmpPath, O_RDONLY)) < 0) {
        perror("open(out) error");
    }
    
	long fileSize = getFileSize(tmpPath);
	outGraphInMem = (int*) malloc(fileSize);
    long ret = pread(fprOut, outGraphInMem, fileSize, 0);
    //printf("%d %d\n",outGraphInMem[0],outGraphInMem[1]);
    
    sprintf(tmpPath, "%s/albdsk.bin", path);
    if ((fprAlbDsk = open(tmpPath, O_RDONLY)) < 0) {
        perror("open(alb) error");
    }
    sprintf(tmpPath, "%s/albmem.bin", path);
    if ((fprAlbMem = open(tmpPath, O_RDONLY)) < 0) {
        perror("open(alb) error");
    }

	mainThread();

    close(fprOut);
    close(fprAlbDsk);
    close(fprAlbMem);

    long totalTriangle = 0;
    long i, td=0, tm=0;
    for (i = 0; i < NTHREADS_MEM; i++) {
        tm += triangleCountMem[i];
    }
    for (i = 0; i < NTHREADS_DISK; i++) {
        td += triangleCountDisk[i];
    }   
    printf("\nTotal Triangles	: D-%ld + M-%ld = %ld\n", td, tm, td+tm);
	printf("\nTotal Triangles	: %ld\n", td+tm);
	return 0;
}

int mainThread() {
    pthread_t threadDisk, threadMem;
    long ret, offset;
    int nodeAdjCnt[2];
    int node;
    int adjCnt;
    int i;
    offset = 0;
    startMemNode = 0;

    while ((ret = pread(fprOut, nodeAdjCnt, 8, offset)) == 8) {
        node = nodeAdjCnt[0];
        adjCnt = nodeAdjCnt[1];
        vDegrees[node] = adjCnt;
        memAdj[node] = (int*) malloc(adjCnt * 4);
        ret = pread(fprOut, memAdj[node], adjCnt * 4, offset + 8);
        //printf("%d %d", node, adjCnt);
        //for(i=0; i<adjCnt; i++){
        //	printf(" %d", memAdj[node][i]);
        //}
        //printf("\n");
        adjCnt += 2;
        offset += adjCnt * 4;
        if(node == countElbows[iterationId][0]){
        	endMemNode = node;
        	pthread_create(&threadMem, NULL, memThread, &nodeAdjCnt);
        	pthread_create(&threadDisk, NULL, diskThread, &nodeAdjCnt);
        	pthread_join(threadMem, NULL);
        	pthread_join(threadDisk, NULL);
            for (i = startMemNode; i <= endMemNode; i++) {
                free(memAdj[i]);
                memAdj[i] = NULL;
                vDegrees[i] = 0;
            }
            iterationId++;
            if(iterationId==maxNumIteration){
            	break;
            }
            startMemNode = node+1;
			//break;
			//printf("Iteration %d\n", iterationId);
        } // perform overall triangulation 
    }//Read All
    //printf("*Last Node = %d\n", node);
    //printf("*Last Node = %d %d\n", countElbows[iterationId-1][0], threadMemIndex);
    //printf("Last Iteration = %d\n", iterationId);

    finalWriteTri();

    return 0;
}

void *memThread(void *x) {
	int i,uu;
	int numVriMem = countElbows[iterationId][2];
	//printf("%d. numVriMem: %d, (%d-%d): %d, (numVriMem*albSize): %d, Avoid: %d\n",iterationId, numVriMem, endMemNode, startMemNode, (endMemNode - startMemNode), (numVriMem*albSize), (endMemNode - startMemNode - numVriMem*albSize));
	if(numVriMem>0){
		int vriRanges[NTHREADS_MEM][3];
		pthread_t threads[NTHREADS_MEM];
		vriMem = (int*) malloc(numVriMem * 4);
		uu = pread(fprAlbMem, vriMem, numVriMem*4, inMemOffset);
		inMemOffset += numVriMem*4;
    	int vriPerThread = numVriMem / NTHREADS_MEM;
        for (i = 0; i < NTHREADS_MEM-1; i++) {
        	vriRanges[i][0] = vriPerThread * i;
            vriRanges[i][1] = vriRanges[i][0] + vriPerThread;
            vriRanges[i][2] = i;
            pthread_create(&threads[i], NULL, listTriangleInMemory, (void *) &vriRanges[i]);
        }
        vriRanges[i][0] = vriPerThread * i;
        vriRanges[i][1] = numVriMem;
        vriRanges[i][2] = i;
        pthread_create(&threads[i], NULL, listTriangleInMemory, (void *) &vriRanges[i]);
	
        for (i = 0; i < NTHREADS_MEM; i++) {
            pthread_join(threads[i], NULL);
        }
        free(vriMem);
        vriMem = NULL;
    }
}

void *diskThread(void *x) {
	int i, uu;
	int numVriDsk = countElbows[iterationId][1];
	if(numVriDsk>0){
	    int vriRanges[NTHREADS_DISK][3];
		pthread_t threads[NTHREADS_DISK];
		vriDsk = (int*) malloc(numVriDsk * 4);
		uu = pread(fprAlbDsk, vriDsk, numVriDsk*4, inDskOffset);
		inDskOffset += numVriDsk*4;
    	int vriPerThread = numVriDsk / NTHREADS_DISK;
        for (i = 0; i < NTHREADS_DISK-1; i++) {
        	vriRanges[i][0] = vriPerThread * i;
            vriRanges[i][1] = vriRanges[i][0] + vriPerThread;
            vriRanges[i][2] = i;
            pthread_create(&threads[i], NULL, listTriangleInDisk, (void *) &vriRanges[i]);
        }
        vriRanges[i][0] = vriPerThread * i;
        vriRanges[i][1] = numVriDsk;
        vriRanges[i][2] = i;
        pthread_create(&threads[i], NULL, listTriangleInDisk, (void *) &vriRanges[i]);
        for (i = 0; i < NTHREADS_DISK; i++) {
            pthread_join(threads[i], NULL);
        }
        free(vriDsk);
        vriDsk = NULL;
    }
}

int readSelecetedALBs(int range, int size, long offset){
	//return pread(fprOut, chunks[range], size, offset);
	memcpy(chunks[range], &outGraphInMem[offset/4], size);
}

// Process disk data by each thread.
void *listTriangleInDisk(void *X) {
    int elbowV, thirdV, PCV = -1;
    int i,j,k,hid;
    int min, mid, max;
    long tc = 0;
    int* adjPCV;
    int* elbowVertices;
    int degreePCV = 0, elbowCount = 0;
    int* range;
    int adjCnt = 0;
    int ci,ret;
    long offset;
    long size;
    int itemsInChunk = 0;
    range = ((int *) X);

	//return;
  	//printf("D-%2d: %5d ~%5d : %ld\n", range[2], range[0], range[1], tc);
    for (k = range[0]; k < range[1]; k++) {  // List of indexes assigned to this thread
        //printf("Ok start1 %d %d\n", k, numVriDsk);
        hid = vriDsk[k];
        //printf("Ok start2 %d\n", hid);
        elbowCount = 0;
        degreePCV = 0;        
        PCV = -1;
        adjCnt = 0;
        //continue;
        offset = vriOffset[hid];
        size = vriOffset[hid + 1] - offset;
        if(size<=0){
        	printf("Size: %ld %d\n", size, hid);
        	exit(1);
        }
        ci = 0;
        while (size > 0) {					// read until all the contents of this index is read
            if (size > chunkSize) {
                ret = readSelecetedALBs(range[2], chunkSize, offset);
                //ret = pread(fprOut, chunks[range[2]], chunkSize, offset);
                itemsInChunk = chunkSize / 4;
                offset += chunkSize;
                size -= chunkSize;
            } else {
                //ret = pread(fprOut, chunks[range[2]], size, offset);
                ret = readSelecetedALBs(range[2], size, offset);
                itemsInChunk = size / 4;
                size = 0;
            }
            while (ci < itemsInChunk) {
                if (PCV == -1) {
                    PCV = chunks[range[2]][ci++];
                    if (PCV / albSize > hid) {
                        size = 0;
                        break;
                    }
                    adjCnt = 0;
	                elbowCount = 0;
	                degreePCV = 0;
                }
                if (adjCnt == 0 && ci < itemsInChunk) {
                    adjCnt = chunks[range[2]][ci++];
	                if(adjCnt<2 || (PCV >= startMemNode && PCV <= endMemNode)) { // if already in memory
	                //if(adjCnt<2) { // consider everything for disk
	                    ci += adjCnt;
	                    PCV = -1;
	                    continue;
	                }
                }//else if(ci >= itemsInChunk){
                //	if (size<=0 && PCV!=-1){
                //		printf("Problem: %d\n", hid);
                //	}
                //}
                if (ci < itemsInChunk){
					if(degreePCV == 0){
                    	adjPCV = (int*) malloc(adjCnt * 4);
                    	elbowVertices = (int*) malloc(adjCnt * 4);
					}
                    while (ci < itemsInChunk && degreePCV < adjCnt) {
                        adjPCV[degreePCV] = chunks[range[2]][ci++];
                        if (adjPCV[degreePCV] >= startMemNode && adjPCV[degreePCV] <= endMemNode) {
                            elbowVertices[elbowCount++] = adjPCV[degreePCV];
                        }
                        degreePCV++;
                    }
                    // Join Operation Starts here.
                    if(adjCnt==degreePCV){  // When all the adjacent vertices are loaded
			            for (i = 0; i < elbowCount; i++) {   // can be considered as R ( here R is the set of adjacent vertices of i which have outgoing edge in memory )
			                elbowV = elbowVertices[i];
			                if (vDegrees[elbowV] > 0) {
			                	if(vDegrees[elbowV] > degreePCV){
					                for (j = 0; j < degreePCV; j++) {   // can be considered as S (here S is the set of all adjacent vertices of i) |R| < |S|   
					                    thirdV = adjPCV[j];
					                    if (elbowV != thirdV) {
						                    min = 0;
						                    max = vDegrees[elbowV];
						                    mid = max / 2;
						                    while (max > min) {		   // perform binary search to find common adjacent vertex
						                        mid = (max + min) / 2;
						                        if (memAdj[elbowV][mid] == thirdV) {
						                        	if(output_type!=1)
			            	    						printf("%d,%d,%d\n", PCV, elbowV, thirdV);
						                        	writeTri(NTHREADS_MEM + range[2], PCV, elbowV, thirdV);
						                            tc++;
						                            break;
						                        } else if (memAdj[elbowV][mid] < thirdV) {
						                            min = mid + 1;
						                        } else if (memAdj[elbowV][mid] > thirdV) {
						                            max = mid;
						                        }
						                    }
					                    }
					                }
			                	}
			                	else{
					                for (j = 0; j < vDegrees[elbowV]; j++) {   // can be considered as S (here S is the set of all adjacent vertices of i) |R| < |S|   
					                    thirdV = memAdj[elbowV][j];
				                        min = 0;
				                        max = degreePCV;
				                        mid = max / 2;
				                        while (max > min) {		   // perform binary search to find common adjacent vertex
				                            mid = (max + min) / 2;
				                            if (adjPCV[mid] == thirdV) {
				                            	if(output_type!=1)
			            	    					printf("%d,%d,%d\n", PCV, elbowV, thirdV);
				                            	writeTri(NTHREADS_MEM + range[2], PCV, elbowV, thirdV);
				                                tc++;
				                                break;
				                            } else if (adjPCV[mid] < thirdV) {
				                                min = mid + 1;
				                            } else if (adjPCV[mid] > thirdV) {
				                                max = mid;
				                            }
				                        }
					                }
			                   	}//else
			                }//if
			            }//for
		                free(adjPCV);
		                adjPCV = NULL;
		                free(elbowVertices);
		                elbowVertices = NULL;
						PCV = -1;
		                adjCnt = 0;
					}
                  	// Join Operation Ends here.
                }
            }
            ci %= itemsInChunk;
        }
        //printf("Ok End\n");
    }//for
    triangleCountDisk[range[2]] += tc;
    //printf("%ld\n", tc);
}

// Process in-memory data by each thread.
void *listTriangleInMemory(void *X) {
    int elbowV;
    int hid;
    int to;
    int PCV;
    int thirdV;
    int i;
    int j;
    int min, mid, max;
    long tc = 0;
    int* range;
    range = ((int *) X);
	//return;
	
	for(hid = range[0]; hid < range[1]; hid++){
        to = vriMem[hid]*albSize+albSize;	
        for (PCV = vriMem[hid]*albSize; PCV < to; PCV++) {	// Potential Cone Vertex. List of adjacencies assigned to this thread
	        if (PCV > endMemNode) { // 
	            break;
	        }
		    //printf("PCV: %d %d\n", PCV, vDegrees[PCV]);
		    if (vDegrees[PCV] > 1) {
				// Join Operation Starts here.       
				for(i = 0; i < vDegrees[PCV]; i++){//Adjacencies of PCV. // can be considered as R ( here R is the set of adjacent vertices of i which have outgoing edge in memory )
			        elbowV = memAdj[PCV][i];
			        //printf("elbowV: %d %d\n", elbowV+1, vDegrees[elbowV]);
			        if (elbowV > endMemNode) { // 
			            break;
			        }
			        for(j = 0; j < vDegrees[elbowV]; j++) {//Adjacencies of elbowV   // can be considered as S (here S is the set of all adjacent vertices of i) |R| < |S|
			            thirdV = memAdj[elbowV][j];
			            min = 0;
			            max = vDegrees[PCV];
			            mid = max / 2;
			            while (max > min) {		   // perform binary search to find common adjacent vertex
			                mid = (max + min) / 2;
			                if(memAdj[PCV][mid] == thirdV) {
								if(output_type!=1)
			            	    	printf("%d,%d,%d\n", PCV, elbowV, thirdV);
			            	    writeTri(range[2], PCV, elbowV, thirdV);
			                    tc++;
			                    break;
			                } else if (memAdj[PCV][mid] < thirdV) {
			                    min = mid + 1;
			                } else if (memAdj[PCV][mid] > thirdV) {
			                    max = mid;
			                }
			            }
			        }//for
				}//for
		    }//if (vDegrees[PCV] > 1)
        }//for
	}
    triangleCountMem[range[2]] += tc;    
    //printf("M-%2d: %9d ~ %9d : %ld\n", range[2], range[0], range[1], tc);
}

void writeTri(int tid, int u, int v, int w){
	if(output_type==0) return;
	long currentOffset;
    writeChunk[tid][writeChunkIndex[tid]++] = u;
	if(writeChunkIndex[tid] == 1024){
		pthread_mutex_lock(&fileWriteOffset_mutex);
		currentOffset = fileWriteOffset;
		fileWriteOffset += 4096;  
		pthread_mutex_unlock(&fileWriteOffset_mutex);
		currentOffset = pwrite(fpwTri, writeChunk[tid], 4096, currentOffset);
		writeChunkIndex[tid] = 0;
		writeChunk[tid][writeChunkIndex[tid]++] = v;
		writeChunk[tid][writeChunkIndex[tid]++] = w;
	}else{
		writeChunk[tid][writeChunkIndex[tid]++] = v;
		if(writeChunkIndex[tid] == 1024){
			pthread_mutex_lock(&fileWriteOffset_mutex);
			currentOffset = fileWriteOffset;
			fileWriteOffset += 4096;  
			pthread_mutex_unlock(&fileWriteOffset_mutex);
			currentOffset = pwrite(fpwTri, writeChunk[tid], 4096, currentOffset);
			writeChunkIndex[tid] = 0;
			writeChunk[tid][writeChunkIndex[tid]++] = w;
		}else{
			writeChunk[tid][writeChunkIndex[tid]++] = w;
			if(writeChunkIndex[tid] == 1024){
				pthread_mutex_lock(&fileWriteOffset_mutex);
				currentOffset = fileWriteOffset;
				fileWriteOffset += 4096;  
				pthread_mutex_unlock(&fileWriteOffset_mutex);
				currentOffset = pwrite(fpwTri, writeChunk[tid], 4096, currentOffset);
				writeChunkIndex[tid] = 0;
			}
		}
	}
}

void finalWriteTri(){
	if(output_type==0) return;
	int i;
	long uu;
	for(i=0; i<NTHREADS_MEM + NTHREADS_DISK; i++){
		if(writeChunkIndex[i]>0){
			uu = pwrite(fpwTri, writeChunk[i], writeChunkIndex[i]*4, fileWriteOffset);
			fileWriteOffset += writeChunkIndex[i]*4;
		}
	}
}

long getFileSize(char path[]){
    struct stat filestatus;
    stat(path, &filestatus);
    return filestatus.st_size;
}
