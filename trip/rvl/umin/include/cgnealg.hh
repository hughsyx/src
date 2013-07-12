// cgnealg.H
// created by ADP 1/8/04
// last modified 06/16/04

// Much code is shamelessly stolen from the umin cgne.H
// originally written by WWS, with his permission

/*************************************************************************

Copyright Rice University, 2004.
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, provided that the above copyright notice(s) and this
permission notice appear in all copies of the Software and that both the
above copyright notice(s) and this permission notice appear in supporting
documentation.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization of the
copyright holder.

**************************************************************************/


#ifndef __RVLALG_UMIN_CGNE_H
#define __RVLALG_UMIN_CGNE_H

#include "alg.hh"
#include "terminator.hh"
#include "linop.hh"
#include "table.hh"

using namespace RVLAlg;

namespace RVLUmin {

  using namespace RVL;
  using namespace RVLAlg;    

  /** Single step of conjugate gradient iteration for the normal
      equations.
      
      On construction, internal workspace allocated and initialized.
      Each step updates internal state of CGNEStep object. Since
      solution vector, residual norm, and normal residual norm are
      stored as mutable references to external objects, these external
      objects are updated as well.

      IMPORTANT NOTE: this version of the algorithm assumes that the
      solution vector reference (internal data member x) refers to a
      zero vector on initialization. To accommodate nontrivial initial
      guess, <i>modify the right-hand-side vector</i> (argument _b)
      externally.

      Solution vector (x), iteration count, residual norm, and normal
      residual (gradient) norm are all references to external objects,
      which may be monitored by appropriate terminators to build a
      LoopAlg out of this Algorithm.

      See CGNEAlg for description of a fully functional algorithm
      class, combining this step with a Terminator to make a LoopAlg.
  */
  template<typename Scalar>
  class CGNEStep : public Algorithm {

    typedef typename ScalarFieldTraits<Scalar>::AbsType atype;

  public:

    CGNEStep(LinearOp<Scalar> const & _A,
	     Vector<Scalar> & _x,
	     Vector<Scalar> const & _b,
	     atype & _rnorm, 
	     atype & _nrnorm)
      : A(_A), x(_x), b(_b), rnorm(_rnorm), nrnorm(_nrnorm), r(A.getRange()), s(A.getDomain()),
	q(A.getRange()), p(A.getDomain()) { 
      // NOTE: initial x assumed to be zero vector
      r.copy(b);
      rnorm=r.norm();
      s.zero();
      A.applyAdjOp(r,s);
      p.copy(s);
      nrnorm=s.norm();
      gamma = nrnorm*nrnorm;
    }
      
    /**
       Run a single step of the conjugate gradients for the normal equations
    */
    void run() {
      try {
	//	cerr<<"CGSTEP::run 0\n";
	A.applyOp(p,q);
	//	cerr<<"CGSTEP::run 1\n";
	atype qtq = q.normsq();
	atype absalpha;
	if (ProtectedDivision<atype>(gamma,qtq,absalpha)) {
	  RVLException e;
	  e<<"Error: CGNEStep::run() from ProtectedDivision: alpha\n";
	  throw e;
	}
	// can use q for workspace since it is not needed again until
	// reinitialized
	//	cerr<<"CGSTEP::run 2\n";

	Scalar alpha=absalpha;
	x.linComb(alpha,p);
	r.linComb(-alpha,q);
	//	cerr<<"CGSTEP::run 3\n";

	A.applyAdjOp(r,s);
	//	cerr<<"CGSTEP::run 4\n";

	atype newgamma = s.normsq();
	atype absbeta;
	if (ProtectedDivision<atype>(newgamma,gamma,absbeta)) {
	  RVLException e;
	  e<<"Error: CGNEStep::run() from ProtectedDivision: beta\n";
	  throw e;
	}
	//	cerr<<"CGSTEP::run 5\n";

	Scalar beta = absbeta;
	p.linComb(ScalarFieldTraits<Scalar>::One(),s,beta);
	gamma=newgamma;
	rnorm=r.norm();
	nrnorm=sqrt(gamma);
	//	cerr<<"CGSTEP::run 6\n";

      }
      catch (RVLException & e) {
	e<<"\ncalled from CGNEStep::run()\n";
	throw e;
      }
     
    }

    ~CGNEStep() {}

  private:

    // references to external objects
    LinearOp<Scalar> const & A;
    Vector<Scalar> & x;
    Vector<Scalar> const & b;
    atype & rnorm;
    atype & nrnorm;

