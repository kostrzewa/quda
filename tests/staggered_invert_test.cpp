#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#include <test_util.h>
#include <blas_reference.h>
#include <staggered_dslash_reference.h>
#include <quda.h>
#include <string.h>
#include "misc.h"

#define mySpinorSiteSize 6

int device = 0;
QudaReconstructType link_recon = QUDA_RECONSTRUCT_12;
QudaPrecision prec = QUDA_SINGLE_PRECISION;
QudaPrecision cpu_prec = QUDA_DOUBLE_PRECISION;

QudaReconstructType link_recon_sloppy = QUDA_RECONSTRUCT_INVALID;
QudaPrecision  prec_sloppy = QUDA_INVALID_PRECISION;

static double tol = 1e-8;

static int testtype = 0;
static int sdim = 8;
static int tdim = 24;

extern int V;

template<typename Float>
void constructSpinorField(Float *res) {
  for(int i = 0; i < V; i++) {
    for (int s = 0; s < 1; s++) {
      for (int m = 0; m < 3; m++) {
	res[i*(1*3*2) + s*(3*2) + m*(2) + 0] = rand() / (Float)RAND_MAX;
	res[i*(1*3*2) + s*(3*2) + m*(2) + 1] = rand() / (Float)RAND_MAX;
      }
    }
  }
}


