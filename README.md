# TriPar
Triangle listing is one of the most fundamental operations in graph mining with wide range of applications in knowledge discovery and network analysis. Triangle listing algorithms typically scan over large graphs in iterations, which causes significant I/O and CPU overhead. TriPar, an open-source C library, overcomes I/O bottlenecks by performing concurrent triangle listing over indexed graphs and significantly reduces processing time. Given a graph, which is indexed by a graph indexing tool such as GraphIdx, TriPar exploits the multicore CPU to process an indexed graph concurrently and lists all triangles by identifying potential cone vertices and elbow vertices in the indexed graph. As a result, TriPar can drastically reduce the number of I/O requests and discovers triangles over indexed graph data in parallel that substantially improve the triangle listing throughput compared to non-indexed, sequential approaches.


## Compile TriPar
```
gcc tripar.c -g -pg -o tripar -O2 -lrt -pthread -lc
```

# Usages

## Include TriPar and TriPar as Headers
```
#include "tripar.h"
```
  
## Configure TriPar
```
/* Graph dataset information
 - Represented using adjacency list
 - Data in binary format */
   
char* dataset_dir  = "...";
char* degree_path = "...";		// contains pairs of vertices and their degrees
char* adjlist_path = "...";		// contains only neighors of each vertex
int albSize = 8;      // number of adjacency list in each batch. Change it for larger datasets
int chunkSize = 128;  // number of bytes in each chanunk/page. Change it for larger datasets
int nt_mem = 1;       // #threads to join adj. list that are already in memory
int nt_disk = 1;      // #threads to join adj. list at memory with indexed adj. list at disk
init_tripar(dataset_dir, adjlist_path, degree_path, albSize, chunkSize, nt_mem, nt_disk);
```
  
## Construct oriented (give direction to edges) indexed graph from input graph
```
  graph_orientation_and_indexing();
```

## List all triangles
```
  int output_type = 0; 
  /* 
    0: console only
    1: file only
    2: both console & file
  */
  list_all_triangles(output_type);
```
##
