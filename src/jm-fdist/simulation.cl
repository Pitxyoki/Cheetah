
/*
 * Bugs:
 * BUG#1:
 * AMD can't compile this:
 *
 * void sim1(__private int  test[2]) { }
 * void sim2(__private int *test[2]) { }
 * void sim3(__local int  test[2]) { }
 * void sim4(__local int *test[2]) { }
 * 
 * __kernel void test_kern(__global int *arg) {
 * 
 *   __private int test[2];    sim1(test);
 * //  __private int *test[2]; sim2(test);
 * //  __local int test[2];    sim3(test);
 * //  __local int *test[2];   sim4(test);
 * 
 * }
 * 
 * BUG#2:
 * NVIDIA Can't compile this:
 * while(1) {
 * ... lots of stuff
 * if (something)
 *   break;
 * }
 * 
 * Use this instead:
 * startcycle:
 * ... lots of stuff
 * if (not something)
 *   goto startcycle;
 * 
 *
 */


//#pragma OPENCL EXTENSION cl_amd_printf:enable

#define PI 3.141592653589793f
#define NOC 200 /* The maximum number of subpopulations possible. Spno - the
        number of subpopulations in the model - must
        be less than or equal to this */

#define MY_RAND_MAX UINT_MAX

#define VPNOC 300

struct node {
  int sp;
  int osp;
  float time;
  int I;
  int cut;
  int dna;
  __global struct node *a[2];
  __global struct node *d[2];
};

struct nodeptr {
  __global struct node *ptr;
};



__global struct node *request_node(__global struct node *NODES, __private int *last_node) {
//    if (*last_node >= 1498)
//     printf("new max: %i (ID %i)\n", *last_node, get_global_id(0));

  return &(NODES[(*last_node)++]);
}



unsigned int myrandom(__private unsigned int *next) {
  (*next) = (*next) * 1103515245 + 12345;
  return ( (unsigned) ( (*next)/*65536*/ ) % MY_RAND_MAX );
}



__private float random1(__private unsigned int *next) {
  //Return a value between  1
  __private float val = myrandom(next) / (MY_RAND_MAX * 1.0f);
  return val;
}

int disrand(int l, int t, __private unsigned int *next) {
  unsigned int val = myrandom(next);
  return ((unsigned) val % (t - l + 1) + l);
}

float expdev(__private unsigned int *next) {
  unsigned int val = myrandom(next);
  float out = (-log(val / (1.0f * MY_RAND_MAX)));
  return out;
}


void cnode(  int sp,

    __global int *Ni,
    __private float Tt,
    __global struct nodeptr *List,
    __global int *Occlist,
    __private int *Ntot,
    __global struct nodeptr *Nlist,
    __private int *N_n,


    __private unsigned int *next,
    __global struct node *NODES,
    __private int *last_node
    ) {

  int ind1, ind2, temp;
  __global struct node *p1;

  while (1) {
    ind1 = disrand(0, Ni[sp] - 1, next);
    ind2 = disrand(0, Ni[sp] - 1, next);
    if (ind2 != ind1)
      break;
  }
  if (ind1 > ind2) {
    temp = ind1;
    ind1 = ind2;
    ind2 = temp;
  }
  p1 = request_node(NODES, last_node);
  p1->time = Tt;
  p1->d[0] = List[(sp*VPNOC) + ind1].ptr;
  p1->d[1] = List[(sp*VPNOC) + ind2].ptr;
  p1 -> a[0] = p1 -> a[1] = 0;
  p1->dna = 0;
  p1->I = 0;
  p1->sp = Occlist[sp];
  List[(sp*VPNOC) + ind1].ptr->a[0] = p1;
  List[(sp*VPNOC) + ind2].ptr->a[0] = p1;
  List[(sp*VPNOC) + ind1].ptr->a[1] = List[(sp*VPNOC) + ind2].ptr->a[1] = 0;
  List[(sp*VPNOC) + ind1].ptr = p1;
  List[(sp*VPNOC) + ind2].ptr = List[(sp*VPNOC) + Ni[sp] - 1].ptr;
  --Ni[sp];
  (*Ntot)--;
  Nlist[(*N_n)].ptr = p1;
  (*N_n)++;


  return;
}





