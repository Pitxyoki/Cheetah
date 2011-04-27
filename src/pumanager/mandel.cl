
__kernel void calc_tile(
  __global float *x1,
  __global float *y1,
  __global float *x2,
  __global float *y2,
  __global int *wd,
  __global int *ht,
  __global int *nite,
  __global int *res) {

  int ix;
  int iy;       /* loop indices */
  float x;
  float y;      /* pixel coords */
  float ar;
  float ai;
  float a; /* accums */
  int ite;          /* number of iter until divergence*/
  float LOCALx1 = *x1;
  float LOCALx2 = *x2;
  float LOCALy1 = *y1;
  float LOCALy2 = *y2;
  int LOCALwd = *wd;
  int LOCALht = *ht;
  int LOCALnite = *nite;


  __private float threadnumy = get_global_id(0); //0 to 1024
  __private float numthreadsy = get_global_size(0);

  __private float threadnumx = get_global_id(1);
  __private float numthreadsx = get_global_size(1);

  LOCALx2 -= LOCALx1;
  LOCALy2 -= LOCALy1;
  iy = 0;
  
  __private int fromy;
  __private int uptoy;
  __private int fromx;
  __private int uptox;

  fromy = (LOCALht/numthreadsy) * threadnumy;
  uptoy = (LOCALht/numthreadsy) * (threadnumy+1);

  fromx = (LOCALwd/numthreadsx) * threadnumx;
  uptox = (LOCALwd/numthreadsx) * (threadnumx+1);

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

