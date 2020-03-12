#include <stdio.h>
#include "smpc.h"
#include "lib-misc.h"
#include "bitut.h"
#include <nettle/sha3.h>
#include "string.h"

gmp_randstate_t SMPC_PRNG;

int smpc_initialize() {
  gmp_randinit_default(SMPC_PRNG);
  if (gmp_randseed_os_rng(SMPC_PRNG, SMPC_SEED_BITS_COUNT)== -1)
    return SMPC_INITIALIZE_ERROR_RANDSEED_OS_FAILURE;
  return SMPC_SUCCESS;
}

int smpc_release() {
    gmp_randclear(SMPC_PRNG);
    return SMPC_SUCCESS;
}


void smpc_random128(void* addr) {
  mpz_t n;
  mpz_init(n);
  mpz_urandomb(n, SMPC_PRNG, SMPC_SEED_BITS_COUNT);
  size_t read_count;
  mpz_export(addr, &read_count,1,4,0,0,n);
  mpz_clear(n);
}

smpc_evgatelist* smpc_evgatelist_create() {
  smpc_evgatelist* a = (smpc_evgatelist*) malloc(sizeof(smpc_evgatelist));
  a->head = NULL;
  a->tail = NULL;
  a->length = 0;
  return a;
}
smpc_wirelist* smpc_wirelist_create() {
  smpc_wirelist* a = (smpc_wirelist*) malloc(sizeof(smpc_wirelist));
  a->head = NULL;
  a->tail = NULL;
  a->length = 0;
  return a;
}

void smpc_evgatelist_insert(smpc_evgatelist* list, smpc_evgate* key) {
  smpc_evgatelist_node* n = (smpc_evgatelist_node*) malloc(sizeof(smpc_evgatelist_node));
  n->key = key;
  if(list->head == NULL)
    list->head = n;
  n->prev = list->tail;
  list->tail = n;
  n->next = NULL;
  if (n->prev != NULL) n->prev->next = n;
  list->length ++;
}

void smpc_wirelist_insert(smpc_wirelist* list, smpc_wire* key) {
  smpc_wirelist_node* n=(smpc_wirelist_node*)malloc(sizeof(smpc_wirelist_node));
  n->key = key;
  if(list->head == NULL)
    list->head = n;
  n->prev = list->tail;
  list->tail = n;
  n->next = NULL;
  if (n->prev != NULL) n->prev->next = n;
  list->length ++;
}

void smpc_topological_sort_visit(smpc_graph* graph, smpc_evgate* gate) {
  if (!gate->visited) {
      gate->visited = true;
      if (gate->out && gate->out->forward) {
        smpc_evgatelist_node* p = gate->out->forward->head;
        while (p) {
          smpc_topological_sort_visit(graph,p->key);
          p = p->next;
        }
      }
      smpc_evgatelist_insert(graph->sorted_gates,gate);
  }
}

void smpc_topological_sort(smpc_graph* g) {
  smpc_wirelist_node* inputWire = g->input_wires->head;
  while (inputWire){
    smpc_evgatelist_node* currentGate = inputWire->key->forward->head;
    while (currentGate) {
      smpc_topological_sort_visit(g,currentGate->key);
      currentGate = currentGate->next;
    }
    inputWire = inputWire->next;
  }
}