void mnode (int sp, int Spno,
    
    __global int *Ni,
    __private unsigned int *nextrand,
    __global int *Occlist,
    __global struct nodeptr *List,
    __private int *Occ,
    __global int *Lmax
    
    
) {
  int ind, j,
      i, k, jj;
  __global struct node *tp;


  ind = disrand(0, Ni[sp] - 1, nextrand);

  /* finite island model */

  while (1) {
    j = disrand(0, Spno - 1, nextrand);
    if (j != Occlist[sp])
      break;
  }

  /* start of stuff */

  List[(sp*VPNOC) + ind].ptr->sp = j;
  tp = List[(sp*VPNOC) + ind].ptr;
  List[(sp*VPNOC) + ind].ptr = List[(sp*VPNOC) + Ni[sp] - 1].ptr;
  for (i = 0; i < (*Occ); ++i) {
    if (Occlist[i] == j)
      break;
  }
  if (i == (*Occ)) {
    if (Ni[sp] == 1) {
      i = sp;
      Occlist[i] = j;
      Ni[i] = 0;
    } else {
      (*Occ)++;
      Occlist[i] = j;
      Ni[i] = 0;
      --Ni[sp];
    }
  } else {
    if (Ni[sp] == 1) {
      for (k = sp; k < (*Occ) - 1; ++k) {
        Occlist[k] = Occlist[k + 1];
        Ni[k] = Ni[k + 1];
        if (Ni[k] >= Lmax[k]) {
          Lmax[k] *= 2; //100 ou 10, *2 = 200 ou 20, 400 ou 40, 800 ou 80, ...
//          List[k] = (struct node **) realloc(List[k], Lmax[k] * sizeof(struct node *));
        }
        for (jj = 0; jj < Ni[k]; ++jj) {
          List[(k*VPNOC) + jj].ptr = List[((k + 1)*VPNOC )+ jj].ptr;
        }
      }
      (*Occ)--;
      if (i > sp)
        --i;
    } else {
      --Ni[sp];
    }
  }
  List[(i*VPNOC) + Ni[i]].ptr = tp;
  ++Ni[i];
}

void dfill(
    __global float *Den,//[NOC][2]
    __global int *Occlist,
    __global float *Sd,
    __global int *Ni,
    __global float *Migrate,
    __private int Occ,
    __private float *Dtop) {

  Den[0 + 0] = Sd[Occlist[0]] * Ni[0] * (Ni[0] - 1.0f) * 0.5f;
  Den[0 + 1] = Den[0 + 0] + Migrate[Occlist[0]] * Ni[0];
  for (int k = 1; k < Occ; ++k) {
    Den[(k*2) + 0] = Den[((k - 1)*2) + 1] + Sd[Occlist[k]] * Ni[k] * (Ni[k] - 1.0f) * 0.5f;
    Den[(k*2) + 1] = Den[(k*2) + 0] + Migrate[Occlist[k]] * Ni[k];
  }
  (*Dtop) = Den[((Occ - 1)*2) + 1];
}


__private int addmut(int p, int Ms,

    __private unsigned int *nextrand,
    __private int *NEXTMUT
) {
  int ic;
  if (Ms) {
    ic = disrand(0, 1, nextrand) * 2 - 1;
    return p + ic;
  } else {
    (*NEXTMUT)++;
    return (*NEXTMUT);
  }
}

float gammln(float xx) {
  float x,
         tmp,
         ser;
  float cof[6] = { 76.18009173f, -86.50532033f, 24.01409822f,
      -1.231739516f, 0.120858003e-2f, -0.536382e-5f };
  int j;

  x = xx - 1.0f;
  tmp = x + 5.5f;
  tmp -= (x + 0.5f) * log(tmp);
  ser = 1.0f;
  for (j = 0; j <= 5; j++) {
    x += 1.0f;
    ser += cof[j] / x;
  }
  return -tmp + log(2.50662827465f * ser);
}

