//------------------------------------------------------------------------------
// LAGraph/experimental/benchmark/helloworld_demo.c: a simple demo
//------------------------------------------------------------------------------

// LAGraph, (c) 2019-2022 by The LAGraph Contributors, All Rights Reserved.
// SPDX-License-Identifier: BSD-2-Clause
//
// For additional details (including references to third party source code and
// other files) see the LICENSE file or contact permission@sei.cmu.edu. See
// Contributors.txt for a full list of contributors. Created, in part, with
// funding and support from the U.S. Government (see Acknowledgments.txt file).
// DM22-0790

// Contributed by Timothy A Davis, Texas A&M University

//------------------------------------------------------------------------------

// This main program is a simple driver for testing and benchmarking the
// LAGraph_HelloWorld "algorithm", in experimental/algorithm.  To use it,
// compile LAGraph while in the build folder with these commands:
//
//      cd LAGraph/build
//      cmake ..
//      make -j8
//
// Then run this demo with an input matrix.  For example:
//
//      ./experimental/benchmark/helloworld_demo ../data/west0067.mtx
//      ./experimental/benchmark/helloworld_demo < ../data/west0067.mtx
//      ./experimental/benchmark/helloworld_demo ../data/karate.mtx
//
// If you create your own algorithm and want to mimic this main program, call
// it write in experimental/benchmark/whatever_demo.c (with "_demo.c" as the
// end of the filename), and the cmake will find it and compile it.

// This main program makes use of supporting utilities in
// src/benchmark/LAGraph_demo.h and src/utility/LG_internal.h.
// See helloworld2_demo.c for a main program that just uses the
// user-callable methods in LAGraph.h and LAGraphX.h.

#include "../../src/benchmark/LAGraph_demo.h"
#include "LAGraphX.h"
#include "LG_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


// LG_FREE_ALL is required by LG_TRY
#undef  LG_FREE_ALL
#define LG_FREE_ALL        \
{                          \
    LAGraph_Delete (&G, "") ;  \
}
// GrB_free (&Y) ;

#define OK(method)                                  \
{                                                   \
    int status = method ;                           \
    if (! (status == GrB_SUCCESS || status == 0))   \
    {                                               \
        printf ("file: %s line: %d status: %d\n",   \
            __FILE__, __LINE__, status) ;           \
        LG_FREE_ALL ;                               \
        return (status) ;                           \
    }                                               \
}




// Represents and edge.
typedef struct {
    GrB_Index src;
    GrB_Index dest;
} Edge;




// Function prototypes.
inline GrB_Index random_number(GrB_Index begin, GrB_Index end);
int generate_edge_deletions(Edge *deletions, LAGraph_Graph G, GrB_Index batch_size, bool is_symmetric);
int generate_edge_insertions(Edge *insertions, LAGraph_Graph G, GrB_Index batch_size, bool is_symmetric);




// Get random number in the range [begin, end].
inline GrB_Index random_number(GrB_Index begin, GrB_Index end) {
  GrB_Index r0 = rand() & 0xFFFF;
  GrB_Index r1 = rand() & 0xFFFF;
  GrB_Index r = (r0 << 16) | r1;
  return begin + r % (end + 1 - begin);
}




// Generate edge deletions.
int generate_edge_deletions(Edge *deletions, LAGraph_Graph G, GrB_Index batch_size, bool is_symmetric) {
    int retries = 5;
    int i = 0;
    GrB_Index n;
    GrB_Vector row;
    GrB_Index *row_indices;
    bool      *row_values;
    OK( GrB_Matrix_nrows(&n, G->A) );
    OK( GrB_Vector_new(&row, GrB_BOOL, n) );
    row_indices = (GrB_Index*) malloc(n * sizeof(GrB_Index));
    row_values  = (bool*)      malloc(n * sizeof(bool));
    for (int b=0; b<batch_size; ++b) {
        for (int r=0; r<retries; ++r) {
            GrB_Index u = random_number(0, n-1);
            OK( GrB_Col_extract(row, NULL, NULL, G->A, GrB_ALL, n, u, GrB_DESC_T0) );  // GrB_DESC_T0
            GrB_Index degree = 0;
            OK( GrB_Vector_nvals(&degree, row) );
            if (degree == 0) continue;
            GrB_Index j = random_number(0, degree-1);
            OK( GrB_Vector_extractTuples_BOOL(row_indices, row_values, &degree, row) );
            GrB_Index v = row_indices[j];
            deletions[i].src  = u;
            deletions[i].dest = v;
            ++i;
            if (is_symmetric) {
                deletions[i].src  = v;
                deletions[i].dest = u;
                ++i;
            }
            break;
        }
    }
    OK( GrB_Vector_free(&row) );
    free(row_indices);
    free(row_values);
    return i;
}