void smpc_evaluate_offline(smpc_graph* g, char* input, char* output) {
  if (g->sorted_gates->length == 0)
    smpc_topological_sort(g);
  smpc_evgatelist_node* p = g->sorted_gates->tail;
  smpc_wirelist_node* wireNode = g->input_wires->head;
  for (size_t i = 0; i < g->input_wires->length; i++) {
    wireNode->key->v[0] = (input[i/8] >> ((i % 8))) & 0x1;
    wireNode = wireNode->next;
  }
  while (p != NULL) {
    switch (p->key->type) {
      case SMPC_EVGATE_TYPE_AND: {
        p->key->out->v[0] = p->key->in0->v[0] & p->key->in1->v[0];
        break;
      }
      case SMPC_EVGATE_TYPE_OR: {
        p->key->out->v[0] = p->key->in0->v[0] | p->key->in1->v[0];
        break;
      }
      case SMPC_EVGATE_TYPE_NAND: {
        p->key->out->v[0] = 1-(p->key->in0->v[0] & p->key->in1->v[0]);
        break;
      }
      case SMPC_EVGATE_TYPE_NOR: {
        p->key->out->v[0] = 1-(p->key->in0->v[0] | p->key->in1->v[0]);
        break;
      }
      case SMPC_EVGATE_TYPE_XOR: {
        p->key->out->v[0] = p->key->in0->v[0] ^ p->key->in1->v[0];
        break;
      }
      case SMPC_EVGATE_TYPE_XNOR: {
        p->key->out->v[0] = ~(p->key->in0->v[0] ^ p->key->in1->v[0]);
        break;
      }
      default: {
        break;
      }
    }
    p = p->prev;
  }

  wireNode = g->output_wires->head;
  for (size_t i = 0; i < g->output_wires->length; i++) {
    output[i/8] |= (wireNode->key->v[0] << (i % 8));
    wireNode = wireNode->next;
  }
}

void _smpc_copy_string_32(char* c, char* a, char* b) {
  size_t i = 0;
  for (; i < 16; i++) {
    c[i] = a[i];
  }
  for (size_t j = 0; j < 16; j++) {
    c[i] = b[j];
    i++;
  }
}

void _smpc_copy_string(char* a, char* b, int size) {
  for (size_t i = 0; i < size; i++) {
    a[i] = b[i];
  }
}

void _smpc_set_lut_pp(smpc_evgatelist_node* p, uint8_t p0, uint8_t p1, uint8_t r){
  if (p0 == 0) {
    ClearBitU(p->key->lut[r][16], 0);
  }
  else {
    SetBitU(p->key->lut[r][16], 0);
  }
  if (p1 == 0) {
    ClearBitU(p->key->lut[r][16], 1);
  }
  else {
    SetBitU(p->key->lut[r][16], 1);
  }
}

void _smpc_copy_string_into_lut_pp(char* a, char* b, uint8_t p0, uint8_t p1) {
  for (size_t i = 0; i < 16; i++) {
    a[i] = b[i];
  }
  if (p0 == 0) {
    ClearBitU(a[16], 0);
  }
  else {
    SetBitU(a[16], 0);
  }
  if (p1 == 0) {
    ClearBitU(a[16], 1);
  }
  else {
    SetBitU(a[16], 1);
  }
}

void _smpc_set_lut_row(smpc_evgatelist_node* p, uint8_t b0, uint8_t b1, uint8_t o, uint8_t r) {
  char *x, *y, *z;
  if (b0 == 0) {
    x = p->key->in0->k0;
  }
  else {
    x = p->key->in0->k1;
  }
  if (b1 == 0) {
    y = p->key->in1->k0;
  }
  else {
    y = p->key->in1->k1;
  }
  if (o == 0) {
    z = p->key->out->k0;
  }
  else {
    z = p->key->out->k1;
  }
  uint8_t tk[32];
  _smpc_copy_string_32(tk, x, y);
  struct sha3_224_ctx context;
  sha3_224_init (&context);
  sha3_224_update (&context, 32, tk);
  uint8_t digest[16];
  sha3_224_digest(&context, 16, digest);
  uint8_t record[16];
  for (size_t i = 0; i < 16; i++) {
    record[i] = digest[i] ^ z[i];
  }
  _smpc_copy_string_into_lut_pp(p->key->lut[r], record,
                               TestBit(x, 127),
                               TestBit(y, 127)
                              );
}

