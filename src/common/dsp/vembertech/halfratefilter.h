#pragma once
#include "shared.h"

const unsigned int halfrate_max_M = 6;

class alignas(16) HalfRateFilter
{
  private:
    __m128 va[halfrate_max_M];
    __m128 vx0[halfrate_max_M];
    __m128 vx1[halfrate_max_M];
    __m128 vx2[halfrate_max_M];
    __m128 vy0[halfrate_max_M];
    __m128 vy1[halfrate_max_M];
    __m128 vy2[halfrate_max_M];
    __m128 oldout;

  public:
    HalfRateFilter(int M, bool steep);
    void process_block(float *L, float *R, int nsamples = 64);
    void process_block_D2(float *L, float *R, int nsamples = 64, float *outL = 0,
                          float *outR = 0); // process in-place. the new block will be half the size
    void process_block_U2(float *L_in, float *R_in, float *L, float *R, int nsamples = 64);
    void load_coefficients();
    void set_coefficients(float *cA, float *cB);
    void reset();

  private:
    int M;
    bool steep;
    float oldoutL, oldoutR;
    // unsigned int BLOCK_SIZE;
};