int poidev(float xm,

    __private unsigned int *nextrand
  ) {

  float sq,
        alxm,
        g,
        oldm = (-1.0f);
  float em,
        t,
        y;

  if (xm < 12.0f) {
    if (xm != oldm) {
      oldm = xm;
      g = exp(-xm);
    }
    em = -1;
    t = 1.0f;
    do {
      em += 1.0f;
      t *= random1(nextrand);
    } while (t > g);
  } else {
    if (xm != oldm) {
      oldm = xm;
      sq = sqrt(2.0f * xm);
      alxm = log(xm);
      g = xm * alxm - gammln(xm + 1.0f);
    }
    do {
      do {
        y = tan(PI * random1(nextrand));
        em = sq * y + xm;
      } while (em < 0.0f);
      em = floor(em);
      t = 0.9f * (1.0f + y * y) * exp(em * alxm - gammln(em + 1.0f) - g);
    } while (random1(nextrand) > t);
  }
  return (int) (em + 0.5f);
}




void treefill(__global struct node *p, int *noall,
              float mu, int Ms, int Subs, int Smp,
              
              __global int *val_arr,//[15][50*100],
              __global int *freq_arr,//[15][50*100],
              __private unsigned int *nextrand,
              __private int *NEXTMUT
  ) {

//  __global struct node *bro;
  int j,
      mutno,
      sp,
      i;
  float time;

  if (p->a[0] == 0 && p->a[1] == 0) {
    return;
  } else { //if (!(p->a[0] == NULL && p->a[1] == NULL)) {
//    if (p->a[0]->d[0] == p) //Useless 'bro'!?!?
//      bro = p->a[0]->d[1];
//    else
//      bro = p->a[0]->d[0];
    p->dna = p->a[0]->dna;
  }
  time = p->a[0]->time - p->time;
  mutno = poidev(time * mu, nextrand);
  for (j = 0; j < mutno; ++j) {
    p->dna = addmut(p->dna, Ms, nextrand, NEXTMUT);
  }
  if (p->d[0] == 0 && p->d[1] == 0) {
    sp = p->osp;
    for (j = 0; j < *noall; ++j) {
      if (val_arr[(sp*50*100) + j] == p->dna) {
        break;
      }
    }
    if (j < *noall) {
      ++freq_arr[(sp*50*100) + j];
    }
    else {
      for (i = 0; i < Subs; ++i) {
        val_arr[(i*50*100) + j] = p->dna;
        freq_arr[(i*50*100) + j] = 0;
      }
      freq_arr[(sp*50*100) + j] = 1;
      (*noall)++;
//      if ((*noall) == Nmax) {
//        Nmax += Smp;
//        for (i = 0; i < Subs; ++i) {
//          val_arr[i] = (int *) realloc(val_arr[i], Nmax * sizeof(int));
//          freq_arr[i] = (int *) realloc(freq_arr[i], Nmax * sizeof(int));
//        }
//      }
    }
    return;
  }
  return;
}



