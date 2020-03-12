#ifndef SMPC_H
#define SMPC_H
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


#define SMPC_INITIALIZE_ERROR_RANDSEED_OS_FAILURE -1
#define SMPC_EVALUATION_ERROR_GC_NOT_SORTED -1
#define SMPC_SUCCESS 0
#define SMPC_SEED_BITS_COUNT 128
#define SMPC_FAIL(x) (x < 0)

#define SMPC_EVGATE_TYPE_UNKNOWN -1
#define SMPC_EVGATE_TYPE_AND 0
#define SMPC_EVGATE_TYPE_OR  1
#define SMPC_EVGATE_TYPE_NAND  2
#define SMPC_EVGATE_TYPE_NOR  3
#define SMPC_EVGATE_TYPE_XOR  4
#define SMPC_EVGATE_TYPE_XNOR  5

//Gate evaluable structure
struct wire;

typedef struct smpc_evgate{
  char  lut[4][17];
  int type;
  bool visited;
  struct smpc_wire* in0;
  struct smpc_wire* in1;
  struct smpc_wire* out;
}smpc_evgate;

typedef struct smpc_evgatelist_node{
  smpc_evgate* key;
  struct smpc_evgatelist_node*  next;
  struct smpc_evgatelist_node*  prev;
}smpc_evgatelist_node;

typedef struct smpc_evgatelist{
  smpc_evgatelist_node* head;
  smpc_evgatelist_node* tail;
  size_t length;
}smpc_evgatelist;

typedef struct smpc_wire{
  smpc_evgate* back;
  smpc_evgatelist* forward;
  char v[16];
  bool state;
  char k0[16];
  char k1[16];
}smpc_wire;

typedef struct smpc_wirelist_node{
  struct smpc_wirelist_node* prev;
  struct smpc_wirelist_node* next;
  smpc_wire* key;
}smpc_wirelist_node;

typedef struct smpc_wirelist{
  smpc_wirelist_node* head;
  smpc_wirelist_node* tail;
  size_t length;
}smpc_wirelist;

typedef struct smpc_graph{
  smpc_wirelist* input_wires;
  smpc_wirelist* output_wires;
  smpc_evgatelist* sorted_gates;
}smpc_graph;

/*
  Input:       None
  Output:      Error Code, SCMP_SUCCESS on success.
  Description: Initializes scmp random engine.
*/
int smpc_initialize();
/*
  Input:       None
  Output:      Error Code, SCMP_SUCCESS on success
  Description: Release the resources that have been allocated through
               scmp_initialize. Fails if scmp_initialize has not been called.
*/
int smpc_release();
/*
  Input:       void* addr  contains the address in which the random number will
                           be written. It has to be initialized by the caller.
  Output:      None. It places the output in addr.
  Description: Generates a random number of 128 bits using a cryptographically
               secure pseudorandom number generator. It requires that
               scmp_initialize has been previously called.
*/
void smpc_random128(void* addr);
/*
  Input:       smpc_graph* G   contains the graph ready to sort.
               smpc_wnode* L   contains the list of wires that not have any
                               back gate.
  Output:      None. It places the output in G.
  Description: Build an adjacency list graph from an existing circuit.
*/
//void smpc_mkgraph(smpc_graph* G, smpc_wnode* L);
/*
  Input:       smpc_graph* G   contains the graph ready to sort.
               smpc_wnode* L   contains the list of wires that not have any
                               back gate.
  Output:      None. It places the output in G.
  Description: Build an adjacency list graph from an existing circuit.
*/
smpc_evgatelist* smpc_evgatelist_create();
/*
  Input:       smpc_graph* G   contains the graph ready to sort.
               smpc_wnode* L   contains the list of wires that not have any
                               back gate.
  Output:      None. It places the output in G.
  Description: Build an adjacency list graph from an existing circuit.
*/
void smpc_evgatelist_insert(smpc_evgatelist* list, smpc_evgate* key);
/*
  Input:       smpc_graph* G   contains the graph ready to sort.
               smpc_wnode* L   contains the list of wires that not have any
                               back gate.
  Output:      None. It places the output in G.
  Description: Build an adjacency list graph from an existing circuit.
*/
smpc_wirelist* smpc_wirelist_create();
/*
  Input:       smpc_graph* G   contains the graph ready to sort.
               smpc_wnode* L   contains the list of wires that not have any
                               back gate.
  Output:      None. It places the output in G.
  Description: Build an adjacency list graph from an existing circuit.
*/
void smpc_wirelist_insert(smpc_wirelist* list, smpc_wire* key);

void smpc_topological_sort(smpc_graph* g);

void smpc_evaluate_offline(smpc_graph* g,char*input,char*output);

void smpc_set_gate_keys(smpc_graph* g);

int smpc_evaluate_offline_gc(smpc_graph* g, char*input, char*output);

void smpc_print_gc_stdout(smpc_graph* g);

#endif