static int
invert_test(void)
{
  void *fatlink[4];
  void *longlink[4];
    
  QudaGaugeParam gauge_param;
  QudaInvertParam inv_param;

  gauge_param.X[0] = sdim;
  gauge_param.X[1] = sdim;
  gauge_param.X[2] = sdim;
  gauge_param.X[3] = tdim;
  setDims(gauge_param.X);
    
  gauge_param.cpu_prec = cpu_prec;
    
  gauge_param.cuda_prec = prec;
  gauge_param.reconstruct = link_recon;

  gauge_param.cuda_prec_sloppy = prec_sloppy;
  gauge_param.reconstruct_sloppy = link_recon_sloppy;
  
  gauge_param.gauge_fix = QUDA_GAUGE_FIXED_NO;

  gauge_param.tadpole_coeff = 0.8;

  inv_param.verbosity = QUDA_VERBOSE;
  inv_param.inv_type = QUDA_CG_INVERTER;

  gauge_param.t_boundary = QUDA_ANTI_PERIODIC_T;
  gauge_param.gauge_order = QUDA_QDP_GAUGE_ORDER;
    
  double mass = 0.95;
  inv_param.mass = mass;
  inv_param.tol = tol;
  inv_param.maxiter = 100;
  inv_param.reliable_delta = 1e-3;
  inv_param.mass_normalization = QUDA_MASS_NORMALIZATION;
  inv_param.cpu_prec = cpu_prec;
  inv_param.cuda_prec = prec; 
  inv_param.cuda_prec_sloppy = prec_sloppy;
  inv_param.solution_type = QUDA_MATDAG_MAT_SOLUTION;
  inv_param.preserve_source = QUDA_PRESERVE_SOURCE_YES;
  inv_param.dirac_order = QUDA_DIRAC_ORDER;
  inv_param.dslash_type = QUDA_STAGGERED_DSLASH;
  gauge_param.ga_pad = sdim*sdim*sdim;
  inv_param.sp_pad = sdim*sdim*sdim;
  inv_param.cl_pad = sdim*sdim*sdim;
  
  size_t gSize = (gauge_param.cpu_prec == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
  size_t sSize = (inv_param.cpu_prec == QUDA_DOUBLE_PRECISION) ? sizeof(double) : sizeof(float);
  
  for (int dir = 0; dir < 4; dir++) {
    fatlink[dir] = malloc(V*gaugeSiteSize*gSize);
    longlink[dir] = malloc(V*gaugeSiteSize*gSize);
  }
  construct_fat_long_gauge_field(fatlink, longlink, 1, gauge_param.cpu_prec, &gauge_param);
    
  for (int dir = 0; dir < 4; dir++) {
    for(int i = 0;i < V*gaugeSiteSize;i++){
      if (gauge_param.cpu_prec == QUDA_DOUBLE_PRECISION){
	((double*)fatlink[dir])[i] = 0.5 *rand()/RAND_MAX;
      }else{
	((float*)fatlink[dir])[i] = 0.5* rand()/RAND_MAX;
      }
    }
  }
    
  void *spinorIn = malloc(V*mySpinorSiteSize*sSize);
  void *spinorOut = malloc(V*mySpinorSiteSize*sSize);
  void *spinorCheck = malloc(V*mySpinorSiteSize*sSize);
  void *tmp = malloc(V*mySpinorSiteSize*sSize);
    
  memset(spinorIn, 0, V*mySpinorSiteSize*sSize);
  memset(spinorOut, 0, V*mySpinorSiteSize*sSize);
  memset(spinorCheck, 0, V*mySpinorSiteSize*sSize);
  memset(tmp, 0, V*mySpinorSiteSize*sSize);

  if (inv_param.cpu_prec == QUDA_SINGLE_PRECISION){
    constructSpinorField((float*)spinorIn);    
  }else{
    constructSpinorField((double*)spinorIn);
  }
  
  void* spinorInOdd = ((char*)spinorIn) + Vh*mySpinorSiteSize*sSize;
  void* spinorOutOdd = ((char*)spinorOut) + Vh*mySpinorSiteSize*sSize;
  void* spinorCheckOdd = ((char*)spinorCheck) + Vh*mySpinorSiteSize*sSize;
  
  initQuda(device);

  gauge_param.type = QUDA_ASQTAD_FAT_LINKS;
  gauge_param.reconstruct = gauge_param.reconstruct_sloppy = QUDA_RECONSTRUCT_NO;
  loadGaugeQuda(fatlink, &gauge_param);

  gauge_param.type = QUDA_ASQTAD_LONG_LINKS;
  gauge_param.reconstruct = link_recon;
  gauge_param.reconstruct_sloppy = link_recon_sloppy;
  loadGaugeQuda(longlink, &gauge_param);

  double time0 = -((double)clock()); // Start the timer
  
  unsigned long volume = Vh;
  unsigned long nflops=2*1187; //from MILC's CG routine
  double nrm2=0;
  double src2=0;
  switch(testtype){

  case 0: //even
    volume = Vh;
    inv_param.solver_type = QUDA_MATPC_SOLVER;
    inv_param.matpc_type = QUDA_MATPC_EVEN_EVEN;
    
    invertQuda(spinorOut, spinorIn, &inv_param);
    
    time0 += clock(); 
    time0 /= CLOCKS_PER_SEC;
    
    matdagmat_milc(spinorCheck, fatlink, longlink, spinorOut, mass, 0, inv_param.cpu_prec, gauge_param.cpu_prec, tmp, QUDA_EVEN);
    
    mxpy(spinorIn, spinorCheck, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    nrm2 = norm_2(spinorCheck, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    src2 = norm_2(spinorIn, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    break;

  case 1: //odd
	
    volume = Vh;    
    inv_param.solver_type = QUDA_MATPC_SOLVER;
    inv_param.matpc_type = QUDA_MATPC_ODD_ODD;
    invertQuda(spinorOutOdd, spinorInOdd, &inv_param);	
    time0 += clock(); // stop the timer
    time0 /= CLOCKS_PER_SEC;
    
    
    matdagmat_milc(spinorCheckOdd, fatlink, longlink, spinorOutOdd, mass, 0, inv_param.cpu_prec, gauge_param.cpu_prec, tmp, QUDA_ODD);	
    mxpy(spinorInOdd, spinorCheckOdd, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    nrm2 = norm_2(spinorCheckOdd, Vh*mySpinorSiteSize, inv_param.cpu_prec);
    src2 = norm_2(spinorInOdd, Vh*mySpinorSiteSize, inv_param.cpu_prec);
	
    break;
    
  case 2: //full spinor

    volume = Vh; //FIXME: the time reported is only parity time
    inv_param.solver_type = QUDA_MAT_SOLVER;
    invertQuda(spinorOut, spinorIn, &inv_param);
    
    time0 += clock(); // stop the timer
    time0 /= CLOCKS_PER_SEC;
    
    matdagmat_milc(spinorCheck, fatlink, longlink, spinorOut, mass, 0, inv_param.cpu_prec, gauge_param.cpu_prec, tmp, QUDA_EVENODD);
    
    mxpy(spinorIn, spinorCheck, V*mySpinorSiteSize, inv_param.cpu_prec);
    nrm2 = norm_2(spinorCheck, V*mySpinorSiteSize, inv_param.cpu_prec);
    src2 = norm_2(spinorIn, V*mySpinorSiteSize, inv_param.cpu_prec);

    break;

  case 3: //multi mass CG, even
  case 4:
  case 5:

#define NUM_OFFSETS 4
        
    nflops = 2*(1205 + 15* NUM_OFFSETS); //from MILC's multimass CG routine
    double masses[NUM_OFFSETS] ={5.05, 1.23, 2.64, 2.33};
    double offsets[NUM_OFFSETS];	
    int num_offsets =NUM_OFFSETS;
    void* spinorOutArray[NUM_OFFSETS];
    void* in;
    int len;
    
    for (int i=0; i< num_offsets;i++){
      offsets[i] = 4*masses[i]*masses[i];
    }
    
    if (testtype == 3){
      in=spinorIn;
      len=Vh;
      volume = Vh;
      
      inv_param.solver_type = QUDA_MATPC_SOLVER;
      inv_param.matpc_type = QUDA_MATPC_EVEN_EVEN;      
      
      spinorOutArray[0] = spinorOut;
      for (int i=1; i< num_offsets;i++){
	spinorOutArray[i] = malloc(Vh*mySpinorSiteSize*sSize);
      }		
    }
    
    else if (testtype ==4){
      in=spinorInOdd;
      len = Vh;
      volume = Vh;

      inv_param.solver_type = QUDA_MATPC_SOLVER;
      inv_param.matpc_type = QUDA_MATPC_ODD_ODD;
      
      spinorOutArray[0] = spinorOutOdd;
      for (int i=1; i< num_offsets;i++){
	spinorOutArray[i] = malloc(Vh*mySpinorSiteSize*sSize);
      }
    }else { //testtype ==5
      in=spinorIn;
      len= V;
      inv_param.solver_type = QUDA_MAT_SOLVER;
      volume = Vh; //FIXME: the time reported is only parity time
      spinorOutArray[0] = spinorOut;
      for (int i=1; i< num_offsets;i++){
	spinorOutArray[i] = malloc(V*mySpinorSiteSize*sSize);
      }		
    }
    
    double residue_sq;
    invertMultiShiftQuda(spinorOutArray, in, &inv_param, offsets, num_offsets, &residue_sq);	
    cudaThreadSynchronize();
    printf("Final residue squred =%g\n", residue_sq);
    time0 += clock(); // stop the timer
    time0 /= CLOCKS_PER_SEC;
    
    printf("done: total time = %g secs, %i iter / %g secs = %g gflops, \n", 
	   time0, inv_param.iter, inv_param.secs,
	   inv_param.gflops/inv_param.secs);

    
    printf("checking the solution\n");
    MyQudaParity parity;
    if (inv_param.solver_type == QUDA_MAT_SOLVER){
      parity = QUDA_EVENODD;
    }else if (inv_param.matpc_type == QUDA_MATPC_EVEN_EVEN){
      parity = QUDA_EVEN;
    }else if (inv_param.matpc_type == QUDA_MATPC_ODD_ODD){
      parity = QUDA_ODD;
    }else{
      printf("ERROR: invalid spinor parity \n");
      exit(1);
    }
    
    for(int i=0;i < num_offsets;i++){
      printf("%dth solution: mass=%f", i, masses[i]);
      matdagmat_milc(spinorCheck, fatlink, longlink, spinorOutArray[i], masses[i], 0, inv_param.cpu_prec, gauge_param.cpu_prec, tmp, parity);
      mxpy(in, spinorCheck, len*mySpinorSiteSize, inv_param.cpu_prec);
      double nrm2 = norm_2(spinorCheck, len*mySpinorSiteSize, inv_param.cpu_prec);
      double src2 = norm_2(in, len*mySpinorSiteSize, inv_param.cpu_prec);
      printf("relative residual, requested = %g, actual = %g\n", inv_param.tol, sqrt(nrm2/src2));
    }
    
    for(int i=1; i < num_offsets;i++){
      free(spinorOutArray[i]);
    }

    
  }//switch
    
  if (testtype <=2){
    printf("Relative residual, requested = %g, actual = %g\n", inv_param.tol, sqrt(nrm2/src2));
	
    printf("done: total time = %g secs, %i iter / %g secs = %g gflops, \n", 
	   time0, inv_param.iter, inv_param.secs,
	   inv_param.gflops/inv_param.secs);
  }
  endQuda();

  if (tmp){
    free(tmp);
  }
  return 0;
}




void
display_test_info()
{
  printf("running the following test:\n");
    
  printf("prec    sloppy_prec    link_recon  sloppy_link_recon test_type  S_dimension T_dimension\n");
  printf("%s   %s             %s            %s            %s         %d          %d \n",
	 get_prec_str(prec),get_prec_str(prec_sloppy),
	 get_recon_str(link_recon), 
	 get_recon_str(link_recon_sloppy), get_test_type(testtype), sdim, tdim);     
  return ;
  
}

void
usage(char** argv )
{
  printf("Usage: %s <args>\n", argv[0]);
  printf("--prec         <double/single/half>     Spinor/gauge precision\n"); 
  printf("--prec_sloppy  <double/single/half>     Spinor/gauge sloppy precision\n"); 
  printf("--recon        <8/12>                   Long link reconstruction type\n"); 
  printf("--test         <0/1/2/3/4/5>            Testing type(0=even, 1=odd, 2=full, 3=multimass even,\n" 
	 "                                                     4=multimass odd, 5=multimass full)\n"); 
  printf("--tdim                                  T dimension\n");
  printf("--sdim                                  S dimension\n");
  printf("--help                                  Print out this message\n"); 
  exit(1);
  return ;
}


int main(int argc, char** argv)
{

  int i;
  for (i =1;i < argc; i++){
	
    if( strcmp(argv[i], "--help")== 0){
      usage(argv);
    }
	
    if( strcmp(argv[i], "--prec") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      prec = get_prec(argv[i+1]);
      i++;
      continue;	    
    }
    
    if( strcmp(argv[i], "--prec_sloppy") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      prec_sloppy =  get_prec(argv[i+1]);
      i++;
      continue;	    
    }
    
    
    if( strcmp(argv[i], "--recon") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      link_recon =  get_recon(argv[i+1]);
      i++;
      continue;	    
    }
    if( strcmp(argv[i], "--tol") == 0){
      float tmpf;
      if (i+1 >= argc){
        usage(argv);
      }
      sscanf(argv[i+1], "%f", &tmpf);
      if (tol <= 0){
        PRINTF("ERROR: invalid tol(%f)\n", tmpf);
        usage(argv);
      }
      tol = tmpf;
      i++;
      continue;
    }


	
    if( strcmp(argv[i], "--recon_sloppy") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      link_recon_sloppy =  get_recon(argv[i+1]);
      i++;
      continue;	    
    }
	
    if( strcmp(argv[i], "--test") == 0){
      if (i+1 >= argc){
	usage(argv);
      }	    
      testtype = atoi(argv[i+1]);
      i++;
      continue;	    
    }

    if( strcmp(argv[i], "--cprec") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      cpu_prec= get_prec(argv[i+1]);
      i++;
      continue;
    }

    if( strcmp(argv[i], "--tdim") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      tdim= atoi(argv[i+1]);
      if (tdim < 0 || tdim > 128){
	printf("ERROR: invalid T dimention (%d)\n", tdim);
	usage(argv);
      }
      i++;
      continue;
    }		
    if( strcmp(argv[i], "--sdim") == 0){
      if (i+1 >= argc){
	usage(argv);
      }
      sdim= atoi(argv[i+1]);
      if (sdim < 0 || sdim > 128){
	printf("ERROR: invalid S dimention (%d)\n", sdim);
	usage(argv);
      }
      i++;
      continue;
    }
    if( strcmp(argv[i], "--device") == 0){
          if (i+1 >= argc){
              usage(argv);
          }
          device =  atoi(argv[i+1]);
          if (device < 0){
              fprintf(stderr, "Error: invalid device number(%d)\n", device);
              exit(1);
          }
          i++;
          continue;
    }


    fprintf(stderr, "ERROR: Invalid option:%s\n", argv[i]);
    usage(argv);
  }


  if (prec_sloppy == QUDA_INVALID_PRECISION){
    prec_sloppy = prec;
  }
  if (link_recon_sloppy == QUDA_RECONSTRUCT_INVALID){
    link_recon_sloppy = link_recon;
  }
  
  display_test_info();
  invert_test();
    

  return 0;
}
