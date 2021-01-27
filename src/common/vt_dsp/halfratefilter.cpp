#include "halfratefilter.h"
#include "assert.h"

const unsigned int hr_BLOCK_SIZE = 256;
const __m128 half = _mm_set_ps1(0.5f);

HalfRateFilter::HalfRateFilter(int M, bool steep)
{
    assert(!(M > halfrate_max_M));
    this->M = M;
    this->steep = steep;
    load_coefficients();
    reset();
}

void HalfRateFilter::process_block(float *__restrict floatL, float *__restrict floatR, int N)
{
    __m128 *__restrict L = (__m128 *)floatL;
    __m128 *__restrict R = (__m128 *)floatR;
    __m128 o[hr_BLOCK_SIZE];
    // fill the buffer with interleaved stereo samples
    for (int k = 0; k < N; k += 4)
    {
        //[o3,o2,o1,o0] = [L0,L0,R0,R0]
        o[k] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(0, 0, 0, 0));
        o[k + 1] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(1, 1, 1, 1));
        o[k + 2] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(2, 2, 2, 2));
        o[k + 3] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(3, 3, 3, 3));
    }

    // process filters
    for (int j = 0; j < M; j++)
    {
        __m128 tx0 = vx0[j];
        __m128 tx1 = vx1[j];
        __m128 tx2 = vx2[j];
        __m128 ty0 = vy0[j];
        __m128 ty1 = vy1[j];
        __m128 ty2 = vy2[j];
        __m128 ta = va[j];

        for (int k = 0; k < N; k += 2)
        {
            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k] = ty0;

            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k + 1];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k + 1] = ty0;
        }
        vx0[j] = tx0;
        vx1[j] = tx1;
        vx2[j] = tx2;
        vy0[j] = ty0;
        vy1[j] = ty1;
        vy2[j] = ty2;
    }

    /*for(int k=0; k<nsamples; k++)
    {
    float *of = (float*)o;
    float out_a = of[(k<<2)];
    float out_b = of[(k<<2)+1];

    float *fL = (float*)L;
    fL[k] = (out_a + oldout_f) * 0.5f;
    oldout_f = out_b;

    out_a = of[(k<<2)+2];
    out_b = of[(k<<2)+3];

    float *fR = (float*)R;
    fR[k] = (out_a + oldoutR_f) * 0.5f;
    oldoutR_f = out_b;
    }*/

    float *fL = (float *)L;
    float *fR = (float *)R;
    __m128 faR = _mm_setzero_ps();
    __m128 fbR = _mm_setzero_ps();

    for (int k = 0; k < N; k++)
    {
        //	const double output=(filter_a.process(input)+oldout)*0.5;
        //	oldout=filter_b.process(input);

        __m128 vL = _mm_add_ss(o[k], oldout);
        vL = _mm_mul_ss(vL, half);
        _mm_store_ss(&fL[k], vL);

        faR = _mm_movehl_ps(faR, o[k]);
        fbR = _mm_movehl_ps(fbR, oldout);

        __m128 vR = _mm_add_ss(faR, fbR);
        vR = _mm_mul_ss(vR, half);
        _mm_store_ss(&fR[k], vR);

        oldout = _mm_shuffle_ps(o[k], o[k], _MM_SHUFFLE(3, 3, 1, 1));
    }
}
#if 0
// Software pipelining.. Nice idea, but the compiler does not like it one bit
// The non-pipelined implementation is quite okay, because ty0 is not used until 2 samples later, therefore the latency dependency
// is divided by 3
void HalfRateFilter::process_block(float * __restrict floatL, float * __restrict floatR, int N)
{		
	int NM1 = N + M - 1;
	for(int k = 0; k<NM1; k++)
	{
		for(int m=Max(0,k-N+1); m < Min(k+1,M); m++)
		{
			int n = k - m;
			//shuffle inputs
			vx2[m]=vx1[m];
			vx1[m]=vx0[m];
			vx0[m]=o[n];
			//shuffle outputs
			vy2[m]=vy1[m];
			vy1[m]=vy0[m];		
			//allpass filter 1
			vy0[m] = _mm_add_ps(vx2[m], _mm_mul_ps(_mm_sub_ps(vx0[m],vy2[m]),va[m]));
			o[n] = vy0[m];
		}
	}					
}
#endif

