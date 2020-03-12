#include <errno.h>
#include <string.h>
#include "smpc_io.h"
#include <nettle/sha3.h>

#define SMPC_IO_PARSE_LINE_BUFFER_SIZE 512

/*
  Reads a line into linebuf from file f skipping all the empty lines before.
  linebuf must be of dimension SMPC_IO_PARSE_LINE_BUFFER_SIZE.
*/
int _smpc_io_readline(char* linebuf, FILE* f) {
  if (feof(f)) return -1;
  while (fgets(linebuf,SMPC_IO_PARSE_LINE_BUFFER_SIZE,f) != NULL && linebuf[0] == '\n') ;
  return 0;
}

bool _smpc_io_string_starts_with(char* str, char* prefix) {
  if (strncmp(str,prefix,strlen(prefix))==0) return true;
  return false;
}

int _smpc_io_parse_graph_gates_section(smpc_graph** graph, smpc_evgate** gates,
                                       int gatesCount,FILE* f, char* line) {
  int gateIndex;
  for (int i = 0; i < gatesCount; ++i) {
    _smpc_io_readline(line,f);
    if (feof(f)) return SMPC_IO_ERROR_PARSE_GATES_EXPECTED_GATE;
    char* endOfIndex;
    gateIndex = strtoul(line, &endOfIndex, 10);
    if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_GATES_INVALID_INDEX;
    if (gateIndex != i) return SMPC_IO_ERROR_PARSE_GATES_INDEX_ORDER;
    while (*endOfIndex == ' ') ++endOfIndex;
    int type = 0;
    if (_smpc_io_string_starts_with(endOfIndex,"AND")) type = SMPC_EVGATE_TYPE_AND;
    else if (_smpc_io_string_starts_with(endOfIndex,"OR")) type = SMPC_EVGATE_TYPE_OR;
    else if (_smpc_io_string_starts_with(endOfIndex,"NAND")) type = SMPC_EVGATE_TYPE_NAND;
    else if (_smpc_io_string_starts_with(endOfIndex,"NOR")) type = SMPC_EVGATE_TYPE_NOR;
    else if (_smpc_io_string_starts_with(endOfIndex,"XOR")) type = SMPC_EVGATE_TYPE_XOR;
    else if (_smpc_io_string_starts_with(endOfIndex,"XNOR")) type = SMPC_EVGATE_TYPE_XNOR;
    else return SMPC_IO_ERROR_PARSE_GATES_INVALID_TYPE;
    gates[i] = (smpc_evgate*)malloc(sizeof(smpc_evgate));
    memset(gates[i]->lut,0,sizeof(gates[i]->lut));
    gates[i]->type = type;
    gates[i]->in0 = NULL;
    gates[i]->in1 = NULL;
    gates[i]->out = NULL;
  }
  return SMPC_SUCCESS;
}

int _smpc_io_parse_graph_input_section(smpc_graph** graph,smpc_evgate** gates,
                                       int gatesCount,FILE* f, char* line) {
  _smpc_io_readline(line,f);
  int i = 0;
  while(!_smpc_io_string_starts_with(line,"Wires")) {
    if (feof(f)) return SMPC_IO_ERROR_PARSE_INPUT_EXPECTED_WIRES;
    char* endOfIndex;
    int inputIndex = strtoul(line, &endOfIndex, 10);
    if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_INPUT_INVALID_INDEX;
    if (inputIndex != i) return SMPC_IO_ERROR_PARSE_INPUT_INDEX_ORDER;
    while (*endOfIndex == ' ') ++endOfIndex;
    if (*endOfIndex != '{') return SMPC_IO_ERROR_PARSE_INPUT_OBRACKET_EXPECTED;
    ++endOfIndex;
    smpc_wire* wire = (smpc_wire*)malloc(sizeof(smpc_wire));
    wire->back = NULL;
    wire->forward = smpc_evgatelist_create();
    memset(wire->v, 0, sizeof(wire->v));
    wire->state = false;
    while (*endOfIndex != 0){
      while(*endOfIndex == ' ') ++endOfIndex;
      int startingGate = strtoul(endOfIndex, &endOfIndex, 10);
      if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_INPUT_INVALID_GATE;
      if (startingGate < 0 || startingGate >= gatesCount)
        return SMPC_IO_ERROR_PARSE_INPUT_GATE_OUT_OF_RANGE;
      smpc_evgatelist_insert(wire->forward, gates[startingGate]);
      if(gates[startingGate]->in0 == NULL) gates[startingGate]->in0 = wire;
      else if (gates[startingGate]->in1 == NULL) gates[startingGate]->in1 = wire;
      else return SMPC_IO_ERROR_PARSE_INPUT_TOO_MANY_GATE_INPUT_LINES;
      while(*endOfIndex == ' ') ++endOfIndex;
      if (*endOfIndex == '}') break;
      else if (*endOfIndex != ',') return SMPC_IO_ERROR_PARSE_INPUT_INVALID_CHAR;
      ++endOfIndex;
    }
    if (*endOfIndex == 0) return SMPC_IO_ERROR_PARSE_INPUT_CBRACKET_EXPECTED;
    smpc_wirelist_insert((*graph)->input_wires,wire);
    _smpc_io_readline(line,f);
    ++i;
  }
  return SMPC_SUCCESS;
}

