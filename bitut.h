#ifndef BITUT_H
#define BITUT_H

#include <string.h>
#define SetBit(A,k)   ( A[(k/8)]  |=  (1 <<  (k%8)) )
#define SetBitU(A,k)   ( A |=  (1 <<  k) )
#define ClearBit(A,k) ( A[(k/8)]  &=  ~(1 <<  (k%8))  )
#define ClearBitU(A,k) ( A  &=  ~(1 <<  k)  )
#define TestBit(A,k)  ( A[(k/8)] & (1  <<  (k%8)) )
#define TestBitU(A,k)  ( A & (1  <<  k) )
#define ClearAll(A) memset(A,0,16)
#define ClearAllU(A) memset(A,0,1)
#define SetAll(A) memset(A,1,16)
#define SetAllU(A) memset(A,1,1)
#endif