void HalfRateFilter::process_block_D2(float *floatL, float *floatR, int nsamples, float *outL,
                                      float *outR)
{
    __m128 *L = (__m128 *)floatL;
    __m128 *R = (__m128 *)floatR;
    __m128 o[hr_BLOCK_SIZE];
    // fill the buffer with interleaved stereo samples
    for (int k = 0; k < nsamples; k += 4)
    {
        //[o3,o2,o1,o0] = [L0,L0,R0,R0]
        o[k] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(0, 0, 0, 0));
        o[k + 1] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(1, 1, 1, 1));
        o[k + 2] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(2, 2, 2, 2));
        o[k + 3] = _mm_shuffle_ps(L[k >> 2], R[k >> 2], _MM_SHUFFLE(3, 3, 3, 3));
    }

    // process filters
    for (int j = 0; j < M; j++)
    {
        __m128 tx0 = vx0[j];
        __m128 tx1 = vx1[j];
        __m128 tx2 = vx2[j];
        __m128 ty0 = vy0[j];
        __m128 ty1 = vy1[j];
        __m128 ty2 = vy2[j];
        __m128 ta = va[j];

        for (int k = 0; k < nsamples; k += 2)
        {
            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k] = ty0;

            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k + 1];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k + 1] = ty0;
        }
        vx0[j] = tx0;
        vx1[j] = tx1;
        vx2[j] = tx2;
        vy0[j] = ty0;
        vy1[j] = ty1;
        vy2[j] = ty2;
    }

    __m128 aR = _mm_setzero_ps();
    __m128 bR = _mm_setzero_ps();
    __m128 cR = _mm_setzero_ps();
    __m128 dR = _mm_setzero_ps();

    /*
    // If you want to avoid downsampling, do this
    for(unsigned int k=0; k<nsamples; k++)
    {

    float *of = (float*)o;
    float out_a = of[(k<<2)];
    float out_b = of[(k<<2)+1];


    float *fL = (float*)L;
    fL[k] = (out_a + oldout_f) * 0.5f;
    oldout_f = out_b;
    }*/

    if (outL)
        L = (__m128 *)outL;
    if (outR)
        R = (__m128 *)outR;

    // 95
    for (int k = 0; k < nsamples; k += 8)
    {
        /*	const double output=(filter_a.process(input)+oldout)*0.5;
        oldout=filter_b.process(input);*/

        __m128 tL0 = _mm_shuffle_ps(o[k], o[k], _MM_SHUFFLE(1, 1, 1, 1));
        __m128 tR0 = _mm_shuffle_ps(o[k], o[k], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 aL = _mm_add_ss(tL0, o[k + 1]);
        aR = _mm_movehl_ps(aR, o[k + 1]);
        aR = _mm_add_ss(aR, tR0);

        tL0 = _mm_shuffle_ps(o[k + 2], o[k + 2], _MM_SHUFFLE(1, 1, 1, 1));
        tR0 = _mm_shuffle_ps(o[k + 2], o[k + 2], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 bL = _mm_add_ss(tL0, o[k + 3]);
        bR = _mm_movehl_ps(aR, o[k + 3]);
        bR = _mm_add_ss(bR, tR0);

        tL0 = _mm_shuffle_ps(o[k + 4], o[k + 4], _MM_SHUFFLE(1, 1, 1, 1));
        tR0 = _mm_shuffle_ps(o[k + 4], o[k + 4], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 cL = _mm_add_ss(tL0, o[k + 5]);
        cR = _mm_movehl_ps(cR, o[k + 5]);
        cR = _mm_add_ss(cR, tR0);

        tL0 = _mm_shuffle_ps(o[k + 6], o[k + 6], _MM_SHUFFLE(1, 1, 1, 1));
        tR0 = _mm_shuffle_ps(o[k + 6], o[k + 6], _MM_SHUFFLE(3, 3, 3, 3));
        __m128 dL = _mm_add_ss(tL0, o[k + 7]);
        dR = _mm_movehl_ps(dR, o[k + 7]);
        dR = _mm_add_ss(dR, tR0);

        aL = _mm_movelh_ps(aL, bL);
        cL = _mm_movelh_ps(cL, dL);
        aR = _mm_movelh_ps(aR, bR);
        cR = _mm_movelh_ps(cR, dR);

        L[k >> 3] = _mm_shuffle_ps(aL, cL, _MM_SHUFFLE(2, 0, 2, 0));
        R[k >> 3] = _mm_shuffle_ps(aR, cR, _MM_SHUFFLE(2, 0, 2, 0));

        // optional: *=0.5;
        const __m128 half = _mm_set_ps1(0.5f);
        L[k >> 3] = _mm_mul_ps(L[k >> 3], half);
        R[k >> 3] = _mm_mul_ps(R[k >> 3], half);
    }
}

void HalfRateFilter::process_block_U2(float *floatL_in, float *floatR_in, float *floatL,
                                      float *floatR, int nsamples)
{
    __m128 *L = (__m128 *)floatL;
    __m128 *R = (__m128 *)floatR;
    __m128 *L_in = (__m128 *)floatL_in;
    __m128 *R_in = (__m128 *)floatR_in;

    __m128 o[hr_BLOCK_SIZE];
    // fill the buffer with interleaved stereo samples
    for (int k = 0; k < nsamples; k += 8)
    {
        //[o3,o2,o1,o0] = [L0,L0,R0,R0]
        o[k] = _mm_shuffle_ps(L_in[k >> 3], R_in[k >> 3], _MM_SHUFFLE(0, 0, 0, 0));
        o[k + 1] = _mm_setzero_ps();
        o[k + 2] = _mm_shuffle_ps(L_in[k >> 3], R_in[k >> 3], _MM_SHUFFLE(1, 1, 1, 1));
        o[k + 3] = _mm_setzero_ps();
        o[k + 4] = _mm_shuffle_ps(L_in[k >> 3], R_in[k >> 3], _MM_SHUFFLE(2, 2, 2, 2));
        o[k + 5] = _mm_setzero_ps();
        o[k + 6] = _mm_shuffle_ps(L_in[k >> 3], R_in[k >> 3], _MM_SHUFFLE(3, 3, 3, 3));
        o[k + 7] = _mm_setzero_ps();
    }

    // process filters
    for (int j = 0; j < M; j++)
    {
        __m128 tx0 = vx0[j];
        __m128 tx1 = vx1[j];
        __m128 tx2 = vx2[j];
        __m128 ty0 = vy0[j];
        __m128 ty1 = vy1[j];
        __m128 ty2 = vy2[j];
        __m128 ta = va[j];

        for (int k = 0; k < nsamples; k += 2)
        {
            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k] = ty0;

            // shuffle inputs
            tx2 = tx1;
            tx1 = tx0;
            tx0 = o[k + 1];
            // shuffle outputs
            ty2 = ty1;
            ty1 = ty0;
            // allpass filter 1
            ty0 = _mm_add_ps(tx2, _mm_mul_ps(_mm_sub_ps(tx0, ty2), ta));
            o[k + 1] = ty0;
        }
        vx0[j] = tx0;
        vx1[j] = tx1;
        vx2[j] = tx2;
        vy0[j] = ty0;
        vy1[j] = ty1;
        vy2[j] = ty2;
    }

    /*__m128 aR = _mm_setzero_ps();
    __m128 bR = _mm_setzero_ps();
    __m128 cR = _mm_setzero_ps();
    __m128 dR = _mm_setzero_ps();*/

    float *fL = (float *)L;
    float *fR = (float *)R;
    __m128 faR = _mm_setzero_ps();
    __m128 fbR = _mm_setzero_ps();

    for (int k = 0; k < nsamples; k++)
    {
        //	const double output=(filter_a.process(input)+oldout)*0.5;
        //	oldout=filter_b.process(input);

        __m128 vL = _mm_add_ss(o[k], oldout);
        vL = _mm_mul_ss(vL, half);
        _mm_store_ss(&fL[k], vL);

        faR = _mm_movehl_ps(faR, o[k]);
        fbR = _mm_movehl_ps(fbR, oldout);

        __m128 vR = _mm_add_ss(faR, fbR);
        vR = _mm_mul_ss(vR, half);
        _mm_store_ss(&fR[k], vR);

        oldout = _mm_shuffle_ps(o[k], o[k], _MM_SHUFFLE(3, 3, 1, 1));
    }

    // If you want to avoid downsampling, do this
    /*float oldout_f = 0.f;
    for(unsigned int k=0; k<nsamples; k++)
    {

    float *of = (float*)o;
    float out_a = of[(k<<2)];
    float out_b = of[(k<<2)+1];


    float *fL = (float*)L;
    fL[k] = (out_a + oldout_f) * 0.5f;
    oldout_f = out_b;
    }		*/
}

void HalfRateFilter::reset()
{
    for (int i = 0; i < M; i++)
    {
        vx0[i] = _mm_setzero_ps();
        vx1[i] = _mm_setzero_ps();
        vx2[i] = _mm_setzero_ps();
        vy0[i] = _mm_setzero_ps();
        vy1[i] = _mm_setzero_ps();
        vy2[i] = _mm_setzero_ps();
    }
    oldout = _mm_setzero_ps();
}

void HalfRateFilter::set_coefficients(float *cA, float *cB)
{
    for (int i = 0; i < M; i++)
    {
        // va[i] = _mm_set_ps(cA[i],cB[i],cA[i],cB[i]);
        va[i] = _mm_set_ps(cB[i], cA[i], cB[i], cA[i]);
    }
}

void HalfRateFilter::load_coefficients()
{
    for (int i = 0; i < M; i++)
    {
        va[i] = _mm_setzero_ps();
    }

    int order = M << 1;
    if (steep)
    {
        if (order == 12) // rejection=104dB, transition band=0.01
        {
            float a_coefficients[6] = {0.036681502163648017f, 0.2746317593794541f,
                                       0.56109896978791948f,  0.769741833862266f,
                                       0.8922608180038789f,   0.962094548378084f};

            float b_coefficients[6] = {0.13654762463195771f, 0.42313861743656667f,
                                       0.6775400499741616f,  0.839889624849638f,
                                       0.9315419599631839f,  0.9878163707328971f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 10) // rejection=86dB, transition band=0.01
        {
            float a_coefficients[5] = {0.051457617441190984f, 0.35978656070567017f,
                                       0.6725475931034693f, 0.8590884928249939f,
                                       0.9540209867860787f};

            float b_coefficients[5] = {0.18621906251989334f, 0.529951372847964f,
                                       0.7810257527489514f, 0.9141815687605308f,
                                       0.985475023014907f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 8) // rejection=69dB, transition band=0.01
        {
            float a_coefficients[4] = {0.07711507983241622f, 0.4820706250610472f,
                                       0.7968204713315797f, 0.9412514277740471f};

            float b_coefficients[4] = {0.2659685265210946f, 0.6651041532634957f,
                                       0.8841015085506159f, 0.9820054141886075f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 6) // rejection=51dB, transition band=0.01
        {
            float a_coefficients[3] = {0.1271414136264853f, 0.6528245886369117f,
                                       0.9176942834328115f};

            float b_coefficients[3] = {0.40056789819445626f, 0.8204163891923343f,
                                       0.9763114515836773f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 4) // rejection=53dB,transition band=0.05
        {
            float a_coefficients[2] = {0.12073211751675449f, 0.6632020224193995f};

            float b_coefficients[2] = {0.3903621872345006f, 0.890786832653497f};

            set_coefficients(a_coefficients, b_coefficients);
        }

        else // order=2, rejection=36dB, transition band=0.1
        {
            float a_coefficients = 0.23647102099689224f;
            float b_coefficients = 0.7145421497126001f;

            set_coefficients(&a_coefficients, &b_coefficients);
        }
    }
    else // softer slopes, more attenuation and less stopband ripple
    {
        if (order == 12) // rejection=150dB, transition band=0.05
        {
            float a_coefficients[6] = {0.01677466677723562f, 0.13902148819717805f,
                                       0.3325011117394731f,  0.53766105314488f,
                                       0.7214184024215805f,  0.8821858402078155f};

            float b_coefficients[6] = {0.06501319274445962f, 0.23094129990840923f,
                                       0.4364942348420355f,  0.6329609551399348f,
                                       0.80378086794111226f, 0.9599687404800694f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 10) // rejection=133dB, transition band=0.05
        {
            float a_coefficients[5] = {0.02366831419883467f, 0.18989476227180174f,
                                       0.43157318062118555f, 0.6632020224193995f,
                                       0.860015542499582f};

            float b_coefficients[5] = {0.09056555904993387f, 0.3078575723749043f,
                                       0.5516782402507934f, 0.7652146863779808f,
                                       0.95247728378667541f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 8) // rejection=106dB, transition band=0.05
        {
            float a_coefficients[4] = {0.03583278843106211f, 0.2720401433964576f,
                                       0.5720571972357003f, 0.827124761997324f};

            float b_coefficients[4] = {0.1340901419430669f, 0.4243248712718685f,
                                       0.7062921421386394f, 0.9415030941737551f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 6) // rejection=80dB, transition band=0.05
        {
            float a_coefficients[3] = {0.06029739095712437f, 0.4125907203610563f,
                                       0.7727156537429234f};

            float b_coefficients[3] = {0.21597144456092948f, 0.6043586264658363f,
                                       0.9238861386532906f};

            set_coefficients(a_coefficients, b_coefficients);
        }
        else if (order == 4) // rejection=70dB,transition band=0.1
        {
            float a_coefficients[2] = {0.07986642623635751f, 0.5453536510711322f};

            float b_coefficients[2] = {0.28382934487410993f, 0.8344118914807379f};

            set_coefficients(a_coefficients, b_coefficients);
        }

        else // order=2, rejection=36dB, transition band=0.1
        {
            float a_coefficients = 0.23647102099689224f;
            float b_coefficients = 0.7145421497126001f;

            set_coefficients(&a_coefficients, &b_coefficients);
        }
    }
}