    // need four work vectors and one scalar as persistent object data
    Vector<Scalar> r;
    Vector<Scalar> s;
    Vector<Scalar> q;
    Vector<Scalar> p;
    atype gamma;
  };
  
  /** Conjugate gradient algorithm - efficient implementation for
      normal equations
      \f[ A^{\prime} A x = A^{\prime} b\f]
      for solving the linear least squares problem
      \f[ \min_{x} \vert A x - b \vert^2 \f].

      This is Algorithm CGLS as stated in Paige and
      Saunders, ACM TOMS vol. 8 pp. 43-72 1982 (see p. 57). We use
      variable names aping Paige and Saunder's notation insofar as
      possible. 

      Structure and function: Combines CGNEStep with a Terminator
      which displays iteration count, residual norm, and normal
      residual norm on output stream (constructor argument _str), and
      terminates if iteration count exceeds max or residual norm or
      normal residual norm fall below threshhold (default =
      10*sqrt(macheps)). Also terminates if the length of the solution
      vector exceeds a specified bound (maxstep argument to
      constructor). In this latter case, the computed step is
      projected onto the ball of radius maxstep centered at the
      initial estimate. This maximum step limit and projection turns
      the algorithm into an approximate trust region subproblem
      solver, similar to Steihaug-Toint. The default choice of maxstep
      is the max Scalar, which effectively turns off the trust region
      feature.

      Usage: construct CGNEAlg object by supplying appropriate
      arguments to constructor. On return from constructor, solution
      vector initialized to zero, residual norm to norm of RHS, and
      normal residual norm to norm of image of RHS under adjoint of
      operator. Then call run() method. Progress of iteration written
      on output unit. On return from run(), solution vector stores
      final estimate of solution, and residual norm and normal
      residual norm scalars have corresponding values.

      Typical Use: see <a href="../../testsrc/testcgne.cc">
      functional test source</a>.

      IMPORTANT NOTE: This class is also an RVLAlg::Terminator
      subclass. Its query() method returns true if the trust region
      constraint was binding (raw LS solution larger than trust
      radius), else false.

      IMPORTANT NOTE: The solution vector and residual and normal
      residual scalars are external objects, for which this algorithm
      stores mutable references. These objects are updated by
      constructing a CGNEAlg object, and by calling its run() method.

      IMPORTANT NOTE: this version of the algorithm initializes the
      solution vector to zero. To accommodate nontrivial initial
      guess, <i>modify the right-hand-side vector</i> (argument _rhs)
      externally.

      IMPORTANT NOTE: in order that this algorithm function properly
      for complex scalar types, a careful distinction is maintained
      between the main template parameter (Scalar) type and its
      absolute value type. All of the scalars appearing in the
      algorithm are actually of the latter type.

      See constructor documentation for description of parameters.


  */

  template<typename Scalar>
  class CGNEAlg: public Algorithm, public Terminator {

    typedef typename ScalarFieldTraits<Scalar>::AbsType atype;

  public:

    /** Constructor

	@param _x - mutable reference to solution vector (external),
	initialized to zero vector on construction, estimated solution
	on return from CGNEAlg::run().

	@param _inA - const reference to LinearOp (external) defining
	problem

	@param _rhs - const reference to RHS or target vector
	(external)

	@param _rnorm - mutable reference to residual norm scalar
	(external), initialized to norm of RHS on construction, norm
	of estimated residual at solution on return from
	CGNEAlg::run()

	@param _nrnorm - mutable reference to normal residual (least
	squares gradient) norm scalar (external), initialized to morm
	of image of RHS under adjoint of problem LinearOp on
	construction, norm of estimated normal residual at solution on
	return from CGNEAlg::run()

	@param _rtol - stopping threshold for residual norm, default
	value = 100.0*macheps

	@param _nrtol - stopping threshold for normal residual norm,
	default value = 100.0*macheps

	@param _maxcount - max number of iterations permitted, default
	value = 10

	@param _maxstep - max permitted step length (trust radius),
	default value = max absval scalar (which makes the trust
	region feature inactive)

	@param _str - output stream

     */
    CGNEAlg(RVL::Vector<Scalar> & _x, 
	    LinearOp<Scalar> const & _inA, 
	    Vector<Scalar> const & _rhs, 
	    atype & _rnorm,
	    atype & _nrnorm,
	    atype _rtol = 100.0*numeric_limits<atype>::epsilon(),
	    atype _nrtol = 100.0*numeric_limits<atype>::epsilon(),
	    int _maxcount = 10,
	    atype _maxstep = numeric_limits<atype>::max(),
	    ostream & _str = cout)
    : inA(_inA), 
      x(_x), 
      rhs(_rhs), 
      rnorm(_rnorm), 
      nrnorm(_nrnorm), 
      rtol(_rtol), 
      nrtol(_nrtol), 
      maxstep(_maxstep), 
      maxcount(_maxcount), 
      count(0), 
      proj(false), 
      str(_str), 
      step(inA,x,rhs,rnorm,nrnorm) 
    { x.zero(); }