int _smpc_lut_pp_metch(smpc_evgatelist_node* p, char* x, char* y, uint8_t r) {
  if ((TestBitU(p->key->lut[r][16], 0) == TestBit(x, 127))
       &&
      (TestBitU(p->key->lut[r][16], 1) == TestBit(y, 127))){
    return 1;
  }
  else {
    return 0;
  }
}

void _smpc_set_grr3_zero_raw(smpc_evgatelist_node* p, int* tv) {
  uint8_t tk[32];
  uint8_t point[16];
  smpc_random128(point);
  uint8_t index = 0;
  if (point[0] % 2 == 1)
    index = 1;
  if (point[1] % 2 == 1)
    index += 2;

  char *a, *b;
  switch (index) {
    case 0:{
      _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k0);
      a = p->key->in0->k0;
      b = p->key->in1->k0;
      break;
    }
    case 1:{
      _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k1);
      a = p->key->in0->k0;
      b = p->key->in1->k1;
      break;
    }
    case 2:{
      _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k0);
      a = p->key->in0->k1;
      b = p->key->in1->k0;
      break;
    }
    case 3:{
      _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k1);
      a = p->key->in0->k1;
      b = p->key->in1->k1;
      break;
    }
  }

  char* theChosenOne;
  char* theOtherOne;
  if (tv[index] == 0) {
    theChosenOne = p->key->out->k0;
    theOtherOne = p->key->out->k1;
  }
  else {
    theChosenOne = p->key->out->k1;
    theOtherOne = p->key->out->k0;
  }
  ClearAll(p->key->lut[0]);
  struct sha3_224_ctx context;
  sha3_224_init (&context);
  sha3_224_update (&context, 32, tk);
  sha3_224_digest(&context, 16, p->key->out->k0);
  _smpc_set_lut_pp(p, TestBit(a, 127), TestBit(b, 127), 0);
  smpc_random128(theOtherOne);
  if (TestBit(theChosenOne, 127) == 0) {
    SetBit(theOtherOne, 127);
  }
  else {
    ClearBit(theOtherOne, 127);
  }
}

void _smpc_set_grr3_other_raw(smpc_evgatelist_node* p, int* tv) {
  size_t i = 1;
  size_t j = 0;
  uint8_t tk[32];
  uint8_t x, y, z;
  char *a, *b;
  while (i < 4) {
    switch (j) {
      case 0:{
        _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k0);
        a = p->key->in0->k0;
        b = p->key->in1->k0;
        x = 0;
        y = 0;
        break;
      }
      case 1:{
        _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k1);
        a = p->key->in0->k0;
        b = p->key->in1->k1;
        x = 0;
        y = 1;
        break;
      }
      case 2:{
        _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k0);
        a = p->key->in0->k1;
        b = p->key->in1->k0;
        x = 1;
        y = 0;
        break;
      }
      case 3:{
        _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k1);
        a = p->key->in0->k1;
        b = p->key->in1->k1;
        x = 1;
        y = 1;
        break;
      }
    }
    z = tv[j];
    if (_smpc_lut_pp_metch(p, a, b, 0) == 0){
      _smpc_set_lut_row(p, x, y, z, i);
      i++;
    }
    j++;
  }
}

void _smpc_exchange_lut_rows(smpc_evgatelist_node* p, uint8_t a, uint8_t b){
  char tmp[17];
  _smpc_copy_string(tmp, p->key->lut[a], 17);
  _smpc_copy_string(p->key->lut[a], p->key->lut[b], 17);
  _smpc_copy_string(p->key->lut[b], tmp, 17);
}

void _smpc_permute_grr3_lut_raw(smpc_evgatelist_node* p) {
  uint8_t prn[16];
  int j = 0;
  smpc_random128(prn);
  for (uint8_t i = 1; i < 4; i++) {
    j = (prn[i] % 3) + 1;
    if (i != j) {
      _smpc_exchange_lut_rows(p, i, j);
    }
  }
}

