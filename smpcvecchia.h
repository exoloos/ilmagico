//Gate standard structure
typedef struct  {
  char  lut[4][17];  //contains the 4 gate output keys
  char  x0[17];      //contain the input key of part x/y corresponding of bit 0/1
  char  x1[17];
  char  y0[17];
  char  y1[17];
  char  type;        //contains an identifier of the type of gate
  bool state;
}gate;

typedef struct{
  gate* k;
  struct  node*  next;
}node;

typedef struct  {
  gate* back;
  node* forward;
  char v[17];
  char w[17];
  bool state;
}wire;

//Gate mailing structure
typedef struct  {
  char  lut[4][17];
  char  x[17];
  char  y0[17];
  char  y1[17];
  bool state;
}sndgate;

//Gate evaluable structure
typedef struct  {
  char  lut[4][17];
  char  x[17];
  char  y[17];
  bool state;
}evgate;

void kcopy(char dest[], char  sorg[]) {
  for (size_t i = 0; i < 17; i++) {
    dest[i] = sorg[i];
  }
}

void  evaluate(evgate G,  char  output[])  {
  char  lab = G.x[16] ^ G.y[16];
  for (size_t i = 0; i < 4; i++) {
    if(lab == G.lut[i][16]) kcopy(output, G.lut[i]);
  }
}
