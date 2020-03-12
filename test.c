#include <stdio.h>
#include "nettle/aes.h"
#include <nettle/sha3.h>
#include <stdlib.h>
#include <stdbool.h>
#include "smpc.h"
#include <gmp.h>
#include "smpc_io.h"
int main(int argc, char const *argv[]) {
  printf("TEST NOW\n");
  smpc_graph* graph = NULL;
  int v = smpc_io_parse_graph_from_file(&graph,"circuit format specification.txt");
  printf("OUTCODE: %d\n", v);
  smpc_topological_sort(graph);
  char input[2] = {0x03,0x01};
  char output;
  smpc_set_gate_keys(graph);

  smpc_evaluate_offline_gc(graph,input,&output);
  printf("Output of evaluation %02X\n",output);
  smpc_print_gc_stdout(graph);
  //smpc_evaluate_offline_gc(graph,input,&output);
  //printf("Output of evaluation %02X\n",output);
/*
  smpc_wirelist_node* tempo = graph->output_wires->head;
  for (size_t i = 0; i < graph->output_wires->length; i++) {
    for (size_t j = 0; j < 16; j++)
      printf("%02X", tempo->key->k0[j]);
    printf("\n");
    for (size_t j = 0; j < 16; j++)
      printf("%02X", tempo->key->k1[j]);
    printf("\n\n");
    tempo = tempo->next;
  }
*/

  return 0;
  // hash
  uint8_t a[16];
  uint8_t b[16];
  if (smpc_initialize() != SMPC_SUCCESS) perror("Error on initialize smpc");
  smpc_random128(a);
  smpc_random128(b);
  if (smpc_release() != SMPC_SUCCESS) perror("Error on release");
  for (size_t i = 0; i < 16; i++)
    printf("%02X", a[i]);
  printf("\n"); fflush(stdout);
  for (size_t i = 0; i < 16; i++)
    printf("%02X", b[i]);
  printf("\n"); fflush(stdout);
  uint8_t c[32];
  size_t i = 0;
  for (; i < 16; i++) {
    c[i] = b[i];
  }
  for (size_t j = 0; j < 16; j++) {
    c[i] = a[j];
    i++;
  }
  for (size_t i = 0; i < 32; i++)
    printf("%02X", c[i]);
  printf("\n"); fflush(stdout);
  struct sha3_224_ctx context;
  sha3_224_init (&context);
  uint8_t digest[16];
  sha3_224_update (&context, 32, c);
  sha3_224_digest(&context, 16, digest);
  for (size_t i = 0; i < 16; i++)
    printf("%02X", digest[i]);
  printf("\n"); fflush(stdout);
// end hash
  struct aes128_ctx* e_ctx = (struct aes128_ctx*) malloc(sizeof(struct aes128_ctx));
  struct aes128_ctx* d_ctx = (struct aes128_ctx*) malloc(sizeof(struct aes128_ctx));
  uint8_t key[16];
  uint8_t src[16] = "ghjkghjkghjkghjk";
  uint8_t dst[16] = "";

  if (smpc_initialize() != SMPC_SUCCESS) perror("Error on initialize smpc");
  smpc_random128(key);

  for (size_t i = 0; i < 16; i++)
    printf("%02X", key[i]);
  printf("\n"); fflush(stdout);
  getchar();

  aes128_set_encrypt_key(e_ctx, key);
  aes128_set_decrypt_key(d_ctx, key);

  for (size_t i = 0; i < 16; i++)
    printf("%02X", src[i]);
  printf("\n"); fflush(stdout);
  getchar();

  aes128_encrypt(e_ctx, 16, (uint8_t*)dst, (uint8_t*)src);

  for (size_t i = 0; i < 16; i++)
    printf("%02X", dst[i]);
  printf("\n"); fflush(stdout);
  getchar();

  uint8_t ndst[16] = "";
  aes128_decrypt(d_ctx, 16, (uint8_t*)ndst, (uint8_t*)dst);

  for (size_t i = 0; i < 16; i++)
    printf("%02X", ndst[i]);
  printf("\n"); fflush(stdout);
  getchar();

  if (smpc_release() != SMPC_SUCCESS) perror("Error on release");

  return 0;
}
