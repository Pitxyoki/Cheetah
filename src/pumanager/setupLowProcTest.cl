/*!
 * Sample kernel which multiplies every element of the input array with
 * a constant and stores it at the corresponding output array
 * Taken from the ATI Stream SDK samples
 */
 
__kernel void setupLowProcTest(__global  unsigned int * input,
                             __global  unsigned int * output)
//                             const     unsigned int multiplier)
{
    uint tid = get_global_id(0);
    output[0] = input[0] * 2;
}