void _smpc_permute_lut_raw(smpc_evgatelist_node* p) {
  uint8_t prn[16];
  int j = 0;
  smpc_random128(prn);
  for (uint8_t i = 0; i < 4; i++) {
    j = (prn[i] % 4);
    if (i != j) {
      _smpc_exchange_lut_rows(p, i, j);
    }
  }
}


void _smpc_set_grr3_lut_raw(smpc_evgatelist_node* p, int* tv) {
  _smpc_set_grr3_zero_raw(p, tv);
  _smpc_set_grr3_other_raw(p, tv);
  _smpc_permute_grr3_lut_raw(p);
}

void _smpc_set_end_lut_row(smpc_evgatelist_node* p, int* tv) {
  uint8_t tk[32];
  uint8_t x, y, z;
  char *a, *b;
  ClearAll(p->key->out->k0);
  ClearAll(p->key->out->k1);

  SetBit(p->key->out->k1, 0);
  for (size_t i = 0; i < 4; i++) {
    switch (i) {
      case 0:{
        _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k0);
        a = p->key->in0->k0;
        b = p->key->in1->k0;
        x = 0;
        y = 0;
        break;
      }
      case 1:{
        _smpc_copy_string_32(tk, p->key->in0->k0, p->key->in1->k1);
        a = p->key->in0->k0;
        b = p->key->in1->k1;
        x = 0;
        y = 1;
        break;
      }
      case 2:{
        _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k0);
        a = p->key->in0->k1;
        b = p->key->in1->k0;
        x = 1;
        y = 0;
        break;
      }
      case 3:{
        _smpc_copy_string_32(tk, p->key->in0->k1, p->key->in1->k1);
        a = p->key->in0->k1;
        b = p->key->in1->k1;
        x = 1;
        y = 1;
        break;
      }
    }
    z = tv[i];
    _smpc_set_lut_row(p, x, y, z, i);
  }
  _smpc_permute_lut_raw(p);
}

int _smpc_is_output(smpc_graph* g, smpc_evgatelist_node* p) {
  smpc_wirelist_node* t = g->output_wires->head;
  for (size_t i = 0; i < g->output_wires->length; i++) {
    if (p->key->out == t->key) {
      return 1;
    }
    t = t->next;
  }
  return 0;
}