int sim1(__global int *init, __private int initot, __private int *noall,
          __private float mu, __private float rm, __private int Spno, __private int Subs,
          __private int Smp, __private int Ms,
          
    //additional, "global" arguments of this item
    __global float *Sd,
    __global float *Migrate,
    __global int *Occlist, //TODO:? pode ser colocado nesta função
    __global int *Ni,
    __global int *Lmax,
    __global struct nodeptr *List,
    __global struct nodeptr *Nlist,
    __global float *Den,//[NOC][2]
    __global struct node *NODES,
    
    __private unsigned int *nextrand,
    __global int *val_arr,//[15][50*100],
    __global int *freq_arr//[15][50*100]
) {

  __private int j;
  __private int nmax;
  __private int ic;
  __private int k;
  __private float rr;

  __private int NEXTMUT;
  __private int Occ;
  __private int N_n;
  __private int Ntot;

  __private float Tt;
  __private float Dtop;
  
  __private int last_node = 0;

  
  NEXTMUT = 0;
  (*noall) = 0;
  for (j = 0; j < Spno; ++j) {
    Sd[j] = Spno / 0.5f;
    Migrate[j] = Sd[j] * rm;
  }
  Occ = Subs;
  for (j = 0; j < Spno; ++j)
    Occlist[j] = 0;
  for (j = 0; j < Subs; ++j) {
    Occlist[j] = j;
  }
  for (j = 0; j < Spno; ++j) {
    Ni[j] = init[j];
  }
  Ntot = initot;
  nmax = 10 * Ntot;
  
  ic = 0;
  for (k = 0; k < Spno; ++k) {
    Lmax[k] = 2 * init[k]; //init[k] = 50 ou 0
    if (Lmax[k] < 10)
      Lmax[k] = 10;
  }
  
  for (k = 0; k < Spno; ++k) { //O(Spno == 100)

    for (j = 0; j < Ni[k]; ++j) { //O(Ni[k] == init[j] == Smp (50) ou 0)
      if (last_node == 8000)
        return -1;
      
      __global struct node *newNode = request_node(NODES, &last_node);/////////////////////////////////////XXX?///////////////

      newNode->d[0] = newNode->d[1] = 0;
      newNode->a[0] = newNode->a[1] = 0;
      newNode->time = 0.0f;
      newNode->dna = 0;
      newNode->I = 0;
      newNode->sp = Occlist[k];
      newNode->osp = Occlist[k];
      Nlist[ic].ptr = newNode;
      
      List[(k*VPNOC) + j].ptr = newNode;
      ++ic;
    }
  }

  N_n = Ntot;
  Tt = 0.0f;

  startcycle:
//  while (1) {
    if (Occ > Spno) {
//      printf("error Occ > Spno\n");
      return 1; 
    }
    for (k = 0; k < Occ; ++k) {
      if (Ni[k] > Lmax[k]) {
//        printf("error in Ni/Lmax\n");
        return 2;
      }
      if (Ni[k] >= Lmax[k] - 5) {
        Lmax[k] = 2 * Lmax[k];
        if (Ni[k] > Lmax[k]) {
//          printf("error - Lmax");
          return 3;
        }
        for (j = 0; j < Ni[k]; ++j) {
          if (List[(k*VPNOC) + j].ptr->sp != Occlist[k]) {
//            printf("error in sp \n");
            return 4;
          }
        }

        for (j = 0; j < Ni[k]; ++j) {
          if (List[(k*VPNOC) + j].ptr->sp != Occlist[k]) {
//            printf("error in sp \n");
            return 5;
          }
        }
      }
    }
    if (N_n >= nmax - 1) {
      nmax = 2 * nmax;
    }

    dfill(Den, Occlist, Sd, Ni, Migrate, Occ, &Dtop);

    Tt += expdev(nextrand) / Dtop;


    bool getout = false;
    while (getout == false) {
      rr = random1(nextrand);
      for (k = 0; k < Occ; ++k) {
        for (j = 0; j < 2; ++j) {
          if (rr < Den[(k*2) + j] / Dtop) {
            getout = true;
            break;
          }
        }
        if (getout)
          break;
      }
    }

    if (j == 0) {
      if (last_node == 8000)
        return -1;
      cnode(k, Ni, Tt, List, Occlist, &Ntot, Nlist, &N_n, nextrand, NODES, &last_node);
//      if (Ntot == 1)
//        break;
    } else {
      mnode(k, Spno, Ni, nextrand, Occlist, List, &Occ, Lmax);
    }
    if (j != 0 || (j == 0 && Ntot != 1))
      goto startcycle;

//  }

  Nlist[N_n - 1].ptr->dna = 0;
  (*noall) = 0;
  for (j = N_n - 1; j >= 0; --j) {
    treefill(Nlist[j].ptr, noall, mu, Ms, Subs, Smp, val_arr, freq_arr, nextrand, &NEXTMUT);
  }

//  for (j = 0; j < N_n; ++j)
//    killtree(Nlist[j]);

  return 0;
}



