namespace mixed {

/*
  Wilson
  double double2 M = 1/12
  single float4  M = 1/6
  half   short4  M = 6/6

  Staggered 
  double double2 M = 1/3
  single float2  M = 1/3
  half   short2  M = 3/3
 */

/**
   Driver for generic reduction routine with two loads.
   @param ReduceType 
   @param siteUnroll - if this is true, then one site corresponds to exactly one thread
 */
template <typename doubleN, typename ReduceType,
	  template <typename ReducerType, typename Float, typename FloatN> class Reducer,
  int writeX, int writeY, int writeZ, int writeW, int writeV, bool siteUnroll>
doubleN reduceCuda(const double2 &a, const double2 &b, ColorSpinorField &x, 
		   ColorSpinorField &y, ColorSpinorField &z, ColorSpinorField &w,
		   ColorSpinorField &v) {

  doubleN value;
  if (checkLocation(x, y, z, w, v) == QUDA_CUDA_FIELD_LOCATION) {

    if (!x.isNative() && !(x.Nspin() == 4 && x.FieldOrder() == QUDA_FLOAT2_FIELD_ORDER && x.Precision() == QUDA_SINGLE_PRECISION) ) {
      warningQuda("Device reductions on non-native fields is not supported\n");
      doubleN value;
      ::quda::zero(value);
      return value;
    }

    // cannot do site unrolling for arbitrary color (needs JIT)
    if (x.Ncolor()!=3) errorQuda("Not supported");

    if (x.Precision() == QUDA_SINGLE_PRECISION && z.Precision() == QUDA_DOUBLE_PRECISION) {
      if (x.Nspin() == 4){ //wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC)
	const int M = 12; // determines how much work per thread to do
	value = reduce::reduceCuda<doubleN,ReduceType,double2,float4,double2,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, x.Volume());
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else if (x.Nspin() == 1) { //staggered
#ifdef GPU_STAGGERED_DIRAC
	const int M = siteUnroll ? 3 : 1; // determines how much work per thread to do
	const int reduce_length = siteUnroll ? x.RealLength() : x.Length();
	value = reduce::reduceCuda<doubleN,ReduceType,double2,float2,double2,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, reduce_length/(2*M));
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else { errorQuda("ERROR: nSpin=%d is not supported\n", x.Nspin()); }
    } else if (x.Precision() == QUDA_HALF_PRECISION && z.Precision() == QUDA_DOUBLE_PRECISION) {
      if (x.Nspin() == 4) { //wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC)
	const int M = 12; // determines how much work per thread to do
	value = reduce::reduceCuda<doubleN,ReduceType,double2,short4,double2,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, x.Volume());
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else if (x.Nspin() == 1) { //staggered
#ifdef GPU_STAGGERED_DIRAC
	const int M = 3; // determines how much work per thread to do
	value = reduce::reduceCuda<doubleN,ReduceType,double2,short2,double2,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, x.Volume());
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else { errorQuda("ERROR: nSpin=%d is not supported\n", x.Nspin()); }
    } else if (z.Precision() == QUDA_SINGLE_PRECISION) {
      if (x.Nspin() == 4) { //wilson
#if defined(GPU_WILSON_DIRAC) || defined(GPU_DOMAIN_WALL_DIRAC)
	const int M = 6;
	value = reduce::reduceCuda<doubleN,ReduceType,float4,short4,float4,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, x.Volume());
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else if (x.Nspin() == 1) {//staggered
#ifdef GPU_STAGGERED_DIRAC
	const int M = 3;
	value = reduce::reduceCuda<doubleN,ReduceType,float2,short2,float2,M,Reducer,
	  writeX,writeY,writeZ,writeW,writeV>
	  (a, b, x, y, z, w, v, x.Volume());
#else
	errorQuda("blas has not been built for Nspin=%d fields", x.Nspin());
#endif
      } else { errorQuda("ERROR: nSpin=%d is not supported\n", x.Nspin()); }
      blas::bytes += Reducer<ReduceType,double2,double2>::streams()*(unsigned long long)x.Volume()*sizeof(float);
    }
  } else {
    // we don't have quad precision support on the GPU so use doubleN instead of ReduceType
    if (x.Precision() == QUDA_SINGLE_PRECISION && z.Precision() == QUDA_DOUBLE_PRECISION) {
      Reducer<doubleN, double2, double2> r(a, b);
      value = genericReduce<doubleN,doubleN,float,double,writeX,writeY,writeZ,writeW,writeV,Reducer<doubleN,double2,double2> >(x,y,z,w,v,r);
    } else {
      errorQuda("Precision %d not implemented", x.Precision());
    }
  }

  const int Nreduce = sizeof(doubleN) / sizeof(double);
  reduceDoubleArray((double*)&value, Nreduce);

  return value;
}

} // namespace mixed