// Generate edge insertions.
int generate_edge_insertions(Edge *insertions, LAGraph_Graph G, GrB_Index batch_size, bool is_symmetric) {
    int retries = 5;
    int i = 0;
    GrB_Index n;
    GrB_Vector row;
    OK( GrB_Matrix_nrows(&n, G->A) );
    OK( GrB_Vector_new(&row, GrB_BOOL, n) );
    for (int b=0; b<batch_size; ++b) {
        for (int r=0; r<retries; ++r) {
            GrB_Index u = random_number(0, n-1);
            GrB_Index v = random_number(0, n-1);
            bool w = false;
            OK( GrB_Col_extract(row, NULL, NULL, G->A, GrB_ALL, n, u, GrB_DESC_T0) );  // GrB_DESC_T0
            int status = GrB_Vector_extractElement_BOOL(&w, row, v);
            if (status == GrB_SUCCESS) continue;
            insertions[i].src  = u;
            insertions[i].dest = v;
            ++i;
            if (is_symmetric) {
                insertions[i].src  = v;
                insertions[i].dest = u;
                ++i;
            }
            break;
        }
    }
    OK( GrB_Vector_free(&row) );
    return i;
}





int main (int argc, char **argv)
{

    //--------------------------------------------------------------------------
    // startup LAGraph and GraphBLAS
    //--------------------------------------------------------------------------

    char msg [LAGRAPH_MSG_LEN] ;        // for error messages from LAGraph
    LAGraph_Graph G = NULL ;
    GrB_Matrix Y = NULL ;

    // start GraphBLAS and LAGraph
    bool burble = false ;               // set true for diagnostic outputs
    demo_init (burble) ;

    //--------------------------------------------------------------------------
    // read in the graph: this method is defined in LAGraph_demo.h
    //--------------------------------------------------------------------------

    // readproblem can read in a file in Matrix Market format, or in a binary
    // format created by binwrite (see LAGraph_demo.h, or the main program,
    // mtx2bin_demo).

    double t = LAGraph_WallClockTime ( ) ;
    char *matrix_name = (argc > 1) ? argv [1] : "stdin" ;
    bool is_symmetric = false;

    LG_TRY (readproblem (
        &G,         // the graph that is read from stdin or a file
        NULL,       // source nodes (none, if NULL)
        false,      // make the graph undirected, if true
        false,      // remove self-edges, if true
        false,      // return G->A as structural, if true,
        NULL,       // prefered GrB_Type of G->A; null if no preference
        false,      // ensure all entries are positive, if true
        argc, argv)) ;  // input to this main program
    t = LAGraph_WallClockTime ( ) - t ;
    printf ("Time to read the graph: %.2f ms\n", t * 1000) ;

    printf ("\n==========================The input graph matrix G:\n") ;
    LG_TRY (LAGraph_Graph_Print (G, LAGraph_SHORT, stdout, msg)) ;
    printf ("\n") ;

    //--------------------------------------------------------------------------
    // Perform batch updates of varying sizes.
    //--------------------------------------------------------------------------

    GrB_Index n, m;
    OK( GrB_Matrix_nrows(&n, G->A) );
    OK( GrB_Matrix_nvals(&m, G->A) );
    for (int batch_power=-7; batch_power<=-1; ++batch_power) {
        double batch_fraction = pow(10.0, batch_power);
        GrB_Index batch_size = (GrB_Index) round(batch_fraction * m);
        printf("Batch fraction: %.1e [%d edges]\n", batch_fraction, (int) batch_size);
        // Perform edge deletions.
        {
            Edge *deletions = (Edge*) malloc(batch_size * sizeof(Edge));
            int num_deletions = generate_edge_deletions(deletions, G, batch_size, is_symmetric);
            GrB_Matrix    X = NULL;
            LAGraph_Graph H = NULL;
            printf("Cloning graph ...\n");
            double t = LAGraph_WallClockTime();
            OK( GrB_Matrix_dup(&X, G->A) );
            LAGraph_New(&H, &X, G->kind, msg);
            t = LAGraph_WallClockTime() - t;
            GrB_Index hn, hm;
            OK( GrB_Matrix_nrows(&hn, H->A) );
            OK( GrB_Matrix_nvals(&hm, H->A) );
            printf("Nodes: %ld, Edges: %ld\n", hn, hm);
            printf("Time to clone the graph: %.2f ms\n", t * 1000);
            printf("Deleting edges [%d edges] ...\n", num_deletions);
            t = LAGraph_WallClockTime();
            for (int i=0; i<num_deletions; ++i) {
                GrB_Index src  = deletions[i].src;
                GrB_Index dest = deletions[i].dest;
                OK( GrB_Matrix_removeElement(H->A, src, dest) );
            }
            t = LAGraph_WallClockTime() - t;
            OK( GrB_Matrix_nrows(&hn, H->A) );
            OK( GrB_Matrix_nvals(&hm, H->A) );
            printf("Nodes: %ld, Edges: %ld\n", hn, hm);
            printf("Time to delete edges: %.2f ms\n", t * 1000);
            for (int i=0; i<num_deletions; ++i) {
                GrB_Index src  = deletions[i].src;
                GrB_Index dest = deletions[i].dest;
                bool has_edge = false;
                int status = GrB_Matrix_extractElement_BOOL(&has_edge, H->A, src, dest);
                LG_ASSERT(status == GrB_NO_VALUE, 1);
            }
            LAGraph_Delete(&H, msg);
            OK( GrB_free(&X) );
            free(deletions);
        }
        // Perform edge insertions.
        {
            Edge *insertions = (Edge*) malloc(batch_size * sizeof(Edge));
            int num_insertions = generate_edge_insertions(insertions, G, batch_size, is_symmetric);
            GrB_Matrix    X = NULL;
            LAGraph_Graph H = NULL;
            printf("Cloning graph ...\n");
            double t = LAGraph_WallClockTime();
            OK( GrB_Matrix_dup(&X, G->A) );
            LAGraph_New(&H, &X, G->kind, msg);
            t = LAGraph_WallClockTime() - t;
            GrB_Index hn, hm;
            OK( GrB_Matrix_nrows(&hn, H->A) );
            OK( GrB_Matrix_nvals(&hm, H->A) );
            printf("Nodes: %ld, Edges: %ld\n", hn, hm);
            printf("Inserting edges [%d edges] ...\n", num_insertions);
            t = LAGraph_WallClockTime();
            for (int i=0; i<num_insertions; ++i) {
                GrB_Index src  = insertions[i].src;
                GrB_Index dest = insertions[i].dest;
                OK( GrB_Matrix_setElement(H->A, 1, src, dest) );
            }
            t = LAGraph_WallClockTime() - t;
            OK( GrB_Matrix_nrows(&hn, H->A) );
            OK( GrB_Matrix_nvals(&hm, H->A) );
            printf("Nodes: %ld, Edges: %ld\n", hn, hm);
            printf("Time to insert edges: %.2f ms\n", t * 1000);
            for (int i=0; i<num_insertions; ++i) {
                GrB_Index src  = insertions[i].src;
                GrB_Index dest = insertions[i].dest;
                bool has_edge = false;
                int status = GrB_Matrix_extractElement_BOOL(&has_edge, H->A, src, dest);
                LG_ASSERT(status == GrB_SUCCESS, 2);
            }
            LAGraph_Delete(&H, msg);
            OK( GrB_free(&X) );
            free(insertions);
        }
        printf("\n");
    }
    LG_FREE_ALL ;
    LG_TRY (LAGraph_Finalize (msg)) ;
    printf ("\n") ;
    return (GrB_SUCCESS) ;
}