void my_thetacal(/*int *gen[], */
    __private int noall,
    __global int *sample_size,
    __private int no_of_samples,

    __private float *het1,
    __private float *fst,
    
    __global int *freq_arr//[15][50*100]
) {
  int i, j, k;
  float x2, x0, yy, y1, q2, q3;

  x0 = 0.0f;
  for (j = 0; j < no_of_samples; ++j) {
    for (i = 0, x2 = 0.0f; i < noall; ++i) {
      x2 += freq_arr[(j*50*100) + i] * freq_arr[(j*50*100) + i];
    }
    x0 += (x2 - sample_size[j]) / (sample_size[j] * (sample_size[j] - 1));
  }

  yy = 0.0f;
  for (j = 0; j < no_of_samples; ++j) {
    for (k = j + 1; k < no_of_samples; ++k) {
      for (i = 0, y1 = 0.0f; i < noall; ++i) {
        y1 += freq_arr[(j*50*100) + i] * freq_arr[(k*50*100) + i];
      }
      yy += y1 / (sample_size[j] * sample_size[k]);
    }
  }

  q2 = x0 / no_of_samples;
  q3 = 2 * yy / (no_of_samples * (no_of_samples - 1));

  float het0 = 1.0f - q2;
  *het1 = 1.0f - q3;
  *fst = 1.0f - (het0) / (*het1);

}


/*Arguments' description:
 * GLOBitno                 <- niters for all items
 * GLOBSmp                  <- Smp    for all items
 * GLOBseed                 <- seed   for all items
 * Ms                       <- Ms     for all items
 * initot                   <- initot for all items
 * init[NOC]                <- init   for all items
 * Subs                     <- Subs   for all items
 * Spno                     <- Spno   for all items
 * efst                     <- efst   for all items
 * 
 *  GLOBNODES[n_global_items][4000]     <- NODES for each item, this must come already alloc'ed from the C program
 *  *GLOBList[n_global_items][NOC][200] <- List  of (allocated space for) pointers for each item, this must come already alloc'ed from the C program   
 * *GLOBNList[n_global_items][4000]     <- NList of (allocated space for) pointers for each item, this must come already alloc'ed from the C program
 * 
 * 
 *       GLOBSd[n_global_items][NOC]*         //For each item, no need for previous allocation
 *  GLOBMigrate[n_global_items][NOC]*
 *      GLOBDen[n_global_items][NOC][2]*
 *       GLOBNi[n_global_items][NOC]* TOCHECK---> disrand being called with an int from here) 
 *  GLOBOcclist[n_global_items][NOC]*
 *     GLOBLmax[n_global_items][NOC]*
 *  GLOBval_arr[n_global_items][15][50*100]
 * GLOBfreq_arr[n_global_items][15][50*100],
 * 
 * 
 * 
 * Output data:
 * fsum[n_global_items]       <- fsum result for each item
 * wsum[n_global_items]       <- wsum result for each item
 * fsts[n_global_items][itno] <- fsts result for each item
 * h1s [n_global_items][itno] <- h1s  result for each item
 */
