struct lattice_sd
{
	struct {				
		double K1,K2,V1,V2,V3,ClipGain;		
	} v,dv;		
	double reg[3];
};

struct lattice_pd
{
	struct {				
		__m128d K2,K1,V1,V2,V3,ClipGain;		 
	} v,dv;		
	__m128d reg[4];
};

typedef void (*iir_lattice_sd_FPtr)(lattice_sd&,double*,int);


extern iir_lattice_sd_FPtr iir_lattice_sd;
//void iir_lattice_sd(lattice_sd&,double*,int);
//void iir_lattice_pd(lattice_pd&,double*,int);

void coeff_LP2_sd(lattice_sd&,double,double);

//void coeff_LP(int mask, float freq, float q);	
//mask styr unit 1, 2 eller 1+2