void smpc_set_gate_keys(smpc_graph* g) {
  if (g->sorted_gates->length == 0)
    smpc_topological_sort(g);
  smpc_evgatelist_node* p = g->sorted_gates->tail;
  smpc_wirelist_node* wireNode = g->input_wires->head;
  char key[16];
  uint8_t point[16];
  if (smpc_initialize() != SMPC_SUCCESS) perror("Error on initialize smpc");
  for (size_t i = 0; i < g->input_wires->length; i++) {
    smpc_random128(key);
    for (size_t j = 0; j < 16; j++) {
      wireNode->key->k0[j] = key[j];
    }
    smpc_random128(key);
    for (size_t j = 0; j < 16; j++) {
      wireNode->key->k1[j] = key[j];
    }
    smpc_random128(point);
    if (point[0] % 2 == 0) {
      ClearBit(wireNode->key->k0, 127);
      SetBit(wireNode->key->k1, 127);
    }
    else {
      ClearBit(wireNode->key->k1, 127);
      SetBit(wireNode->key->k0, 127);
    }
    wireNode = wireNode->next;
  }
  while (p != NULL) {
    switch (p->key->type) {
      case SMPC_EVGATE_TYPE_AND: {
        int tv[4] = {0, 0, 0, 1};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      case SMPC_EVGATE_TYPE_OR: {
        int tv[4] = {0, 1, 1, 1};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      case SMPC_EVGATE_TYPE_NAND: {
        int tv[4] = {1, 1, 1, 0};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      case SMPC_EVGATE_TYPE_NOR: {
        int tv[4] = {1, 0, 0, 0};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      case SMPC_EVGATE_TYPE_XOR: {
        int tv[4] = {0, 1, 1, 0};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      case SMPC_EVGATE_TYPE_XNOR: {
        int tv[4] = {1, 0, 0, 1};
        if (_smpc_is_output(g, p) == 1)
          _smpc_set_end_lut_row(p,tv);
        else
          _smpc_set_grr3_lut_raw(p, tv);
        break;
      }
      default: {
        break;
      }
    }
    p = p->prev;
  }
  if (smpc_release() != SMPC_SUCCESS) perror("Error on release");
}

void _smpc_print_key_stdout(char* a, int length) {
  printf("%01X |  ", TestBit(a, 127));
  for (size_t i = 0; i < length; i++) {
    printf("%02X", a[i]);
  }
  printf("\n"); fflush(stdout);
}

void _smpc_print_wire_stdout(smpc_wire* w) {
  printf("v: ");
  _smpc_print_key_stdout(w->v, 16);
  printf("k0: ");
  _smpc_print_key_stdout(w->k0, 16);
  printf("k1: ");
  _smpc_print_key_stdout(w->k1, 16);

}

void _smpc_print_gate_stdout(smpc_evgate* g) {
  printf("lut: \n");
  for (size_t i = 0; i < 4; i++) {
    printf("%01X | %01X || ", TestBitU(g->lut[i][16], 0), TestBitU(g->lut[i][16], 1));
    for (size_t j = 0; j < 16; j++) {
      printf("%02X", g->lut[i][j]);
    }
    printf("\n"); fflush(stdout);
  }
  printf("\n");
  printf("in0:\n");
  _smpc_print_wire_stdout(g->in0);
  printf("in1:\n");
  _smpc_print_wire_stdout(g->in1);
  printf("out:\n");
  _smpc_print_wire_stdout(g->out);
  printf("\n");
}

void smpc_print_gc_stdout(smpc_graph* g) {
  if (g->sorted_gates->length == 0)
    smpc_topological_sort(g);
  smpc_evgatelist_node* p = g->sorted_gates->tail;
  int i = 0;
  while (p != NULL) {
    printf("Gate %d:\n", i);
    _smpc_print_gate_stdout(p->key);
    i++;
    p = p->prev;
    printf("\n");
  }
}

int smpc_evaluate_offline_gc(smpc_graph* g, char* input, char* output) {
  char zeroChar;
  ClearAllU(&zeroChar);
  if (g->sorted_gates->length == 0)
    return SMPC_EVALUATION_ERROR_GC_NOT_SORTED;
  smpc_evgatelist_node* p = g->sorted_gates->tail;
  smpc_wirelist_node* wireNode = g->input_wires->head;
  for (size_t i = 0; i < g->input_wires->length; i++) {
    if (((input[i/8] >> ((i % 8))) & 0x1) == zeroChar) {
      _smpc_copy_string(wireNode->key->v, wireNode->key->k0, 16);
    }
    else {
      _smpc_copy_string(wireNode->key->v, wireNode->key->k1, 16);
    }
    wireNode = wireNode->next;
  }
  while (p != NULL) {
    int c = 0;
    char tmp[32];
    char tt[16];
    for (size_t i = 0; i < 4; i++) {
      if (_smpc_lut_pp_metch(p, p->key->in0->v, p->key->in1->v, i) == 1) {
        c = i;
        i = 4;
      }
    }
    _smpc_copy_string_32(tmp, p->key->in0->v, p->key->in1->v);
    struct sha3_224_ctx context;
    sha3_224_init (&context);
    sha3_224_update (&context, 32, tmp);
    sha3_224_digest(&context, 16, tt);
    for (size_t i = 0; i < 16; i++) {
      p->key->out->v[i] = tt[i] ^ p->key->lut[c][i];
    }
    p = p->prev;
  }
  wireNode = g->output_wires->head;
  for (size_t i = 0; i < g->output_wires->length; i++) {
    output[i/8] |= (wireNode->key->v[0] << (i % 8));
    wireNode = wireNode->next;
  }
}