__kernel void simulation(
    //arguments for this function
    __global int *GLOBitno,   __global int *GLOBSmp,  __global unsigned int *GLOBseed, __global int *GLOBMs,
    __global int *GLOBinitot, __global int *init, //TODO: bring this init to a nearer memory location? (is this even possible? :\)
                                                  __global int *GLOBSubs, __global int *GLOBSpno, __global float *efst,
                         
    //global variables for each item
    __global struct node    *GLOBNODES,
    __global struct nodeptr *GLOBList,
    __global struct nodeptr *GLOBNlist,
    
    __global float *GLOBSd,
    __global float *GLOBMigrate,
    __global float *GLOBDen,
    __global int *GLOBNi,
    __global int *GLOBOcclist,
    __global int *GLOBLmax,
    __global int *GLOBval_arr,
    __global int *GLOBfreq_arr,
//                         __global struct node *val_arr,
//                         __global struct node *freq_arr,

    //output arguments
    __global float* fsum, __global float* wsum, __global float* fsts, __global float* h1s
) {

  //getting my ID
//  printf("global size: %i, GLitno: %i, size 0: %i\n", get_global_size(1), *GLOBitno, get_global_size(0));
  __private int myGID = get_global_id(0);
  if (get_global_size(1) > 1)
    myGID += (get_global_id(1) * get_global_size(0));


  //acquisition of this function arguments' data
  __private int itno = (*GLOBitno) / get_global_size(0);
  if (get_global_size(1) > 1)
    itno = itno / get_global_size(1);

  __private int Smp = *GLOBSmp;
  __private unsigned int seed = (*GLOBseed) + myGID; 
  __private int Ms = *GLOBMs;
  __private int initot = *GLOBinitot;
  __private int Subs = *GLOBSubs;
  __private int Spno = *GLOBSpno;
  


  //acquisition of this item's global variables (must be passed to the called functions)
  __global struct node    *NODES = &(GLOBNODES[myGID * 8000     ]);//must be seen as node NODES[4000]
  __global struct nodeptr *List  = &(GLOBList [myGID * NOC*VPNOC]);//must be seen as node *List[NOC][200]
  __global struct nodeptr *Nlist = &(GLOBNlist[myGID * 8000     ]);//must be seen as node *Nlist[4000];


  //more "global" variables, private to this item (must be passed to the called functions)
//  __private int val_arr[15][50*100];
//  __private int freq_arr[15][50*100];// = freq_arr[myGID];

  __global float *Sd      = &(GLOBSd     [myGID * NOC]);
  __global float *Migrate = &(GLOBMigrate[myGID * NOC]);
  __global float *Den     = &(GLOBDen    [myGID * (NOC*2)]);

//  __private int Nmax = Nmax[myGID];

  __global int *Ni      = &(GLOBNi       [myGID * NOC]);
  __global int *Occlist = &(GLOBOcclist  [myGID * NOC]);
  __global int *Lmax    = &(GLOBLmax     [myGID * NOC]);
  
  __global int *val_arr  = &(GLOBval_arr [myGID * (Subs * Smp*100)]);
  __global int *freq_arr = &(GLOBfreq_arr[myGID * (Subs * Smp*100)]);

  
  //local variables to this function (not passed as arguments to any function)
  __private int i = 0;
  __private int keepmu = 0;
  __private int noall;
  __private float mu;
  __private float dd;
  __private float rm;
  __private float rr;
  __private float fst;
  __private float h1;
  

  rm = 0.5f * (1.0f / (*efst) - 1);

  //this is the final result...
  fsum[myGID] = wsum[myGID] = 0.0f; //TODO: save this locally and only touch the global mem at the end


  while(true) {
//    barrier(CLK_GLOBAL_MEM_FENCE);
//     printf("GID %i: going to start iter %i (Spno %i, Subs %i, efst %f, Smp %i, Ms %i, iters tot %i, keepmu %i)\n", myGID, i, Spno, Subs, *efst, Smp, Ms, itno, keepmu);
     if (keepmu == 0) {
//       printf("GID %i: getting random (seed: %u)\n", myGID, seed);
       rr = random1(&seed); //TODO
//       printf("GID %i: rr is %f ", myGID, rr);
//       printf("GID %i: seed is now %u\n", myGID, seed);
       if (!Ms)
         mu = rr / (1 - rr);
       else {
         dd = 1 / (1 - rr);
         dd *= dd;
         mu = (dd - 1) * 0.5f;
       }
       if (mu > 100000)
         mu = 100000;
     }

     int er = sim1(init, initot, &noall, mu, rm, Spno, Subs, Smp, Ms,
              //additional arguments
              Sd, Migrate, Occlist, Ni, Lmax, List, Nlist, Den, NODES,
              &seed,
              val_arr,
              freq_arr);
     if (er != 0) {
       h1s [(myGID*itno) + 1] = 999.0f; //this should be used to detect errors
       fsts[(myGID*itno) + 1] = 999.0f;
//       printf("ERROR AT GID %i: %i\n", myGID, er);
       //an error occurred, treat it
       return;
     }
     if (noall > 1) {
//       printf("GID %i: going to thetacal\n",myGID);
       my_thetacal(noall, init, Subs, &h1, &fst, freq_arr);////////////////////////////////////////XXX///////////////////////////////////////////

        h1s[(myGID*itno) + i] = h1; //TODO
       fsts[(myGID*itno) + i] = fst;
       fsum[myGID]           += fst * h1;
       wsum[myGID]           += h1;
       ++i;
       if (i == itno)
         break;
       keepmu = 0;
     } else {
       ++keepmu;
       if (keepmu > 1000)
         keepmu = 0;
     }
   }
  
}









