/*
 * fdist.h
 *
 *  Created on: 3 Nov 2010
 *      Author: luis
 */

#ifndef FDIST_H_
#define FDIST_H_

#include "distribCL.h"

struct node {
  cl_int sp;
  cl_int osp;
  cl_float time;
  cl_int I;
  cl_int cut;
  cl_int dna;
  struct node *a[2];
  struct node *d[2];
};//76Byte

struct nodeptr { //ugly hack to allow passing an array of pointers to nodes to a kernel
  struct node *ptr;
};

#endif /* FDIST_H_ */