int _smpc_io_parse_graph_wires_section(smpc_graph** graph,smpc_evgate** gates,
                                       int gatesCount,FILE* f, char* line) {
  _smpc_io_readline(line,f);
  int i = 0;
  while(!_smpc_io_string_starts_with(line,"Output")) {
    if (feof(f)) return SMPC_IO_ERROR_PARSE_WIRES_EXPECTED_OUTPUT;

    if (*line != '(') return SMPC_IO_ERROR_PARSE_WIRES_LPARENS_EXPECTED;
    ++line;
    while(*line == ' ') ++line;
    char* endOfNumber, *endOfNumber2;
    int srcID = strtoul(line, &endOfNumber, 10);
    if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_WIRES_PARAM1_INVALID;
    while(*endOfNumber == ' ') ++endOfNumber;
    if (*endOfNumber != ',') return SMPC_IO_ERROR_PARSE_WIRES_COMMA_EXPECTED;
    endOfNumber++;
    int dstID = strtoul(endOfNumber, &endOfNumber2, 10);
    if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_WIRES_PARAM2_INVALID;
    while(*endOfNumber2 == ' ') endOfNumber2++;
    if (*endOfNumber2 != ')') return SMPC_IO_ERROR_PARSE_WIRES_RPARENS_EXPECTED;
    if (srcID < 0 || srcID >= gatesCount) return SMPC_IO_ERROR_PARSE_WIRES_PARAM1_OUT_OF_RANGE;
    if (dstID < 0 || dstID >= gatesCount) return SMPC_IO_ERROR_PARSE_WIRES_PARAM2_OUT_OF_RANGE;
    if (!gates[srcID]->out) {
      gates[srcID]->out = (smpc_wire*)malloc(sizeof(smpc_wire));
      gates[srcID]->out->back = gates[srcID];
      gates[srcID]->out->forward = smpc_evgatelist_create();
    }
    smpc_evgatelist_insert(gates[srcID]->out->forward, gates[dstID]);
    if (gates[dstID]->in0 == NULL) gates[dstID]->in0 = gates[srcID]->out;
    else if (gates[dstID]->in1 == NULL) gates[dstID]->in1 = gates[srcID]->out;
    else return SMPC_IO_ERROR_PARSE_WIRES_TOO_MANY_GATE_INPUT_LINES;
    _smpc_io_readline(line,f);
    ++i;
  }
  return SMPC_SUCCESS;
}

int _smpc_io_parse_graph_output_section(smpc_graph** graph,smpc_evgate** gates,
                                       int gatesCount,FILE* f, char* line) {
  _smpc_io_readline(line,f);
  while(!feof(f)) {
    int index = strtoul(line, NULL, 10);
    if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_OUTPUT_INVALID_NUMBER;
    if (index < 0 || index >= gatesCount)
      return SMPC_IO_ERROR_PARSE_OUTPUT_OUT_OF_RANGE;
    smpc_wire* wire = (smpc_wire*)malloc(sizeof(smpc_wire));
    wire->back = gates[index];
    wire->forward = NULL;
    smpc_wirelist_insert((*graph)->output_wires,wire);
    gates[index]->out = wire;
    _smpc_io_readline(line,f);
  }
  return SMPC_SUCCESS;
}