    ~CGNEAlg() {}

    bool query() { return proj; }

    void run() { 
      // terminator for CGNE iteration
      vector<string> names(2);
      vector<atype *> nums(2);
      vector<atype> tols(2);
      names[0]="Residual Norm"; nums[0]=&rnorm; tols[0]=rtol;
      names[1]="Gradient Norm"; nums[1]=&nrnorm; tols[1]=nrtol;
      str<<"========================== BEGIN CGNE =========================\n";
      VectorCountingThresholdIterationTable<atype> stop1(maxcount,names,nums,tols,str);
      stop1.init();
      // terminator for Trust Region test and projection
      //      BallProjTerminator<Scalar> stop2(x,maxstep,str);
      BallProjTerminator<Scalar> stop2(x,maxstep,str);
      // terminate if either
      OrTerminator stop(stop1,stop2);
      // loop
      LoopAlg doit(step,stop);
      doit.run();
      // must recompute residual if scaling occured 
      proj = stop2.query();
      if (proj) {
	Vector<Scalar> temp(inA.getRange());
	inA.applyOp(x,temp);
	temp.linComb(-1.0,rhs);
	rnorm=temp.norm();
	Vector<Scalar> temp1(inA.getDomain());
	inA.applyAdjOp(temp,temp1);
	nrnorm=temp1.norm();
      }
      count = stop1.getCount();
      str<<"=========================== END CGNE ==========================\n";
    }

    int getCount() const { return count; }

  private:

    LinearOp<Scalar> const & inA;  // operator
    Vector<Scalar> & x;            // state - solution vector
    Vector<Scalar> const & rhs;    // reference to rhs
    atype & rnorm;                 // residual norm
    atype & nrnorm;                // gradient norm
    atype rtol;                    // tolerance residual norm
    atype nrtol;                   // tolerance gradient norm 
    atype maxstep;                 // upper bound for net step x-x0
    int maxcount;                  // upper bound for iteration count
    int count;                     // actual iteration count
    mutable bool proj;             // whether step is projected onto TR boundary
    ostream & str;                 // stream for report output
    CGNEStep<Scalar> step;         // single step of CGNE

    // disable default, copy constructors
    CGNEAlg();
    CGNEAlg(CGNEAlg<Scalar> const &);

  };

  /** data class for CGNE policy
  */
  template<typename Scalar>
  class CGNEPolicyData {

    typedef typename ScalarFieldTraits<Scalar>::AbsType atype;
  
  public:

    atype rtol;
    atype nrtol;
    atype Delta;
    int maxcount;
    bool verbose;

    CGNEPolicyData(atype _rtol = numeric_limits<atype>::max(),
		   atype _nrtol = numeric_limits<atype>::max(),
		   atype _Delta = numeric_limits<atype>::max(),
		   int _maxcount = 0,
		   bool _verbose = false)
      : rtol(_rtol), nrtol(_nrtol), Delta(_Delta), maxcount(_maxcount), verbose(_verbose) {}
      
    CGNEPolicyData(CGNEPolicyData<Scalar> const & a) 
      : rtol(a.rtol), nrtol(a.nrtol), Delta(a.Delta), maxcount(a.maxcount), verbose(a.verbose) {}

    ostream & write(ostream & str) const {
      str<<"\n";
      str<<"==============================================\n";
      str<<"CGNEPolicyData: \n";
      str<<"rtol      = "<<rtol<<"\n";
      str<<"nrtol     = "<<nrtol<<"\n";
      str<<"Delta     = "<<Delta<<"\n";
      str<<"maxcount  = "<<maxcount<<"\n";
      str<<"verbose   = "<<verbose<<"\n";
      str<<"==============================================\n";
      return str;
    }
  };

