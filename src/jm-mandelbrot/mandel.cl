
__kernel void calc_tile(
  __global float *x1,
  __global float *y1,
  __global float *x2,
  __global float *y2,
  __global int *wd,
  __global int *ht,
  __global int *nite,
  __global int *res) {

  __private int ix;
  __private int iy;       /* loop indices */
  __private float x;
  __private float y;      /* pixel coords */
  __private float ar;
  __private float ai;
  __private float a; /* accums */
  __private int ite;          /* number of iter until divergence*/
  __private float LOCALx1 = *x1;
  __private float LOCALx2 = *x2;
  __private float LOCALy1 = *y1;
  __private float LOCALy2 = *y2;
  __private int LOCALwd = *wd;
  __private int LOCALht = *ht;
  __private int LOCALnite = *nite;


  __private int threadnumx = get_global_id(0);
  __private int numthreadsx = get_global_size(0);

  __private int threadnumy = get_global_id(1); //0 to 1024
  __private int numthreadsy = get_global_size(1);


  LOCALx2 -= LOCALx1;
  LOCALy2 -= LOCALy1;
  iy = 0;
  
  __private int fromy;
  __private int uptoy;
  __private int fromx;
  __private int uptox;

  if (numthreadsy > LOCALht) {
    if (threadnumy < LOCALht)
      fromy = threadnumy;
    else
      return;
  }
  else {
    fromy = (1.0 * LOCALht/numthreadsy) * threadnumy;
    uptoy = (1.0 * LOCALht/numthreadsy) * (threadnumy+1);
  }

  if (numthreadsx > LOCALwd) {
    if (threadnumx < LOCALwd)
      fromx = threadnumx;
    else
      return;
  }
  else {
    fromx = (LOCALwd/numthreadsx) * threadnumx;
    uptox = (LOCALwd/numthreadsx) * (threadnumx+1);
  }


  for (iy = fromy; iy < uptoy; iy++) {
    y = (iy * LOCALy2) / LOCALht + LOCALy1;

    for (ix = fromx; ix < uptox; ix++) {
      x = (ix * LOCALx2) / LOCALwd + LOCALx1;

      ar = x;
      ai = y;
      for (ite = 0; ite < LOCALnite; ite++) {
        a = (ar * ar) + (ai * ai);
        if (a > 4)
          break;
        a = ar * ar - ai * ai;
        ai *= 2 * ar;
        ar = a + x;
        ai += y;
      }
      res[iy * LOCALwd + ix] = LOCALnite - ite;
    }
  }
}