int smpc_io_parse_graph_from_file(smpc_graph** graph, char* filename) {
  if (graph == NULL) return SMPC_IO_ERROR_PARSE_INVALID_GRAPH_POINTER;
  if (*graph == NULL){
    *graph = (smpc_graph*)malloc(sizeof(smpc_graph));
    (*graph)->input_wires = smpc_wirelist_create();
    (*graph)->output_wires = smpc_wirelist_create();
    (*graph)->sorted_gates = smpc_evgatelist_create();
  }
  FILE* f = fopen(filename, "r");
  int errorCode;
  if (!f) return SMPC_IO_ERROR_FILENOTFOUND;

  char lineBuffer[SMPC_IO_PARSE_LINE_BUFFER_SIZE];

  _smpc_io_readline(lineBuffer,f);
  if (!_smpc_io_string_starts_with(lineBuffer,"Gates"))
    return SMPC_IO_ERROR_PARSE_GATES_EXPECTED;
  char* strp = lineBuffer + 6;
  while (*strp == ' ') ++strp;
  int gatesCount = strtoul(strp, NULL, 10);
  if (errno == EINVAL) return SMPC_IO_ERROR_PARSE_GATES_INVALID_LENGTH;
  smpc_evgate** gates = malloc(sizeof(smpc_evgate) * gatesCount);
  errorCode = _smpc_io_parse_graph_gates_section(graph,gates,gatesCount,f,lineBuffer);
  if (SMPC_FAIL(errorCode)) return errorCode;

  _smpc_io_readline(lineBuffer,f);
  if (!_smpc_io_string_starts_with(lineBuffer,"Input"))
    return SMPC_IO_ERROR_PARSE_INPUT_EXPECTED;
  errorCode = _smpc_io_parse_graph_input_section(graph,gates,gatesCount,
                                                 f,lineBuffer);
  if (SMPC_FAIL(errorCode)) return errorCode;
  errorCode = _smpc_io_parse_graph_wires_section(graph,gates,gatesCount,
                                                 f,lineBuffer);
  if (SMPC_FAIL(errorCode)) return errorCode;
  errorCode = _smpc_io_parse_graph_output_section(graph,gates,gatesCount,
                                                 f,lineBuffer);
  if (SMPC_FAIL(errorCode)) return errorCode;

  free(gates);
  fclose(f);
  return SMPC_SUCCESS;
}

#ifdef INCLUDE_BAD_CODE
void _smpc_io_print_graph_recursive(smpc_wire* wire, int tabLevel, FILE* f) {
  if (tabLevel >= 255 || !wire) return;
  char tabs[256]; memset(tabs,'\t',256*sizeof(char));
  tabs[tabLevel] = 0;
  printf("%sWire %016lX\n",tabs,wire);
  printf("%s\tBack   : 0x%016lX\n", tabs, wire->back);
  printf("%s\tForward: \n", tabs);
  if (!wire->forward) return;
  smpc_evgatelist_node* n = wire->forward->head;
  while (n != NULL) {
    printf("%s\t\tEVGATE 0x%016lX (0x%016lX,0x%016lX)\n",tabs, n->key,n->key->in0,n->key->in1);
    _smpc_io_print_graph_recursive(n->key->out,tabLevel + 2,f);
    n = n->next;
  }
}

void smpc_io_print_graph(smpc_graph* graph, FILE* f) {
  smpc_wirelist_node* p = graph->input_wires->head;
  while (p != NULL) {
    printf("Initial Wire 0x%016lX\n", p->key);
    printf("\tBack   : 0x%016lX\n", p->key->back);
    printf("\tForward: \n");
    smpc_evgatelist_node* n = p->key->forward->head;
    while (n != NULL) {
      printf("\t\tEVGATE 0x%016lX (0x%016lX,0x%016lX)\n", n->key,n->key->in0,n->key->in1);
      if (!n->key) break;
      _smpc_io_print_graph_recursive(n->key->out,2,f);
      n = n->next;
    }
    p = p->next;
  }
}

#endif