  /** policy class for creation of CGNEAlg in trust region solver and 
      any other algorithm needing a least squares solver component - build
      method creates CGNEAlg with these attributes:

      rtol     = residual threshhold for convergence
      nrtol    = normal residual (LS gradient) tolerance for convergence
      Delta    = trust radius - truncate iteration when reached
      maxcount = max number of iterations permitted

      Default values set to cause immediate return from CGNEAlg::run.

      Other attributes are arguments of build method.

      Conforms to specifications described in TRGNAlg docs.

      Usage: use as policy type, i.e. class name as template parameter
      to TRGNAlg constructor. After construction of TRGNAlg, which IS
      a CGNEPolicy by inheritance, call the assign method on it to set
      rtol, nrtol, and maxcount, BEFORE calling TRGNAlg::run() - else
      you will get immediate termination, as intended.
  */

  template<typename Scalar> 
  class CGNEPolicy {

    typedef typename ScalarFieldTraits<Scalar>::AbsType atype;

  public:
    /**
       build method - see TRGNAlg specs

       @param x - solution vector, initialize to zero on input,
       estimated solution on output 

       @param A - Linear Operator of least squares problem

       @param d - data vector of least squares problem

       @param rnorm - reference to residual norm scalar, norm of RHS
       on input, of residual on output

       @param nrnorm - reference to normal residual norm scalar, norm
       of normal residual (least squares gradient) on input, updated
       to estimated solution on output
       
       @param str - verbose output stream
    */
    CGNEAlg<Scalar> * build(Vector<Scalar> & x, 
			    LinearOp<Scalar> const & A,
			    Vector<Scalar> const & d,
			    atype & rnorm,
			    atype & nrnorm,
			    ostream & str) const {
      if (verbose) 
	return new CGNEAlg<Scalar>(x,A,d,rnorm,nrnorm,rtol,nrtol,maxcount,Delta,str);
      else
	return new CGNEAlg<Scalar>(x,A,d,rnorm,nrnorm,rtol,nrtol,maxcount,Delta,nullstr);
    }

    /** post-construction initialization
	@param _rtol - residual norm stopping threshhold
	@param _nrtol - normal residual (LS gradient) norm stopping threshhold
	@param _maxcount - max number of permitted iterations
    */
    void assign(atype _rtol, atype _nrtol, atype _Delta, int _maxcount, bool _verbose) {
      rtol=_rtol; nrtol=_nrtol; Delta=_Delta; maxcount=_maxcount; verbose=_verbose;
    }

    /** parameter table overload */
    void assign(Table const & t) {
      rtol=getValueFromTable<atype>(t,"CGNE_ResTol");
      nrtol=getValueFromTable<atype>(t,"CGNE_GradTol"); 
      Delta=getValueFromTable<atype>(t,"TR_Delta");
      maxcount=getValueFromTable<int>(t,"CGNE_MaxItn"); 
      verbose=getValueFromTable<bool>(t,"CGNE_Verbose");
    }

    /** data struct overload */
    void assign(CGNEPolicyData<Scalar> const & s) {
      rtol=s.rtol;
      nrtol=s.nrtol;
      Delta=s.Delta;
      maxcount=s.maxcount;
      verbose=s.verbose;
    }

    /** only Delta need be changed repeatedly, as opposed
	to set post-construction. Simplest way to do this - make
	it public
    */
    mutable atype Delta;

    /** main constructor - acts as default. Default values of
	parameters set to result in immediate return, no
	iteration. Note that policy design requires that default
	construction must be valid, and all run-time instance data be
	initiated post-construction, in this case by the assign
	function, to be called by drivers of user classes (subclassed
	from this and with this as template param).
    */
    CGNEPolicy(atype _rtol = numeric_limits<atype>::max(),
	       atype _nrtol = numeric_limits<atype>::max(),
	       atype _Delta = numeric_limits<atype>::max(),
	       int _maxcount = 0,
	       bool _verbose = true)
      : rtol(_rtol), nrtol(_nrtol), Delta(_Delta), maxcount(_maxcount), verbose(_verbose), nullstr(0) {}

    CGNEPolicy(CGNEPolicy<Scalar> const & p)
      : rtol(p.rtol), 
	nrtol(p.nrtol), 
	Delta(p.Delta), 
	maxcount(p.maxcount), 
	verbose(p.verbose), 
	nullstr(0) {}
      
  private:
    mutable atype rtol;
    mutable atype nrtol;
    mutable int maxcount;
    mutable bool verbose;
    mutable std::ostream nullstr;
  };
}

#endif








