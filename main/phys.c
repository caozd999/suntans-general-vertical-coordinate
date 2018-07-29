/*
 * File: phys.c
 * Author: Oliver B. Fringer
 * Institution: Stanford University
 * --------------------------------
 * This file contains physically-based functions.
 *
 * Copyright (C) 2005-2006 The Board of Trustees of the Leland Stanford Junior 
 * University. All Rights Reserved.
 *
 */
#include "suntans.h"
#include "phys.h"
#include "grid.h"
#include "util.h"
#include "initialization.h"
#include "memory.h"
#include "turbulence.h"
#include "boundaries.h"
#include "check.h"
#include "scalars.h"
#include "timer.h"
#include "profiles.h"
#include "state.h"
#include "diffusion.h"
#include "sources.h"
#include "mynetcdf.h"
#include "met.h"
#include "age.h"
#include "physio.h"
#include "merge.h"
#include "sediments.h"
#include "marsh.h"
#include "vertcoordinate.h"
#include "culvert.h"
#include "wave.h"
#include "subgrid.h"
#include "sendrecv.h"
/*
 * Private Function declarations.
 *
 */

static void UPredictor(gridT *grid, physT *phys, 
    propT *prop, int myproc, int numprocs, MPI_Comm comm);
static void Corrector(REAL **qc, gridT *grid, physT *phys, propT *prop, int myproc, 
    int numprocs, MPI_Comm comm);
static void ComputeQSource(REAL **src, gridT *grid, physT *phys, propT *prop, 
    int myproc, int numprocs);
static void CGSolve(gridT *grid, physT *phys, propT *prop, 
    int myproc, int numprocs, MPI_Comm comm);
static void HPreconditioner(REAL *x, REAL *y, gridT *grid, physT *phys, propT *prop);
static void HCoefficients(REAL *coef, REAL *fcoef, gridT *grid, physT *phys, 
    propT *prop);
static void CGSolveQ(REAL **q, REAL **src, REAL **c, gridT *grid, physT *phys, 
    propT *prop, 
    int myproc, int numprocs, MPI_Comm comm);
static void ConditionQ(REAL **x, gridT *grid, physT *phys, propT *prop, int myproc, 
    MPI_Comm comm);
static void Preconditioner(REAL **x, REAL **xc, REAL **coef, gridT *grid, physT *phys, 
    propT *prop);
static void GuessQ(REAL **q, REAL **wold, REAL **w, gridT *grid, physT *phys, 
    propT *prop, int myproc, int numprocs, MPI_Comm comm);
static void GSSolve(gridT *grid, physT *phys, propT *prop, 
    int myproc, int numprocs, MPI_Comm comm);
static REAL InnerProduct(REAL *x, REAL *y, gridT *grid, int myproc, int numprocs, 
    MPI_Comm comm);
static REAL InnerProduct3(REAL **x, REAL **y, gridT *grid, int myproc, int numprocs, 
    MPI_Comm comm);
static void OperatorH(REAL *x, REAL *y, REAL *coef, REAL *fcoef, gridT *grid, 
    physT *phys, propT *prop);
static void OperatorQC(REAL **coef, REAL **fcoef, REAL **x, REAL **y, REAL **c, 
    gridT *grid, physT *phys, propT *prop);
static void QCoefficients(REAL **coef, REAL **fcoef, REAL **c, gridT *grid, 
    physT *phys, propT *prop);
static void OperatorQ(REAL **coef, REAL **x, REAL **y, REAL **c, gridT *grid, 
    physT *phys, propT *prop);
static void Continuity(REAL **w, gridT *grid, physT *phys, propT *prop);
void Continuity(REAL **w, gridT *grid, physT *phys, propT *prop);
static void EddyViscosity(gridT *grid, physT *phys, propT *prop, REAL **wnew, 
    MPI_Comm comm, int myproc);
static void HorizontalSource(gridT *grid, physT *phys, propT *prop,
    int myproc, int numprocs, MPI_Comm comm);
static void StoreVariables(gridT *grid, physT *phys);
static void NewCells(gridT *grid, physT *phys, propT *prop);
static void WPredictor(gridT *grid, physT *phys, propT *prop,
    int myproc, int numprocs, MPI_Comm comm);
void ComputeUC(REAL **ui, REAL **vi, physT *phys, gridT *grid, int myproc, interpolation interp, int kinterp, int subgridmodel);
static void ComputeUCPerot(REAL **u, REAL **uc, REAL **vc, REAL *h, int kinterp, int subgridmodel, gridT *grid);
static void ComputeUCLSQ(REAL **u, REAL **uc, REAL **vc, gridT *grid, physT *phys);
static void ComputeUCRT(REAL **ui, REAL **vi, physT *phys, gridT *grid, int myproc);
static void ComputeNodalVelocity(physT *phys, gridT *grid, interpolation interp, int myproc);
static void  ComputeTangentialVelocity(physT *phys, gridT *grid, interpolation ninterp, interpolation tinterp,int myproc);
static void  ComputeQuadraticInterp(REAL x, REAL y, int ic, int ik, REAL **uc, 
    REAL **vc, physT *phys, gridT *grid, interpolation ninterp, 
    interpolation tinterp, int myproc);
static void ComputeRT0Velocity(REAL* tempu, REAL* tempv, REAL e1n1, REAL e1n2, 
    REAL e2n1, REAL e2n2, REAL Uj1, REAL Uj2);
static void BarycentricCoordsFromCartesian(gridT *grid, int cell, 
    REAL x, REAL y, REAL* lambda);
static void BarycentricCoordsFromCartesianEdge(gridT *grid, int cell, 
    REAL x, REAL y, REAL* lambda);
static REAL UFaceFlux(int j, int k, REAL **phi, REAL **u, gridT *grid, REAL dt, 
    int method);
static REAL HFaceFlux(int j, int k, REAL *phi, REAL **u, gridT *grid, REAL dt, 
    int method);
//static void SetFluxHeight(gridT *grid, physT *phys, propT *prop);
void SetFluxHeight(gridT *grid, physT *phys, propT *prop);
static void GetMomentumFaceValues(REAL **uface, REAL **ui, REAL **boundary_ui, REAL **U, gridT *grid, physT *phys, propT *prop, MPI_Comm comm, int myproc, int nonlinear);
static void getTsurf(gridT *grid, physT *phys);
static void getchangeT(gridT *grid, physT *phys);
// added drag function Yun Zhang
static void InterpDrag(gridT *grid, physT *phys, propT *prop, int myproc);
static void OutputDrag(gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm);
/*
 * Function: AllocatePhysicalVariables
 * Usage: AllocatePhysicalVariables(grid,phys,prop);
 * -------------------------------------------------
 * This function allocates space for the physical arrays but does not
 * allocate space for the grid as this has already been allocated.
 *
 */
void AllocatePhysicalVariables(gridT *grid, physT **phys, propT *prop)
{
  int flag=0, i, j, jptr, ib, Nc=grid->Nc, Ne=grid->Ne, Np=grid->Np, nf, k;

  // allocate physical structure
  *phys = (physT *)SunMalloc(sizeof(physT),"AllocatePhysicalVariables");

  // allocate  variables in plan
  (*phys)->u = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->uc = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->vc = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->wc = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");

  // new variables for higher-order interpolation following Wang et al 2011
  (*phys)->nRT1u = (REAL ***)SunMalloc(Np*sizeof(REAL **),"AllocatePhysicalVariables");
  (*phys)->nRT1v = (REAL ***)SunMalloc(Np*sizeof(REAL **),"AllocatePhysicalVariables");
  (*phys)->nRT2u = (REAL **)SunMalloc(Np*sizeof(REAL*),"AllocatePhysicalVariables");
  (*phys)->nRT2v = (REAL **)SunMalloc(Np*sizeof(REAL*),"AllocatePhysicalVariables");
  (*phys)->tRT1 = (REAL **)SunMalloc(Ne*sizeof(REAL*),"AllocatePhysicalVariables");
  (*phys)->tRT2 = (REAL **)SunMalloc(Ne*sizeof(REAL*),"AllocatePhysicalVariables");

  // allocate rest of variables in plan
  (*phys)->uold = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->vold = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->D = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->utmp = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->u_old = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->u_old2 = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->ut = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_U = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_U2 = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables"); //AB3


  // for each variable in plan consider the number of layers it affects
  for(j=0;j<Ne;j++) {
    // the following line seems somewhat dubious...
    if(grid->Nkc[j] < grid->Nke[j]) {
      printf("Error!  Nkc(=%d)<Nke(=%d) at edge %d\n",grid->Nkc[j],grid->Nke[j],j);
      flag = 1;
    }
    // allocate memory for the max (cell-centered) quanity on the edge (from definition above)
    (*phys)->u[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->utmp[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->u_old[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->u_old2[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->ut[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_U[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_U2[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");//AB3
    /* new interpolation variables */
    // loop over the edges (Nkc vs Nke since for cells Nkc < ik < Nke there should be 0 velocity
    // on face to prevent mass from leaving the system)
    (*phys)->tRT1[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->tRT2[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
  }
  // if we have an error quit MPI
  if(flag) {
    MPI_Finalize();
    exit(0);
  }

  // user defined variable
  (*phys)->user_def_nc = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->user_def_nc_nk = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  for(i=0;i<Nc;i++)
    (*phys)->user_def_nc_nk[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");

  // cell-centered physical variables in plan (no vertical direction)
  (*phys)->h = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->dhdt = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->hcorr = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->active = (unsigned char *)SunMalloc(Nc*sizeof(char),"AllocatePhysicalVariables");
  (*phys)->hold = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->h_old = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->htmp = (REAL *)SunMalloc(10*Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->htmp2 = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->htmp3 = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->hcoef = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->hfcoef = (REAL *)SunMalloc(grid->maxfaces*Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->Tsurf = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->dT = (REAL *)SunMalloc(Nc*sizeof(REAL),"AllocatePhysicalVariables");

  // cell-centered values that are also depth-varying
  (*phys)->w = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->wnew = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->wtmp = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->w_old = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->w_old2 = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->w_im = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_W = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_W2 = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables"); //AB3
  (*phys)->q = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->qc = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->qtmp = (REAL **)SunMalloc(grid->maxfaces*Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->s = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->T = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->s_old = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->T_old = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Ttmp = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->s0 = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->rho = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_R = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Cn_T = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->stmp = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->stmp2 = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->stmp3 = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->nu_tv = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->kappa_tv = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->nu_lax = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  if(prop->turbmodel>=1) {
    (*phys)->qT = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
    (*phys)->lT = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
    (*phys)->qT_old = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
    (*phys)->lT_old = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
    (*phys)->Cn_q = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
    (*phys)->Cn_l = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  }
  (*phys)->tau_T = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->tau_B = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->CdT = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->CdB = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");

  //add phys->z0T and phys->z0B
  (*phys)->z0B = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->z0T = (REAL *)SunMalloc(Ne*sizeof(REAL),"AllocatePhysicalVariables");

  /* new interpolation variables */
  // loop over the nodes
  for(i=0; i < Np; i++) {
    // most complex one...
    (*phys)->nRT1u[i] = (REAL **)SunMalloc(grid->Nkp[i]*sizeof(REAL *),
        "AllocatePhysicalVariables");
    (*phys)->nRT1v[i] = (REAL **)SunMalloc(grid->Nkp[i]*sizeof(REAL *),
        "AllocatePhysicalVariables");
    // loop over all the cell over depth and allocate (note that for rapidly varying
    // bathymetery this will result in too much memory used in some places)
    for(k=0; k < grid->Nkp[i]; k++) {
      (*phys)->nRT1u[i][k] = (REAL *)SunMalloc(grid->numpcneighs[i]*sizeof(REAL),
          "AllocatePhysicalVariables");
      (*phys)->nRT1v[i][k] = (REAL *)SunMalloc(grid->numpcneighs[i]*sizeof(REAL),
          "AllocatePhysicalVariables");
    }
    // simpler one
    (*phys)->nRT2u[i] = (REAL *)SunMalloc(grid->Nkp[i]*sizeof(REAL),
        "AllocatePhysicalVariables");
    (*phys)->nRT2v[i] = (REAL *)SunMalloc(grid->Nkp[i]*sizeof(REAL),
        "AllocatePhysicalVariables");
  }
  // Netcdf write variables
  (*phys)->tmpvar = (REAL *)SunMalloc(grid->Nc*grid->Nkmax*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->tmpvarE = (REAL *)SunMalloc(grid->Ne*grid->Nkmax*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->tmpvarW = (REAL *)SunMalloc(grid->Nc*(grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
 
  // for each cell allocate memory for the number of layers at that location
  for(i=0;i<Nc;i++) {
    (*phys)->uc[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->vc[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->wc[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->uold[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->vold[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->w[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->wnew[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->wtmp[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->w_old[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->w_old2[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->w_im[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_W[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_W2[i] = (REAL *)SunMalloc((grid->Nk[i]+1)*sizeof(REAL),"AllocatePhysicalVariables"); //AB3
    (*phys)->q[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->qc[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    for(nf=0;nf<grid->nfaces[i];nf++)
      (*phys)->qtmp[i*grid->maxfaces+nf] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->s[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->T[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->s_old[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->T_old[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Ttmp[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->s0[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->rho[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_R[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->Cn_T[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    if(prop->turbmodel>=1) {
      (*phys)->Cn_q[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
      (*phys)->Cn_l[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
      (*phys)->qT[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
      (*phys)->lT[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
      (*phys)->qT_old[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
      (*phys)->lT_old[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    }
    (*phys)->stmp[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->stmp2[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->stmp3[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->nu_tv[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->kappa_tv[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->nu_lax[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
  }
 
  // allocate boundary value memory
  (*phys)->boundary_u = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_v = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_w = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_s = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_T = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_rho = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_tmp = (REAL **)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->boundary_h = (REAL *)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->boundary_flag = (REAL *)SunMalloc((grid->edgedist[5]-grid->edgedist[2])*sizeof(REAL),"AllocatePhysicalVariables");
  // allocate over vertical layers
  for(jptr=grid->edgedist[2];jptr<grid->edgedist[5];jptr++) {
    j=grid->edgep[jptr];

    (*phys)->boundary_u[jptr-grid->edgedist[2]] = (REAL *)SunMalloc(grid->Nke[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_v[jptr-grid->edgedist[2]] = (REAL *)SunMalloc(grid->Nke[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_w[jptr-grid->edgedist[2]] = (REAL *)SunMalloc((grid->Nke[j]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_s[jptr-grid->edgedist[2]] = (REAL *)SunMalloc(grid->Nke[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_T[jptr-grid->edgedist[2]] = (REAL *)SunMalloc(grid->Nke[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_tmp[jptr-grid->edgedist[2]] = (REAL *)SunMalloc((grid->Nke[j]+1)*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->boundary_rho[jptr-grid->edgedist[2]] = (REAL *)SunMalloc(grid->Nke[j]*sizeof(REAL),"AllocatePhysicalVariables");
    }

  // allocate coefficients
  (*phys)->ap = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->am = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->bp = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->bm = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->a = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->b = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->c = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->d = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");

  // Allocate for the face scalar
  (*phys)->SfHp = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->SfHm = (REAL **)SunMalloc(Ne*sizeof(REAL *),"AllocatePhysicalVariables");
  for(j=0;j<Ne;j++) {
    (*phys)->SfHp[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->SfHm[j] = (REAL *)SunMalloc(grid->Nkc[j]*sizeof(REAL),"AllocatePhysicalVariables");
  }

  // Allocate for TVD schemes
  (*phys)->Cp = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->Cm = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->rp = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->rm = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");

  (*phys)->wp = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");
  (*phys)->wm = (REAL *)SunMalloc((grid->Nkmax+1)*sizeof(REAL),"AllocatePhysicalVariables");

  (*phys)->gradSx = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->gradSy = (REAL **)SunMalloc(Nc*sizeof(REAL *),"AllocatePhysicalVariables");
  for(i=0;i<Nc;i++) {
    (*phys)->gradSx[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
    (*phys)->gradSy[i] = (REAL *)SunMalloc(grid->Nk[i]*sizeof(REAL),"AllocatePhysicalVariables");
  }


  // Allocate for least squares velocity fitting
  (*phys)->A = (REAL **)SunMalloc(grid->maxfaces*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->AT = (REAL **)SunMalloc(2*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->Apr = (REAL **)SunMalloc(2*sizeof(REAL *),"AllocatePhysicalVariables");
  (*phys)->bpr = (REAL *)SunMalloc(2*sizeof(REAL),"AllocatePhysicalVariables");
  for(i=0;i<grid->maxfaces;i++){
      (*phys)->A[i] = (REAL *)SunMalloc(2*sizeof(REAL),"AllocatePhysicalVariables");  
  }
  for(i=0;i<2;i++){
      (*phys)->AT[i] = (REAL *)SunMalloc(grid->maxfaces*sizeof(REAL),"AllocatePhysicalVariables");  
      (*phys)->Apr[i] = (REAL *)SunMalloc(2*sizeof(REAL),"AllocatePhysicalVariables");  
  }

}

/*
 * Function: FreePhysicalVariables
 * Usage: FreePhysicalVariables(grid,phys,prop);
 * ---------------------------------------------
 * This function frees all space allocated in AllocatePhysicalVariables
 *
 */
void FreePhysicalVariables(gridT *grid, physT *phys, propT *prop)
{
  int i, j, Nc=grid->Nc, Ne=grid->Ne, Np=grid->Np, nf;

  /* free variables for higher-order interpolation */
  // note that this isn't even currently called!
  // over each node
  for(i=0; i < Np; i++) {
    free(phys->nRT2u[i]);
    free(phys->nRT2v[i]);
    // over each layer
    for(j=0; j < grid->Nkp[i]; j++) {
      // free the memory
      free(phys->nRT1u[i][j]);
      free(phys->nRT1v[i][j]);
    }
  }
  // over each edge
  for(i=0; i < Ne; i++) {
    free(phys->tRT1[i]);
    free(phys->tRT2[i]);
  }

  // free all the arrays over depth for edge-oriented
  for(j=0;j<Ne;j++) {
    free(phys->u[j]);
    free(phys->utmp[j]);
    free(phys->u_old[j]);
    free(phys->u_old2[j]);
    free(phys->ut[j]);
    free(phys->Cn_U[j]);
    free(phys->Cn_U2[j]); //AB3
  }

  // free all the arrays over depth for cell-oriented
  for(i=0;i<Nc;i++) {
    free(phys->uc[i]);
    free(phys->vc[i]);
    free(phys->wc[i]);
    free(phys->uold[i]);
    free(phys->vold[i]);
    free(phys->w[i]);
    free(phys->wnew[i]);
    free(phys->wtmp[i]);
    free(phys->w_old[i]);
    free(phys->w_old2[i]);
    free(phys->w_im[i]);
    free(phys->Cn_W[i]);
    free(phys->Cn_W2[i]); //AB3
    free(phys->q[i]);
    free(phys->qc[i]);
    for(nf=0;nf<grid->maxfaces;nf++)
      free(phys->qtmp[i*grid->maxfaces+nf]);
    free(phys->s[i]);
    free(phys->T[i]);
    free(phys->s_old[i]);
    free(phys->T_old[i]);
    free(phys->s0[i]);
    free(phys->rho[i]);
    free(phys->Cn_R[i]);
    free(phys->Cn_T[i]);
    if(prop->turbmodel>=1) {
      free(phys->Cn_q[i]);
      free(phys->Cn_l[i]);
      free(phys->qT[i]);
      free(phys->lT[i]);
      free(phys->qT_old[i]);
      free(phys->lT_old[i]);
    }
    free(phys->stmp[i]);
    free(phys->stmp2[i]);
    free(phys->stmp3[i]);
    free(phys->nu_tv[i]);
    free(phys->kappa_tv[i]);
    free(phys->nu_lax[i]);
    free(phys->user_def_nc_nk[i]);
  }
  free(phys->user_def_nc);
  free(phys->user_def_nc_nk);
  free(phys->h);
  free(phys->hcorr);
  free(phys->htmp);
  free(phys->htmp2);
  free(phys->htmp3);
  free(phys->h_old);
  free(phys->hold);
  free(phys->hcoef);
  free(phys->hfcoef);
  free(phys->uc);
  free(phys->vc);
  free(phys->wc);
  free(phys->w);
  free(phys->wnew);
  free(phys->wtmp);
  free(phys->w_old);
  free(phys->w_old2);
  free(phys->w_im);
  free(phys->Cn_W);
  free(phys->Cn_W2); //AB3
  free(phys->q);
  free(phys->qc);
  free(phys->qtmp);
  free(phys->s);
  free(phys->T);
  free(phys->s_old);
  free(phys->T_old);
  free(phys->s0);
  free(phys->rho);
  free(phys->Cn_R);
  free(phys->Cn_T);
  if(prop->turbmodel>=1) {
    free(phys->Cn_q);
    free(phys->Cn_l);
    free(phys->qT);
    free(phys->lT);
    free(phys->qT_old);
    free(phys->lT_old);
  }  
  free(phys->stmp);
  free(phys->stmp2);
  free(phys->stmp3);
  free(phys->nu_tv);
  free(phys->kappa_tv);
  free(phys->nu_lax);
  free(phys->tau_T);
  free(phys->tau_B);
  free(phys->CdT);
  free(phys->CdB);
  free(phys->z0T);
  free(phys->z0B);
  free(phys->u);
  free(phys->D);
  free(phys->utmp);
  free(phys->u_old);
  free(phys->u_old2);
  free(phys->ut);
  free(phys->Cn_U);
  free(phys->Cn_U2);
  free(phys->ap);
  free(phys->am);
  free(phys->bp);
  free(phys->bm);
  free(phys->a);
  free(phys->b);
  free(phys->c);
  free(phys->d);

  // Free the horizontal facial scalar  
  for(j=0;j<Ne;j++) {
    free( phys->SfHp[j] );
    free( phys->SfHm[j] );
  }
  free(phys->SfHp);
  free(phys->SfHm);

  // Free the variables for TVD scheme
  free(phys->Cp);
  free(phys->Cm);
  free(phys->rp);
  free(phys->rm);
  free(phys->wp);
  free(phys->wm);

  free(phys->gradSx);
  free(phys->gradSy);

  free(phys);
}

/*
 * Function: InitializePhyiscalVariables
 * Usage: InitializePhyiscalVariables(grid,phys,prop,myproc,comm);
 * ---------------------------------------------------------------
 * This function initializes the physical variables by calling
 * the routines defined in the file initialize.c
 *
 */
void InitializePhysicalVariables(gridT *grid, physT *phys, propT *prop, int myproc, MPI_Comm comm)
{
  int i, j, k, ktop, Nc=grid->Nc,nc1,nc2;
  REAL z, *stmp;
  REAL *ncscratch;
  int Nci, Nki, T0;

  prop->nstart=0;
  prop->n=prop->nstart;
  // Initialise the netcdf time
  prop->toffSet = getToffSet(prop->basetime,prop->starttime);
  prop->nctime = prop->toffSet*86400.0 + prop->nstart*prop->dt;

  if (prop->readinitialnc>0){
    ReadInitialNCcoord(prop,grid,&Nci,&Nki,&T0,myproc);

    printf("myproc: %d, Nci: %d, Nki: %d, T0: %d\n",myproc,Nci,Nki,T0);

    // Initialise a scratch variable for reading arrays
    ncscratch = (REAL *)SunMalloc(Nki*Nci*sizeof(REAL),"InitializePhysicalVariables");

  }

  // Need to update the vertical grid and fix any cells in which
  // dzz is too small when h=0.
  if(prop->vertcoord==1 || prop->vertcoord==5)
   UpdateDZ(grid,phys,prop, -1);

  // Initialize the free surface
  if (prop->readinitialnc){
     ReturnFreeSurfaceNC(prop,phys,grid,ncscratch,Nci,T0,myproc);
  }else{
    for(i=0;i<Nc;i++) {
      phys->dhdt[i]=0;
      phys->h[i]=ReturnFreeSurface(grid->xv[i],grid->yv[i],grid->dv[i]);
      if(phys->h[i]<-grid->dv[i] + DRYCELLHEIGHT){ 
        phys->h[i]=-grid->dv[i] + DRYCELLHEIGHT;
        phys->active[i]=0;  
      }
    }
  }
  // Need to update the vertical grid after updating the free surface.
  // The 1 indicates that this is the first call to UpdateDZ
  if(prop->vertcoord!=1){
   if(prop->vertcoord==5)
     UpdateDZ(grid,phys,prop, 1);
   VertCoordinateBasic(grid,prop,phys,myproc);
  }
  else
   UpdateDZ(grid,phys,prop, 1);

  // initailize variables to 0 (except for filter "pressure")
  for(i=0;i<Nc;i++) {
    phys->w[i][grid->Nk[i]]=0;
    for(k=0;k<grid->Nk[i];k++) {
      phys->w[i][k]=0;
      phys->q[i][k]=0;
      phys->s[i][k]=0;
      phys->T[i][k]=0;
      phys->s_old[i][k]=0;
      phys->T_old[i][k]=0;
      phys->s0[i][k]=0;
    }
  }

  for(j=0;j<grid->Ne;j++)
    for(k=0;k<grid->Nke[j];k++)
      phys->u[i][j]=0;  

  // Initialize the temperature, salinity, and background salinity
  // distributions.  Since z is not stored, need to use dz[k] to get
  // z[k].
  if(prop->readSalinity && prop->readinitialnc == 0) {
    stmp = (REAL *)SunMalloc(grid->Nkmax*sizeof(REAL),"InitializePhysicalVariables");
    if(fread(stmp,sizeof(REAL),grid->Nkmax,prop->InitSalinityFID) != grid->Nkmax)
      printf("Error reading stmp first\n");
    fclose(prop->InitSalinityFID);

    for(i=0;i<Nc;i++) 
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        phys->s[i][k]=stmp[k];
        phys->s0[i][k]=stmp[k];
      }
    SunFree(stmp,grid->Nkmax*sizeof(REAL),"InitializePhysicalVariables");
  } else if(prop->readinitialnc){
     ReturnSalinityNC(prop,phys,grid,ncscratch,Nci,Nki,T0,myproc);
  } else {
    for(i=0;i<Nc;i++) {
      z = 0;
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        z-=grid->dzz[i][k]/2;
        if(prop->vertcoord!=1 && prop->vertcoord!=2)
        {
          phys->s[i][k]=ReturnSalinity(grid->xv[i],grid->yv[i],vert->zc[i][k]);
          phys->s0[i][k]=ReturnSalinity(grid->xv[i],grid->yv[i],vert->zc[i][k]);           
        } 
        if(prop->vertcoord==1){     
          phys->s[i][k]=ReturnSalinity(grid->xv[i],grid->yv[i],z);
          phys->s0[i][k]=ReturnSalinity(grid->xv[i],grid->yv[i],z);                
        }
        if(prop->vertcoord==2){     
          phys->s[i][k]=IsoReturnSalinity(grid->xv[i],grid->yv[i],z,i,k);
          phys->s0[i][k]=IsoReturnSalinity(grid->xv[i],grid->yv[i],z,i,k); 
        }        
        z-=grid->dzz[i][k]/2;
      }
    }
  }

  if(prop->readTemperature && prop->readinitialnc == 0) {
    stmp = (REAL *)SunMalloc(grid->Nkmax*sizeof(REAL),"InitializePhysicalVariables");
    if(fread(stmp,sizeof(REAL),grid->Nkmax,prop->InitTemperatureFID) != grid->Nkmax)
      printf("Error reading stmp second\n");
    fclose(prop->InitTemperatureFID);    

    for(i=0;i<Nc;i++) 
      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        phys->T[i][k]=stmp[k];

    SunFree(stmp,grid->Nkmax*sizeof(REAL),"InitializePhysicalVariables");
   } else if(prop->readinitialnc){
        ReturnTemperatureNC(prop,phys,grid,ncscratch,Nci,Nki,T0,myproc);
  } else {  
    for(i=0;i<Nc;i++) {
      z = 0;
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        z-=grid->dzz[i][k]/2;
        if(prop->vertcoord!=1 && prop->vertcoord!=2)
          phys->T[i][k]=ReturnTemperature(grid->xv[i],grid->yv[i],vert->zc[i][k],grid->dv[i]);          
        if(prop->vertcoord==1)
          phys->T[i][k]=ReturnTemperature(grid->xv[i],grid->yv[i],z,grid->dv[i]);
        if(prop->vertcoord==2)
          phys->T[i][k]=IsoReturnTemperature(grid->xv[i],grid->yv[i],z,grid->dv[i],i,k);
        z-=grid->dzz[i][k]/2;
      }
    }
  }

  // set the old values s^n-1 T^n-1=s^n T^n at prop->n==1
  for(i=0;i<Nc;i++)
    for(k=grid->ctop[i];k<grid->Nk[i];k++)
    {
      phys->T_old[i][k]=phys->T[i][k];
      phys->s_old[i][k]=phys->s[i][k];      
    }

  // Initialize the velocity field 
  for(j=0;j<grid->Ne;j++) {
    z = 0;
    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];
    if(nc1==-1)
      nc1=nc2;
    if(nc2==-1)
      nc2=nc1;
    for(k=0;k<grid->Nke[j];k++) {
      z-=grid->dz[k]/2;
      if(prop->vertcoord==1)
        phys->u[j][k]=ReturnHorizontalVelocity(
                grid->xe[j],grid->ye[j],grid->n1[j],grid->n2[j],z);
      else
        phys->u[j][k]=ReturnHorizontalVelocity(
                grid->xe[j],grid->ye[j],grid->n1[j],grid->n2[j],InterpToFace(j,k,vert->zc,phys->u,grid));     
      z-=grid->dz[k]/2;
    }

  }
  // Initialise the heat flux arrays
  for(i=0;i<Nc;i++) {
    ktop = grid->ctop[i];
    phys->Tsurf[i] = phys->T[i][ktop];
    phys->dT[i] = 0.001; // Needs to be != 0
    
    for(k=0;k<grid->Nk[i];k++){
	phys->Ttmp[i][k]=phys->T[i][k];
    }
  }


  // Need to compute the velocity vectors at the cell centers based
  // on the initialized velocities at the faces.
  // since subgrid is not allocated yet, so just do the regular perot interpolation here
  ComputeUC(phys->uc, phys->vc, phys, grid, myproc, prop->interp,prop->kinterp,0);
  ComputeUC(phys->uold, phys->vold, phys, grid, myproc, prop->interp,prop->kinterp,0);

  // send and receive interprocessor data
  ISendRecvCellData3D(phys->uc,grid,myproc,comm);
  ISendRecvCellData3D(phys->vc,grid,myproc,comm);
  ISendRecvCellData3D(phys->uold,grid,myproc,comm);
  ISendRecvCellData3D(phys->vold,grid,myproc,comm);
  // Determine minimum and maximum salinity
  phys->smin=phys->s[0][0];
  phys->smax=phys->s[0][0];

  // overall cells in plan
  for(i=0;i<grid->Nc;i++) {
    // and in the vertical
    for(k=0;k<grid->Nk[i];k++) {
      if(phys->s[i][k]<phys->smin) phys->smin=phys->s[i][k];      
      if(phys->s[i][k]>phys->smax) phys->smax=phys->s[i][k];      
    }
  }
  // Set the density from s and T using the equation of state 
  SetDensity(grid,phys,prop);

  // Initialize the eddy-viscosity and scalar diffusivity
  for(i=0;i<grid->Nc;i++) {
    for(k=0;k<grid->Nk[i];k++) {
      phys->nu_tv[i][k]=0;
      phys->kappa_tv[i][k]=0;
      phys->nu_lax[i][k]=0;
    }
  }

  if(prop->turbmodel>=1) {
    for(i=0;i<grid->Nc;i++) {
      for(k=0;k<grid->Nk[i];k++) {
        phys->qT[i][k]=0;
        phys->lT[i][k]=0;
        phys->qT_old[i][k]=0;
        phys->lT_old[i][k]=0;
      }
    }
  }

  // Free the scratch array
  if (prop->readinitialnc>0)
      SunFree(ncscratch,Nki*Nci*sizeof(REAL),"InitializePhyiscalVariables");
      //Close the initial condition netcdf file
      //MPI_NCClose(prop->initialNCfileID );
}

/*
 * Function: SetDragCoefficients
 * Usage: SetDragCoefficents(grid,phys,prop,myproc);
 * ------------------------------------------
 * Set the drag coefficients based on the log law as well as the applied shear stress.
 * to get CdT and CdB
 */
void SetDragCoefficients(gridT *grid, physT *phys, propT *prop) {
  int i, j, k,nc1,nc2;
  REAL z,dz,u_sum,A_sum,zfb;
  // if set Z0, it will calculate Cd from log law. if Cd is given directly, it will set value to 
  // phys->Cd
  // z0T
  if(prop->z0T==0) 
    for(j=0;j<grid->Ne;j++) 
      phys->CdT[j]=prop->CdT;
  else
    for(j=0;j<grid->Ne;j++) 
      phys->CdT[j]=pow(log(0.5*grid->dzf[j][grid->etop[j]]/phys->z0T[j])/KAPPA_VK,-2);

  // z0B
  if(prop->z0B==0) 
    for(j=0;j<grid->Ne;j++) 
      phys->CdB[j]=prop->CdB;
  else
    for(j=0;j<grid->Ne;j++) 
    {
      if(prop->vertcoord==1)
        if(!prop->subgrid)
          zfb=0.5*grid->dzf[j][grid->Nke[j]-1];
        else
          zfb=0.5*subgrid->dzboteff[j];
      else
        // need new modification for subgrid bathymetry
        zfb=vert->zfb[j];

      if(grid->Nke[j]>1 && grid->etop[j]!=(grid->Nke[j]-1))
        phys->CdB[j]=pow(log(zfb/phys->z0B[j])/KAPPA_VK,-2);
      else
        phys->CdB[j]=pow((log(2*zfb/phys->z0B[j])+phys->z0B[j]/2/zfb-1)/KAPPA_VK,-2);
    }

  if(prop->subgrid && grid->Nkmax==1)
    if(subgrid->dragpara)
      CalculateSubgridDragCoef(grid,phys,prop);
  
  for(j=0;j<grid->Ne;j++){
    if(prop->vertcoord==1)
      if(!prop->subgrid)
        zfb=0.5*grid->dzf[j][grid->Nke[j]-1];
      else
        zfb=0.5*subgrid->dzboteff[j];
    else
      // need new modification for subgrid bathymetry
      zfb=vert->zfb[j];
  
    if(prop->vertcoord==1)
    {  
      if(2*zfb<BUFFERHEIGHT && grid->etop[j]==(grid->Nke[j]-1))
        phys->CdB[j]=100;  
    }else {
      // for the new vertical coordinate there is always constant layer numbers
      if(2*zfb<BUFFERHEIGHT)
        phys->CdB[j]=100;
    } 
  }
}

/*
* Function: InterpDrag(grid,phys,prop,myproc)
* usage: interpolate the value for z0T and z0B
* --------------------------------------------
* Author: Yun Zhang        
* Interpolate z0T and z0B according to z0t.dat and z0b.dat
* Intz0B==1, interpolate value
* Intz0B==2, read center point data directly
*/
static void InterpDrag(gridT *grid, physT *phys, propT *prop,int myproc)
{
   int n, Nb, Nt;
   REAL *xb, *yb, *z0b, *xt, *yt, *z0t;
   char str[BUFFERLENGTH];
   FILE *fid;
   // for z0B
   if(prop->Intz0B==1){
     Nb = MPI_GetSize(prop->INPUTZ0BFILE,"InterpDrag",myproc);
     xb = (REAL *)SunMalloc(Nb*sizeof(REAL),"InterpDrag");
     yb = (REAL *)SunMalloc(Nb*sizeof(REAL),"InterpDrag");
     z0b = (REAL *)SunMalloc(Nb*sizeof(REAL),"InterpDrag");
     fid = MPI_FOpen(prop->INPUTZ0BFILE,"r","InterpDrag",myproc);
     for(n=0;n<Nb;n++) {
       xb[n]=getfield(fid,str);
       yb[n]=getfield(fid,str);
       z0b[n]=getfield(fid,str); // but cd should be positive already
     }
     
     fclose(fid);
     // grid have xe ye
    /* for(n=0;n<grid->Ne;n++){
       // get the middle point of each edge
       xe[n]=0.5*(grid->xp[grid->edges[n*NUMEDGECOLUMNS]]+grid->xp[grid->edges[n*NUMEDGECOLUMNS+1]]);
       ye[n]=0.5*(grid->yp[grid->edges[n*NUMEDGECOLUMNS]]+grid->yp[grid->edges[n*NUMEDGECOLUMNS+1]]);
     }*/

     Interp(xb,yb,z0b,Nb,&(grid->xe[0]), &(grid->ye[0]), &(phys->z0B[0]), grid->Ne, grid->maxfaces);
     free(xb);
     free(yb);
     free(z0b);

   } else if(prop->Intz0B==2){
     sprintf(str,"%s-edge",prop->INPUTZ0BFILE);
     fid = MPI_FOpen(str,"r","InterpDrag",myproc);
     for(n=0;n<grid->Ne;n++) {
       getfield(fid,str);
       getfield(fid,str);
       phys->z0B[n]=getfield(fid,str);
     }
     fclose(fid);
   } else if(prop->Intz0B==0){
     for(n=0;n<grid->Ne;n++)
       phys->z0B[n]=prop->z0B;
   } else if(prop->Intz0B!=0){
     printf("Intz0B=%d, Intz0B can only be 0, 1 and 2\n",prop->Intz0B);
     MPI_Finalize();
     exit(EXIT_FAILURE);
   }

   // for z0T
   if(prop->Intz0T==1){

     Nt = MPI_GetSize(prop->INPUTZ0TFILE,"InterpDrag",myproc); // change to z0TFILE
     xt = (REAL *)SunMalloc(Nt*sizeof(REAL),"InterpDrag");
     yt = (REAL *)SunMalloc(Nt*sizeof(REAL),"InterpDrag");
     z0t = (REAL *)SunMalloc(Nt*sizeof(REAL),"InterpDrag");
     fid = MPI_FOpen(prop->INPUTZ0TFILE,"r","InterpDrag",myproc);
     for(n=0;n<Nt;n++) {
       xt[n]=getfield(fid,str);
       yt[n]=getfield(fid,str);
       z0t[n]=getfield(fid,str); // but cd should be positive already
     }
     fclose(fid);
     Interp(xt,yt,z0t,Nt,&(grid->xe[0]), &(grid->ye[0]),&(phys->z0T[0]),grid->Ne,grid->maxfaces);
     free(xt);
     free(yt);
     free(z0t);

   } else if(prop->Intz0T==2){
     sprintf(str,"%s-edge",prop->INPUTZ0TFILE);
     fid = MPI_FOpen(str,"r","InterpDrag",myproc);
     for(n=0;n<grid->Ne;n++) {
       getfield(fid,str);
       getfield(fid,str);
       phys->z0T[n]=getfield(fid,str);
     }
     fclose(fid);
   } else if(prop->Intz0T==0){
     
     for(n=0;n<grid->Ne;n++)
       phys->z0T[n]=prop->z0T;

   } else if(prop->Intz0T!=0){
     printf("Intz0T=%d, Intz0T can only be 0, 1 and 2\n",prop->Intz0T);
     MPI_Finalize();
     exit(EXIT_FAILURE);
   }
}

/*
* Function OutputDrag(grid,phys,prop,myproc)
* -------------------------------------------
* Author: Yun Zhang
* usage: output the phys->z0t and phys->z0b 
*         all the data is located 
*        at the center of cell
*/
static void OutputDrag(gridT *grid,physT *phys, propT *prop,int myproc, int numprocs, MPI_Comm comm)
{ 
  int n,nf,ne;
  REAL *z0b, *z0t;
  char str[BUFFERLENGTH], str1[BUFFERLENGTH], str2[BUFFERLENGTH];
  FILE *ofile;
  // for z0t
  if(prop->Intz0T==1 || prop->Intz0T==2){
    MPI_GetFile(str1,DATAFILE,"z0TFile","OutputDrag",myproc);
    z0t = (REAL *)SunMalloc(grid->Nc*sizeof(REAL),"OutputDrag");
    if(prop->mergeArrays)
      strcpy(str1,str);
    else
      sprintf(str1,"%s.%d",str,myproc);
    if(VERBOSE>2) printf("Outputting %s...\n",str); 
    ofile = MPI_FOpen(str,"w","OutputDrag",myproc);
    for(n=0;n<grid->Nc;n++) {
      z0t[n]=0;
      for(nf=0;nf<grid->nfaces[n];nf++) {
        ne = grid->face[n*grid->maxfaces+nf];
        z0t[n]+=phys->z0T[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
      }
      z0t[n]/=2*grid->Ac[n];
    }
    Write2DData(z0t,prop->mergeArrays,ofile,"Error outputting surface roughness data!\n",
		  grid,numprocs,myproc,comm);
    fclose(ofile);	
    free(z0t);
  }
  
  // for z0b
  if(prop->Intz0B==1 || prop->Intz0B==2){
    MPI_GetFile(str2,DATAFILE,"z0BFile","OutputDrag",myproc);
    z0b = (REAL *)SunMalloc(grid->Nc*sizeof(REAL),"OutputDrag");
    if(prop->mergeArrays)
      strcpy(str2,str);
    else
      sprintf(str2,"%s.%d",str,myproc);
    if(VERBOSE>2) printf("Outputting %s...\n",str); 
    ofile = MPI_FOpen(str,"w","OutputDrag",myproc);
    for(n=0;n<grid->Nc;n++) {
      z0b[n]=0;
      for(nf=0;nf<grid->nfaces[n];nf++) {
        ne = grid->face[n*grid->maxfaces+nf];
        z0b[n]+=phys->z0B[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
      }
      z0b[n]/=2*grid->Ac[n];
    }
    Write2DData(z0b,prop->mergeArrays,ofile,"Error outputting bottom roughness data!\n",
		  grid,numprocs,myproc,comm);
    fclose(ofile);
    free(z0b);
  }
}

/*
 * Function: InitializeVerticalGrid
 * Usage: InitializeVerticalGrid(grid);
 * ------------------------------------
 * Initialize the vertical grid by allocating space for grid->dzz and grid->dzzold
 * This just sets dzz and dzzold to dz since upon initialization dzz and dzzold
 * do not vary in the horizontal.
 *
 * This function is necessary so that gridding in veritcal can be redone each
 * simulation to account for changes in suntans.dat for Nkmax
 *
 */
void InitializeVerticalGrid(gridT **grid,int myproc)
{
  int i, j, k, Nc=(*grid)->Nc, Ne=(*grid)->Ne;

  // initialize in plan for the face (dzf dzfB) and 
  // cell centered (dzz dzzold) quantities
  (*grid)->stairstep = MPI_GetValue(DATAFILE,"stairstep","InitializeVerticalGrid",myproc);
  (*grid)->fixdzz = MPI_GetValue(DATAFILE,"fixdzz","InitializeVerticalGrid",myproc);
  (*grid)->dzsmall = (REAL)MPI_GetValue(DATAFILE,"dzsmall","InitializeVerticalGrid",myproc);
  (*grid)->smoothbot = (REAL)MPI_GetValue(DATAFILE,"smoothbot","InitializeVerticalGrid",myproc);  

  (*grid)->dzf = (REAL **)SunMalloc(Ne*sizeof(REAL *),"InitializeVerticalGrid");
  (*grid)->hf=(REAL *)SunMalloc(Ne*sizeof(REAL),"InitializeVerticalGrid");
  (*grid)->dzfB = (REAL *)SunMalloc(Ne*sizeof(REAL),"InitializeVerticalGrid");
  (*grid)->dzz = (REAL **)SunMalloc(Nc*sizeof(REAL *),"InitializeVerticalGrid");
  (*grid)->dzzold = (REAL **)SunMalloc(Nc*sizeof(REAL *),"InitializeVerticalGrid");
  (*grid)->dzbot = (REAL *)SunMalloc(Nc*sizeof(REAL),"InitializeVerticalGrid");


  // initialize over depth for edge-oriented quantities
  for(j=0;j<Ne;j++) 
    (*grid)->dzf[j]=(REAL *)SunMalloc(((*grid)->Nkc[j])*sizeof(REAL),"InitializeVerticalGrid");

  // initialize over depth for cell-centered quantities
  for(i=0;i<Nc;i++) {
    (*grid)->dzz[i]=(REAL *)SunMalloc(((*grid)->Nk[i])*sizeof(REAL),"InitializeVerticalGrid");
    (*grid)->dzzold[i]=(REAL *)SunMalloc(((*grid)->Nk[i])*sizeof(REAL),"InitializeVerticalGrid");
    
    for(k=0;k<(*grid)->Nk[i];k++) {
      (*grid)->dzz[i][k]=(*grid)->dz[k];  
      (*grid)->dzzold[i][k]=(*grid)->dz[k];  
    }
  }
}

/*
 * Function: UpdateDZ
 * Usage: UpdateDZ(grid,phys,0);
 * -----------------------------
 * This function updates the vertical grid spacings based on the free surface and
 * the bottom bathymetry.  That is, if the free surface cuts through cells and leaves
 * any cells dry (or wet), then this function will set the vertical grid spacing 
 * accordingly.
 *
 * If option==1, then it assumes this is the first call and sets dzzold to dzz at
 *   the end of this function.
 * Otherwise it sets dzzold to dzz at the beginning of the function and updates dzz
 *   thereafter.
 *
 */
void UpdateDZ(gridT *grid, physT *phys, propT *prop, int option)
{
  int i, j, k, ne1, ne2, Nc=grid->Nc, Ne=grid->Ne, flag, nc1, nc2;
  REAL z, dzz1, dzz2;

  // don't need to recompute for linearized FS
  if(prop->linearFS) {
    return;
  }

  // If this is not an initial call then set dzzold to store the old value of dzz
  // and also set the etopold and ctopold pointers to store the top indices of
  // the grid.
  if(!option) {
    for(j=0;j<Ne;j++)
      grid->etopold[j]=grid->etop[j];
    for(i=0;i<Nc;i++) {
      grid->ctopold[i]=grid->ctop[i];
      for(k=0;k<grid->ctop[i];k++)
        grid->dzzold[i][k]=0;
      for(k=grid->ctop[i];k<grid->Nk[i];k++)
        grid->dzzold[i][k]=grid->dzz[i][k];
    }
  }
  //fixdzz
  if(option==-1)
    for(i=0;i<Nc;i++)
      phys->h[i]=.0; 

  // First set the thickness of the bottom grid layer.  If this is a partial-step
  // grid then the dzz will vary over the horizontal at the bottom layer.  Otherwise,
  // the dzz at the bottom will be equal to dz at the bottom.
  for(i=0;i<Nc;i++) {
    z = 0;
    for(k=0;k<grid->Nk[i];k++)
      z-=grid->dz[k];
    grid->dzz[i][grid->Nk[i]-1]=grid->dz[grid->Nk[i]-1]+grid->dv[i]+z;
  }

  // Loop through and set the vertical grid thickness when the free surface cuts through 
  // a particular cell.
  if(grid->Nkmax>1) {
    for(i=0;i<Nc;i++) {
      z = 0;
      flag = 0;
      for(k=0;k<grid->Nk[i];k++) {
        z-=grid->dz[k];
        if(phys->h[i]>=z) 
          if(!flag) {
            if(k==grid->Nk[i]-1) {
              grid->dzz[i][k]=phys->h[i]+grid->dv[i];
              grid->ctop[i]=k;
            } else if(phys->h[i]==z) {
              grid->dzz[i][k]=0;
              grid->ctop[i]=k+1;
            } else {
              grid->dzz[i][k]=phys->h[i]-z;
              grid->ctop[i]=k;
            }
            flag=1;
          } else {
            if(k==grid->Nk[i]-1) 
              grid->dzz[i][k]=grid->dz[k]+grid->dv[i]+z;
            else 
              if(z<-grid->dv[i])
                grid->dzz[i][k]=0;
              else 
                grid->dzz[i][k]=grid->dz[k];
          } 
        else {
          // change 10/01/2014
          if(flag==0 && k==(grid->Nk[i]-1)){
            grid->dzz[i][k]=grid->dv[i]+phys->h[i];
            grid->ctop[i]=k;
          }  
          else
            grid->dzz[i][k]=0;
        }
      }
    }
  } else 
    for(i=0;i<Nc;i++) 
      grid->dzz[i][0]=grid->dv[i]+phys->h[i];

  // Now set grid->etop and ctop which store the index of the top cell  
  for(j=0;j<grid->Ne;j++) {
    ne1 = grid->grad[2*j];
    ne2 = grid->grad[2*j+1];
    if(ne1 == -1)
      grid->etop[j]=grid->ctop[ne2];
    else if(ne2 == -1)
      grid->etop[j]=grid->ctop[ne1];
    else if(grid->ctop[ne1]<grid->ctop[ne2])
      grid->etop[j]=grid->ctop[ne1];
    else
      grid->etop[j]=grid->ctop[ne2];
  }

  // If this is an initial call set the old values to the new values and
  // Determine the bottom-most flux-face height in the absence of h
  // Check the smallest dzz and set to minimum  
  if(option==-1) {
    for(i=0;i<Nc;i++){
      k=grid->Nk[i]-1;      
      grid->dzbot[i]=grid->dzz[i][k];
      if(!grid->stairstep && grid->fixdzz )   
        if(grid->dzz[i][k]<grid->dz[k]*grid->dzsmall) {
          grid->dv[i]+= (grid->dz[k]*grid->dzsmall-grid->dzz[i][k]);	
          grid->dzz[i][k]=grid->dz[k]*grid->dzsmall;
        }

    }
  }

  if(option==1) {    
    for(j=0;j<Ne;j++) 
      grid->etopold[j]=grid->etop[j];
    for(i=0;i<Nc;i++) {
      grid->ctopold[i]=grid->ctop[i];
      for(k=0;k<grid->Nk[i];k++)
        grid->dzzold[i][k]=grid->dzz[i][k];
    }

    for(j=0;j<Ne;j++) {
      nc1 = grid->grad[2*j];
      nc2 = grid->grad[2*j+1];
      if(nc1==-1) nc1=nc2;
      if(nc2==-1) nc2=nc1;

      dzz1 = grid->dzz[nc1][grid->Nk[nc1]-1];
      dzz2 = grid->dzz[nc2][grid->Nk[nc2]-1];
      z=0;
      for(k=0;k<grid->Nke[j]-1;k++)
        z-=grid->dz[k];
      if(phys->h[nc1]<z) 
        dzz1 = dzz1-z+phys->h[nc1];
      if(phys->h[nc2]<z) 
        dzz2 = dzz2-z+phys->h[nc2];
      grid->dzfB[j] = Min(dzz1,dzz2);
    }
  }
}

/*
 * Function: DepthFromDZ
 * Usage: z = DepthFromDZ(grid,phys,i,k);
 * --------------------------------------
 * Return the depth beneath the free surface at location i, k.
 *
 */
REAL DepthFromDZ(gridT *grid, physT *phys, int i, int kind) {
  if(i==-1) {
    printf("!!Error with pointer => h[-1]!!\n");
    //return NAN;  // not consistent with all C compilers
    return -1;
  }
  else {
    int k;
    REAL z = phys->h[i]-0.5*grid->dzz[i][grid->ctop[i]];
    for(k=grid->ctop[i];k<kind;k++) {
      z-=0.5*grid->dzz[i][k-1];
      z-=0.5*grid->dzz[i][k];
    }
    //  printf("DepthfromDZ done\n");
    return z;
  }
}

/*
 * Function: Solve
 * Usage: Solve(grid,phys,prop,myproc,numprocs,comm);
 * --------------------------------------------------
 * This is the main solving routine and is called from suntans.c.
 *
 */
void Solve(gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm)
{

  int i,k,j, n, blowup=0,ne,id,nc1,nc2,nf;
  REAL t0,sum1,v_average,flux,normal;
  metinT *metin;
  metT *met;
  averageT *average;

  // Compute the initial quantities for comparison to determine conservative properties
  prop->n=0;
  // this make sure that we aren't loosing mass/energy
  ComputeConservatives(grid,phys,prop,myproc,numprocs,comm);

  // Print out memory usage per processor and total memory if this is the first time step
  if(VERBOSE>2) MemoryStats(grid,myproc,numprocs,comm);
  // initialize theta0
  prop->theta0=prop->theta;

  // initialize the timers
  t_start=Timer();
  t_source=t_predictor=t_nonhydro=t_turb=t_transport=t_io=t_comm=t_check=0;

  // Set all boundary values at time t=nstart*dt;
  prop->n=prop->nstart;
  // initialize the time (often used for boundary/initial conditions)
  prop->rtime=prop->nstart*prop->dt;
 
  // Initialise the netcdf time (moved to InitializePhysicalVariables)
  // Get the toffSet property
  //printf("myproc: %d, starttime: %s\n",prop->starttime);
  //prop->toffSet = getToffSet(prop->basetime,prop->starttime);
  //prop->nctime = prop->toffSet*86400.0 + prop->nstart*prop->dt;
  //printf("myproc: %d, toffSet = %f (%s, %s)\n",myproc,prop->toffSet,&prop->basetime,&prop->starttime);

  // Initialise the boundary data from a netcdf file
  if(prop->netcdfBdy==1){
    AllocateBoundaryData(prop, grid, &bound, myproc,comm);
    InitBoundaryData(prop, grid, myproc,comm);
  }

  // get the boundary velocities (boundaries.c)
  BoundaryVelocities(grid,phys,prop,myproc, comm); 
  // get the openboundary flux (boundaries.c)
  OpenBoundaryFluxes(NULL,phys->u,NULL,grid,phys,prop);
  // get the boundary scalars (boundaries.c)
  BoundaryScalars(grid,phys,prop,myproc,comm);
  // set the height of the face bewteen cells to compute the flux
  if(prop->vertcoord!=1 && prop->vertcoord!=5)
    TvdFluxHeight(grid, phys, prop, vert->dzfmeth,comm, myproc);
  SetFluxHeight(grid,phys,prop);
  // find the bottom layer number which zfb>Bufferheight only works for new vertical coordinate
  if(prop->vertcoord!=1)
    FindBottomLayer(grid,prop,phys,myproc);

  // get the boundary velocities (boundaries.c)
  BoundaryVelocities(grid,phys,prop,myproc, comm); 
  // get the openboundary flux (boundaries.c)
  OpenBoundaryFluxes(NULL,phys->u,NULL,grid,phys,prop);

  // Set up arrays to merge output
  if(prop->mergeArrays) {
    if(VERBOSE>2 && myproc==0) printf("Initializing arrays for merging...\n");
    InitializeMerging(grid,prop->outputNetcdf,numprocs,myproc,comm);
  }

  // culvert model
  if(prop->culvertmodel){
    if(myproc==0)
      printf("\n\nculvert model has beed started\n\n");
    SetupCulvertmodel(grid,phys,prop,myproc);
  }

  // interp hmarsh and CdV and get hmarshleft and marshtop if consider marshmodel
  if(prop->marshmodel){
    SetupMarshmodel(grid,phys,prop,myproc,numprocs,comm);
    SetMarshTop(grid,phys,myproc);
  }

  // use subgrid method 
  if(prop->subgrid)
  {
    SubgridBasic(grid,phys,prop,myproc,numprocs,comm);
    UpdateSubgridVeff(grid, phys, prop, myproc);
    UpdateSubgridFluxHeight(grid, phys, prop, myproc);
    UpdateSubgridAceff(grid, phys, prop, myproc);
    UpdateSubgridHeff(grid,phys,prop,myproc);
    UpdateSubgridVerticalAceff(grid, phys, prop, 0, myproc);
    if(prop->culvertmodel)
      SubgridCulverttopArea(grid, prop, myproc);
    SubgridFluxCheck(grid, phys, prop,myproc);
    //prop->thetaM=-1;
    //printf("Subgrid module is turned on, set thetaM=-1 which means vertical momentum advection calculation is explicit\n");
  }


  // interpolate z0B and z0T if Intz0t or Intz0B not zero
  InterpDrag(grid,phys,prop,myproc);

  // set the drag coefficients for bottom friction
  SetDragCoefficients(grid,phys,prop);

  // add culvert part change drag coefficient for culvert part
  if(prop->culvertmodel)
    SetCulvertDragCoefficient(grid, phys, myproc); 

  // output the drag coefficient for sunplot
  // including CdV z0B and z0T
  // if intz0B and intz0T is zero, z0B=prop->z0B and z0T=prop->z0T
  // output drag z0B zOT and CdV

  OutputDrag(grid,phys,prop,myproc,numprocs,comm);

  // for laxWendroff and central differencing- compute the numerical diffusion 
  // coefficients required for stability
  if(prop->laxWendroff && prop->nonlinear==2) LaxWendroff(grid,phys,prop,myproc,comm);

  // Initialize the Sponge Layer
  InitSponge(grid,myproc);


  // Initialise the meteorological forcing input fields
  if(prop->metmodel>0)
  {
      if (prop->gamma==0.0){
        if(myproc==0) 
          printf("Warning gamma must be > 1 for heat flux model.\n");
      }else{
        if(myproc==0) 
          printf("Initial temperature = %f.\n",phys->T[0][0]);
      }
      AllocateMetIn(prop,grid,&metin,myproc);
      AllocateMet(prop,grid,&met,myproc);
      InitialiseMetFields(prop, grid, metin, met,myproc);
      
      // Initialise the heat flux variables
      updateMetData(prop, grid, metin, met, myproc, comm); 
     
    if(prop->metmodel>=2) {      
      updateAirSeaFluxes(prop, grid, phys, met, phys->T);

      //Communicate across processors
      ISendRecvCellData2D(met->Hs,grid,myproc,comm);
      ISendRecvCellData2D(met->Hl,grid,myproc,comm);
      ISendRecvCellData2D(met->Hsw,grid,myproc,comm);
      ISendRecvCellData2D(met->Hlw,grid,myproc,comm);
      ISendRecvCellData2D(met->tau_x,grid,myproc,comm);
      ISendRecvCellData2D(met->tau_y,grid,myproc,comm);
    }
  }

  // Initialise the output netcdf file metadata
  if(prop->outputNetcdf==1 &&prop->mergeArrays==0){
    InitialiseOutputNCugrid(prop, grid, phys, met, myproc);
  }

  // Initialise the average arrays and netcdf file
  if(prop->calcaverage>0){
    AllocateAverageVariables(grid,&average,prop);
    ZeroAverageVariables(grid,average,prop);
    if(prop->mergeArrays==0)
      InitialiseAverageNCugrid(prop, grid, average, myproc);
  }
  // get the windstress (boundaries.c) - this needs to go after met data allocation -MR
  WindStress(grid,phys,prop,met,myproc);

  // main time loop
  for(n=prop->nstart+1;n<=prop->nsteps+prop->nstart;n++) {
    prop->n = n;
    // compute the runtime 
    prop->rtime = n*prop->dt;
    
    // netcdf file time
    prop->nctime = prop->toffSet*86400.0 + n*prop->dt;
    //prop->nctime +=  n*prop->dt;
    //prop->nctime += prop->rtime;
    // Set nsteps<0 for debugging i/o without hydro/seds/etc...
    if(prop->nsteps>0) {

      // Ramp down theta from 1 to the value specified in suntans.dat over
      // the time thetaramptime specified in suntans.dat to damp out transient
      // oscillations
      if(prop->thetaramptime!=0)
        prop->theta=(1-exp(-prop->rtime/prop->thetaramptime))*prop->theta0+
          exp(-prop->rtime/prop->thetaramptime);

      // Compute the horizontal source term phys->utmp which contains the explicit part
      // or the right hand side of the free-surface equation. 
      // begin the timer
      t0=Timer();
      // get the flux height (since free surface is changing) which is stored in dzf
      if(prop->vertcoord!=1 && prop->vertcoord!=5)
        TvdFluxHeight(grid, phys, prop, vert->dzfmeth,comm, myproc);
      SetFluxHeight(grid,phys,prop);

      // Store the old velocity and scalar fields
      // Store the old values of s, u, and w into stmp3, u_old, and w_old
      StoreVariables(grid,phys);
      
      // store old omega for the new vertical coordinate
      if(prop->vertcoord!=1)
        StoreVertVariables(grid,phys);

      // find the bottom layer number which zfb>Bufferheight only works for new vertical coordinate
      if(prop->vertcoord!=1)
        FindBottomLayer(grid,prop,phys,myproc);
     
      // compute CdB and CdT
      SetDragCoefficients(grid,phys,prop);

      // use subgrid method
      if(prop->subgrid)
        UpdateSubgridFluxHeight(grid, phys, prop, myproc);

      if(prop->culvertmodel)
      {
        // change phy->h back to pressure field
        StoreCulvertPressure(phys->h, grid->Nc, 0, myproc);
        // add culvert part change drag coefficient for culvert part
        SetCulvertDragCoefficient(grid, phys, myproc);  
      }

      // laxWendroff central differencing
      if(prop->laxWendroff && prop->nonlinear==2) 
        LaxWendroff(grid,phys,prop,myproc,comm);
   
      // compute the horizontal source terms (like 
      /* 
       * 1) Old nonhydrostatic pressure gradient with theta m ethod
       * 2) Coriolis terms with AB2
       * 3) Baroclinic term with AB2
       * 4) Horizontal and vertical advection of horizontal momentum with AB2
       * 5) Horizontal laminar+turbulent diffusion of horizontal momentum
       */

      // calculate the preparation for HorizontalSource function due to the new vertical coordinate
      // time comsuming function!
      if(prop->vertcoord!=1)
        VertCoordinateHorizontalSource(grid, phys, prop, myproc, numprocs, comm);
      HorizontalSource(grid,phys,prop,myproc,numprocs,comm);

      // add wave part 
      if(prop->wavemodel)
        UpdateWave(grid, phys, prop, comm, blowup, myproc, numprocs);	

      // compute the time required for the source
      t_source+=Timer()-t0;

      // Use the explicit part created in HorizontalSource and solve for the 
      // free-surface
      // and hence compute the predicted or hydrostatic horizontal velocity field.  Then
      // send and receive the free surface interprocessor boundary data 
      // to the neighboring processors.
      // The predicted horizontal velocity is now in phys->u
      t0=Timer();

      // compute U^* and h^* (Eqn 40 and Eqn 31)
      UPredictor(grid,phys,prop,myproc,numprocs,comm);
      ISendRecvCellData2D(phys->h_old,grid,myproc,comm);
      ISendRecvCellData2D(phys->h,grid,myproc,comm);

      t_predictor+=Timer()-t0;
      t0=Timer();
      blowup = CheckDZ(grid,phys,prop,myproc,numprocs,comm);
      t_check+=Timer()-t0;

      // apply continuity via Eqn 82
      if(prop->vertcoord==1)
      {
        // w_im is calculated
        Continuity(phys->wnew,grid,phys,prop);
        ISendRecvWData(phys->wnew,grid,myproc,comm);
      } else {
        // here only calculates the omega value which means the w* is still unknown.
        // the reason is that w is not used for hydrostatic calculation and scalar transport
        // after this calculation vert->omega=omega* vert->omega_old=omega^n vert->omega_old2=omega^n-1
        // omega_im is calculated 
        LayerAveragedContinuity(vert->omega,grid,prop,phys,myproc);
        ISendRecvWData(vert->omega,grid,myproc,comm);
      }

      t0=Timer();
      // calculate flux in/out to each cell to ensure bounded scalar concentration under subgrid
      //if(prop->subgrid)
        //SubgridFluxCheck(grid, phys, prop,myproc);

      // Compute the eddy viscosity
      t0=Timer();

      if(prop->vertcoord==1)
        EddyViscosity(grid,phys,prop,phys->w_im,comm,myproc);
      else
        EddyViscosity(grid,phys,prop,vert->omega_im,comm,myproc);

      t_turb+=Timer()-t0;

      // Update the meteorological data
      if(prop->metmodel>0){
        updateMetData(prop, grid, metin, met, myproc, comm);
        //if(prop->metmodel==2){
        //    updateAirSeaFluxes(prop, grid, phys, met, phys->T);
        //}
      }
      
     // Update the age (passive) tracers
      if(prop->calcage>0){
        UpdateAge(grid,phys,prop,comm,myproc);
      }
     
      // Update the temperature only if gamma is nonzero in suntans.dat
      if(prop->gamma && prop->vertcoord!=2) {
        t0=Timer();
        getTsurf(grid,phys); // Find the surface temperature
        HeatSource(phys->wtmp,phys->uold,grid,phys,prop,met, myproc, comm);
        if(prop->vertcoord==1){
          UpdateScalars(grid,phys,prop,phys->w_im,phys->T,phys->T_old,phys->boundary_T,phys->Cn_T,
                prop->kappa_T,prop->kappa_TH,phys->kappa_tv,prop->theta,
                phys->uold,phys->wtmp,NULL,NULL,0,0,comm,myproc,0,prop->TVDtemp);
          getchangeT(grid,phys); // Get the change in surface temp
        }else{
          UpdateScalars(grid,phys,prop,vert->omega_im,phys->T,phys->T_old,phys->boundary_T,phys->Cn_T,
            prop->kappa_T,prop->kappa_TH,phys->kappa_tv,prop->theta,
            phys->uold,phys->wtmp,NULL,NULL,0,0,comm,myproc,0,prop->TVDtemp);
          getchangeT(grid,phys);
        }
        ISendRecvCellData3D(phys->T,grid,myproc,comm);	
        ISendRecvCellData3D(phys->Ttmp,grid,myproc,comm);
        ISendRecvCellData2D(phys->dT,grid,myproc,comm);
        ISendRecvCellData2D(phys->Tsurf,grid,myproc,comm);

        t_transport+=Timer()-t0;
      }

      // Update the air-sea fluxes --> these are used for the previous time step source term and for the salt flux implicit term (salt tracer solver therefore needs to go next)
      if(prop->metmodel>=2){
        updateAirSeaFluxes(prop, grid, phys, met, phys->T);
        //Communicate across processors
        ISendRecvCellData2D(met->Hs,grid,myproc,comm);
        ISendRecvCellData2D(met->Hl,grid,myproc,comm);
        ISendRecvCellData2D(met->Hsw,grid,myproc,comm);
        ISendRecvCellData2D(met->Hlw,grid,myproc,comm);
        ISendRecvCellData2D(met->tau_x,grid,myproc,comm);
        ISendRecvCellData2D(met->tau_y,grid,myproc,comm);
      }

      // Update the salinity only if beta is nonzero in suntans.dat
      if(prop->beta && prop->vertcoord!=2) {
        t0=Timer();
        if(prop->metmodel>0){
            SaltSource(phys->wtmp,phys->uold,grid,phys,prop,met);
            if(prop->vertcoord==1)
              UpdateScalars(grid,phys,prop,phys->w_im,phys->s,phys->s_old,phys->boundary_s,phys->Cn_R,
                prop->kappa_s,prop->kappa_sH,phys->kappa_tv,prop->theta,
                phys->uold,phys->wtmp,NULL,NULL,0,0,comm,myproc,1,prop->TVDsalt);
            else
              UpdateScalars(grid,phys,prop,vert->omega_im,phys->s,phys->s_old,phys->boundary_s,phys->Cn_R,
                prop->kappa_s,prop->kappa_sH,phys->kappa_tv,prop->theta,
                phys->uold,phys->wtmp,NULL,NULL,0,0,comm,myproc,1,prop->TVDsalt);        
        }else{ 
            if(prop->vertcoord==1)
              UpdateScalars(grid,phys,prop,phys->w_im,phys->s,phys->s_old,phys->boundary_s,phys->Cn_R,
                prop->kappa_s,prop->kappa_sH,phys->kappa_tv,prop->theta,
                NULL,NULL,NULL,NULL,0,0,comm,myproc,1,prop->TVDsalt);
            else
              UpdateScalars(grid,phys,prop,vert->omega_im,phys->s,phys->s_old,phys->boundary_s,phys->Cn_R,
                prop->kappa_s,prop->kappa_sH,phys->kappa_tv,prop->theta,
                NULL,NULL,NULL,NULL,0,0,comm,myproc,1,prop->TVDsalt);
        }

        ISendRecvCellData3D(phys->s,grid,myproc,comm);

        if(prop->metmodel>0){
          //Communicate across processors
          ISendRecvCellData2D(met->EP,grid,myproc,comm);
        }

        t_transport+=Timer()-t0;
      }

      // Compute sediment transport when prop->computeSediments=1
      if(prop->computeSediments){
        t0=Timer(); 
        // update uc without non-hydrostatic pressure
        // ComputeUC(phys->uc, phys->vc, phys,grid, myproc, prop->interp,prop->kinterp,prop->subgrid);
        ComputeSediments(grid,phys,prop,myproc,numprocs,blowup,comm);
        t_transport+=Timer()-t0;
      }

      // update subgrid->Acceff and subgrid->Acveff for non hydrostatic
      if(prop->subgrid)
        UpdateSubgridVerticalAceff(grid, phys, prop, 1, myproc);
      // Compute vertical momentum and the nonhydrostatic pressure
      if(prop->nonhydrostatic && !blowup) {
        // Predicted vertical velocity field is in phys->w
        WPredictor(grid,phys,prop,myproc,numprocs,comm);

        // Wpredictor calculate w^*
        // now calculate omega^* for the new generalized vertical coordinate
        if(prop->vertcoord!=1){
          // recalculate uc and vc for the predictor velocity field
          ComputeUC(phys->uc, phys->vc, phys,grid, myproc, prop->interp,prop->kinterp,prop->subgrid);
          // now we have uc^* and vc^*
          // compute ul^* and vl^* at each layer top
          ComputeUl(grid, prop, phys, myproc);
          // compute zc gradient
          // zf is calculate in ComputeZc function
          ComputeCellAveragedHorizontalGradient(vert->dzdx, 0, vert->zf, grid, prop, phys, myproc);
          ComputeCellAveragedHorizontalGradient(vert->dzdy, 1, vert->zf, grid, prop, phys, myproc); 
 
          // compute U3
          ComputeOmega(grid, prop, phys,-1, myproc);
        }

        // Source term for the pressure-Poisson equation is in phys->stmp
        ComputeQSource(phys->stmp,grid,phys,prop,myproc,numprocs);

        // Solve for the nonhydrostatic pressure.  
        // phys->stmp2/qc contains the initial guess
        // phys->stmp contains the source term
        // phys->stmp3 is used for temporary storage
        CGSolveQ(phys->qc,phys->stmp,phys->stmp3,grid,phys,prop,myproc,numprocs,comm);

        // Correct the nonhydrostatic velocity field with the nonhydrostatic pressure
        // correction field phys->stmp2/qc.  This will correct phys->u so that it is now
        // the volume-conserving horizontal velocity field.  
        // phys->w is not corrected since
        // it is obtained via continuity.  
        // Also, update the total nonhydrostatic pressure
        // with the pressure correction. 
        Corrector(phys->qc,grid,phys,prop,myproc,numprocs,comm);

        // Send/recv the horizontal velocity data after it has been corrected.
        ISendRecvEdgeData3D(phys->u,grid,myproc,comm);

        // Send q to the boundary cells now that it has been updated
        ISendRecvCellData3D(phys->q,grid,myproc,comm);
      } else if(!(prop->interp == PEROT)) {
        // support quadratic interpolation work
        // Send/recv the horizontal velocity data for use with more complex interpolation
        ISendRecvEdgeData3D(phys->u,grid,myproc,comm);
      }
      t_nonhydro+=Timer()-t0;

     // apply continuity via Eqn 82
      if(prop->vertcoord==1)
      {
        // w_im is calculated
        Continuity(phys->w,grid,phys,prop);
        ISendRecvWData(phys->w,grid,myproc,comm);
      } else {
        // NOW vert->omega stores the omega^n+1 with nonhydrostatic pressure correction
        if(!prop->nonhydrostatic || prop->vertcoord==5)   
        {
          LayerAveragedContinuity(vert->omega,grid,prop,phys,myproc);
          ISendRecvWData(vert->omega,grid,myproc,comm);          
        }
        // if nonhydrostatic=1, w is solved in the corrector function
        // no need to solve from omega
        // w is recalculated by omega only for hydrostatic case
        if(!prop->nonhydrostatic)
        {  
          // recalculate uc and vc for the predictor velocity field
          ComputeUC(phys->uc, phys->vc, phys,grid, myproc, prop->interp,prop->kinterp,prop->subgrid);
          // now we have uc^* and vc^*
          // compute ul^* and vl^* at each layer top
          ComputeUl(grid, prop, phys, myproc);
          // compute zc gradient
          // zf is calculate in ComputeZc function
          ComputeCellAveragedHorizontalGradient(vert->dzdx, 0, vert->zf, grid, prop, phys, myproc);
          ComputeCellAveragedHorizontalGradient(vert->dzdy, 1, vert->zf, grid, prop, phys, myproc); 

        }

        if(!prop->nonhydrostatic || prop->vertcoord==5)   
          // compute w from omega   
          ComputeOmega(grid, prop, phys,0, myproc);   

        ISendRecvWData(phys->w,grid,myproc,comm);   

        // update U3 with the new w
        ComputeOmega(grid, prop, phys,-1, myproc);
        ISendRecvWData(vert->U3,grid,myproc,comm);
      }

      // Set scalar and wind stress boundary values at new time step n; 
      // (n-1) is old time step.
      // BoundaryVelocities and OpenBoundaryFluxes were called in UPredictor to set the
      // boundary velocities to the new time step values for use in the 
      // free surface calculation.
      BoundaryScalars(grid,phys,prop,myproc,comm);

      WindStress(grid,phys,prop,met,myproc);

      // dz change so hmarshleft and marshtop may change
      if(prop->marshmodel)
        SetMarshTop(grid,phys,myproc);

      if(prop->beta || prop->gamma)
        SetDensity(grid,phys,prop);

      // calculate any variable values
      UserDefinedFunction(grid,phys,prop,myproc);

      // u now contains velocity on all edges at the new time step
      ComputeUC(phys->uc, phys->vc, phys,grid, myproc, prop->interp,prop->kinterp,prop->subgrid);
      //printf("Done (%d).\n",myproc);

      // now send interprocessor data
      ISendRecvCellData3D(phys->uc,grid,myproc,comm);
      ISendRecvCellData3D(phys->vc,grid,myproc,comm);
    }

    // Adjust the velocity field in the new cells if the newcells variable is set 
    // to 1 in suntans.dat.  Once this is done, send the interprocessor 
    // u-velocities to the neighboring processors.
    if(prop->newcells) {
      NewCells(grid,phys,prop);
      ISendRecvEdgeData3D(phys->u,grid,myproc,comm);
    }

    // Compute average
    if(prop->calcaverage){
      UpdateAverageVariables(grid,average,phys,met,prop,comm,myproc); 
      UpdateAverageScalars(grid,average,phys,met,prop,comm,myproc); 
    }

    // Check whether or not run is blowing up
    t0=Timer();
    blowup=(Check(grid,phys,prop,myproc,numprocs,comm) || blowup);
    t_check+=Timer()-t0;
    
    // Output data based on ntout specified in suntans.dat
    t0=Timer();
    if (prop->outputNetcdf==0){
      // Write to binary
      OutputPhysicalVariables(grid,phys,prop,myproc,numprocs,blowup,comm); 
      // subgrid
      if(prop->subgrid)
        OutputSubgridVariables(grid, prop, myproc, numprocs, comm);
      if(prop->vertcoord!=1)
        OutputVertCoordinate(grid,prop,myproc,numprocs,comm);
    }else {
      // Output data to netcdf
      WriteOutputNC(prop, grid, phys, met, blowup, myproc);
    }

    // Output the average arrays
    if(prop->calcaverage){
      if(prop->mergeArrays){
        WriteAverageNCmerge(prop,grid,average,phys,met,blowup,numprocs,comm,myproc);
      }else{
        WriteAverageNC(prop,grid,average,phys,met,blowup,comm,myproc);
      }
    }
    InterpData(grid,phys,prop,comm,numprocs,myproc);

    t_io+=Timer()-t0;
    // Output progress
    Progress(prop,myproc,numprocs);
    if(blowup)
      break;

    //Close all open netcdf file
    /*
    if(prop->n==prop->nsteps+prop->nstart) {
      if(prop->outputNetcdf==1){
        //printf("Closing output netcdf file on processor: %d\n",myproc);
      	MPI_NCClose(prop->outputNetcdfFileID);
      }
      if(prop->netcdfBdy==1){
        //printf("Closing boundary netcdf file on processor: %d\n",myproc);
      	MPI_NCClose(prop->netcdfBdyFileID);
      }
      if(prop->readinitialnc==1){
        //printf("Closing initial netcdf file on processor: %d\n",myproc);
      	MPI_NCClose(prop->initialNCfileID );
      }
      if(prop->metmodel>0){
        //printf("Closing met netcdf file on processor: %d\n",myproc);
      	MPI_NCClose(prop->metncid);
      }
    }
    */
  }

  if(prop->mergeArrays) {
    if(VERBOSE>2 && myproc==0) printf("Freeing merging arrays...\n");
    FreeMergingArrays(grid,myproc);
  }
}

/*
 * Function: StoreVariables
 * Usage: StoreVariables(grid,phys);
 * ---------------------------------
 * Store the old values of s, u, and w into stmp3, u_old, and w_old,
 * respectively.
 *
 */
static void StoreVariables(gridT *grid, physT *phys) {
  int i, j, k, iptr, jptr;

  for(i=0;i<grid->Nc;i++) 
    for(k=0;k<grid->Nk[i];k++) {
      phys->stmp3[i][k]=phys->s[i][k];
      phys->w_old2[i][k]=phys->w_old[i][k];
      phys->w_old[i][k]=phys->w[i][k];      
    }

  for(j=0;j<grid->Ne;j++) {
    phys->D[j]=0;
    for(k=0;k<grid->Nke[j];k++)
    {  
      phys->u_old2[j][k]=phys->u_old[j][k];
      phys->utmp[j][k]=phys->u_old[j][k]=phys->u[j][k];
    }
  }
}

/*
 * Function: HorizontalSource
 * Usage: HorizontalSource(grid,phys,prop,myproc,numprocs);
 * --------------------------------------------------------
 * Compute the horizontal source term that is used to obtain the free surface.
 *
 * This function adds the following to the horizontal source term:
 *
 * 1) Old nonhydrostatic pressure gradient with theta method
 * 2) Coriolis terms with AB2
 * 3) Baroclinic term with AB2
 * 4) Horizontal and vertical advection of horizontal momentum with AB2
 * 5) Horizontal laminar+turbulent diffusion of horizontal momentum
 *
 * Cn_U contains the Adams-Bashforth terms at time step n-1.
 * If wetting and drying is employed, no advection is computed in 
 * the upper cell.
 *
 */
static void HorizontalSource(gridT *grid, physT *phys, propT *prop,
    int myproc, int numprocs, MPI_Comm comm) 
{
  int i, ib, iptr, boundary_index, nf, j, jptr, k, nc, nc1, nc2, ne, 
  k0, kmin, kmax;
  REAL *a, *b, *c, fab1, fab2, fab3, sum, def1, def2, dgf, Cz, tempu,Ac,f_sum, ke1,ke2,Vm; //AB3
  // additions to test divergence averaging for w in momentum calc
  REAL wedge[3], lambda[3], wik, tmp_x, tmp_y,tmp;
  int aneigh;

  a = phys->a;
  b = phys->b;
  c = phys->c;

  // get the Adams-Bashforth multi-step integration started
  // fab is 1 for a forward Euler calculation on the first time step,
  // for which Cn_U is 0.  Otherwise, fab=3/2 and Cn_U contains the
  // Adams-Bashforth terms at time step n-1

 // Adams Bashforth coefficients
  if(prop->n==1 || prop->wetdry) {
    fab1=1;
    fab2=fab3=0;

    for(j=0;j<grid->Ne;j++)
      for(k=0;k<grid->Nke[j];k++)
        phys->Cn_U[j][k]=phys->Cn_U2[j][k]=0;
  } else if(prop->n==2) {
    fab1=3.0/2.0;
    fab2=-1.0/2.0;
    fab3=0;
  } else {
    fab1=prop->exfac1;
    fab2=prop->exfac2;
    fab3=prop->exfac3;   
  }

  // Set utmp and ut to zero since utmp will store the source term of the
  // horizontal momentum equation
  for(j=0;j<grid->Ne;j++) {
    for(k=0;k<grid->Nke[j];k++) {
      phys->utmp[j][k]=0;
      phys->ut[j][k]=0;
    }
  }

  // Update with old AB term
  // correct velocity based on non-hydrostatic pressure
  // over all computational edges
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr]; 

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    //AB3
    // for each edge over depth
    for(k=grid->etop[j];k<grid->Nke[j];k++) {
      // Equation 39: U_j,k^n+1 = U_j,k^* - dt*(qc_G2j,k - qc_G1j,k)/Dj
      // note that EE is used for the first time step
      phys->utmp[j][k]=fab2*phys->Cn_U[j][k]+fab3*phys->Cn_U2[j][k]+phys->u[j][k]
        -prop->dt/grid->dg[j]*(phys->q[nc1][k]-phys->q[nc2][k]);
      phys->Cn_U2[j][k]=phys->Cn_U[j][k];
      phys->Cn_U[j][k]=0;
    }
  }

  // Add on explicit term to boundary edges (type 4 BCs)
  for(jptr=grid->edgedist[4];jptr<grid->edgedist[5];jptr++) {
    j = grid->edgep[jptr]; 

    for(k=grid->etop[j];k<grid->Nke[j];k++) {
      phys->utmp[j][k]=fab3*phys->Cn_U2[j][k]+fab2*phys->Cn_U[j][k]+phys->u[j][k];
      phys->Cn_U2[j][k]=phys->Cn_U[j][k];
      phys->Cn_U[j][k]=0;
    }
  }

  // note that the above lines appear to allow use to "flush" Cn_U so
  // that it's values are utilizes and then it is free for additional
  // computations

  // Add on a momentum source to momentum equation
  // currently covers the sponge layer and can be used for Coriolis for the 
  // 2D problem
  MomentumSource(phys->utmp,grid,phys,prop);

  // 3D Coriolis terms
  // note that this uses linear interpolation to the faces from the cell centers
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) 
  {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];
    for(k=grid->etop[j];k<grid->Nke[j];k++)
    {
      // if not z-level add relative voricity due to momentum advection
      if(prop->vertcoord!=1 && prop->nonlinear && prop->wetdry)  
        f_sum=prop->Coriolis_f+vert->f_re[j][k];
      else
        f_sum=prop->Coriolis_f;
      phys->Cn_U[j][k]+=prop->dt*f_sum*(
          InterpToFace(j,k,phys->vc,phys->u,grid)*grid->n1[j]-
          InterpToFace(j,k,phys->uc,phys->u,grid)*grid->n2[j]);
    }
  }

  // Baroclinic term
  // over computational cells
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    // why only has baroclinic pressure when Nke=1
    if(prop->vertcoord==1 || prop->vertcoord==5){
      if(grid->etop[j]<grid->Nke[j]-1) 
        for(k=grid->etop[j];k<grid->Nke[j];k++) {
          // from the top of the water surface to the bottom edge layer for the given step
          for(k0=Max(grid->ctop[nc1],grid->ctop[nc2]);k0<k;k0++) {
            // for the Cn_U at the particular layer, integrate density gradient over the depth
            // using standard integration to the half cell (extra factor of 1/2 in last term)
            // this is probably only 1st order accurate for the integration
            phys->Cn_U[j][k]-=0.5*prop->grav*prop->dt*
              (phys->rho[nc1][k0]-phys->rho[nc2][k0])*
              (grid->dzz[nc1][k0]+grid->dzz[nc2][k0])/grid->dg[j];
          }
          phys->Cn_U[j][k]-=0.25*prop->grav*prop->dt*
            (phys->rho[nc1][k]-phys->rho[nc2][k])*
            (grid->dzz[nc1][k]+grid->dzz[nc2][k])/grid->dg[j];
        }
    } else {
      if(grid->etop[j]<grid->Nke[j]-1) 
        for(k=grid->etop[j];k<grid->Nke[j];k++) {
          // from the top of the water surface to the bottom edge layer for the given step
          for(k0=grid->etop[j];k0<k;k0++) {
            // for the Cn_U at the particular layer, integrate density gradient over the depth
            // using standard integration to the half cell (extra factor of 1/2 in last term)
            // this is probably only 1st order accurate for the integration

            phys->Cn_U[j][k]-=prop->grav*prop->dt*
              (phys->rho[nc1][k0]*grid->dzz[nc1][k0]-phys->rho[nc2][k0]*
              grid->dzz[nc2][k0])/grid->dg[j];
          }
          phys->Cn_U[j][k]-=0.5*prop->grav*prop->dt*
            (phys->rho[nc1][k]*grid->dzz[nc1][k]-phys->rho[nc2][k]*grid->dzz[nc2][k])/grid->dg[j];

        }

        for(k=grid->etop[j];k<grid->Nke[j];k++)
           phys->Cn_U[j][k]-=prop->dt*prop->grav*InterpToFace(j,k,phys->rho,phys->u,grid)*
             (vert->zc[nc1][k]-vert->zc[nc2][k])/grid->dg[j];
    }
  }

  // Set stmp and stmp2 to zero since these are used as temporary variables for advection and
  // diffusion.
  for(i=0;i<grid->Nc;i++)
    for(k=0;k<grid->Nk[i];k++) 
      phys->stmp[i][k]=phys->stmp2[i][k]=0;

  // new scheme for mometum advection if not z-level
  // keep the advection term conservative while add the additionterm u/J*dJ/dt
  // comment out since it is solved by implicit method, see function Upredictor

  if(prop->nonlinear && prop->vertcoord!=1)
  {
    if(!prop->wetdry)
    {
      if(vert->dJdtmeth==1)
      {
        for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) 
        {
           j = grid->edgep[jptr];
           nc1 = grid->grad[2*j];
           nc2 = grid->grad[2*j+1];
           def1 = grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
           def2 = grid->def[nc2*grid->maxfaces+grid->gradf[2*j+1]];
           dgf = def1+def2;
           for(k=grid->etop[j];k<grid->Nke[j];k++) 
             phys->Cn_U[j][k]-=phys->u[j][k]*
             (def2/dgf*(1-grid->dzzold[nc1][k]/grid->dzz[nc1][k])+def1/dgf*(1-grid->dzzold[nc2][k]/grid->dzz[nc2][k]));
        }
      }
    } else {
      for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) 
      {
        j = grid->edgep[jptr];
        nc1 = grid->grad[2*j];
        nc2 = grid->grad[2*j+1];
        def1 = grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
        def2 = grid->def[nc2*grid->maxfaces+grid->gradf[2*j+1]];
        for(k=grid->etop[j];k<grid->Nke[j];k++) 
        {  
          Vm=def1*grid->dzz[nc1][k]+def2*grid->dzz[nc2][k];
          ke1=(phys->uc[nc1][k]*phys->uc[nc1][k]+phys->vc[nc1][k]*phys->vc[nc1][k])*grid->dzz[nc1][k]/2;
          ke2=(phys->uc[nc2][k]*phys->uc[nc2][k]+phys->vc[nc2][k]*phys->vc[nc2][k])*grid->dzz[nc2][k]/2;
          phys->Cn_U[j][k]-=prop->dt*(ke1-ke2)/Vm;
        }  
      }
    }
  }  
  // Compute Eulerian advection of momentum (nonlinear!=0)
  if(prop->nonlinear && (prop->vertcoord==1 || (prop->vertcoord!=1 && !prop->wetdry))) 
  {

    // Interpolate uc to faces and place into ut
    GetMomentumFaceValues(phys->ut,phys->uc,phys->boundary_u,phys->u,grid,phys,prop,comm,myproc,prop->nonlinear);

    // Conservative method assumes ut is a flux
    if(prop->conserveMomentum)
      for(jptr=grid->edgedist[0];jptr<grid->edgedist[5];jptr++) 
      {
        j=grid->edgep[jptr];

        for(k=grid->etop[j];k<grid->Nke[j];k++){
          phys->ut[j][k]*=grid->dzf[j][k];
        }
      }

    // Now compute the cell-centered source terms and put them into stmp
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i=grid->cellp[iptr];
      // Store dzz in a since for conservative scheme need to divide by depth (since ut is a flux)
      if(prop->conserveMomentum) {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=grid->dzz[i][k];
      } else {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=1.0;
      }

      // for each face
      for(nf=0;nf<grid->nfaces[i];nf++) {

        // get the edge pointer
        ne = grid->face[i*grid->maxfaces+nf];

        // for all but the top cell layer
        for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        {
          // this is basically Eqn 50, u-component
          if(!prop->subgrid || prop->wetdry)
            Ac=grid->Ac[i];
          else
            Ac=subgrid->Acceff[i][k];
          phys->stmp[i][k]+=
            phys->ut[ne][k]*phys->u[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*Ac);         
        }  

        // Top cell is filled with momentum from neighboring cells
        if(prop->conserveMomentum)
          for(k=grid->etop[ne];k<grid->ctop[i];k++) 
          {
            if(!prop->subgrid || prop->wetdry)
              Ac=grid->Ac[i];
            else
              Ac=subgrid->Acceff[i][k];	  
            phys->stmp[i][grid->ctop[i]]+=
            phys->ut[ne][k]*phys->u[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[grid->ctop[i]]*Ac);
          }
      }
    }

    // Interpolate vc to faces and place into ut
    GetMomentumFaceValues(phys->ut,phys->vc,phys->boundary_v,phys->u,grid,phys,prop,comm,myproc,prop->nonlinear);

    // Conservative method assumes ut is a flux
    if(prop->conserveMomentum)
      for(jptr=grid->edgedist[0];jptr<grid->edgedist[5];jptr++) {
	     j=grid->edgep[jptr];
	     for(k=grid->etop[j];k<grid->Nke[j];k++)
	       phys->ut[j][k]*=grid->dzf[j][k];
      }

    // Now compute the cell-centered source terms and put them into stmp.
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i=grid->cellp[iptr];

      for(k=0;k<grid->Nk[i];k++) 
        phys->stmp2[i][k]=0;

      // Store dzz in a since for conservative scheme need to divide by depth (since ut is a flux)
      if(prop->conserveMomentum) {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=grid->dzz[i][k];
      } else {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=1.0;
      }

      for(nf=0;nf<grid->nfaces[i];nf++) {

        ne = grid->face[i*grid->maxfaces+nf];

        for(k=grid->ctop[i];k<grid->Nk[i];k++)
        {
          if(!prop->subgrid || prop->wetdry)
            Ac=grid->Ac[i];
          else
            Ac=subgrid->Acceff[i][k];
          // Eqn 50, v-component
          phys->stmp2[i][k]+=
            phys->ut[ne][k]*phys->u[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*Ac);
        }
        // Top cell is filled with momentum from neighboring cells
        if(prop->conserveMomentum)
          for(k=grid->etop[ne];k<grid->ctop[i];k++) 
          {
            if(!prop->subgrid || prop->wetdry)
              Ac=grid->Ac[i];
            else
              Ac=subgrid->Acceff[i][k];
            phys->stmp2[i][grid->ctop[i]]+=phys->ut[ne][k]*phys->u[ne][k]*
              grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*Ac);
          }
      }
    }

    // stmp2 holds the v-component of advection of momentum and
    // note that this could also be sped up by pulling out common
    // factors to reduce redundant calculations

    /* Vertical advection of momentum calc */
    // Only if thetaM<0, otherwise use implicit scheme in UPredictor()

    if(prop->thetaM<0 && prop->vertcoord==1) {
      // Now do vertical advection of momentum
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i=grid->cellp[iptr];
        switch(prop->nonlinear) {
      	  case 1:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = 0.5*((phys->w[i][k]+fabs(phys->w[i][k]))*phys->uc[i][k]+
                (phys->w[i][k]-fabs(phys->w[i][k]))*phys->uc[i][k-1]);
              b[k] = 0.5*((phys->w[i][k]+fabs(phys->w[i][k]))*phys->vc[i][k]+
                (phys->w[i][k]-fabs(phys->w[i][k]))*phys->vc[i][k-1]);
            }
            break;
          case 2: case 5:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = phys->w[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k-1]));
              b[k] = phys->w[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k-1]));
            }
            break;
        	case 4:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              Cz = 2.0*phys->w[i][k]*prop->dt/(grid->dzz[i][k]+grid->dzz[i][k-1]);
              a[k] = phys->w[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k-1])-
                0.5*Cz*(phys->uc[i][k-1]-phys->uc[i][k]));
              b[k] = phys->w[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k-1])-
                0.5*Cz*(phys->vc[i][k-1]-phys->vc[i][k]));
            }
            break;
        	default:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = 0.5*((phys->w[i][k]+fabs(phys->w[i][k]))*phys->uc[i][k]+
                (phys->w[i][k]-fabs(phys->w[i][k]))*phys->uc[i][k-1]);
              b[k] = 0.5*((phys->w[i][k]+fabs(phys->w[i][k]))*phys->vc[i][k]+
                (phys->w[i][k]-fabs(phys->w[i][k]))*phys->vc[i][k-1]);
            }
            break;
        }
        // Always do first-order upwind in bottom cell if partial stepping is on
        if(prop->stairstep==0) {
          k = grid->Nk[i]-1;
          a[k] = 0.5*(
            (phys->w[i][k]+fabs(phys->w[i][k]))*phys->uc[i][k]+
            (phys->w[i][k]-fabs(phys->w[i][k]))*phys->uc[i][k-1]);
          b[k] = 0.5*(
            (phys->w[i][k]+fabs(phys->w[i][k]))*phys->vc[i][k]+
            (phys->w[i][k]-fabs(phys->w[i][k]))*phys->vc[i][k-1]);
        }
        a[grid->ctop[i]]=phys->w[i][grid->ctop[i]]*phys->uc[i][grid->ctop[i]];
        b[grid->ctop[i]]=phys->w[i][grid->ctop[i]]*phys->vc[i][grid->ctop[i]];
        a[grid->Nk[i]]=0;
        b[grid->Nk[i]]=0;

        for(k=grid->ctop[i];k<grid->Nk[i];k++) {
          if(prop->subgrid)
          {
            // if wetting and drying happens use Ac insteady of acceff
            if(!prop->wetdry){
              phys->stmp[i][k]+=(a[k]*subgrid->Acveff[i][k]-a[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];
              phys->stmp2[i][k]+=(b[k]*subgrid->Acveff[i][k]-b[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];                
            } else {
              phys->stmp[i][k]+=(a[k]*subgrid->Acveff[i][k]-a[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/grid->Ac[i];
              phys->stmp2[i][k]+=(b[k]*subgrid->Acveff[i][k]-b[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/grid->Ac[i];                  
            }
          } else {
            phys->stmp[i][k]+=(a[k]-a[k+1])/grid->dzz[i][k];
            phys->stmp2[i][k]+=(b[k]-b[k+1])/grid->dzz[i][k];
          }
        }
      }
    } 
   
    // add explicit form for vertical momentum advection
    if(prop->thetaM<0 && prop->vertcoord!=1) {
      // Now do vertical advection of momentum
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i=grid->cellp[iptr];
        switch(prop->nonlinear) {
          case 1:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = 0.5*((vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->uc[i][k]+
                (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->uc[i][k-1]);
              b[k] = 0.5*((vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->vc[i][k]+
                (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->vc[i][k-1]);
            }
            break;
          case 2: case 5:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = vert->omega_old[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k-1]));
              b[k] = vert->omega_old[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k-1]));
            }
            break;

          case 4:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              Cz = 2.0*vert->omega_old[i][k]*prop->dt/(grid->dzz[i][k]+grid->dzz[i][k-1]);
              a[k] = vert->omega_old[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->uc[i][k-1])-
                0.5*Cz*(phys->uc[i][k-1]-phys->uc[i][k]));
              b[k] = vert->omega_old[i][k]*((grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k]+
                grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])*phys->vc[i][k-1])-
                0.5*Cz*(phys->vc[i][k-1]-phys->vc[i][k]));
            }
            break;
          default:
            for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
              a[k] = 0.5*((vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->uc[i][k]+
                (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->uc[i][k-1]);
              b[k] = 0.5*((vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->vc[i][k]+
                (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->vc[i][k-1]);
            }
            break;
        }
        
        // Always do first-order upwind in bottom cell if partial stepping is on
        if(prop->stairstep==0) {
          k = grid->Nk[i]-1;
          a[k] = 0.5*(
                      (vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->uc[i][k]+
                      (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->uc[i][k-1]);
          b[k] = 0.5*(
                      (vert->omega_old[i][k]+fabs(vert->omega_old[i][k]))*phys->vc[i][k]+
                      (vert->omega_old[i][k]-fabs(vert->omega_old[i][k]))*phys->vc[i][k-1]);
        }
        
        a[grid->ctop[i]]=vert->omega_old[i][grid->ctop[i]]*phys->uc[i][grid->ctop[i]];
        b[grid->ctop[i]]=vert->omega_old[i][grid->ctop[i]]*phys->vc[i][grid->ctop[i]];
        a[grid->Nk[i]]=0;
        b[grid->Nk[i]]=0;
        
        for(k=grid->ctop[i];k<grid->Nk[i];k++) {
          if(prop->subgrid)
          {
            // if wetting and drying happens use Ac insteady of acceff
            if(!prop->wetdry){
              phys->stmp[i][k]+=(a[k]*subgrid->Acveff[i][k]-a[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];
              phys->stmp2[i][k]+=(b[k]*subgrid->Acveff[i][k]-b[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];                
            } else {
              phys->stmp[i][k]+=(a[k]*subgrid->Acveff[i][k]-a[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/grid->Ac[i];
              phys->stmp2[i][k]+=(b[k]*subgrid->Acveff[i][k]-b[k+1]*subgrid->Acveff[i][k+1])/grid->dzz[i][k]/grid->Ac[i];                  
            }

          } else {
            phys->stmp[i][k]+=(a[k]-a[k+1])/grid->dzz[i][k];
            phys->stmp2[i][k]+=(b[k]-b[k+1])/grid->dzz[i][k];
          }
        }
      }
    } // end of nonlinear computation
  }

  // stmp and stmp2 just store the summed C_H and C_V values for horizontal
  // advection (prior to utilization via Eqn 41 or Eqn 47)

  /* Horizontal diffusion calculations */

  // now compute for no slip regions for type 4 boundary conditions
  for (jptr = grid->edgedist[4]; jptr < grid->edgedist[5]; jptr++)
  {
    // get index for edge pointers
    j = grid->edgep[jptr];
    //    ib=grid->grad[2*j];
    boundary_index = jptr-grid->edgedist[2];

    // get neighbor indices
    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    // check to see which of the neighboring cells is the ghost cell
    if (nc1 == -1)  // indicating boundary
      nc = nc2;
    else
      nc = nc1;

    // loop over the entire depth using a ghost cell for the ghost-neighbor
    // on type 4 boundary condition
    for (k=grid->ctop[nc]; k<grid->Nk[nc]; k++){
      if(!prop->subgrid){
        phys->stmp[nc][k]  += -2.0*prop->nu_H*(
          phys->boundary_u[boundary_index][k] - phys->uc[nc][k])/grid->dg[j]*
          grid->df[j]/grid->Ac[nc];
        phys->stmp2[nc][k] += -2.0*prop->nu_H*(
          phys->boundary_v[boundary_index][k] - phys->vc[nc][k])/grid->dg[j]*
          grid->df[j]/grid->Ac[nc];
      } else {
        phys->stmp[nc][k]  += -2.0*prop->nu_H*(
          phys->boundary_u[boundary_index][k] - phys->uc[nc][k])/grid->dg[j]*grid->dzf[j][k]*
          grid->df[j]/subgrid->Acceff[nc][k]/grid->dzz[nc][k];
        phys->stmp2[nc][k] += -2.0*prop->nu_H*(
          phys->boundary_v[boundary_index][k] - phys->vc[nc][k])/grid->dg[j]*grid->dzf[j][k]*
          grid->df[j]/subgrid->Acceff[nc][k]/grid->dzz[nc][k];        
      }
    }
  }

  // Now add on horizontal diffusion to stmp and stmp2
  // for the computational cells
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    if(nc1==-1) nc1=nc2;
    if(nc2==-1) nc2=nc1;

    if(grid->ctop[nc1]>grid->ctop[nc2])
      kmin = grid->ctop[nc1];
    else
      kmin = grid->ctop[nc2]; 

    for(k=kmin;k<grid->Nke[j];k++) {
      // Eqn 58 and Eqn 59
      // seems like nu_lax should be distance weighted as in Eqn 60
      a[k]=(prop->nu_H+0.5*(phys->nu_lax[nc1][k]+phys->nu_lax[nc2][k]))*
        (phys->uc[nc2][k]-phys->uc[nc1][k])*grid->df[j]/grid->dg[j];
      b[k]=(prop->nu_H+0.5*(phys->nu_lax[nc1][k]+phys->nu_lax[nc2][k]))*
        (phys->vc[nc2][k]-phys->vc[nc1][k])*grid->df[j]/grid->dg[j];

      if(!prop->subgrid)
      {
        phys->stmp[nc1][k]-=a[k]/grid->Ac[nc1];
        phys->stmp[nc2][k]+=a[k]/grid->Ac[nc2];
        phys->stmp2[nc1][k]-=b[k]/grid->Ac[nc1];
        phys->stmp2[nc2][k]+=b[k]/grid->Ac[nc2];
      }else{
        phys->stmp[nc1][k]-=a[k]/subgrid->Acceff[nc1][k]/grid->dzz[nc1][k]*grid->dzf[j][k];
        phys->stmp[nc2][k]+=a[k]/subgrid->Acceff[nc2][k]/grid->dzz[nc2][k]*grid->dzf[j][k];
        phys->stmp2[nc1][k]-=b[k]/subgrid->Acceff[nc1][k]/grid->dzz[nc1][k]*grid->dzf[j][k];
        phys->stmp2[nc2][k]+=b[k]/subgrid->Acceff[nc2][k]/grid->dzz[nc2][k]*grid->dzf[j][k];
      }
    }

    // compute wall-drag for diffusion of momentum BC (only on side walls)
    // Eqn 64 and Eqn 65 and Eqn 58
    for(k=Max(grid->Nke[j],grid->ctop[nc1]);k<grid->Nk[nc1];k++) {
      if(!prop->subgrid)
      {
        phys->stmp[nc1][k]+=
          prop->CdW*fabs(phys->uc[nc1][k])*phys->uc[nc1][k]*grid->df[j]/grid->Ac[nc1];
        phys->stmp2[nc1][k]+=
          prop->CdW*fabs(phys->vc[nc1][k])*phys->vc[nc1][k]*grid->df[j]/grid->Ac[nc1];
      } else {
        phys->stmp[nc1][k]+=
          prop->CdW*fabs(phys->uc[nc1][k])*phys->uc[nc1][k]*grid->df[j]*grid->dzf[j][k]/subgrid->Acceff[nc1][k]/grid->dzz[nc1][k];
        phys->stmp2[nc1][k]+=
          prop->CdW*fabs(phys->vc[nc1][k])*phys->vc[nc1][k]*grid->df[j]*grid->dzf[j][k]/subgrid->Acceff[nc1][k]/grid->dzz[nc1][k];
      }
    }

    for(k=Max(grid->Nke[j],grid->ctop[nc2]);k<grid->Nk[nc2];k++) {
      if(!prop->subgrid)
      {
        phys->stmp[nc2][k]+=
          prop->CdW*fabs(phys->uc[nc2][k])*phys->uc[nc2][k]*grid->df[j]/grid->Ac[nc2];
        phys->stmp2[nc2][k]+=
          prop->CdW*fabs(phys->vc[nc2][k])*phys->vc[nc2][k]*grid->df[j]/grid->Ac[nc2]; 
      } else {
        phys->stmp[nc2][k]+=
          prop->CdW*fabs(phys->uc[nc2][k])*phys->uc[nc2][k]*grid->df[j]*grid->dzf[j][k]/subgrid->Acceff[nc2][k]/grid->dzz[nc2][k];
        phys->stmp2[nc2][k]+=
          prop->CdW*fabs(phys->vc[nc2][k])*phys->vc[nc2][k]*grid->df[j]*grid->dzf[j][k]/subgrid->Acceff[nc2][k]/grid->dzz[nc2][k];         
      }
    }

  }

  // Check to make sure integrated fluxes are 0 for conservation
  // This will not be conservative if CdW or nu_H are nonzero!
  if(WARNING && prop->CdW==0 && prop->nu_H==0) {
    sum=0;
    for(i=0;i<grid->Nc;i++) {
      for(k=grid->ctop[i];k<grid->Nk[i];k++)
        if(!prop->subgrid)
          sum+=grid->Ac[i]*phys->stmp[i][k]*grid->dzz[i][k];
        else
          sum+=subgrid->Acceff[i][k]*phys->stmp[i][k]*grid->dzz[i][k];      
    }
    if(fabs(sum)>CONSERVED) printf("Warning, not U-momentum conservative!\n");

    sum=0;
    for(i=0;i<grid->Nc;i++) {
      for(k=grid->ctop[i];k<grid->Nk[i];k++)
        if(!prop->subgrid)
          sum+=grid->Ac[i]*phys->stmp2[i][k]*grid->dzz[i][k];
        else
          sum+=subgrid->Acceff[i][k]*phys->stmp2[i][k]*grid->dzz[i][k];
    }
    if(fabs(sum)>CONSERVED) printf("Warning, not V-momentum conservative!\n");
  }

  // Send/recv stmp and stmp2 to account for advective fluxes in ghost cells at
  // interproc boundaries.
  ISendRecvCellData3D(phys->stmp,grid,myproc,comm);
  ISendRecvCellData3D(phys->stmp2,grid,myproc,comm);

  // type 2 boundary condition (specified flux in)
  for(jptr=grid->edgedist[2];jptr<0*grid->edgedist[3];jptr++) {
    j = grid->edgep[jptr];

    i = grid->grad[2*j];
    // zero existing calculations for type 2 edge
    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      phys->stmp[i][k]=phys->stmp2[i][k]=0;

    sum=0;
    for(nf=0;nf<grid->nfaces[i];nf++) {
      if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1) {
           sum+=grid->Ac[nc];
        for(k=grid->ctop[nc];k<grid->Nk[nc];k++) {
          if(!prop->subgrid)
            Ac=grid->Ac[nc];
          else
            Ac=subgrid->Acceff[nc][k]*grid->dzz[nc][k];
          // get fluxes from other non-boundary cells
          phys->stmp[i][k]+=Ac*phys->stmp[nc][k];
          phys->stmp2[i][k]+=Ac*phys->stmp2[nc][k];
        }
      }
    }
    sum=1/sum;
    for(k=grid->ctop[i];k<grid->Nk[i];k++) {
      if(prop->subgrid)
      {
        sum=0;
        for(nf=0;nf<grid->nfaces[i];nf++)
          if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1)  
            if(k>=grid->ctop[nc])     
              sum+=subgrid->Acceff[nc][k]*grid->dzz[nc][k];
        sum=1/sum;
      } 
      // area-averaged calc
      phys->stmp[i][k]*=sum;
      phys->stmp2[i][k]*=sum;
    }
  }

  // computational cells
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr]; 

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];
    if(nc1==-1) nc1=nc2;
    if(nc2==-1) nc2=nc1;

    // Note that dgf==dg only when the cells are orthogonal!
    def1 = grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
    def2 = grid->def[nc2*grid->maxfaces+grid->gradf[2*j+1]];
    dgf = def1+def2;

    if(grid->ctop[nc1]>grid->ctop[nc2])
      k0=grid->ctop[nc1];
    else
      k0=grid->ctop[nc2];

    // compute momentum advection and diffusion contributions to Cn_U, note
    // the minus sign (why we needed it for the no-slip boundary condition)
    // for each face compute Cn_U performing averaging operation such as in
    // Eqn 41 and Eqn 42 and Eqn 55 etc.
    // the two equations correspond to the two adjacent cells to the edge and 
    // their contributions
    for(k=k0;k<grid->Nk[nc1];k++) 
      phys->Cn_U[j][k]-=def1/dgf
        *prop->dt*(phys->stmp[nc1][k]*grid->n1[j]+phys->stmp2[nc1][k]*grid->n2[j]);
    for(k=k0;k<grid->Nk[nc2];k++) 
      phys->Cn_U[j][k]-=def2/dgf
        *prop->dt*(phys->stmp[nc2][k]*grid->n1[j]+phys->stmp2[nc2][k]*grid->n2[j]);
  }

  // Now add on stmp and stmp2 from the boundaries 
  // for type 3 boundary condition
  for(jptr=grid->edgedist[3];jptr<grid->edgedist[4];jptr++) {
    j = grid->edgep[jptr]; 

    nc1 = grid->grad[2*j];
    k0=grid->ctop[nc1];

    for(nf=0;nf<grid->nfaces[nc1];nf++) {
      if((nc2=grid->neigh[nc1*grid->maxfaces+nf])!=-1) {
        ne=grid->face[nc1*grid->maxfaces+nf];
        for(k=k0;k<grid->Nk[nc1];k++) {
          phys->Cn_U[ne][k]-=
            grid->def[nc1*grid->maxfaces+nf]/grid->dg[ne]*
            prop->dt*(
                phys->stmp[nc2][k]*grid->n1[ne]+phys->stmp2[nc2][k]*grid->n2[ne]);
        }
      }
    }
  }

  // note that we now basically have the term dt*F_j,k in Equation 33
  // update utmp 
  // this will complete the adams-bashforth time stepping
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr]; 

    for(k=grid->etop[j];k<grid->Nke[j];k++){
      phys->utmp[j][k]+=fab1*phys->Cn_U[j][k];
    }  
  }
  
//  // check to make sure we don't have a blow-up
//  for(j=0;j<grid->Ne;j++) 
//    for(k=grid->etop[j];k<grid->Nke[j];k++) 
//      if(phys->utmp[j][k]!=phys->utmp[j][k]) {
//        printf("0Error in utmp at j=%d k=%d (U***=nan)\n",j,k);
//        exit(1);
//      }
}

/*
 * Function: NewCells
 * Usage: NewCells(grid,phys,prop);
 * --------------------------------
 * Adjust the velocity in the new cells that were previously dry.
 * This function is required for an Eulerian advection scheme because
 * it is difficult to compute the finite volume form of advection in the
 * upper cells when wetting and drying is employed.  In this function
 * the velocity in the new cells is set such that the quantity u*dz is
 * conserved from one time step to the next.  This works well without
 * wetting and drying.  When wetting and drying is employed, it is best
 * to extrapolate from the lower cells to obtain the velocity in the new
 * cells.
 *
 */
static void NewCells(gridT *grid, physT *phys, propT *prop) {
  int j, jptr, k;

  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    if(grid->etop[j]<grid->Nke[j]-1) {    // Only update new cells if there is more than one layer

      // If a new cell is created above the old cell then set the velocity in the new cell equal to that in
      // the old one.
      //
      // Otherwise if the free surface jumps up more than one cell then set the velocity equal to the cell beneath
      // the old one.

      if(grid->etop[j]==grid->etopold[j]-1) {
        phys->u[j][grid->etop[j]]=phys->u[j][grid->etopold[j]];
      } else { 
        for(k=grid->etop[j];k<=grid->etopold[j];k++)
          phys->u[j][k]=phys->u[j][grid->etopold[j]+1];
      }
    }
  }
}

/*
 * Function: WPredictor
 * Usage: WPredictor(grid,phys,prop,myproc,numprocs,comm);
 * -------------------------------------------------------
 * This function updates the vertical predicted velocity field with:
 *
 * 1) Old nonhydrostatic pressure gradient with theta method
 * 2) Horizontal and vertical advection of vertical momentum with AB2
 * 3) Horizontal laminar+turbulent diffusion of vertical momentum with AB2
 * 4) Vertical laminar+turbulent diffusion of vertical momentum with theta method
 *
 * Cn_W contains the Adams-Bashforth terms at time step n-1.
 * If wetting and drying is employed, no advection is computed in 
 * the upper cell.
 *
 */
static void WPredictor(gridT *grid, physT *phys, propT *prop,
    int myproc, int numprocs, MPI_Comm comm) {
  int i, ib, iptr, j, jptr, k, ne, nf, nc, nc1, nc2, kmin, boundary_index;
  REAL fab1,fab2,fab3, fac1,fac2,fac3, sum, *a, *b, *c, Cz;

  a = phys->a;
  b = phys->b;
  c = phys->c;

 // AB3
  if(prop->n==1) {
    fab1=1;
    fab2=fab3=0;
    for(i=0;i<grid->Nc;i++)
      for(k=0;k<grid->Nk[i];k++)
        phys->Cn_W[i][k]=phys->Cn_W2[i][k]=0;
  } else if(prop->n==2) {
    fab1=3.0/2.0;
    fab2=-1.0/2.0;
    fab3=0;
  }  else {
    fab1=prop->exfac1;
    fab2=prop->exfac2;
    fab3=prop->exfac3;   
  }

  fac1=prop->imfac1;
  fac2=prop->imfac2;
  fac3=prop->imfac3;

  // Add on the nonhydrostatic pressure gradient from the previous time
  // step to compute the source term for the tridiagonal inversion.
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr]; 

    for(k=grid->ctop[i];k<grid->Nk[i];k++) {
      // phys->wtmp[i][k]=phys->w[i][k]+(1-fab)*phys->Cn_W[i][k];
      //AB3
      phys->wtmp[i][k]=phys->w[i][k] + fab2*phys->Cn_W[i][k] + fab3*phys->Cn_W2[i][k];
      phys->Cn_W2[i][k]=phys->Cn_W[i][k];
      phys->Cn_W[i][k]=0;

    }

    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
      phys->wtmp[i][k]-=2.0*prop->dt/(grid->dzz[i][k-1]+grid->dzz[i][k])*
        (phys->q[i][k-1]-phys->q[i][k]);
    phys->wtmp[i][grid->ctop[i]]+=2.0*prop->dt/grid->dzz[i][grid->ctop[i]]*
      phys->q[i][grid->ctop[i]];
  }

  for(i=0;i<grid->Nc;i++)
    for(k=0;k<grid->Nk[i];k++) 
      phys->stmp[i][k]=0;

  // Compute Eulerian advection (nonlinear!=0)
  if(prop->nonlinear && (prop->vertcoord==1 || (prop->vertcoord!=1 && !prop->wetdry))) {
    // Compute the w-component fluxes at the faces
    
    // First compute w at the cell centers (since w is defined at the faces)
    for(i=0;i<grid->Nc;i++) {
      for(k=grid->ctop[i];k<grid->Nk[i];k++)
        phys->wc[i][k]=0.5*(phys->w[i][k]+phys->w[i][k+1]);
    }

    // Interpolate wc to faces and place into ut
    GetMomentumFaceValues(phys->ut,phys->wc,phys->boundary_w,phys->u_old,grid,phys,prop,comm,myproc,prop->nonlinear);
    
    if(prop->conserveMomentum)
      for(jptr=grid->edgedist[0];jptr<grid->edgedist[5];jptr++) {
        j=grid->edgep[jptr];

        for(k=grid->etop[j];k<grid->Nke[j];k++)
          phys->ut[j][k]*=grid->dzf[j][k];
      }

    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i=grid->cellp[iptr];

      // For conservative scheme need to divide by depth (since ut is a flux)
      if(prop->conserveMomentum) {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=grid->dzz[i][k];
      } else {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          a[k]=1.0;
      }

      for(nf=0;nf<grid->nfaces[i];nf++) {
        ne = grid->face[i*grid->maxfaces+nf];
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          if(!prop->subgrid || prop->wetdry)
            // this is basically Eqn 50, w-component
            phys->stmp[i][k]+=phys->ut[ne][k]*phys->u_old[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*grid->Ac[i]);
          else 
            phys->stmp[i][k]+=phys->ut[ne][k]*phys->u_old[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*subgrid->Acceff[i][k]); // consistent with dzz not dzzold

        // Top cell is filled with momentum from neighboring cells
        if(prop->conserveMomentum){
          for(k=grid->etop[ne];k<grid->ctop[i];k++)
            if(!prop->subgrid || prop->wetdry) 
              phys->stmp[i][grid->ctop[i]]+=phys->ut[ne][k]*phys->u_old[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*grid->Ac[i]);
            else
              phys->stmp[i][grid->ctop[i]]+=phys->ut[ne][k]*phys->u_old[ne][k]*grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/(a[k]*subgrid->Acceff[i][k]);
        }
      }

      // Vertical advection; note that in this formulation first-order upwinding is not implemented.
      // if not z-level, the vertical momentum advection part is added later
      if(prop->nonlinear==1 || prop->nonlinear==2 || prop->nonlinear==5) {
        for(k=grid->ctop[i];k<grid->Nk[i];k++) {
          // subgrid->Acceff and subgrid->Acveff have been updated to n+1 step.
          // explicit vertical advection, so use acceffold and acveffold, same as below.
          //????z
          if(prop->vertcoord==1)
          {
            if(!prop->subgrid || prop->wetdry)
              phys->stmp[i][k]+=(pow(phys->w[i][k],2)-pow(phys->w[i][k+1],2))/grid->dzz[i][k];
            else
              phys->stmp[i][k]+=(pow(phys->w[i][k],2)*subgrid->Acveffold[i][k]-pow(phys->w[i][k+1],2)*subgrid->Acveffold[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];            
          }else{
            if(!prop->subgrid)
              phys->stmp[i][k]+=(vert->omega_old[i][k]*phys->w[i][k]-vert->omega_old[i][k+1]*phys->w[i][k+1])/grid->dzz[i][k];
            else
              phys->stmp[i][k]+=(vert->omega_old[i][k]*phys->w[i][k]*subgrid->Acveffold[i][k]-
                vert->omega_old[i][k+1]*phys->w[i][k+1]*subgrid->Acveffold[i][k+1])/grid->dzz[i][k]/subgrid->Acceff[i][k];                 
          }
        }
      }
    }

    // Check to make sure integrated fluxes are 0 for conservation
    if(WARNING && prop->CdW==0 && prop->nu_H==0) {
      sum=0;
      for(i=0;i<grid->Nc;i++) {
        for(k=grid->ctop[i];k<grid->Nk[i];k++)
          if(!prop->subgrid || prop->wetdry)
            sum+=grid->Ac[i]*phys->stmp[i][k]*grid->dzz[i][k];
          else
            sum+=subgrid->Acceff[i][k]*phys->stmp[i][k]*grid->dzz[i][k];
      }
      if(fabs(sum)>CONSERVED) 
        printf("Warning, not W-momentum conservative!\n");
    }
  }

  // Add horizontal diffusion to stmp
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];
    if(grid->ctop[nc1]>grid->ctop[nc2])
      kmin = grid->ctop[nc1];
    else
      kmin = grid->ctop[nc2];

    for(k=kmin;k<grid->Nke[j];k++) {
      a[k]=.5*(prop->nu_H+0.5*(phys->nu_lax[nc1][k]+phys->nu_lax[nc2][k]))*
        (phys->w[nc2][k]-phys->w[nc1][k]+phys->w[nc2][k+1]-phys->w[nc1][k+1])*grid->df[j]/grid->dg[j];
      if(!prop->subgrid)
      {
        phys->stmp[nc1][k]-=a[k]/grid->Ac[nc1];
        phys->stmp[nc2][k]+=a[k]/grid->Ac[nc2];
      } else {
        phys->stmp[nc1][k]-=a[k]/subgrid->Acceff[nc1][k]/grid->dzz[nc1][k]*grid->dzf[j][k];
        phys->stmp[nc2][k]+=a[k]/subgrid->Acceff[nc2][k]/grid->dzz[nc2][k]*grid->dzf[j][k];        
      }
      //      phys->stmp[nc1][k]-=prop->nu_H*(phys->w[nc2][k]-phys->w[nc1][k])*grid->df[j]/grid->dg[j]/grid->Ac[nc1];
      //      phys->stmp[nc2][k]-=prop->nu_H*(phys->w[nc1][k]-phys->w[nc2][k])*grid->df[j]/grid->dg[j]/grid->Ac[nc2];
    }
    for(k=grid->Nke[j];k<grid->Nk[nc1];k++) 
      if(!prop->subgrid)
        phys->stmp[nc1][k]+=0.25*prop->CdW*fabs(phys->w[nc1][k]+phys->w[nc1][k+1])*
         (phys->w[nc1][k]+phys->w[nc1][k+1])*grid->df[j]/grid->Ac[nc1];
      else
        phys->stmp[nc1][k]+=0.25*prop->CdW*fabs(phys->w[nc1][k]+phys->w[nc1][k+1])*
         (phys->w[nc1][k]+phys->w[nc1][k+1])*grid->df[j]*grid->dzf[j][k]/grid->dzz[nc1][k]/subgrid->Acceff[nc1][k];
    for(k=grid->Nke[j];k<grid->Nk[nc2];k++) 
      if(!prop->subgrid)
        phys->stmp[nc2][k]+=0.25*prop->CdW*fabs(phys->w[nc2][k]+phys->w[nc2][k+1])*
         (phys->w[nc2][k]+phys->w[nc2][k+1])*grid->df[j]/grid->Ac[nc2];
      else
        phys->stmp[nc2][k]+=0.25*prop->CdW*fabs(phys->w[nc2][k]+phys->w[nc2][k+1])*
         (phys->w[nc2][k]+phys->w[nc2][k+1])*grid->df[j]*grid->dzf[j][k]/grid->dzz[nc2][k]/subgrid->Acceff[nc2][k];      
  }

  // do the same for type 4 boundary conditions but utilize no-slip boundary condition
  for (jptr = grid->edgedist[4]; jptr < grid->edgedist[5]; jptr++){
    // get index for edge pointers
    j = grid->edgep[jptr];
    ib=grid->grad[2*j];
    boundary_index = jptr-grid->edgedist[2];

    // get neighbor indices
    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    // check to see which of the neighboring cells is the ghost cell
    if (nc1 == -1)  // indicating boundary
      nc = nc2;
    else
      nc = nc1;

    // loop over the entire depth
    for (k=grid->ctop[nc]; k<grid->Nke[nc]; k++){
      if(!prop->subgrid)
        phys->stmp[nc][k]  += -2.0*prop->nu_H*
          (phys->boundary_w[boundary_index][k] - 0.5*(phys->w[nc][k] + phys->w[nc][k+1]))/grid->dg[j]*
          grid->df[j]/grid->Ac[nc];
      else
        phys->stmp[nc][k]  += -2.0*prop->nu_H*
          (phys->boundary_w[boundary_index][k] - 0.5*(phys->w[nc][k] + phys->w[nc][k+1]))/grid->dg[j]*
          grid->df[j]/subgrid->Acceff[nc][k]*grid->dzf[j][k]/grid->dzz[nc][k];
    }
  }

  //Now use the cell-centered advection terms to update the advection at the faces
  // if not use z-level, use non-conservative method udwdx+vdwdy
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr]; 

    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
      phys->Cn_W[i][k]-=prop->dt*(grid->dzz[i][k-1]*phys->stmp[i][k-1]+grid->dzz[i][k]*phys->stmp[i][k])/
        (grid->dzz[i][k-1]+grid->dzz[i][k]);

    // Top flux advection consists only of top cell
    k=grid->ctop[i];
    phys->Cn_W[i][k]-=prop->dt*phys->stmp[i][k];

    // add the additional part for the new vertical coordinate
    // comment out since it is solved by implicit method for stability
    if(prop->vertcoord!=1 && prop->nonlinear)
    {
      if(!prop->wetdry){
        if(vert->dJdtmeth==1)
        {
          for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
            phys->Cn_W[i][k]-=phys->w[i][k]*(grid->dzz[i][k]*(1-grid->dzzold[i][k-1]/grid->dzz[i][k-1])+
                grid->dzz[i][k-1]*(1-grid->dzzold[i][k]/grid->dzz[i][k]))/(grid->dzz[i][k]+grid->dzz[i][k-1]);
          k=grid->ctop[i];
          phys->Cn_W[i][k]-=phys->w[i][k]*(1-grid->dzzold[i][k]/grid->dzz[i][k]);
        }
      } else {
        for(k=grid->ctop[i]+1;k<grid->Nk[i];k++){
          // subtract the addition part of horizontal advection
          phys->Cn_W[i][k]-=prop->dt*(InterpToLayerTopFace(i,k,phys->uc,grid)*
                vert->dwdx[i][k]+InterpToLayerTopFace(i,k,phys->vc,grid)*vert->dwdy[i][k]);
        // add explicit vertical advection
        if(prop->thetaM<0)    
          phys->Cn_W[i][k]-=prop->dt*vert->omega_old[i][k]*(phys->w[i][k-1]-phys->w[i][k+1])/(grid->dzz[i][k]+grid->dzz[i][k-1]);
        }

        k=grid->ctop[i];
        phys->Cn_W[i][k]-=prop->dt*(InterpToLayerTopFace(i,k,phys->uc,grid)*vert->dwdx[i][k]+
                  InterpToLayerTopFace(i,k,phys->vc,grid)*vert->dwdy[i][k]);
        // top omega*dwdz
        // can be comment out since omega_top=0
        if(prop->thetaM<0)
          phys->Cn_W[i][k]-=prop->dt*vert->omega_old[i][k]*(phys->w[i][k]-phys->w[i][k+1])/grid->dzz[i][k];         
      }
    }
  }

  // Vertical advection using Lax-Wendroff
  if(prop->nonlinear==4 && (prop->vertcoord==1 || (prop->vertcoord!=1 && !prop->wetdry))) 
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr]; 
      if(prop->vertcoord==1)
        for(k=grid->ctop[i]+1;k<grid->Nk[i]+1;k++) {
          Cz = 0.5*(phys->w[i][k-1]+phys->w[i][k])*prop->dt/grid->dzz[i][k-1];
          a[k]=0.5*(phys->w[i][k-1]+phys->w[i][k])*(0.5*(phys->w[i][k-1]+phys->w[i][k])-0.5*Cz*(phys->w[i][k-1]-phys->w[i][k]));
        }
      else
        for(k=grid->ctop[i]+1;k<grid->Nk[i]+1;k++) {
          Cz = 0.5*(vert->omega_old[i][k-1]+vert->omega_old[i][k])*prop->dt/grid->dzz[i][k-1];
          a[k]=0.5*(vert->omega_old[i][k-1]+vert->omega_old[i][k])*(0.5*(phys->w[i][k-1]+phys->w[i][k])-0.5*Cz*(phys->w[i][k-1]-phys->w[i][k]));
        }        
      for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
        phys->Cn_W[i][k]-=2.0*prop->dt*(a[k]-a[k+1])/(grid->dzz[i][k]+grid->dzz[i][k+1]);
      }
    }

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr]; 

    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      phys->wtmp[i][k]+=fab1*phys->Cn_W[i][k];  //AB3
  }

  // wtmp now contains the right hand side without the vertical diffusion terms.  Now we
  // add the vertical diffusion terms to the explicit side and invert the tridiagonal for
  // vertical diffusion (only if grid->Nk[i]-grid->ctop[i]>=2)
  // no need to change for the new vertical coordinate
  // since the vertical diffusion is the same
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr]; 

    if(grid->Nk[i]-grid->ctop[i]>1) {
      for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) { // multiple layers
        a[k] = 2*(prop->nu+prop->laxWendroff_Vertical*phys->nu_lax[i][k-1]+
            phys->nu_tv[i][k-1])/grid->dzz[i][k-1]/(grid->dzz[i][k]+grid->dzz[i][k-1]);
        b[k] = 2*(prop->nu+prop->laxWendroff_Vertical*phys->nu_lax[i][k]+
            phys->nu_tv[i][k])/grid->dzz[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1]);
      }
      b[grid->ctop[i]]=(prop->nu+prop->laxWendroff_Vertical*phys->nu_lax[i][grid->ctop[i]]+
          phys->nu_tv[i][grid->ctop[i]])/pow(grid->dzz[i][grid->ctop[i]],2);
      a[grid->ctop[i]]=b[grid->ctop[i]];

      // Add on the explicit part of the vertical diffusion term
      // add the new implicit scheme
      for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
        phys->wtmp[i][k]+=prop->dt*(a[k]*(fac2*phys->w_old[i][k-1]+fac3*phys->w_old2[i][k-1])
            -(a[k]+b[k])*(fac2*phys->w_old[i][k]+fac3*phys->w_old2[i][k])
            +b[k]*(fac2*phys->w_old[i][k+1]+fac3*phys->w_old2[i][k+1]));
      phys->wtmp[i][grid->ctop[i]]+=prop->dt*(-(a[grid->ctop[i]]+b[grid->ctop[i]])*(fac2*phys->w_old[i][grid->ctop[i]]+fac3*phys->w_old2[i][grid->ctop[i]])
          +(a[grid->ctop[i]]+b[grid->ctop[i]])*(fac2*phys->w_old[i][grid->ctop[i]+1]+fac3*phys->w_old2[i][grid->ctop[i]+1]));

      // Now formulate the components of the tridiagonal inversion.
      // c is the diagonal entry, a is the lower diagonal, and b is the upper diagonal.
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        c[k]=1+prop->dt*fac1*(a[k]+b[k]);
        a[k]*=(-prop->dt*fac1);
        b[k]*=(-prop->dt*fac1);
      }
      b[grid->ctop[i]]+=a[grid->ctop[i]];

      // the w/JdJdt term for momentum advection
      // treat with implicit method
      // fully implicit
      if(prop->vertcoord!=1 && prop->nonlinear && !prop->wetdry)
        if(vert->dJdtmeth==0)
        {
          for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
            c[k]+=1*(grid->dzz[i][k]*(1-grid->dzzold[i][k-1]/grid->dzz[i][k-1])+
              grid->dzz[i][k-1]*(1-grid->dzzold[i][k]/grid->dzz[i][k]))/(grid->dzz[i][k]+grid->dzz[i][k-1]);     
            phys->wtmp[i][k]-=(0*phys->w_old[i][k]+0*phys->w_old2[i][k])*
              (grid->dzz[i][k]*(1-grid->dzzold[i][k-1]/grid->dzz[i][k-1])+
              grid->dzz[i][k-1]*(1-grid->dzzold[i][k]/grid->dzz[i][k]))/(grid->dzz[i][k]+grid->dzz[i][k-1]);
          }        
          k=grid->ctop[i];
          c[k]+=1*(1-grid->dzzold[i][k]/grid->dzz[i][k]);
          phys->wtmp[i][k]-=(0*phys->w_old[i][k]+0*phys->w_old2[i][k])*(1-grid->dzzold[i][k]/grid->dzz[i][k]);
        }

      // implicit method for vertical advection under generalized vertical coordinate with wetdry=1
      if(prop->nonlinear && prop->vertcoord!=1 && prop->wetdry && prop->thetaM>=0)
      {
        for(k=grid->ctop[i]+1;k<grid->Nk[i];k++)
        {
          phys->wtmp[i][k]-=prop->dt*vert->omega_old[i][k]*(fac2*phys->w_old[i][k-1]+fac3*phys->w_old2[i][k-1]-
                fac2*phys->w_old[i][k+1]-fac3*phys->w_old2[i][k+1])/(grid->dzz[i][k]+grid->dzz[i][k-1]);
          a[k]+=prop->dt*fac1*vert->omega_old[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1]);
          b[k]-=prop->dt*fac1*vert->omega_old[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1]);    
        }
        k=grid->ctop[i];
        phys->wtmp[i][k]-=prop->dt*vert->omega_old[i][k]*(fac2*phys->w_old[i][k]+fac3*phys->w_old2[i][k]-
                fac2*phys->w_old[i][k+1]-fac3*phys->w_old2[i][k+1])/grid->dzz[i][k]; 
        c[k]+=prop->dt*fac1*vert->omega_old[i][k]/grid->dzz[i][k];
        b[k]-=prop->dt*fac1*vert->omega_old[i][k]/grid->dzz[i][k];
      } 

      TriSolve(&(a[grid->ctop[i]]),&(c[grid->ctop[i]]),&(b[grid->ctop[i]]),
          &(phys->wtmp[i][grid->ctop[i]]),&(phys->w[i][grid->ctop[i]]),grid->Nk[i]-grid->ctop[i]);
    } else { // one layer
      for(k=grid->ctop[i];k<grid->Nk[i];k++)
        phys->w[i][k]=phys->wtmp[i][k];
    }
  }
}

/*
 * Function: Corrector
 * Usage: Corrector(qc,grid,phys,prop,myproc,numprocs,comm);
 * ---------------------------------------------------------
 * Correct the horizontal velocity field with the pressure correction.
 * Do not correct velocities for which D[j]==0 since these are boundary
 * cells.
 *
 * After correcting the horizontal velocity, update the total nonhydrostatic
 * pressure with the pressure correction.
 *
 */
static void Corrector(REAL **qc, gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm) {

  int i, iptr, j, jptr, k;

  // Correct the horizontal velocity only if this is not a boundary point.
  // no boundary points are corrected for non-hydrostatic pressure!!!
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr]; 
    if(phys->D[j]!=0 && grid->etop[j]<grid->Nke[j]-1)
      for(k=grid->etop[j];k<grid->Nke[j];k++)
        phys->u[j][k]-=prop->dt/grid->dg[j]*
          (qc[grid->grad[2*j]][k]-qc[grid->grad[2*j+1]][k]);
  }

  // Correct the vertical velocity
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr]; 
    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++)
      phys->w[i][k]-=2.0*prop->dt/(grid->dzz[i][k-1]+grid->dzz[i][k])*
        (qc[i][k-1]-qc[i][k]);
    phys->w[i][grid->ctop[i]]+=2.0*prop->dt/grid->dzz[i][grid->ctop[i]]*
      qc[i][grid->ctop[i]];
  }

  // Update the pressure since qc is a pressure correction
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    if(grid->ctop[i]<grid->Nk[i]-1)
      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        phys->q[i][k]+=qc[i][k];
  }
}

/*
 * Function: ComputeQSource
 * Usage: ComputeQSource(src,grid,phys,prop,myproc,numprocs);
 * ----------------------------------------------------------
 * Compute the source term for the nonhydrostatic pressure by computing
 * the divergence of the predicted velocity field, which is in phys->u and
 * phys->w.  The upwind flux face heights are used in order to ensure
 * consistency with continuity.
 *
 */
static void ComputeQSource(REAL **src, gridT *grid, physT *phys, propT *prop, int myproc, int numprocs) 
{

  int i, iptr, j, jptr, k, nf, ne, nc1, nc2;
  REAL *ap=phys->a, *am=phys->b, thetafactor=(1-prop->theta)/prop->theta,fac1,fac2,fac3,flux;

  // new implicit method
  fac1=prop->imfac1;
  fac2=prop->imfac2;
  fac3=prop->imfac3;
 
  // for each cell
  for(i=0;i<grid->Nc;i++)
    // initialize to zero
    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      src[i][k] = 0;

  // for each computational cell
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    // get the cell pointer
    i = grid->cellp[iptr];
    /* VERTICAL CONTRIBUTION */
    // over all cells that are defined to a depth
    if(prop->vertcoord==1)
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        // compute the vertical contributions to the source term
        if(!prop->subgrid)
          src[i][k] = grid->Ac[i]*(phys->w[i][k]-phys->w[i][k+1])
            +fac2/fac1*grid->Ac[i]*(phys->w_old[i][k]-phys->w_old[i][k+1])+fac3/fac1*grid->Ac[i]*(phys->w_old2[i][k]-phys->w_old2[i][k+1]);
        else
          src[i][k] = subgrid->Acveff[i][k]*phys->w[i][k]-subgrid->Acveff[i][k+1]*phys->w[i][k+1]
            +fac2/fac1*(subgrid->Acveffold[i][k]*phys->w_old[i][k]-subgrid->Acveffold[i][k+1]*phys->w_old[i][k+1])
            +fac3/fac1*(subgrid->Acveffold2[i][k]*phys->w_old2[i][k]-subgrid->Acveffold2[i][k+1]*phys->w_old2[i][k+1]);
      }
    else
      // modification for the new generalized vertical coordinate
      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        // compute the vertical contributions to the source term
        if(!prop->subgrid)
          src[i][k] = grid->Ac[i]*(vert->U3[i][k]-vert->U3[i][k+1])+
                fac2/fac1*grid->Ac[i]*(vert->U3_old[i][k]-vert->U3_old[i][k+1])+
                fac3/fac1*grid->Ac[i]*(vert->U3_old2[i][k]-vert->U3_old2[i][k+1]);
        else
          src[i][k] =subgrid->Acveff[i][k]*vert->U3[i][k]-subgrid->Acveff[i][k+1]*vert->U3[i][k+1]+
             fac2/fac1*(subgrid->Acveffold[i][k]*vert->U3_old[i][k]-subgrid->Acveffold[i][k+1]*vert->U3_old[i][k+1])+
             fac3/fac1*(subgrid->Acveffold2[i][k]*vert->U3_old2[i][k]-subgrid->Acveffold2[i][k+1]*vert->U3_old2[i][k+1]);
      }        

    /* HORIZONTAL CONTRIBUTION */
    // over each face to get the horizontal contributions to the source term
    // no change is needed for the new generalized vertical coordinate
    for(nf=0;nf<grid->nfaces[i];nf++) {

      // get the edge pointer
      ne = grid->face[i*grid->maxfaces+nf];
      // for each of the defined edges over depth
      for(k=grid->ctop[i];k<grid->Nke[ne];k++) 
        // compute the horizontal source term via the (D_H)(u^*)
        src[i][k]+=(phys->u[ne][k]+fac2/fac1*phys->u_old[ne][k]+fac3/fac1*
          phys->u_old2[ne][k])*grid->dzf[ne][k]*grid->normal[i*grid->maxfaces+nf]*grid->df[ne];
    }
   
 
    // divide final result by dt
    for(k=grid->ctop[i];k<grid->Nk[i];k++)
      src[i][k]/=prop->dt;
  }

  // D[j] is used in OperatorQ, and it must be zero to ensure no gradient
  // at the hydrostatic faces.
  // this can artificially be used to control the boundary condition
  for(j=0;j<grid->Ne;j++) {
    phys->D[j]=grid->df[j]/grid->dg[j];
  }
}

/*
 * Function: CGSolveQ
 * Usage: CGSolveQ(q,src,c,grid,phys,prop,myproc,numprocs,comm);
 * -------------------------------------------------------------
 * Solve for the nonhydrostatic pressure with the preconditioned
 * conjugate gradient algorithm.
 *
 * The preconditioner stores the diagonal preconditioning elements in 
 * the temporary c array.
 *
 * This function replaces q with x and src with p.  phys->uc and phys->vc
 * are used as temporary arrays as well to store z and r.
 *
 */
static void CGSolveQ(REAL **q, REAL **src, REAL **c, gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm) {

  int i, iptr, k, n, niters;

  REAL **x, **r, **rtmp, **p, **z, mu, nu, alpha, alpha0, eps, eps0;

  z = phys->stmp2;
  x = q;
  r = phys->stmp3;
  rtmp = phys->uold;
  p = src;

  // Compute the preconditioner and the preconditioned solution
  // and send it to neighboring processors
  if(prop->qprecond==1) {
    ConditionQ(c,grid,phys,prop,myproc,comm);
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        p[i][k]/=c[i][k];
        x[i][k]*=c[i][k];
      }
    }
  }
  ISendRecvCellData3D(x,grid,myproc,comm);

  niters = prop->qmaxiters;

  // Create the coefficients for the operator
  QCoefficients(phys->wtmp,phys->qtmp,c,grid,phys,prop);

  // Initialization for CG
  if(prop->qprecond==1) OperatorQC(phys->wtmp,phys->qtmp,x,z,c,grid,phys,prop);
  else OperatorQ(phys->wtmp,x,z,c,grid,phys,prop);
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      r[i][k] = p[i][k]-z[i][k];
  }    
  if(prop->qprecond==2) {
    Preconditioner(r,rtmp,phys->wtmp,grid,phys,prop);
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        p[i][k] = rtmp[i][k];
    }
    alpha = alpha0 = InnerProduct3(r,rtmp,grid,myproc,numprocs,comm);
  } else {
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        p[i][k] = r[i][k];
    }
    alpha = alpha0 = InnerProduct3(r,r,grid,myproc,numprocs,comm);
  }
  if(!prop->resnorm) alpha0 = 1;

  if(prop->qprecond==2)
    eps=eps0=InnerProduct3(r,r,grid,myproc,numprocs,comm);
  else
    eps=eps0=alpha0;

  // Iterate until residual is less than prop->qepsilon
  for(n=0;n<niters && eps!=0;n++) {

    ISendRecvCellData3D(p,grid,myproc,comm);
    if(prop->qprecond==1) OperatorQC(phys->wtmp,phys->qtmp,p,z,c,grid,phys,prop);
    else OperatorQ(phys->wtmp,p,z,c,grid,phys,prop);

    mu = 1/alpha;
    nu = alpha/InnerProduct3(p,z,grid,myproc,numprocs,comm);

    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      for(k=grid->ctop[i];k<grid->Nk[i];k++) {
        x[i][k] += nu*p[i][k];
        r[i][k] -= nu*z[i][k];
      }
    }
    if(prop->qprecond==2) {
      Preconditioner(r,rtmp,phys->wtmp,grid,phys,prop);
      alpha = InnerProduct3(r,rtmp,grid,myproc,numprocs,comm);
      mu*=alpha;
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];

        for(k=grid->ctop[i];k<grid->Nk[i];k++) 
          p[i][k] = rtmp[i][k] + mu*p[i][k];
      }
    } else {
      alpha = InnerProduct3(r,r,grid,myproc,numprocs,comm);
      mu*=alpha;
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];

        for(k=grid->ctop[i];k<grid->Nk[i];k++) 
          p[i][k] = r[i][k] + mu*p[i][k];
      }
    }

    if(prop->qprecond==2)
      eps=InnerProduct3(r,r,grid,myproc,numprocs,comm);
    else
      eps=alpha;

    //    if(myproc==0) printf("%d %e %e %e %e\n",myproc,mu,nu,eps0,sqrt(eps/eps0));
    if(VERBOSE>2 && myproc==0) printf("CGSolve Pressure Iteration: %d, resid=%e\n",n,sqrt(eps/eps0));
    if(sqrt(eps/eps0)<prop->qepsilon) 
      break;
  }

  if(myproc==0 && VERBOSE>2) {
    if(eps==0)
      printf("Warning...Time step %d, norm of pressure source is 0.\n",prop->n);
    else
      if(n==niters)  printf("Warning... Time step %d, Pressure iteration not converging after %d steps! RES=%e > %.2e\n",
          prop->n,n,sqrt(eps/eps0),prop->qepsilon);
      else printf("Time step %d, CGSolve pressure converged after %d iterations, res=%e < %.2e\n",
          prop->n,n,sqrt(eps/eps0),prop->qepsilon);
  }

  // Rescale the preconditioned solution 
  if(prop->qprecond==1) {
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
        x[i][k]/=c[i][k];
    }
  }

  // Send the solution to the neighboring processors
  ISendRecvCellData3D(x,grid,myproc,comm);
}

/*
 * Function: EddyViscosity
 * Usage: EddyViscosity(grid,phys,prop,w,comm,myproc);
 * ---------------------------------------------------
 * This function is used to compute the eddy viscosity, the
 * shear stresses, and the drag coefficients at the upper and lower
 * boundaries.
 *
 */
static void EddyViscosity(gridT *grid, physT *phys, propT *prop, REAL **wnew, MPI_Comm comm, int myproc)
{
  if(prop->turbmodel==1)
    my25(grid,phys,prop,wnew,phys->qT,phys->qT_old,phys->lT,phys->lT_old,phys->Cn_q,phys->Cn_l,phys->nu_tv,phys->kappa_tv,comm,myproc);
}

/*
 * Function: UPredictor 
 * Usage: UPredictor(grid,phys,prop,myproc,numprocs,comm);
 * -------------------------------------------------------
 * Predictor step for the horizontal velocity field.  This function
 * computes the free surface using the theta method and then uses 
 * it to update the predicted
 * velocity field in the absence of the nonhydrostatic pressure.
 *
 * Upon entry, phys->utmp contains the right hand side of the u-momentum equation
 *
 */
static void UPredictor(gridT *grid, physT *phys, 
    propT *prop, int myproc, int numprocs, MPI_Comm comm)
{
  int i, iptr, j, jptr, ne, nf, nf1, normal, nc1, nc2, k, n0, n1,iv,jv, botinterp,flag, Nkeb;
  REAL hmax,sum,sum0,sum1, dt=prop->dt, theta=prop->theta, h0, boundary_flag,fac1,fac2,fac3,tmp,tmp_x,tmp_y,tmp2;
  REAL *a, *b, *c, *d, *e1, **E, *a0, *b0, *c0, *d0, theta0, alpha,min,min2,def1,def2,dgf,l0,l1,zfb;

  a = phys->a;
  b = phys->b;
  c = phys->c;
  d = phys->d;
  e1 = phys->ap;
  E = phys->ut;

  a0 = phys->am;
  b0 = phys->bp;
  c0 = phys->bm;
  
  fac1=prop->imfac1;
  fac2=prop->imfac2;
  fac3=prop->imfac3;

  if(prop->n==1) {
    for(j=0;j<grid->Ne;j++)
      for(k=0;k<grid->Nke[j];k++){
        phys->u_old2[j][k]=phys->u[j][k];
      }
    for(i=0;i<grid->Nc;i++) 
      for(k=0;k<grid->Nk[i]+1;k++) 
      {
        phys->w_old2[i][k]=phys->w[i][k];
        phys->w_im[i][k]=0;
      }
    for(i=0;i<grid->Nc;i++)
      phys->h_old[i]=phys->h[i]; 
  }

  // Set D[j] = 0 
  for(i=0;i<grid->Nc;i++) 
    for(k=0;k<grid->Nk[i]+1;k++) 
      phys->w_old[i][k]=phys->w[i][k];

  for(j=0;j<grid->Ne;j++) {
    phys->D[j]=0;
    for(k=0;k<grid->Nke[j];k++)
      phys->u_old[j][k]=phys->u[j][k];
  }

  // note that phys->utmp is the horizontalsource term computed 
  // in HorizontalSource for 1/2(3F_j,k^n -F_j,k^n-1).
  // can be AB3

  // phys->u contains the velocity specified at the open boundaries
  // It is also the velocity at time step n. (type 2)
  for(jptr=grid->edgedist[2];jptr<grid->edgedist[3];jptr++) {
    j = grid->edgep[jptr];

    // transfer boundary flux velocities onto the horizontal source
    // term (exact as specified)
    for(k=grid->etop[j];k<grid->Nke[j];k++) 
      phys->utmp[j][k]=phys->u[j][k];
  }

  // Update the velocity in the interior nodes with the old free-surface gradient
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    // Add the explicit part of the free-surface to create U**.
    // 5th term of Eqn 31
    for(k=grid->etop[j];k<grid->Nke[j];k++){
      phys->utmp[j][k]-=
        prop->grav*dt*(fac2*(phys->h[nc1]-phys->h[nc2])+fac3*(phys->h_old[nc1]-phys->h_old[nc2]))/grid->dg[j];
    }
  }

  // Drag term must be fully implicit
  theta0=theta;
  theta=1;

  // Advection term for vertical momentum.  When alpha=1, first-order upwind,
  // alpha=0 is second-order central.  Always do first-order upwind when doing
  // vertically-implicit momentum advection
  alpha=1;
  //alpha=0;

  // for each of the computational edges
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    if(nc1==-1)
      nc1=nc2;
    if(nc2==-1)
      nc2=nc1;
    
    def1 = grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
    def2 = grid->def[nc2*grid->maxfaces+grid->gradf[2*j+1]];

    // Add the wind shear stress from the top cell
    phys->utmp[j][grid->etop[j]]+=2.0*dt*phys->tau_T[j]/
      (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]]);

    // define the bottom layer
    if(prop->vertcoord==1 || phys->CdB[j]==-1)
      Nkeb=grid->Nke[j]-1;
    else
      Nkeb=vert->Nkeb[j];

    // Create the tridiagonal entries and formulate U***
    // provided that we don't have a zero-depth top cell
    if(!(grid->dzz[nc1][grid->etop[j]]==0 && grid->dzz[nc2][grid->etop[j]]==0)) {
      // initialize coefficients
      for(k=grid->etop[j];k<grid->Nke[j];k++) {
        a[k]=0;
        b[k]=0;
        c[k]=0;
        d[k]=0;
      }

      // Vertical eddy-viscosity interpolated to faces since it is stored
      // at cell-centers.
      for(k=grid->etop[j]+1;k<grid->Nke[j];k++){ 
        c[k]=0.25*(phys->nu_tv[nc1][k-1]+phys->nu_tv[nc2][k-1]+
            phys->nu_tv[nc1][k]+phys->nu_tv[nc2][k]+
            prop->laxWendroff_Vertical*(phys->nu_lax[nc1][k-1]+phys->nu_lax[nc2][k-1]+
              phys->nu_lax[nc1][k]+phys->nu_lax[nc2][k]));
      }

      // Coefficients for the viscous terms.  Face heights are taken as
      // the average of the face heights on either side of the face (not upwinded).
      for(k=grid->etop[j]+1;k<grid->Nke[j];k++) 
        a[k]=2.0*(prop->nu+c[k])/(0.25*(grid->dzz[nc1][k]+grid->dzz[nc2][k])*
            (grid->dzz[nc1][k-1]+grid->dzz[nc2][k-1]+
             grid->dzz[nc1][k]+grid->dzz[nc2][k]));

      for(k=grid->etop[j];k<grid->Nke[j]-1;k++) {
        b[k]=2.0*(prop->nu+c[k+1])/(0.25*(grid->dzz[nc1][k]+grid->dzz[nc2][k])*
            (grid->dzz[nc1][k]+grid->dzz[nc2][k]+
             grid->dzz[nc1][k+1]+grid->dzz[nc2][k+1]));
      }

      // Coefficients for vertical momentum advection terms
      // d[] stores vertical velocity interpolated to faces vertically half-way between U locations
      // So d[k] contains w defined at the vertical w-location of cell k
      if(prop->vertcoord==1 && prop->nonlinear && 
        prop->thetaM>=0 && grid->Nke[j]-grid->etop[j]>1) {
        if(grid->ctop[nc1]>grid->ctop[nc2]) {
          n0=nc2;
          n1=nc1;
        } else {
          n0=nc1;
          n1=nc2;
        }
        // Don't do advection on vertical faces without water on both sides.
        for(k=0;k<grid->ctop[n1];k++)
          d[k]=0;
        for(k=grid->ctop[n1];k<grid->Nke[j];k++)
          d[k] = 0.5*(phys->w[n0][k]+phys->w[n1][k]);
        d[grid->Nke[j]]=0; // Assume w=0 at a corners (even if w is nonzero on one side of the face)
        for(k=grid->etop[j];k<grid->Nke[j];k++) {
          a0[k] = (alpha*0.5*(d[k]-fabs(d[k])) + 0.5*(1-alpha)*d[k])/(0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]));
          b0[k] = (alpha*0.5*(d[k]+fabs(d[k])-d[k+1]+fabs(d[k+1]))+0.5*(1-alpha)*(d[k]-d[k+1]))/(0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]));
          c0[k] = -(alpha*0.5*(d[k+1]+fabs(d[k+1])) + 0.5*(1-alpha)*d[k+1])/(0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]));			
        }
      }

      // add the vertical momentum advection part for the new general vertical coordinate
      // the vertical momentum is divided into two parts
      // wdudz=d(wu)dz-udwdz
      // the first part is the same as the original method while exchange w into omega
      if(prop->vertcoord!=1 && prop->nonlinear && grid->Nke[j]-grid->etop[j]>1 && prop->thetaM>=0)
      {
        // may be useless for vertical coordinate without z-level
        if(grid->ctop[nc1]>grid->ctop[nc2]) {
          n0=nc2;
          n1=nc1;
          l0=def2;
          l1=def1;
        } else {
          n0=nc1;
          n1=nc2;
          l0=def1;
          l1=def2;
        }

        // Don't do advection on vertical faces without water on both sides.
        for(k=0;k<grid->ctop[n1];k++)
          d[k]=0;
        // omega[top]=0 omega[bot]=0
        for(k=grid->ctop[n1];k<grid->Nke[j];k++)
          d[k] =(l1*vert->omega_old[n0][k]+l0*vert->omega_old[n1][k])/grid->dg[j];
        d[grid->Nke[j]]=0; // Assume w=0 at a corners (even if w is nonzero on one side of the face)
        for(k=grid->etop[j];k<grid->Nke[j];k++) 
        {
          a0[k] = (d[k]-fabs(d[k]))/(grid->dzz[nc1][k]+grid->dzz[nc2][k]);
          b0[k] = (d[k]+fabs(d[k])-d[k+1]+fabs(d[k+1]))/(grid->dzz[nc1][k]+grid->dzz[nc2][k]);
          c0[k] = -(d[k+1]+fabs(d[k+1]))/(grid->dzz[nc1][k]+grid->dzz[nc2][k]);                        
        }
      }

      // add on explicit diffusion to RHS (utmp)
      if(grid->Nke[j]-grid->etop[j]>1) { // more than one vertical layer on edge
        // Explicit part of the viscous term over wetted parts of edge
        // for the interior cells
        // for the new vertical coordinate, the bottom layer should be the layer zfb>bufferheight since interior dzf can be zero.
        for(k=grid->etop[j]+1;k<Nkeb;k++)
            phys->utmp[j][k]+=dt*(1-theta)*(a[k]*phys->u[j][k-1]-(a[k]+b[k])*phys->u[j][k]+b[k]*phys->u[j][k+1]);

        // Top cell
        // account for no slip conditions which are assumed if CdT = -1 
        if(phys->CdT[j] == -1){ // no slip on top
          phys->utmp[j][grid->etop[j]]+=
            dt*(1-theta)*(a[grid->etop[j]]*-phys->u[j][grid->etop[j]]-
              (a[grid->etop[j]]+b[grid->etop[j]])*phys->u[j][grid->etop[j]]+
              b[grid->etop[j]]*phys->u[j][grid->etop[j]+1]);
        }
        else{ // standard drag law code
          phys->utmp[j][grid->etop[j]]+=dt*(1-theta)*(-(b[grid->etop[j]]+2.0*phys->CdT[j]*
                fabs(phys->u[j][grid->etop[j]])/
                (grid->dzz[nc1][grid->etop[j]]+
                 grid->dzz[nc2][grid->etop[j]]))*
              phys->u[j][grid->etop[j]]
              +b[grid->etop[j]]*phys->u[j][grid->etop[j]+1]);
        }

        // Bottom cell
        // account for no slip conditions which are assumed if CdB = -1
        if(phys->CdB[j] == -1){ // no slip on bottom
         // some sort of strange error here... in previous code, now fixed 
         // phys->utmp[j][grid->etop[j]]-=2.0*dt*(1-theta)*(phys->CdB[j]+phys->CdT[j])/
         //(grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
        //fabs(phys->u[j][grid->etop[j]])*phys->u[j][grid->etop[j]];
          phys->utmp[j][grid->Nke[j]-1]+=dt*(1-theta)*(a[grid->Nke[j]-1]*phys->u[j][grid->Nke[j]-2]-
              (a[grid->Nke[j]-1]+b[grid->Nke[j]-1])*phys->u[j][grid->Nke[j]-1]+
              b[grid->Nke[j]-1]*-phys->u[j][grid->Nke[j]-1]);

        } else{ 
          // standard drag law code
          if(prop->vertcoord==1)
            if(!prop->subgrid)
              phys->utmp[j][grid->Nke[j]-1]+=dt*(1-theta)*(
                  a[grid->Nke[j]-1]*phys->u[j][grid->Nke[j]-2] -
                  (a[grid->Nke[j]-1] 
                   + 2.0*phys->CdB[j]*fabs(phys->u[j][grid->Nke[j]-1])/
                   (grid->dzz[nc1][grid->Nke[j]-1]+
                    grid->dzz[nc2][grid->Nke[j]-1]))*
                  phys->u[j][grid->Nke[j]-1]);
            else
              phys->utmp[j][grid->Nke[j]-1]+=dt*(1-theta)*(
                  a[grid->Nke[j]-1]*phys->u[j][grid->Nke[j]-2] -
                  (a[grid->Nke[j]-1] 
                   + phys->CdB[j]*fabs(phys->u[j][grid->Nke[j]-1])/
                  subgrid->dzboteff[j])*phys->u[j][grid->Nke[j]-1]);
           else
           {
              if(!prop->subgrid)
                // apply drag forcing at the layer where zfb>buffer height
                phys->utmp[j][Nkeb]+=dt*(1-theta)*(
                  a[Nkeb]*phys->u[j][Nkeb-1] -
                  (a[Nkeb]+2.0*phys->CdB[j]*fabs(phys->u[j][Nkeb])/
                   (grid->dzz[nc1][Nkeb]+grid->dzz[nc2][Nkeb]))*phys->u[j][Nkeb]);
              else
                phys->utmp[j][Nkeb]+=dt*(1-theta)*(
                  a[Nkeb]*phys->u[j][Nkeb-1] -
                  (a[Nkeb]+phys->CdB[j]*fabs(phys->u[j][Nkeb])/
                   subgrid->dzboteff[j])*phys->u[j][Nkeb]);                
             // for other cell give a 100 drag coefficient
             for(k=Nkeb+1;k<grid->Nke[j];k++)
                phys->utmp[j][k]+=-dt*(1-theta)*2.0*100*fabs(phys->u[j][k])/
                   (grid->dzz[nc1][k]+grid->dzz[nc2][k])*phys->u[j][k];
           }
        }
      } else {  // one layer for edge
        // drag on bottom boundary
        if(phys->CdB[j] == -1){ // no slip on bottom
          phys->utmp[j][grid->etop[j]]-=2.0*dt*(1-theta)*(
              2.0*(2.0*(prop->nu + c[k]))*phys->u[j][grid->etop[j]]/
              ((grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
               (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])));
        }
        else{ // standard drag law formation on bottom
          // need to change for the new vertical coordinate if there is only one layer
          // same as the original model
          // subgrid part
          if(!prop->subgrid)
            phys->utmp[j][grid->etop[j]]-=2.0*dt*(1-theta)*(phys->CdB[j])/
              (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
              fabs(phys->u[j][grid->etop[j]])*phys->u[j][grid->etop[j]];
          else
            phys->utmp[j][grid->etop[j]]-=dt*(1-theta)*(phys->CdB[j])/
              subgrid->dzboteff[j]*fabs(phys->u[j][grid->etop[j]])*phys->u[j][grid->etop[j]];         
        }

        // drag on top boundary
        if(phys->CdT[j] == -1){ // no slip on top
          phys->utmp[j][grid->etop[j]]-=2.0*dt*(1-theta)*(
              2.0*(2.0*(prop->nu + c[k]))*phys->u[j][grid->etop[j]]/
              ((grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
               (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])));
        }
        else{ // standard drag law formulation on top
          if(!prop->subgrid)
            phys->utmp[j][grid->etop[j]]-=2.0*dt*(1-theta)*(phys->CdT[j])/
              (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
              fabs(phys->u[j][grid->etop[j]])*phys->u[j][grid->etop[j]];
          else
            phys->utmp[j][grid->etop[j]]-=dt*(1-theta)*(phys->CdT[j])/
              subgrid->dzboteff[j]*fabs(phys->u[j][grid->etop[j]])*phys->u[j][grid->etop[j]];          
        }
      }

      // add marsh explicit term
      if(!prop->subgrid){
        if(prop->marshmodel)
          MarshExplicitTerm(grid,phys,prop,j,theta,dt,myproc);
      }else{
        if(!subgrid->dragpara && prop->marshmodel)
          MarshExplicitTerm(grid,phys,prop,j,theta,dt,myproc);
      }

      // add on explicit vertical momentum advection only if there is more than one vertical layer edge.
      if(prop->vertcoord==1 && prop->nonlinear && prop->thetaM>=0 && grid->Nke[j]-grid->etop[j]>1) {
        for(k=grid->etop[j]+1;k<grid->Nke[j]-1;k++)
          phys->utmp[j][k]-=prop->dt*(1-prop->thetaM)*(a0[k]*phys->u[j][k-1]+b0[k]*phys->u[j][k]+c0[k]*phys->u[j][k+1]);
	
      	// Top boundary
        phys->utmp[j][grid->etop[j]]-=prop->dt*(1-prop->thetaM)*((a0[grid->etop[j]]+b0[grid->etop[j]])*phys->u[j][grid->etop[j]]+c0[grid->etop[j]]*phys->u[j][grid->etop[j]+1]);
	
      	// Bottom boundary
        phys->utmp[j][grid->Nke[j]-1]-=prop->dt*(1-prop->thetaM)*(a0[grid->Nke[j]-1]*phys->u[j][grid->Nke[j]-2]+(b0[grid->Nke[j]-1]+c0[grid->Nke[j]-1])*phys->u[j][grid->Nke[j]-1]);
      }

      // add new vertical momentum advection for the general vertical coordinate
      // here phys->u still store u^n 
      if(prop->vertcoord!=1 && prop->nonlinear && prop->thetaM>=0 && grid->Nke[j]-grid->etop[j]>1)
      {
        // conservative form part
        for(k=grid->etop[j]+1;k<grid->Nke[j]-1;k++)
          phys->utmp[j][k]-=prop->dt*(a0[k]*(fac2*phys->u_old[j][k-1]+fac3*phys->u_old2[j][k-1])+
                b0[k]*(fac2*phys->u_old[j][k]+fac3*phys->u_old2[j][k])+c0[k]*(fac2*phys->u_old[j][k+1]+fac3*phys->u_old2[j][k+1]));
        // Top boundary
        phys->utmp[j][grid->etop[j]]-=prop->dt*((a0[grid->etop[j]]+b0[grid->etop[j]])*
                (fac2*phys->u_old[j][grid->etop[j]]+fac3*phys->u_old2[j][grid->etop[j]])+
                c0[grid->etop[j]]*(fac2*phys->u_old[j][grid->etop[j]+1]+fac3*phys->u_old2[j][grid->etop[j]+1]));
        
        // Bottom boundary
        phys->utmp[j][grid->Nke[j]-1]-=prop->dt*(a0[grid->Nke[j]-1]*
                (fac2*phys->u_old[j][grid->Nke[j]-2]+fac3*phys->u_old2[j][grid->Nke[j]-2])+
                (b0[grid->Nke[j]-1]+c0[grid->Nke[j]-1])*(fac2*phys->u_old[j][grid->Nke[j]-1]+fac3*phys->u_old2[j][grid->Nke[j]-1]));
      
        // second part udomegadz
        if(prop->wetdry)
          for(k=grid->etop[j];k<grid->Nke[j];k++)
            phys->utmp[j][k]+=prop->dt*(fac2*phys->u[j][k]+fac3*phys->u_old2[j][k])*
            (def2*(vert->omega_old[nc1][k]-vert->omega_old[nc1][k+1])+def1*(vert->omega_old[nc2][k]-vert->omega_old[nc2][k+1]))/
            grid->dg[j]/(0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]));
      }

      // Now set up the coefficients for the tridiagonal inversion for the
      // implicit part.  These are given from the arrays above in the discrete operator
      // d^2U/dz^2 = -theta dt a_k U_{k-1} + (1+theta dt (a_k+b_k)) U_k - theta dt b_k U_{k+1}
      // = RHS of utmp

      // Right hand side U** is given by d[k] here.
      for(k=grid->etop[j];k<grid->Nke[j];k++) {
        e1[k]=1.0;
        d[k]=phys->utmp[j][k];
      }


      if(grid->Nke[j]-grid->etop[j]>1) { // for more than one vertical layer
        // Top cells
        c[grid->etop[j]]=-theta*dt*b[grid->etop[j]];
        // account for no slip conditions which are assumed if CdT = -1 
        if(phys->CdT[j] == -1){ // no slip
          b[grid->etop[j]]=1.0+theta*dt*(a[grid->etop[j]]+a[grid->etop[j]+1]+b[grid->etop[j]]);
        } else { // standard drag law
          b[grid->etop[j]]=1.0+theta*dt*(b[grid->etop[j]]+
              2.0*phys->CdT[j]*fabs(phys->u[j][grid->etop[j]])/
              (grid->dzz[nc1][grid->etop[j]]+
               grid->dzz[nc2][grid->etop[j]]));
        }
        a[grid->etop[j]]=0;     // set a_1=0 (not used in tridiag solve)

        // Bottom cell
        c[grid->Nke[j]-1]=0;   // set c_N=0 (not used in tridiag solve)
        // account for no slip conditions which are assumed if CdB = -1  
        if(phys->CdB[j] == -1){ // no slip
          b[grid->Nke[j]-1]=1.0+theta*dt*(a[grid->Nke[j]-1]+b[grid->Nke[j]-1]+b[grid->Nke[j]-2]);
        } else { 
          // standard drag law
          if(prop->vertcoord==1)
            if(!prop->subgrid)
              b[grid->Nke[j]-1]=1.0+theta*dt*(a[grid->Nke[j]-1]+
                  2.0*phys->CdB[j]*fabs(phys->u[j][grid->Nke[j]-1])/
                  (grid->dzz[nc1][grid->Nke[j]-1]+
                   grid->dzz[nc2][grid->Nke[j]-1]));
            else
              b[grid->Nke[j]-1]=1.0+theta*dt*(a[grid->Nke[j]-1]+
                  phys->CdB[j]*fabs(phys->u[j][grid->Nke[j]-1])/
                  subgrid->dzboteff[j]);       
          else
          {
            if(!prop->subgrid)
              b[Nkeb]=1.0+theta*dt*(a[Nkeb]+
                2.0*phys->CdB[j]*fabs(phys->u[j][Nkeb])/
                (grid->dzz[nc1][Nkeb]+
                grid->dzz[nc2][Nkeb]));
            else
              b[Nkeb]=1.0+theta*dt*(a[Nkeb]+
                phys->CdB[j]*fabs(phys->u[j][Nkeb])/
                subgrid->dzboteff[j]);    
            // for the layer below the effective layer (zc>bufferheight) give 100 drag coefficient
            for(k=Nkeb+1;k<grid->Nke[j];k++){
              b[k]=1.0+theta*dt*2.0*100*fabs(phys->u[j][k])/
                (grid->dzz[nc1][k]+grid->dzz[nc2][k]);
              a[k]=0;
              c[k]=0;
            }
          }
        }
        if(prop->vertcoord==1)
        {
          a[grid->Nke[j]-1]=-theta*dt*a[grid->Nke[j]-1];
          // Interior cells
          for(k=grid->etop[j]+1;k<grid->Nke[j]-1;k++) {
            c[k]=-theta*dt*b[k];
            b[k]=1.0+theta*dt*(a[k]+b[k]);
            a[k]=-theta*dt*a[k];
          }
        } else {
          // defined bottom cell
          a[Nkeb]=-theta*dt*a[Nkeb];
          c[Nkeb]=0;
          // interior cell
          for(k=grid->etop[j]+1;k<Nkeb;k++) {
            c[k]=-theta*dt*b[k];
            b[k]=1.0+theta*dt*(a[k]+b[k]);
            a[k]=-theta*dt*a[k];
          }            
        }
      } else {

        // for a single vertical layer
        b[grid->etop[j]] = 1.0;

        // account for no slip conditions which are assumed if CdB = -1  
        if(phys->CdB[j] == -1){ // no slip
          b[grid->etop[j]]+=4.0*theta*dt*2.0*(prop->nu+c[k])/
            ((grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
             (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]]));
        }
        else{
          if(!prop->subgrid)
            b[grid->etop[j]]+=2.0*theta*dt*fabs(phys->utmp[j][grid->etop[j]])/
              (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
              (phys->CdB[j]);
          else
            b[grid->etop[j]]+=theta*dt*fabs(phys->utmp[j][grid->etop[j]])/
              subgrid->dzboteff[j]*(phys->CdB[j]); 
        }

        // account for no slip conditions which are assumed if CdT = -1 
        if(phys->CdT[j] == -1){
          b[grid->etop[j]]+=4.0*theta*dt*2.0*(prop->nu+c[k])/
            ((grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
             (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]]));
        }
        else{
          if(!prop->subgrid)
            b[grid->etop[j]]+=2.0*theta*dt*fabs(phys->utmp[j][grid->etop[j]])/
              (grid->dzz[nc1][grid->etop[j]]+grid->dzz[nc2][grid->etop[j]])*
              (phys->CdT[j]);
          else
            b[grid->etop[j]]+=theta*dt*fabs(phys->utmp[j][grid->etop[j]])/
              subgrid->dzboteff[j]*(phys->CdT[j]);
        }
      }	  

      // Now add implicit term from marsh friction
      if(!prop->subgrid)
      {
        if(prop->marshmodel)
          for(jv=marsh->marshtop[j];jv<grid->Nke[j];jv++)
            b[jv]+=MarshImplicitTerm(grid,phys,prop,j,jv,theta,dt,myproc);
      } else {
        if(!subgrid->dragpara && prop->marshmodel)
        {
          for(jv=marsh->marshtop[j];jv<grid->Nke[j];jv++)
            b[jv]+=MarshImplicitTerm(grid,phys,prop,j,jv,theta,dt,myproc);
        }
      }

      // Now add on implicit terms for vertical momentum advection, only if there is more than one layer
      if(prop->vertcoord==1 && prop->nonlinear && prop->thetaM>=0 && grid->Nke[j]-grid->etop[j]>1) {
        for(k=grid->etop[j]+1;k<grid->Nke[j]-1;k++) {
          a[k]+=prop->dt*prop->thetaM*a0[k];
          b[k]+=prop->dt*prop->thetaM*b0[k];
          c[k]+=prop->dt*prop->thetaM*c0[k];
        }
        // Top boundary
        b[grid->etop[j]]+=prop->dt*prop->thetaM*(a0[grid->etop[j]]+b0[grid->etop[j]]);
        c[grid->etop[j]]+=prop->dt*prop->thetaM*c0[grid->etop[j]];
        // Bottom boundary 
        a[grid->Nke[j]-1]+=prop->dt*prop->thetaM*a0[grid->Nke[j]-1];
        b[grid->Nke[j]-1]+=prop->dt*prop->thetaM*(b0[grid->Nke[j]-1]+c0[grid->Nke[j]-1]);
      }

      // add vertical momentum advection in the new general vertical coordinate
      if(prop->vertcoord!=1 && prop->nonlinear && grid->Nke[j]-grid->etop[j]>1 && prop->thetaM>=0) {
        // conservative part d(\omega u)dz
        for(k=grid->etop[j]+1;k<grid->Nke[j]-1;k++) {
          a[k]+=prop->dt*fac1*a0[k];
          b[k]+=prop->dt*fac1*b0[k];
          c[k]+=prop->dt*fac1*c0[k];
        }
        // Top boundary
        b[grid->etop[j]]+=prop->dt*fac1*(a0[grid->etop[j]]+b0[grid->etop[j]]);
        c[grid->etop[j]]+=prop->dt*fac1*c0[grid->etop[j]];
        // Bottom boundary 
        a[grid->Nke[j]-1]+=prop->dt*fac1*a0[grid->Nke[j]-1];
        b[grid->Nke[j]-1]+=prop->dt*fac1*(b0[grid->Nke[j]-1]+c0[grid->Nke[j]-1]);
        // second part -udwdz
        if(prop->wetdry)
          for(k=grid->etop[j];k<grid->Nke[j];k++)
            b[k]-=prop->dt*fac1*
            (def2*(vert->omega_old[nc1][k]-vert->omega_old[nc1][k+1])+def1*(vert->omega_old[nc2][k]-vert->omega_old[nc2][k+1]))/
            grid->dg[j]/(0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]));         
      }

      // implicit method for u/JdJdt term
      // fully implicit
      if(prop->vertcoord!=1 && prop->nonlinear && !prop->wetdry)
        if(vert->dJdtmeth==0)
        {
          def1 = grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
          def2 = grid->def[nc2*grid->maxfaces+grid->gradf[2*j+1]];
          dgf = def1+def2;
          for(k=grid->etop[j];k<grid->Nke[j];k++) 
          {
            phys->utmp[j][k]-=(0*phys->u_old[j][k]+0*phys->u_old2[j][k])*
             (def2/dgf*(1-grid->dzzold[nc1][k]/grid->dzz[nc1][k])+def1/dgf*(1-grid->dzzold[nc2][k]/grid->dzz[nc2][k]));
           
            b[k]+=1*(def2/dgf*(1-grid->dzzold[nc1][k]/grid->dzz[nc1][k])+def1/dgf*(1-grid->dzzold[nc2][k]/grid->dzz[nc2][k])); 
          }           
        }  



      for(k=grid->etop[j];k<grid->Nke[j];k++) {

        if(grid->dzz[nc1][k]==0 && grid->dzz[nc2][k]==0) {
          printf("Exiting because j %d dzz[%d][%d]=%f or dzz[%d][%d]=%f dv1 %e dv2 %e nk1 %d nk2 %d nke %d\n",j,
              nc1,k,grid->dzz[nc1][k],nc2,k,grid->dzz[nc2][k],grid->dv[nc1],grid->dv[nc2],grid->Nk[nc1],grid->Nk[nc2],grid->Nke[j]);
          exit(0);
        }

        if(a[k]!=a[k]) printf("a[%d] problems, dzz[%d][%d]=%f\n",k,j,k,grid->dzz[j][k]);
        
        if(b[k]!=b[k] || b[k]==0){
          if(prop->subgrid)
            printf("proc %d n %d ne %d b[%d] problems, b=%f dzf %e nke %d etop %d Nk %d %d dv %e %e hmin %e %e Cd %e\n",myproc,prop->n,j, k,b[k],
              grid->dzf[j][k],grid->Nke[j],grid->etop[j],grid->Nk[grid->grad[2*j]],grid->Nk[grid->grad[2*j+1]],grid->dv[grid->grad[2*j]],grid->dv[grid->grad[2*j+1]],
              subgrid->hmin[grid->grad[2*j]],subgrid->hmin[grid->grad[2*j+1]],phys->CdB[j]);
          else
            printf("proc %d n %d ne %d b[%d] problems, b=%f dzf %e nke %d etop %d Nk %d %d dv %e %e Cd %e\n",myproc,prop->n,j, k,b[k],
              grid->dzf[j][k],grid->Nke[j],grid->etop[j],grid->Nk[grid->grad[2*j]],grid->Nk[grid->grad[2*j+1]],grid->dv[grid->grad[2*j]],grid->dv[grid->grad[2*j+1]]
              ,phys->CdB[j]);                
        }

        if(c[k]!=c[k]) printf("c[%d] problems\n",k);
      }

      // Now utmp will have U*** in it, which is given by A^{-1}U**, and E will have
      // A^{-1}e1, where e1 = [1,1,1,1,1,...,1]^T 
      // Store the tridiagonals so they can be used twice (TriSolve alters the values
      // of the elements in the diagonals!!! 
      for(k=0;k<grid->Nke[j];k++) {
        a0[k]=a[k];
        b0[k]=b[k];
        c0[k]=c[k];
      }

      if(grid->Nke[j]-grid->etop[j]>1) { // more than one layer (z level)
        TriSolve(&(a[grid->etop[j]]),&(b[grid->etop[j]]),&(c[grid->etop[j]]),
            &(d[grid->etop[j]]),&(phys->utmp[j][grid->etop[j]]),grid->Nke[j]-grid->etop[j]);
        TriSolve(&(a0[grid->etop[j]]),&(b0[grid->etop[j]]),&(c0[grid->etop[j]]),
            &(e1[grid->etop[j]]),&(E[j][grid->etop[j]]),grid->Nke[j]-grid->etop[j]);	
      } else {  // one layer (z level)
        phys->utmp[j][grid->etop[j]]/=b[grid->etop[j]];
        E[j][grid->etop[j]]=1.0/b[grid->etop[j]];
      }

      // Now vertically integrate E to create the vertically integrated flux-face
      // values that comprise the coefficients of the free-surface solver.  This
      // will create the D vector, where D=DZ^T E (which should be given by the
      // depth when there is no viscosity.
      phys->D[j]=0;
      for(k=grid->etop[j];k<grid->Nke[j];k++) 
      {  
        phys->D[j]+=E[j][k]*grid->dzf[j][k];
      } 
    }
  }

  theta=theta0;

  for(j=0;j<grid->Ne;j++) 
    for(k=grid->etop[j];k<grid->Nke[j];k++)
      if(phys->utmp[j][k]!=phys->utmp[j][k]) {
        if(prop->subgrid)
          printf("n %d Error in function Predictor at j=%d k=%d Nke %d etop %d (U***=nan) cd %e nc1 %d nc2 %d V %e %e \n",prop->n,j,k,grid->Nke[j],grid->etop[j],phys->CdB[j],grid->grad[2*j],grid->grad[2*j+1],\
                  subgrid->Veff[grid->grad[2*j]],subgrid->Veff[grid->grad[2*j+1]]);
        else
          printf("n %d Error in function Predictor at j=%d k=%d Nke %d etop %d (U***=nan) cd %e nc1 %d nc2 %d\n",prop->n,j,k,grid->Nke[j],grid->etop[j],phys->CdB[j],grid->grad[2*j],grid->grad[2*j+1]);               
        exit(1);
      }

  // So far we have U*** and D.  Now we need to create h* in htmp.   This
  // will comprise the source term for the free-surface solver.  Before we
  // do this we need to set the new velocity at the open boundary faces and
  // place them into utmp.  
  BoundaryVelocities(grid,phys,prop,myproc,comm);
  OpenBoundaryFluxes(NULL,phys->utmp,NULL,grid,phys,prop);

  for(j=0;j<grid->Ne;j++) 
    for(k=grid->etop[j];k<grid->Nke[j];k++) 
      if(phys->utmp[j][k]!=phys->utmp[j][k]) {
        printf("n %d Error in function Predictor at j=%d k=%d (U***=nan) cd %e nc1 %d nc2 %d\n",prop->n,j,k,phys->CdB[j],grid->grad[2*j],grid->grad[2*j+1]);
        exit(1);
      }

  // for computational cells
  // now phys->u=u^n phys->u_old2=u^n-1 phys->utmp=u*
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    sum = 0;

    for(nf=0;nf<grid->nfaces[i];nf++) {
      ne = grid->face[i*grid->maxfaces+nf];
      normal = grid->normal[i*grid->maxfaces+nf];
      nc1=grid->grad[2*ne];
      nc2=grid->grad[2*ne+1];
      if(nc1==-1)
        nc1=nc2;
      if(nc2==-1)
        nc2=nc1;
      for(k=grid->etop[ne];k<grid->Nke[ne];k++) 
      {  
        sum+=(fac2*phys->u_old[ne][k]+fac1*phys->utmp[ne][k]+fac3*phys->u_old2[ne][k])*
          grid->df[ne]*normal*grid->dzf[ne][k];  
      }
    }
    if(prop->subgrid)
    {
      phys->htmp[i]=subgrid->Veff[i]-dt*sum;
      StoreSubgridOldAceffandVeff(grid, myproc);
      subgrid->rhs[i]=phys->htmp[i];
    }
    else
      phys->htmp[i]=grid->Ac[i]*phys->h[i]-dt*sum;
    if(phys->htmp[i]!=phys->htmp[i]){
      printf("n %d something wrong on the source term of h at cell %d\n",prop->n,i);
      //exit(1);
    }
  }
 

  for(i=0;i<grid->Nc;i++){
    // store the old h as h_n-1 in the next time step
    phys->h_old[i]=phys->h[i];
    phys->dhdt[i]=phys->h[i];
  }
  
  sum=0;
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    sum+=phys->htmp[i];
  }

  // test whether the total mass of the model is positive
  if(sum<0 && prop->subgrid)
  {
    printf("something wrong in b>=0 n=%d sum=%e\n",prop->n,sum);
      MPI_Finalize();
      exit(EXIT_FAILURE);
  }

  // culvert model part
  if(prop->culvertmodel)
    CulvertInitIteration(grid,phys, prop, 1,myproc);

  // Now we have the required components for the CG solver for the free-surface:
  //
  // h^{n+1} - g*(theta*dt)^2/Ac * Sum_{faces} D_{face} dh^{n+1}/dn df N = htmp
  //
  // L(h) = b
  //
  // L(h) = h + 1/Ac * Sum_{faces} D_{face} dh^{n+1}/dn N
  // b = htmp
  //
  // As the initial guess let h^{n+1} = h^n, so just leave it as it is to
  // begin the solver.

  if(!prop->culvertmodel)
  {
    nf=0;
    min=INFTY;
    while(1){
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];
        if(prop->subgrid)
          subgrid->residual[i]=-subgrid->Veff[i]+subgrid->Aceff[i]*phys->h[i];
      }

      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];
        if(prop->subgrid){
          phys->htmp[i]=subgrid->rhs[i];
          phys->htmp[i]-=(subgrid->Veff[i]-phys->h[i]*subgrid->Aceff[i]); 
        }
        
        if(nf==0 && prop->subgrid){
          if(subgrid->rhs[i]!=subgrid->rhs[i])
            printf("%d right hand side wrong=%f\n",i,subgrid->rhs[i]);
          if(subgrid->Veff[i]!=subgrid->Veff[i])
            printf("%d Veff wrong=%f\n",i,subgrid->Veff[i]);
          if(subgrid->Aceff[i]!=subgrid->Aceff[i])
            printf("%d Aceff wrong=%f\n",i,subgrid->Aceff[i]);
          if(phys->htmp[i]!=phys->htmp[i])
            printf("%d htmp wrong=%f\n",i,phys->htmp[i]);
        }
      }
      
      // store h for each iteration
      if(prop->subgrid)
        for(i=0;i<grid->Nc;i++)
          subgrid->hiter[i]=phys->h[i];

      CGSolve(grid,phys,prop,myproc,numprocs,comm); 

      // for original suntans	
      if(!prop->subgrid)
        break;
      
      // subgrid part
      if(prop->subgrid)
        UpdateSubgridVeff(grid, phys, prop, myproc);

      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];

        if(prop->subgrid) {
          subgrid->residual[i]+=subgrid->Veff[i]-subgrid->Aceff[i]*phys->h[i];
        }
      }     

      ISendRecvCellData2D(subgrid->residual,grid,myproc,comm);
      sum=InnerProduct(subgrid->residual,subgrid->residual,grid,myproc,numprocs,comm);

      if(nf==0){
        if(sum>1)
          sum0=sum;
        else
          sum0=1;
      }

      if(prop->subgrid)
        UpdateSubgridAceff(grid, phys, prop, myproc);

      if(sqrt(sum)<subgrid->eps)
        break;

      if(sqrt(sum/sum0)<subgrid->eps)
        break;

      nf++;
      if(min>sqrt(sum/sum0)){
        if(prop->subgrid)
          for(i=0;i<grid->Nc;i++)
            subgrid->hiter_min[i]=subgrid->hiter[i];
        min=sqrt(sum/sum0);        
      }

      if(nf>10) { 
        if(fabs(sqrt(sum/sum0)-min)<0.001)
          break;

        /*if(nf==11)
        {
          for(i=0;i<grid->Nc;i++)
            phys->h[i]=subgrid->hiter_min[i];
          ISendRecvCellData2D(phys->h,grid,myproc,comm);
          UpdateSubgridVeff(grid, phys, prop, myproc);
          UpdateSubgridAceff(grid, phys, prop, myproc);
        }*/

        if(nf>50)
        {
           printf("n %d nf %d something maybe wrong for convergence at time step min %e sum %e sum0 %e r %e\n",prop->n,nf,min,sum,sum0,sqrt(sum/sum0));
           printf("iteration for subgrid is more than 50 times. stop program\n");
           exit(1);
        }
      }
    }
  } else {

    // outer loop
    UpdateCulvertQcoef(grid,prop,0, myproc);
    nf=0;
    min2=INFTY;
    while(1)
    {
      for(i=0;i<grid->Nc;i++){
        culvert->pressure3[i]=phys->h[i];           
      }    
      min=INFTY;
      nf1=0;

      while(1){
        // inner loop
        if(prop->subgrid)
        {
          UpdateSubgridVeff(grid, phys, prop, myproc);
          UpdateSubgridAceff(grid, phys, prop, myproc);
        }
        
        // initialize each iteration 
        CulvertInitIteration(grid,phys,prop,-1,myproc);
        // calculate the source term for free surface eqns. based on casulli's method
        CulvertIterationSource(grid,phys,prop,theta,dt,myproc);      
        // CG solver for free surface
        CGSolve(grid,phys,prop,myproc,numprocs,comm);
 
        if(prop->subgrid)
          UpdateSubgridVeff(grid, phys, prop, myproc);

        // calculate residual
        CheckCulvertCondition(grid,phys,prop,myproc);

        if(prop->subgrid)
          UpdateSubgridAceff(grid, phys, prop, myproc);

        // calculate norm error
        ISendRecvCellData2D(culvert->condition,grid,myproc,comm);

        // the total residual
        culvert->sum=InnerProduct(culvert->condition,culvert->condition,grid,myproc,numprocs,comm);

        // infinity norm
        if(nf1==0){
          if(culvert->sum>1)
            sum0=culvert->sum;
          else
            sum0=1;
        }

        if(sqrt(culvert->sum)<culvert->eps)
          break;

        // check culvert condition
        if(sqrt(culvert->sum/sum0)<culvert->eps)
          break;
      
        if(min>sqrt(culvert->sum/sum0))
          min=sqrt(culvert->sum/sum0);
      
        if(nf1>10)
        {
          printf("proc %d 1 n %d nf %d something maybe wrong for convergence at time step min %e sum %e sum0 %e r %e\n",myproc,prop->n,nf1 ,
            min,culvert->sum,sum0,sqrt(culvert->sum/sum0));

          if(fabs(sqrt(culvert->sum/sum0)-min)<1e-3)
            break;

          if(nf1>50){
            printf("iteration for subgrid is more than 50 times. stop program\n");
            exit(1);
          }
        }

        nf1++;
      }
  
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];           
        culvert->condition2[i]=culvert->Qcoef[i]*(culvert->top[i]-phys->h[i]);
        culvert->pressure2[i]=phys->h[i];
      } 

      UpdateCulvertQcoef(grid,prop,0,myproc);

      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];           
        culvert->condition2[i]+=culvert->Qcoef[i]*(-culvert->top[i]+phys->h[i]);
      } 

      // calculate norm error
      ISendRecvCellData2D(culvert->condition2,grid,myproc,comm);

      // the total residual
      culvert->sum=InnerProduct(culvert->condition2,culvert->condition2,grid,myproc,numprocs,comm);
      
      if(nf==0){
        if(culvert->sum>1)
          sum=culvert->sum;
        else
          sum=1;
      }
      nf++;

      if(sqrt(culvert->sum)<culvert->eps)
        break;

      // check culvert condition
      if(sqrt(culvert->sum/sum)<culvert->eps)
        break; 

      if(min2>sqrt(culvert->sum/sum)){
        min2=sqrt(culvert->sum/sum);
        for(i=0;i<grid->Nc;i++)
          culvert->pressure4[i]=culvert->pressure3[i];
      }

      if(nf>10) { 
        if(nf>50){
          exit(1);
        }
        //exit(EXIT_FAILURE);
        if(fabs(sqrt(culvert->sum/sum)-min2)<0.001)
          break;

        if(nf==20)
        {
          printf("myproc %d 2 n %d nf %d something maybe wrong for convergence at time step min %e sum %e sum0 %e r %e\n",myproc,prop->n,nf,min2,culvert->sum,sum,sqrt(culvert->sum/sum));
          for(i=0;i<grid->Nc;i++)
          {
            phys->h[i]=culvert->pressure[i];
            culvert->pressure2[i]=phys->h[i];
          }
          ISendRecvCellData2D(phys->h,grid,myproc,comm);
          ISendRecvCellData2D(culvert->pressure2,grid,myproc,comm);
          UpdateCulvertQcoef(grid,prop,0,myproc);
        }
        if(fabs(sqrt(culvert->sum/sum)-min2)<0.001)
          break;        
      }
    }
  }

  // Add back the implicit barotropic term to obtain the 
  // hydrostatic horizontal velocity field.
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];

    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    for(k=grid->etop[j];k<grid->Nke[j];k++) {
      phys->u[j][k]=phys->utmp[j][k]-prop->grav*fac1*dt*E[j][k]*
        (phys->h[nc1]-phys->h[nc2])/grid->dg[j];
    }

    // set dry cells (with zero height) to have zero velocity
    // CHANGE
    if(grid->etop[j]==grid->Nke[j]-1 && grid->dzz[nc1][grid->etop[j]]<=1*DRYCELLHEIGHT &&
        grid->dzz[nc2][grid->etop[j]]<=1*DRYCELLHEIGHT)
      phys->u[j][grid->etop[j]]=0;
  }

  // correct cells drying below DRYCELLHEIGHT above the 
  // bathymetry
  // make sure Culvert height is much bigger than DRYCELLHEIGHT when culvertmodel==1 
  for(i=0;i<grid->Nc;i++){
    // for a cell with 0 dzf at all edges reset to the old phys->h[i]
    // It will not affect the results since dzf is all zero. And any neighbouring cell will have no effects     
    if(prop->subgrid && prop->wetdry)
    {
      flag=0;
      for(nf=0;nf<grid->nfaces[i];nf++) {
        ne = grid->face[i*grid->maxfaces+nf];
        if(grid->etop[ne]<(grid->Nke[ne]-1) || grid->dzf[ne][grid->Nke[ne]-1]>0)
          flag=1;
      }
      if(!flag)
        phys->h[i]=phys->h_old[i]; 
    }  

    if(phys->h[i]<=(-grid->dv[i]+DRYCELLHEIGHT)) {
      phys->hcorr[i]=-grid->dv[i]+DRYCELLHEIGHT-phys->h[i];
      phys->h[i]=-grid->dv[i]+DRYCELLHEIGHT;
      phys->active[i]=0;
      //phys->s[i][grid->Nk[i]-1]=0;
      //phys->T[i][grid->Nk[i]-1]=0;
      //if(prop->computeSediments && prop->n>1+prop->nstart)
        //for(k=0;k<sediments->Nsize;k++)
          //sediments->SediC[k][i][grid->Nk[i]-1]=0;

    } else {
      phys->hcorr[i]=0;
      phys->active[i]=1;
    }

    //if(phys->h[i]<=(-grid->dv[i]+1e-3))
      //phys->active[i]=0;
  }

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    phys->dhdt[i]=(phys->h[i]-phys->dhdt[i])/dt;
  }

  // added culvert part
  if(prop->culvertmodel){
    StoreCulvertPressure(phys->h, grid->Nc, 1, myproc);
  }

  if(prop->subgrid)
  {
    UpdateSubgridVeff(grid, phys, prop, myproc);
    UpdateSubgridAceff(grid, phys, prop, myproc);
  }

  // add back residual in free surface solver to ensure restrict mass conservation
  if(prop->wetdry && prop->subgrid){
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];
      sum0=0;
      for(nf=0;nf<grid->nfaces[i];nf++) {
        ne = grid->face[i*grid->maxfaces+nf];
        normal = grid->normal[i*grid->maxfaces+nf];
        for(k=grid->etop[ne];k<grid->Nke[ne];k++){ 
          sum0+=prop->dt*(fac1*phys->u[ne][k]+fac2*phys->u_old[ne][k]+fac3*phys->u_old2[ne][k])*grid->df[ne]*normal*grid->dzf[ne][k];
        }
      }
      subgrid->Verr[i]=subgrid->Veff[i]-subgrid->Veffold[i]+sum0;
      subgrid->Veff[i]=subgrid->Veffold[i]-sum0;   
    }
  }
   
  if(prop->subgrid)
    ISendRecvCellData2D(subgrid->Veff,grid,myproc,comm);

  if(prop->subgrid)
    for(i=0;i<grid->Nc;i++){
      if(subgrid->Veff[i]<=(DRYCELLHEIGHT*grid->Ac[i]))
      {
        phys->h[i]=-grid->dv[i]+DRYCELLHEIGHT;
        phys->active[i]=0;
        subgrid->Veff[i]=(DRYCELLHEIGHT*grid->Ac[i]);
        //phys->s[i][grid->Nk[i]-1]=0;
        //if(prop->computeSediments && prop->n>1+prop->nstart)
          //for(k=0;k<sediments->Nsize;k++)
            //sediments->SediC[k][i][grid->Nk[i]-1]=0;
      }
      if(subgrid->Veff[i]<1e-3*grid->Ac[i])
        phys->active[i]=0;
    }
  
  // recalculate new free surface based on the modified volume
  if(prop->subgrid && prop->wetdry){
    UpdateSubgridFreeSurface(grid,phys,prop,myproc);
    ISendRecvCellData2D(phys->h,grid,myproc,comm);  
    if(prop->culvertmodel){
      ISendRecvCellData2D(culvert->pressure,grid,myproc,comm);  
      ISendRecvCellData2D(culvert->pressure2,grid,myproc,comm);  
    }
    UpdateSubgridVeff(grid, phys, prop, myproc);
    UpdateSubgridAceff(grid, phys, prop, myproc);
    UpdateSubgridHeff(grid, phys, prop, myproc);
  }

  // Use the new free surface to add the implicit part of the free-surface
  // pressure gradient to the horizontal momentum.
  //
  // This was removed because it violates the assumption of linearity in that
  // the discretization only knows about grid cells below grid->ctopold[].
  /*
     for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
     j = grid->edgep[jptr];

     nc1 = grid->grad[2*j];
     nc2 = grid->grad[2*j+1];

     if(grid->etop[j]>grid->etopold[j]) 
     for(k=0;k<grid->etop[j];k++)
     phys->u[j][k]=0;
     else 
     for(k=grid->etop[j];k<grid->etopold[j];k++)
     phys->u[j][k]=phys->utmp[j][k]-prop->grav*theta*dt*
     (phys->h[nc1]-phys->h[nc2])/grid->dg[j];
     }
     */

  // Set the flux values at the open boundary (marker=2).  These
  // were set to utmp previously in OpenBoundaryFluxes.
  for(jptr=grid->edgedist[2];jptr<grid->edgedist[3];jptr++) {
    j = grid->edgep[jptr];

    for(k=grid->etop[j];k<grid->Nke[j];k++) 
      phys->u[j][k] = phys->utmp[j][k];
  }

  // Now set the fluxes at the free-surface boundary by assuming dw/dz = 0
  // this is for type 3 boundary conditions
  for(iptr=grid->celldist[1];iptr<grid->celldist[2];iptr++) {
    i = grid->cellp[iptr];
    for(nf=0;nf<grid->nfaces[i];nf++) {
      ne = grid->face[i*grid->maxfaces+nf];
      if(grid->mark[ne]==3) {
        for(k=grid->etop[ne];k<grid->Nke[ne];k++) {
          phys->u[ne][k] = 0;
          sum=0;
          for(nf1=0;nf1<grid->nfaces[i];nf1++)
            sum+=phys->u[grid->face[i*grid->maxfaces+nf1]][k]*grid->df[grid->face[i*grid->maxfaces+nf1]]*grid->normal[i*grid->maxfaces+nf1];
          phys->u[ne][k]=-sum/grid->df[ne]/grid->normal[i*grid->maxfaces+nf];
        }
      }
    } 
  }

  if(prop->vertcoord!=1 && prop->vertcoord!=5)
    if(vert->modifydzf)
    {
      VerifyFluxHeight(grid,prop,phys,myproc);
      UpdateCellcenteredFreeSurface(grid,prop,phys,myproc);
      ISendRecvCellData2D(phys->h,grid,myproc,comm);  
    }

  // Now update the vertical grid spacing with the new free surface.
  // can comment this out to linearize the free surface 
  if(prop->vertcoord==1)
    UpdateDZ(grid,phys,prop, 0); 
  else {
    // use new method to update layerthickness, the old value is stored in zc_old
    UpdateLayerThickness(grid, prop, phys, 0,myproc, numprocs, comm);
    ISendRecvCellData3D(grid->dzz,grid,myproc,comm);
    for(i=0;i<grid->Nc;i++){
        sum=0;
        for(k=0;k<grid->Nk[i];k++)
          sum+=grid->dzz[i][k];
        if(fabs(sum-(phys->h[i]+grid->dv[i]))>1e-6)
          printf("n %d something wrong on the cell depth calculation at cell %d error %e sum %e H %e\n",prop->n, i,fabs(sum-(phys->h[i]+grid->dv[i])),sum,phys->h[i]+grid->dv[i]);
    }

    // compute the new zc
    ComputeZc(grid,prop,phys,1,myproc);
  }
  // update vertical ac for scalar transport
  if(prop->subgrid)
    UpdateSubgridVerticalAceff(grid, phys, prop, 0, myproc);
}

/*
 * Function: CGSolve
 * Usage: CGSolve(grid,phys,prop,myproc,numprocs,comm);
 * ----------------------------------------------------
 * Solve the free surface equation using the conjugate gradient algorithm.
 *
 * The source term upon entry is in phys->htmp, which is placed into p, and
 * the free surface upon entry is in phys->h, which is placed into x.
 *
 */
static void CGSolve(gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm) {

  int i, iptr, n, niters;
  REAL *x, *r, *rtmp, *p, *z, mu, nu, eps, eps0, alpha, alpha0;

  x = phys->h;
  r = phys->hold;
  rtmp = phys->htmp2;
  z = phys->htmp3;
  p = phys->htmp;

  niters = prop->maxiters;

  // Create the coefficients for the operator
  if(!prop->culvertmodel){
    HCoefficients(phys->hcoef,phys->hfcoef,grid,phys,prop);
  }else{
    CulvertHCoefficients(phys->hcoef,phys->hfcoef,grid,phys,prop,myproc);
  }

  // For the boundary term (marker of type 3):
  // 1) Need to set x to zero in the interior points, but
  //    leave it as is for the boundary points.
  // 2) Then set z=Ax and substract b = b-z so that
  //    the new problem is Ax=b with the boundary values
  //    on the right hand side acting as forcing terms.
  // 3) After b=b-z for the interior points, then need to
  //    set b=0 for the boundary points.

  /* Fix to account for boundary cells (type 3) in h */
  // 1) x=0 interior cells 
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    x[i]=0;
  }
  ISendRecvCellData2D(x,grid,myproc,comm);
  OperatorH(x,z,phys->hcoef,phys->hfcoef,grid,phys,prop);

  // 2) b = b-z
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    p[i] = p[i] - z[i];    
    r[i] = p[i];
    x[i] = 0;
  }    
  // 3) b=0 for the boundary cells
  for(iptr=grid->celldist[1];iptr<grid->celldist[2];iptr++) { 
    i = grid->cellp[iptr]; 

    p[i] = 0; 
  }     

  // continue with CG as expected now that boundaries are handled
  if(prop->hprecond==1) {
    HPreconditioner(r,rtmp,grid,phys,prop);
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      p[i] = rtmp[i];
    }
    alpha = alpha0 = InnerProduct(r,rtmp,grid,myproc,numprocs,comm);
  } else {
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      p[i] = r[i];
    }
    alpha = alpha0 = InnerProduct(r,r,grid,myproc,numprocs,comm);
  }
  if(!prop->resnorm) alpha0 = 1;

  if(prop->hprecond==1)
    eps=eps0=InnerProduct(r,r,grid,myproc,numprocs,comm);
  else
    eps=eps0=alpha0;

  // Iterate until residual is less than prop->epsilon
  for(n=0;n<niters && eps!=0 && alpha!=0;n++) {

    ISendRecvCellData2D(p,grid,myproc,comm);
    OperatorH(p,z,phys->hcoef,phys->hfcoef,grid,phys,prop);

    mu = 1/alpha;
    nu = alpha/InnerProduct(p,z,grid,myproc,numprocs,comm);

    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      x[i] += nu*p[i];
      r[i] -= nu*z[i];
    }
    if(prop->hprecond==1) {
      HPreconditioner(r,rtmp,grid,phys,prop);
      alpha = InnerProduct(r,rtmp,grid,myproc,numprocs,comm);
      mu*=alpha;
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];

        p[i] = rtmp[i] + mu*p[i];
      }
    } else {
      alpha = InnerProduct(r,r,grid,myproc,numprocs,comm);
      mu*=alpha;
      for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
        i = grid->cellp[iptr];

        p[i] = r[i] + mu*p[i];
      }
    }

    if(prop->hprecond==1)
      eps=InnerProduct(r,r,grid,myproc,numprocs,comm);
    else
      eps=alpha;

    if(VERBOSE>3 && myproc==0) printf("CGSolve free-surface Iteration: %d, resid=%e\n",n,sqrt(eps/eps0));
    if(sqrt(eps/eps0)<prop->epsilon) 
      break;
  }
  if(myproc==0 && VERBOSE>2){
    if(eps==0){
      printf("Warning...Time step %d, norm of free-surface source is 0.\n",prop->n);
    } else {
      if(n==niters)  printf("Warning... Time step %d, Free-surface iteration not converging after %d steps! RES=%e > %.2e\n",
          prop->n,n,sqrt(eps/eps0),prop->qepsilon);
      else printf("Time step %d, CGSolve free-surface converged after %d iterations, res=%e < %.2e\n",
          prop->n,n,sqrt(eps/eps0),prop->epsilon);
    }
  }
  // Send the solution to the neighboring processors
  ISendRecvCellData2D(x,grid,myproc,comm);
}

/*
 * Function: HPreconditioner
 * Usage: HPreconditioner(r,rtmp,grid,phys,prop);
 * ----------------------------------------------
 * Multiply the vector x by the inverse of the preconditioner M with
 * xc = M^{-1} x
 *
 */
static void HPreconditioner(REAL *x, REAL *y, gridT *grid, physT *phys, propT *prop) {
  int i, iptr;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    y[i]=x[i]/phys->hcoef[i];
  }
}

/*
 * Function: HCoefficients
 * Usage: HCoefficients(coef,fcoef,grid,phys,prop);
 * --------------------------------------------------
 * Compute coefficients for the free-surface solver.  fcoef stores
 * coefficients at the flux faces while coef stores coefficients
 * at the cell center.  If L is the linear operator on x, then
 *
 * L(x(i)) = coef(i)*x(i) - sum(m=1:3) fcoef(ne)*x(neigh)
 * coef(i) = (Ac(i) + sum(m=1:3) tmp*D(ne)*df(ne)/dg(ne))
 * fcoef(ne) = tmp*D(ne)*df(ne)/dg(ne)
 *
 * where tmp = prop->grav*(theta*dt)^2
 *
 */
static void HCoefficients(REAL *coef, REAL *fcoef, gridT *grid, physT *phys, propT *prop) {

  int i, j, iptr, jptr, ne, nf, check;
  REAL tmp, h0, boundary_flag,fac;

  fac=prop->imfac1;

  tmp=prop->grav*pow(fac*prop->dt,2);

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    check=1;
    i = grid->cellp[iptr];
    if(!prop->subgrid)
      coef[i] = grid->Ac[i];
    else
      coef[i] = subgrid->Aceff[i];

    for(nf=0;nf<grid->nfaces[i];nf++) 
      if(grid->neigh[i*grid->maxfaces+nf]!=-1) {
        ne = grid->face[i*grid->maxfaces+nf];
        fcoef[i*grid->maxfaces+nf]=tmp*phys->D[ne]*grid->df[ne]/grid->dg[ne];
        coef[i]+=fcoef[i*grid->maxfaces+nf];

        if(fcoef[i*grid->maxfaces+nf]>0)
          check=0;
      } 
 
    // when 0=0 exists make sure hnew=hold
    if(check) //&& prop->subgrid)
    {
      coef[i]=1.0;
      phys->htmp[i]=phys->h[i];
    }
  }

}

/*
 *
 * Function: InnerProduct
 * Usage: InnerProduct(x,y,grid,myproc,numprocs,comm);
 * ---------------------------------------------------
 * Compute the inner product of two one-dimensional arrays x and y.
 * Used for the CG method to solve for the free surface.
 *
 */
static REAL InnerProduct(REAL *x, REAL *y, gridT *grid, int myproc, int numprocs, MPI_Comm comm) {

  int i, iptr;
  REAL sum, mysum=0;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    mysum+=x[i]*y[i];
  }
  MPI_Reduce(&mysum,&(sum),1,MPI_DOUBLE,MPI_SUM,0,comm);
  MPI_Bcast(&sum,1,MPI_DOUBLE,0,comm);

  return sum;
}  

/*
 * Function: InnerProduct3
 * Usage: InnerProduct3(x,y,grid,myproc,numprocs,comm);
 * ---------------------------------------------------
 * Compute the inner product of two two-dimensional arrays x and y.
 * Used for the CG method to solve for the nonhydrostatic pressure.
 *
 */
static REAL InnerProduct3(REAL **x, REAL **y, gridT *grid, int myproc, int numprocs, MPI_Comm comm) {

  int i, k, iptr;
  REAL sum, mysum=0;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    for(k=grid->ctop[i];k<grid->Nk[i];k++)
      mysum+=x[i][k]*y[i][k];
  }
  MPI_Reduce(&mysum,&(sum),1,MPI_DOUBLE,MPI_SUM,0,comm);
  MPI_Bcast(&sum,1,MPI_DOUBLE,0,comm);

  return sum;
}  

/*
 * Usage: OperatorH(x,y,grid,phys,prop);
 * -------------------------------------
 * Given a vector x, computes the left hand side of the free surface 
 * Poisson equation and places it into y with y = L(x), where
 *
 * L(x(i)) = coef(i)*x(i) + sum(m=1:3) fcoef(ne)*x(neigh)
 * coef(i) = (Ac(i) + sum(m=1:3) tmp*D(ne)*df(ne)/dg(ne))
 * fcoef(ne) = tmp*D(ne)*df(ne)/dg(ne)
 *
 * where tmp = prop->grav*(theta*dt)^2
 *
 */
static void OperatorH(REAL *x, REAL *y, REAL *coef, REAL *fcoef, gridT *grid, physT *phys, propT *prop) {

  int i, j, iptr, jptr, ne, nf;
  REAL tmp = prop->grav*pow(prop->theta*prop->dt,2), h0, boundary_flag;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    y[i] = coef[i]*x[i];
    for(nf=0;nf<grid->nfaces[i];nf++) 
      if(grid->neigh[i*grid->maxfaces+nf]!=-1)
        y[i]-=fcoef[i*grid->maxfaces+nf]*x[grid->neigh[i*grid->maxfaces+nf]];
  }

}

/*
 * Function: OperatorQC
 * Usage: OperatorQC(coef,fcoef,x,y,c,grid,phys,prop);
 * ---------------------------------------------------
 * Given a vector x, computes the left hand side of the nonhydrostatic pressure
 * Poisson equation and places it into y with y = L(x) for the preconditioned
 * solver.
 *
 * The coef array contains coefficients for the vertical derivative terms in the operator
 * while the fcoef array contains coefficients for the horizontal derivative terms.  These
 * are computed before the iteration in QCoefficients. The array c stores the preconditioner.
 *
 */
static void OperatorQC(REAL **coef, REAL **fcoef, REAL **x, REAL **y, REAL **c, gridT *grid, physT *phys, propT *prop) {

  int i, iptr, k, ne, nf, nc, kmin, kmax;
  REAL *a = phys->a;

  // sum over all computational cells
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    // over the depth of defined cells
    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      y[i][k]=-x[i][k];

    // over each face
    for(nf=0;nf<grid->nfaces[i];nf++) {
      if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1) {

        ne = grid->face[i*grid->maxfaces+nf];

        if(grid->ctop[nc]>grid->ctop[i])
          kmin = grid->ctop[nc];
        else
          kmin = grid->ctop[i];

        // now sum over depth too
        for(k=kmin;k<grid->Nke[ne];k++) 
          y[i][k]+=x[nc][k]*fcoef[i*grid->maxfaces+nf][k];
      }
    }

    // over depth now
    for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++)
      y[i][k]+=coef[i][k]*x[i][k-1]+coef[i][k+1]*x[i][k+1];

    // apply top/bottom bcs in vertical since we have no flux on sides
    if(grid->ctop[i]<grid->Nk[i]-1) {
      // Top q=0 so q[i][grid->ctop[i]-1]=-q[i][grid->ctop[i]]
      k=grid->ctop[i];
      y[i][k]+=coef[i][k+1]*x[i][k+1];

      // Bottom dq/dz = 0 so q[i][grid->Nk[i]]=q[i][grid->Nk[i]-1]
      k=grid->Nk[i]-1;
      y[i][k]+=coef[i][k]*x[i][k-1];
    }
  }
}

/*
 * Function: QCoefficients
 * Usage: QCoefficients(coef,fcoef,c,grid,phys,prop);
 * --------------------------------------------------
 * Compute coefficients for the pressure-Poisson equation.  fcoef stores
 * coefficients at the vertical flux faces while coef stores coefficients
 * at the horizontal faces for vertical derivatives of q.
 *
 */
static void QCoefficients(REAL **coef, REAL **fcoef, REAL **c, gridT *grid, 
    physT *phys, propT *prop) {

  int i, iptr, k, kmin, nf, nc, ne;

  // if we want to use the preconditioner
  if(prop->qprecond==1) {
    // over all computational cells
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];
      coef[i][grid->ctop[i]]=grid->Ac[i]/grid->dzz[i][grid->ctop[i]]/c[i][grid->ctop[i]];
      if(prop->subgrid)
        coef[i][grid->ctop[i]]=subgrid->Acveff[i][grid->ctop[i]]/grid->dzz[i][grid->ctop[i]]/c[i][grid->ctop[i]];
      // over all the compuational cell depths
      for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
        if(!prop->subgrid)
          // compute the cell coefficients
          coef[i][k] = 2*grid->Ac[i]/(grid->dzz[i][k]+grid->dzz[i][k-1])/(c[i][k]*c[i][k-1]);
        else
          coef[i][k] = 2*subgrid->Acveff[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1])/(c[i][k]*c[i][k-1]);          
      // over all the faces 
      for(nf=0;nf<grid->nfaces[i];nf++) 
        if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1) {

          ne = grid->face[i*grid->maxfaces+nf];

          if(grid->ctop[nc]>grid->ctop[i])
            kmin = grid->ctop[nc];
          else
            kmin = grid->ctop[i];

          // over the edge depths
          for(k=kmin;k<grid->Nke[ne];k++) 
            // compute the face coefficients
            fcoef[i*grid->maxfaces+nf][k]=grid->dzz[i][k]*phys->D[ne]/(c[i][k]*c[nc][k]);
        }
    }
  }
  // no preconditioner
  else {
    /*for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++)  */
    // over each and every cell
    for(i=0;i<grid->Nc;i++) {
      //      i = grid->cellp[iptr];

      // compute the cell coeficient for the top cell
      coef[i][grid->ctop[i]]=grid->Ac[i]/grid->dzz[i][grid->ctop[i]];
      if(prop->subgrid)
        coef[i][grid->ctop[i]]=subgrid->Acveff[i][grid->ctop[i]]/grid->dzz[i][grid->ctop[i]];
      // compute the coefficients for the rest of the cells (towards bottom)
      // where dzz is averaged to the face 
      for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
        if(!prop->subgrid)
          // compute the cell coefficients
          coef[i][k] = 2*grid->Ac[i]/(grid->dzz[i][k]+grid->dzz[i][k-1]);
        else
          coef[i][k] = 2*subgrid->Acveff[i][k]/(grid->dzz[i][k]+grid->dzz[i][k-1]);      
    }
  }
  // where is fcoef solved for???
}

/*
 * Function: OperatorQ
 * Usage: OperatorQ(coef,x,y,c,grid,phys,prop);
 * --------------------------------------------
 * Given a vector x, computes the left hand side of the nonhydrostatic pressure
 * Poisson equation and places it into y with y = L(x) for the non-preconditioned
 * solver.
 *
 * The coef array contains coefficients for the vertical derivative terms in the operator.
 * This is computed before the iteration in QCoefficients. The array c stores the preconditioner.
 * The preconditioner stored in c is not used.
 *
 */
static void OperatorQ(REAL **coef, REAL **x, REAL **y, REAL **c, gridT *grid, physT *phys, propT *prop) {

  int i, iptr, k, ne, nf, nc, kmin, kmax;

  // over each computational cell
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    // over cells that exist and aren't cut off by bathymetry
    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      y[i][k]=0;

    // over each face
    for(nf=0;nf<grid->nfaces[i];nf++) 
      // we only apply this to non-boundary cells 
      // (meaning phys->D implicitly 0 at this point)
      if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1) {

        ne = grid->face[i*grid->maxfaces+nf];

        // determine the minimal k based on each cell over edge
        if(grid->ctop[nc]>grid->ctop[i])
          kmin = grid->ctop[nc];
        else
          kmin = grid->ctop[i];

        // create summed contributions for cell along the vertical
        for(k=kmin;k<grid->Nke[ne];k++) 
          y[i][k]+=(x[nc][k]-x[i][k])*grid->dzf[ne][k]*phys->D[ne];
      }
    // otherwise y[i][...] = 0 (boundary cell with no gradient in horiz.)

    // over all the depth (z dependence on Laplacian)
    for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++)
      // we are adding on the vertical diffusional part 
      y[i][k]+=coef[i][k]*x[i][k-1]-(coef[i][k]+coef[i][k+1])*x[i][k]+coef[i][k+1]*x[i][k+1];

    // apply top and bottom BCs in vertical, no flux on sides
    if(grid->ctop[i]<grid->Nk[i]-1) {
      // Top q=0 so q[i][grid->ctop[i]-1]=-q[i][grid->ctop[i]]
      k=grid->ctop[i];
      y[i][k]+=(-2*coef[i][k]-coef[i][k+1])*x[i][k]+coef[i][k+1]*x[i][k+1];

      // Bottom dq/dz = 0 so q[i][grid->Nk[i]]=q[i][grid->Nk[i]-1]
      k=grid->Nk[i]-1;
      y[i][k]+=coef[i][k]*x[i][k-1]-coef[i][k]*x[i][k];
    } 
    else
      y[i][grid->ctop[i]]-=2.0*coef[i][grid->ctop[i]]*x[i][grid->ctop[i]];
  }
}

/*
 * Function: GuessQ
 * Usage: Guessq(q,wold,w,grid,phys,prop,myproc,numprocs,comm);
 * ------------------------------------------------------------
 * Guess a pressure correction field that will enforce the hydrostatic velocity
 * field to speed up the convergence of the pressure Poisson equation.
 *
 */
static void GuessQ(REAL **q, REAL **wold, REAL **w, gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm) {

  int i, iptr, k;
  REAL qerror;

  // First compute the vertical velocity field that would satisfy continuity
  Continuity(w,grid,phys,prop);

  // Then use this velocity field to back out the required pressure field by
  // integrating w^{n+1}=w*-dt dq/dz and imposing the q=0 boundary condition at
  // the free-surface.
  for(iptr=grid->celldist[0];iptr<grid->celldist[2];iptr++) {
    i = grid->cellp[iptr];

    q[i][grid->ctop[i]]=grid->dzz[i][grid->ctop[i]]/2/prop->dt/prop->theta*
      (w[i][grid->ctop[i]]-wold[i][grid->ctop[i]]);
    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) {
      q[i][k]=q[i][k-1]+(grid->dzz[i][k]+grid->dzz[i][k-1])/(2.0*prop->dt*prop->theta)*
        (w[i][k]-wold[i][k]);
    }
  }
}

/*
 * Function: Preconditioner
 * Usage: Preconditioner(x,xc,coef,grid,phys,prop);
 * ------------------------------------------------
 * Multiply the vector x by the inverse of the preconditioner M with
 * xc = M^{-1} x
 *
 */
static void Preconditioner(REAL **x, REAL **xc, REAL **coef, gridT *grid, physT *phys, propT *prop) {
  int i, iptr, k, nf, ne, nc, kmin;
  REAL *a = phys->a, *b = phys->b, *c = phys->c, *d = phys->d;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i=grid->cellp[iptr];

    if(grid->ctop[i]<grid->Nk[i]-1) {
      for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++) {
        a[k]=coef[i][k];
        b[k]=-coef[i][k]-coef[i][k+1];
        c[k]=coef[i][k+1];
        d[k]=x[i][k];
      }

      // Top q=0 so q[i][grid->ctop[i]-1]=-q[i][grid->ctop[i]]
      k=grid->ctop[i];
      b[k]=-2*coef[i][k]-coef[i][k+1];
      c[k]=coef[i][k+1];
      d[k]=x[i][k];

      // Bottom dq/dz = 0 so q[i][grid->Nk[i]]=q[i][grid->Nk[i]-1]
      k=grid->Nk[i]-1;
      a[k]=coef[i][k];
      b[k]=-coef[i][k];
      d[k]=x[i][k];

      TriSolve(&(a[grid->ctop[i]]),&(b[grid->ctop[i]]),&(c[grid->ctop[i]]),
          &(d[grid->ctop[i]]),&(xc[i][grid->ctop[i]]),grid->Nk[i]-grid->ctop[i]);
      //      for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      //	xc[i][k]=x[i][k];
    } else 
      xc[i][grid->ctop[i]]=-0.5*x[i][grid->ctop[i]]/coef[i][grid->ctop[i]];
  }
}

/*
 * Function: ConditionQ
 * Usage: ConditionQ(x,grid,phys,prop,myproc,comm);
 * ------------------------------------------------
 * Compute the magnitude of the diagonal elements of the coefficient matrix
 * for the pressure-Poisson equation and place it into x after taking its square root.
 *
 */
static void ConditionQ(REAL **x, gridT *grid, physT *phys, propT *prop, int myproc, MPI_Comm comm) {

  int i, iptr, k, ne, nf, nc, kmin, warn=0;
  REAL *a = phys->a;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    for(k=grid->ctop[i];k<grid->Nk[i];k++) 
      x[i][k]=0;

    for(nf=0;nf<grid->nfaces[i];nf++) 
      if((nc=grid->neigh[i*grid->maxfaces+nf])!=-1) {

        ne = grid->face[i*grid->maxfaces+nf];

        if(grid->ctop[nc]>grid->ctop[i])
          kmin = grid->ctop[nc];
        else
          kmin = grid->ctop[i];

        for(k=kmin;k<grid->Nke[ne];k++) 
          x[i][k]+=grid->dzz[i][k]*phys->D[ne];
      }

    a[grid->ctop[i]]=grid->Ac[i]/grid->dzz[i][grid->ctop[i]];
    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++) 
      a[k] = 2*grid->Ac[i]/(grid->dzz[i][k]+grid->dzz[i][k-1]);

    for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++)
      x[i][k]+=(a[k]+a[k+1]);

    if(grid->ctop[i]<grid->Nk[i]-1) {
      // Top q=0 so q[i][grid->ctop[i]-1]=-q[i][grid->ctop[i]]
      k=grid->ctop[i];
      x[i][k]+=2*a[k]+a[k+1];

      // Bottom dq/dz = 0 so q[i][grid->Nk[i]]=q[i][grid->Nk[i]-1]
      k=grid->Nk[i]-1;
      x[i][k]+=a[k];
    }
  }

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    for(k=grid->ctop[i];k<grid->Nk[i];k++) {
      if(x[i][k]<=0) {
        x[i][k]=1;
        warn=1;
      }
      x[i][k]=sqrt(x[i][k]);
    }
  }
  if(WARNING && warn) printf("Warning...invalid preconditioner!\n");

  // Send the preconditioner to the neighboring processors.
  ISendRecvCellData3D(x,grid,myproc,comm);
}

/*
 * Function: GSSolve
 * Usage: GSSolve(grid,phys,prop,myproc,numprocs,comm);
 * ----------------------------------------------------
 * Solve the free surface equation with a Gauss-Seidell relaxation.
 * This function is used for debugging only.
 *
 */
// classic GS following nearly directly from Oliver's notes
static void GSSolve(gridT *grid, physT *phys, propT *prop, int myproc, int numprocs, MPI_Comm comm)
{
  int i, iptr, nf, ne, n, niters, *N;
  REAL *h, *hold, *D, *hsrc, myresid, resid, residold, tmp, relax, myNsrc, Nsrc, coef;

  h = phys->h;
  hold = phys->hold;
  D = phys->D;
  hsrc = phys->htmp;
  N = grid->normal;

  tmp = prop->grav*pow(prop->theta*prop->dt,2);

  // each processor must have all the boundary information 
  // for the starting values of h (as in Lh)
  ISendRecvCellData2D(h,grid,myproc,comm);

  relax = prop->relax;
  niters = prop->maxiters;
  resid=0;
  myresid=0;

  // debugging code since it's not used below
  //    myNsrc=0;
  //
  //    // for interior calculation cells 
  //    // this seems like debugging code
  //    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
  //      i = grid->cellp[iptr];
  //
  //      myNsrc+=pow(hsrc[i],2);
  //    }
  //    MPI_Reduce(&myNsrc,&(Nsrc),1,MPI_DOUBLE,MPI_SUM,0,comm);
  //    MPI_Bcast(&Nsrc,1,MPI_DOUBLE,0,comm);
  //    Nsrc=sqrt(Nsrc);

  for(n=0;n<niters;n++) {

    // hold = h;
    for(i=0;i<grid->Nc;i++) {
      hold[i] = h[i];
    }

    // for all the computational cells (since the boundary cells are 
    // already set in celldist[1] to celldist[...]
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      // get teh cell pointer
      i = grid->cellp[iptr];

      // right hand side (h_src)
      h[i] = hsrc[i];

      coef=1;
      for(nf=0;nf<grid->nfaces[i];nf++) 
        if(grid->neigh[i*grid->maxfaces+nf]!=-1) {
          ne = grid->face[i*grid->maxfaces+nf];

          // coef is the diagonal coefficient term
          coef+=tmp*phys->D[ne]*grid->df[ne]/grid->dg[ne]/grid->Ac[i];
          h[i]+=relax*tmp*phys->D[ne]*grid->df[ne]/grid->dg[ne]*
            phys->h[grid->neigh[i*grid->maxfaces+nf]]/grid->Ac[i];
        }
      // divide by diagonal coefficient term
      h[i]/=coef;
    }

    // now need to compare against the residual term to 
    // determine when GS has converged
    residold=resid;
    myresid=0;
    for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
      i = grid->cellp[iptr];

      hold[i] = hsrc[i];

      coef=1;
      for(nf=0;nf<grid->nfaces[i];nf++) 
        if(grid->neigh[i*grid->maxfaces+nf]!=-1) {
          ne = grid->face[i*grid->maxfaces+nf];
          coef+=tmp*phys->D[ne]*grid->df[ne]/grid->dg[ne]/grid->Ac[i];
          hold[i]+=tmp*phys->D[ne]*grid->df[ne]/grid->dg[ne]*
            phys->h[grid->neigh[i*grid->maxfaces+nf]]/grid->Ac[i];
        }
      // compute the residual
      myresid+=pow(hold[i]/coef-h[i],2);
    }
    MPI_Reduce(&myresid,&(resid),1,MPI_DOUBLE,MPI_SUM,0,comm);
    // - is this line necessary?
    MPI_Bcast(&resid,1,MPI_DOUBLE,0,comm);
    resid=sqrt(resid);

    // send all final results out to each processor now
    ISendRecvCellData2D(h,grid,myproc,comm);
    // wait for communication to finish
    MPI_Barrier(comm);

    // if we have met the tolerance criteria
    if(fabs(resid)<prop->epsilon)
      break;
  }
  if(n==niters && myproc==0 && WARNING) 
    printf("Warning... Iteration not converging after %d steps! RES=%e\n",n,resid);

  for(i=0;i<grid->Nc;i++)
    if(h[i]!=h[i]) 
      printf("NaN h[%d] in gssolve!\n",i);
}

/*
 * Function: Continuity
 * Usage: Continuity(w,grid,phys,prop);
 * ------------------------------------
 * Compute the vertical velocity field that satisfies continuity.  Use
 * the upwind flux face heights to ensure consistency with continuity.
 *
 */
void Continuity(REAL **w, gridT *grid, physT *phys, propT *prop)
//static void Continuity(REAL **w, gridT *grid, physT *phys, propT *prop)
{
  int i, k, nf, iptr, ne, nc1, nc2, j, jptr;
  REAL ap, am, dzfnew, theta=prop->theta,fac1,fac2,fac3,Ac,sum;
  
  fac1=prop->imfac1;
  fac2=prop->imfac2;
  fac3=prop->imfac3;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];

    for(k=0;k<grid->Nk[i]+1;k++)
      w[i][k] = 0;

    // Continuity is written in terms of flux-face heights at time level n
    // Even though w is updated to grid->ctop[i], it only uses dzf which is
    // 0 for new cells (i.e. if grid->ctop[i]<grid->ctopold[i]).
    // no flux into bottom cells
    w[i][grid->Nk[i]] = 0;
    for(k=grid->Nk[i]-1;k>=grid->ctop[i];k--) {
      // w_old is the old value for w (basically at timestep n) where
      // w in this context is for n+1
      if(prop->subgrid)
        w[i][k] = (subgrid->Acveff[i][k+1]*w[i][k+1]- fac2/fac1*(subgrid->Acveffold[i][k]*phys->w_old[i][k]-subgrid->Acveffold[i][k+1]*phys->w_old[i][k+1])-\
          fac3/fac1*(phys->w_old2[i][k]*subgrid->Acveffold2[i][k]-subgrid->Acveffold2[i][k+1]*phys->w_old2[i][k+1]))/subgrid->Acveff[i][k];
      else
        w[i][k] = w[i][k+1]- fac2/fac1*(phys->w_old[i][k]-phys->w_old[i][k+1])-fac3/fac1*(phys->w_old2[i][k]-phys->w_old2[i][k+1]);

      for(nf=0;nf<grid->nfaces[i];nf++) {
        ne = grid->face[i*grid->maxfaces+nf];
        // subgrid change
        if(prop->subgrid)
          Ac=subgrid->Acveff[i][k];
        else 
          Ac=grid->Ac[i];

        // set the flux vertical area is explicit
        if(k<grid->Nke[ne])
          w[i][k]-=(fac1*phys->u[ne][k]+fac2*phys->u_old[ne][k]+fac3*phys->u_old2[ne][k])*
            grid->df[ne]*grid->normal[i*grid->maxfaces+nf]/Ac/fac1*grid->dzf[ne][k];
      }
    }
  }

  // calculate w_im for update scalar
  for(i=0;i<grid->Nc;i++) {
    for(k=0;k<grid->Nk[i];k++)
      if(!prop->subgrid)
        phys->w_im[i][k]=fac2*phys->w_old[i][k]+fac3*phys->w_old2[i][k]+fac1*w[i][k];
      else
        phys->w_im[i][k]=(fac2*phys->w_old[i][k]*subgrid->Acveffold[i][k]+
                fac3*phys->w_old2[i][k]*subgrid->Acveffold2[i][k]+
                fac1*w[i][k]*subgrid->Acveff[i][k])/subgrid->Acveff[i][k];
    phys->w_im[i][grid->Nk[i]]=0;
  }
}

/*
 * Function: ComputeConservatives
 * Usage: ComputeConservatives(grid,phys,prop,myproc,numprocs,comm);
 * -----------------------------------------------------------------
 * Compute the total mass, volume, and potential energy within the entire
 * domain and return a warning if the mass and volume are not conserved to within
 * the tolerance CONSERVED specified in suntans.h 
 *
 */
void ComputeConservatives(gridT *grid, physT *phys, propT *prop, int myproc, int numprocs,
			  MPI_Comm comm)
{
  int i, iptr, k;
  REAL mass, volume, volh, height, Ep;

  if(myproc==0) phys->mass=0;
  if(myproc==0) phys->volume=0;
  if(myproc==0) phys->Ep=0;

  // volh is the horizontal integral of h+d, whereas vol is the
  // 3-d integral of dzz
  mass=0;
  volume=0;
  volh=0;
  Ep=0;

  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    height = 0;
    volh+=grid->Ac[i]*(grid->dv[i]+phys->h[i]);
    Ep+=0.5*prop->grav*grid->Ac[i]*(phys->h[i]+grid->dv[i])*(phys->h[i]-grid->dv[i]);
    for(k=grid->ctop[i];k<grid->Nk[i];k++) {
      height += grid->dzz[i][k];
      volume+=grid->Ac[i]*grid->dzz[i][k];
      mass+=phys->s[i][k]*grid->Ac[i]*grid->dzz[i][k];
    }
  }

  // Comment out the volh reduce if that integral is desired.  The
  // volume integral is used since the volh integral is useful
  // only for debugging.
  MPI_Reduce(&mass,&(phys->mass),1,MPI_DOUBLE,MPI_SUM,0,comm);
  //MPI_Reduce(&volh,&(phys->volume),1,MPI_DOUBLE,MPI_SUM,0,comm);
  MPI_Reduce(&volume,&(phys->volume),1,MPI_DOUBLE,MPI_SUM,0,comm);
  MPI_Reduce(&Ep,&(phys->Ep),1,MPI_DOUBLE,MPI_SUM,0,comm);

  // Compare the quantities to the original values at the beginning of the
  // computation.  If prop->n==0 (beginning of simulation), then store the
  // starting values for comparison.
  if(myproc==0) {
    if(prop->n==0) {
      phys->volume0 = phys->volume;
      phys->mass0 = phys->mass;
      phys->Ep0 = phys->Ep;
    } else {
      if(fabs((phys->volume-phys->volume0)/phys->volume0)>CONSERVED && prop->volcheck)
        printf("Warning! Not volume conservative at step %d! V(0)=%e, V(t)=%e\n",prop->n,
            phys->volume0,phys->volume);
      if(fabs((phys->mass-phys->mass0)/phys->volume0)>CONSERVED && prop->masscheck) 
        printf("Warning! Not mass conservative at step %d! M(0)=%e, M(t)=%e\n", prop->n,
            phys->mass0,phys->mass);
    }
  }
}

/*
 * Function: ComputeUCPerot
 * Usage: ComputeUCPerot(u,uc,vc,grid);
 * -------------------------------------------
 * Compute the cell-centered components of the velocity vector and place them
 * into uc and vc.  This function estimates the velocity vector with
 *
 * u = 1/Area * Sum_{faces} u_{face} normal_{face} df_{face}*d_{ef,face}
 *
 */
static void ComputeUCPerot(REAL **u, REAL **uc, REAL **vc, REAL *h, int kinterp, int subgridmodel, gridT *grid) {

  int k, n, ne, nf, iptr;
  REAL sum;

  // for each computational cell (non-stage defined)
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    // get cell pointer transfering from boundary coordinates 
    // to grid coordinates
    n=grid->cellp[iptr];

    // initialize over all depths
    for(k=0;k<grid->Nk[n];k++) {
      uc[n][k]=0;
      vc[n][k]=0;
    }
    // over all interior cells
    for(k=grid->ctop[n]+1;k<grid->Nk[n];k++) {
      // over each face
      for(nf=0;nf<grid->nfaces[n];nf++) {
        ne = grid->face[n*grid->maxfaces+nf];
        if(!(grid->smoothbot) || k<grid->Nke[ne]){
          uc[n][k]+=u[ne][k]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne]*grid->dzf[ne][k];
          vc[n][k]+=u[ne][k]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne]*grid->dzf[ne][k];
        }else{ 
          uc[n][k]+=u[ne][grid->Nke[ne]-1]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne]*grid->dzf[ne][grid->Nke[ne]-1];
          vc[n][k]+=u[ne][grid->Nke[ne]-1]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne]*grid->dzf[ne][grid->Nke[ne]-1];
        }
      }

      // In case of divide by zero (shouldn't happen)
      if (grid->dzz[n][k] > DRYCELLHEIGHT) {
          uc[n][k]/=(grid->Ac[n]*grid->dzz[n][k]);
          vc[n][k]/=(grid->Ac[n]*grid->dzz[n][k]);
      } else {
          uc[n][k] = 0;
          vc[n][k] = 0;
      }
    }

    //top cell only - don't account for depth
    k=grid->ctop[n];
    // over each face
    for(nf=0;nf<grid->nfaces[n];nf++) {
      ne = grid->face[n*grid->maxfaces+nf];
      if(!(grid->smoothbot) || k<grid->Nke[ne]){
        uc[n][k]+=u[ne][k]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
        vc[n][k]+=u[ne][k]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
      }else{ 
        uc[n][k]+=u[ne][grid->Nke[ne]-1]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
        vc[n][k]+=u[ne][grid->Nke[ne]-1]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];       
      }
    }
    uc[n][k]/=grid->Ac[n];
    vc[n][k]/=grid->Ac[n];
  }
}


/*static void ComputeUCPerot(REAL **u, REAL **uc, REAL **vc, REAL *h, int kinterp, int subgridmodel, gridT *grid) {

  int i,k, n, ne, nf, iptr,nc1,nc2,dry=1;
  REAL alpha,d,V;
  REAL sum;

  // for each computational cell (non-stage defined)
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    // get cell pointer transfering from boundary coordinates 
    // to grid coordinates
    n=grid->cellp[iptr];
    d=h[n]+grid->dv[n];

    // initialize over all depths
    for(k=0;k<grid->Nk[n];k++) {
      uc[n][k]=0;
      vc[n][k]=0;
    }
    
    // over the entire depth (cell depth)
    for(k=grid->ctop[n];k<grid->Nk[n];k++) {
      V=0;
      dry=1;
      for(nf=0;nf<grid->nfaces[n];nf++) {
        ne = grid->face[n*grid->maxfaces+nf];
        if(grid->dzf[ne][k]>0)
          dry=0;
        V+=grid->dzf[ne][k]*grid->df[ne]*grid->def[n*grid->maxfaces+nf];
      }        
      V/=2;
      // over each face
      for(nf=0;nf<grid->nfaces[n];nf++) {
        ne = grid->face[n*grid->maxfaces+nf];
        nc1=grid->grad[2*ne];
        nc2=grid->grad[2*ne+1];
        if(nc1==-1)
          nc1=nc2;
        if(nc2==-1)
          nc2=nc1;
      
        if(subgridmodel)
          if(V/subgrid->Acceff[n][k]/grid->dzz[n][k]<=1)
            alpha=grid->dzf[ne][k]/grid->dzz[n][k]*grid->Ac[n]/subgrid->Acceff[n][k];
          else
            alpha=grid->dzf[ne][k]*grid->Ac[n]/V;
        else{
          if(V<=grid->dzz[n][k]*grid->Ac[n])
            alpha=grid->dzf[ne][k]/grid->dzz[n][k];
          else
            alpha=grid->dzf[ne][k]*grid->Ac[n]/V;
        }

        if(dry)
          alpha=1;

        //most stable with different dzf
        // not best for varying dzf
        alpha=1;
        if(!(grid->smoothbot)|| k<grid->Nke[ne]){
          uc[n][k]+=alpha*u[ne][k]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
          vc[n][k]+=alpha*u[ne][k]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
        }
        else{	
          uc[n][k]+=alpha*u[ne][grid->Nke[ne]-1]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
          vc[n][k]+=alpha*u[ne][grid->Nke[ne]-1]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
        }

        if(grid->dzz[n][k]<=DRYCELLHEIGHT)
        {
          uc[n][k]=0;
          vc[n][k]=0;
        }  
      }

      uc[n][k]/=grid->Ac[n];
      vc[n][k]/=grid->Ac[n];
    }
  }
}*/

/*
 * Function: ReadProperties
 * Usage: ReadProperties(prop,grid,myproc);
 * -----------------------------------
 * This function reads in the properties specified in the suntans.dat
 * data file.  Note that if an entry does not exist, a default can be used.
 *
 */
void ReadProperties(propT **prop, gridT *grid, int myproc)
{ 
  // allocate memory
  *prop = (propT *)SunMalloc(sizeof(propT),"ReadProperties");

  // set values from suntans.dat file (DATAFILE)
  (*prop)->thetaramptime = MPI_GetValue(DATAFILE,"thetaramptime","ReadProperties",myproc);
  (*prop)->theta = MPI_GetValue(DATAFILE,"theta","ReadProperties",myproc);
  (*prop)->thetaS = MPI_GetValue(DATAFILE,"thetaS","ReadProperties",myproc);
  (*prop)->thetaB = MPI_GetValue(DATAFILE,"thetaB","ReadProperties",myproc);
  (*prop)->beta = MPI_GetValue(DATAFILE,"beta","ReadProperties",myproc);
  (*prop)->kappa_s = MPI_GetValue(DATAFILE,"kappa_s","ReadProperties",myproc);
  (*prop)->kappa_sH = MPI_GetValue(DATAFILE,"kappa_sH","ReadProperties",myproc);
  (*prop)->gamma = MPI_GetValue(DATAFILE,"gamma","ReadProperties",myproc);
  (*prop)->kappa_T = MPI_GetValue(DATAFILE,"kappa_T","ReadProperties",myproc);
  (*prop)->kappa_TH = MPI_GetValue(DATAFILE,"kappa_TH","ReadProperties",myproc);
  (*prop)->nu = MPI_GetValue(DATAFILE,"nu","ReadProperties",myproc);
  (*prop)->nu_H = MPI_GetValue(DATAFILE,"nu_H","ReadProperties",myproc);
  (*prop)->tau_T = MPI_GetValue(DATAFILE,"tau_T","ReadProperties",myproc);
  (*prop)->z0T = MPI_GetValue(DATAFILE,"z0T","ReadProperties",myproc);
  (*prop)->z0B = MPI_GetValue(DATAFILE,"z0B","ReadProperties",myproc);
  (*prop)->Intz0B= MPI_GetValue(DATAFILE,"Intz0B","ReadProperties",myproc);
  (*prop)->Intz0T= MPI_GetValue(DATAFILE,"Intz0T","ReadProperties",myproc); 

   // user_def_var for output 
   (*prop)->output_user_var= MPI_GetValue(DATAFILE,"outputuservar","ReadProperties",myproc); 

  if((*prop)->Intz0B==1)
    MPI_GetFile((*prop)->INPUTZ0BFILE,DATAFILE,"inputz0Bfile","ReadFileNames",myproc);
  if((*prop)->Intz0T==1)
    MPI_GetFile((*prop)->INPUTZ0TFILE,DATAFILE,"inputz0Tfile","ReadFileNames",myproc);

  (*prop)->CdT = MPI_GetValue(DATAFILE,"CdT","ReadProperties",myproc);
  (*prop)->CdB = MPI_GetValue(DATAFILE,"CdB","ReadProperties",myproc);
  (*prop)->CdW = MPI_GetValue(DATAFILE,"CdW","ReadProperties",myproc);
  (*prop)->grav= MPI_GetValue(DATAFILE,"grav","ReadProperties",myproc);
  (*prop)->turbmodel = (int)MPI_GetValue(DATAFILE,"turbmodel","ReadProperties",myproc);
  (*prop)->dt = MPI_GetValue(DATAFILE,"dt","ReadProperties",myproc);
  (*prop)->Cmax = MPI_GetValue(DATAFILE,"Cmax","ReadProperties",myproc);
  (*prop)->nsteps = (int)MPI_GetValue(DATAFILE,"nsteps","ReadProperties",myproc);
  (*prop)->ntout = (int)MPI_GetValue(DATAFILE,"ntout","ReadProperties",myproc);
  (*prop)->ntoutStore = (int)MPI_GetValue(DATAFILE,"ntoutStore","ReadProperties",myproc);

  if((*prop)->ntoutStore==0)
    (*prop)->ntoutStore=(*prop)->nsteps;

  (*prop)->ntprog = (int)MPI_GetValue(DATAFILE,"ntprog","ReadProperties",myproc);
  (*prop)->ntconserve = (int)MPI_GetValue(DATAFILE,"ntconserve","ReadProperties",myproc);
  (*prop)->nonhydrostatic = (int)MPI_GetValue(DATAFILE,"nonhydrostatic","ReadProperties",myproc);
  (*prop)->cgsolver = (int)MPI_GetValue(DATAFILE,"cgsolver","ReadProperties",myproc);
  (*prop)->maxiters = (int)MPI_GetValue(DATAFILE,"maxiters","ReadProperties",myproc);
  (*prop)->qmaxiters = (int)MPI_GetValue(DATAFILE,"qmaxiters","ReadProperties",myproc);
  (*prop)->qprecond = (int)MPI_GetValue(DATAFILE,"qprecond","ReadProperties",myproc);
  (*prop)->epsilon = MPI_GetValue(DATAFILE,"epsilon","ReadProperties",myproc);
  (*prop)->qepsilon = MPI_GetValue(DATAFILE,"qepsilon","ReadProperties",myproc);
  (*prop)->resnorm = MPI_GetValue(DATAFILE,"resnorm","ReadProperties",myproc);
  (*prop)->relax = MPI_GetValue(DATAFILE,"relax","ReadProperties",myproc);
  (*prop)->amp = MPI_GetValue(DATAFILE,"amp","ReadProperties",myproc);
  (*prop)->omega = MPI_GetValue(DATAFILE,"omega","ReadProperties",myproc);
  (*prop)->timescale = MPI_GetValue(DATAFILE,"timescale","ReadProperties",myproc);
  (*prop)->flux = MPI_GetValue(DATAFILE,"flux","ReadProperties",myproc);
  (*prop)->volcheck = MPI_GetValue(DATAFILE,"volcheck","ReadProperties",myproc);
  (*prop)->masscheck = MPI_GetValue(DATAFILE,"masscheck","ReadProperties",myproc);
  (*prop)->nonlinear = MPI_GetValue(DATAFILE,"nonlinear","ReadProperties",myproc);
  (*prop)->wetdry = MPI_GetValue(DATAFILE,"wetdry","ReadProperties",myproc);
  (*prop)->Coriolis_f = MPI_GetValue(DATAFILE,"Coriolis_f","ReadProperties",myproc);
  (*prop)->sponge_distance = MPI_GetValue(DATAFILE,"sponge_distance","ReadProperties",myproc);
  (*prop)->sponge_decay = MPI_GetValue(DATAFILE,"sponge_decay","ReadProperties",myproc);
  (*prop)->readSalinity = MPI_GetValue(DATAFILE,"readSalinity","ReadProperties",myproc);
  (*prop)->readTemperature = MPI_GetValue(DATAFILE,"readTemperature","ReadProperties",myproc);
  (*prop)->TVDsalt = MPI_GetValue(DATAFILE,"TVDsalt","ReadProperties",myproc);
  (*prop)->TVDtemp = MPI_GetValue(DATAFILE,"TVDtemp","ReadProperties",myproc);
  (*prop)->TVDturb = MPI_GetValue(DATAFILE,"TVDturb","ReadProperties",myproc);
  (*prop)->stairstep = MPI_GetValue(DATAFILE,"stairstep","ReadProperties",myproc);
  (*prop)->TVDmomentum = MPI_GetValue(DATAFILE,"TVDmomentum","ReadProperties",myproc); 
  (*prop)->conserveMomentum = MPI_GetValue(DATAFILE,"conserveMomentum","ReadProperties",myproc); 
  (*prop)->thetaM = MPI_GetValue(DATAFILE,"thetaM","ReadProperties",myproc); 
  (*prop)->newcells = MPI_GetValue(DATAFILE,"newcells","ReadProperties",myproc); 
  (*prop)->mergeArrays = MPI_GetValue(DATAFILE,"mergeArrays","ReadProperties",myproc); 
  (*prop)->computeSediments = MPI_GetValue(DATAFILE,"computeSediments","ReadProperties",myproc); 
  (*prop)->subgrid = MPI_GetValue(DATAFILE,"subgrid","ReadProperties",myproc); 
  (*prop)->marshmodel = MPI_GetValue(DATAFILE,"marshmodel","ReadProperties",myproc);
  (*prop)->wavemodel = MPI_GetValue(DATAFILE,"wavemodel","ReadProperties",myproc);
  (*prop)->culvertmodel = MPI_GetValue(DATAFILE,"culvertmodel","ReadProperties",myproc);
  (*prop)->vertcoord = MPI_GetValue(DATAFILE,"vertcoord","ReadProperties",myproc);
  (*prop)->ex = MPI_GetValue(DATAFILE,"ex","ReadProperties",myproc); //AB3
  (*prop)->im = MPI_GetValue(DATAFILE,"im","ReadProperties",myproc); //implicit method for momentum eqn.
  
  // setup the factors for implicit and explicit schemes
  if((*prop)->ex==1) // AX2
  {
    (*prop)->exfac1=7.0/4.0;
    (*prop)->exfac2=-1.0;
    (*prop)->exfac3=1.0/4.0;
  } else if((*prop)->ex==2) { // AB2
    (*prop)->exfac1=1.5;
    (*prop)->exfac2=-0.5;
    (*prop)->exfac3=0;
  } else { //AB3
    (*prop)->exfac1=23.0/12.0;
    (*prop)->exfac2=-4.0/3.0;
    (*prop)->exfac3=5.0/12.0;
  }

  if((*prop)->im==0) // theta
  {
    (*prop)->imfac1=(*prop)->theta;
    (*prop)->imfac2=1.0-(*prop)->theta;
    (*prop)->imfac3=0;
  } else if((*prop)->im==1) { // AM2
    (*prop)->imfac1=0.75;
    (*prop)->imfac2=0;
    (*prop)->imfac3=0.25;
  } else { //AI2
    (*prop)->imfac1=5.0/4.0;
    (*prop)->imfac2=-1.0;
    (*prop)->imfac3=3.0/4.0;
  }

  // When wetting and drying is desired:
  // -Do nonconservative momentum advection (conserveMomentum=0)
  // -Use backward Euler for vertical advection of horizontal momentum (thetaM=1)
  // -Update new cells (newcells=1)
  if((*prop)->wetdry) {
    (*prop)->conserveMomentum = 0;
    (*prop)->thetaM = 1;//Fully implicit
    //(*prop)->thetaM = 0.5;
    if((*prop)->vertcoord==1 || (*prop)->vertcoord==5)
      (*prop)->newcells = 1;
  }
  
  (*prop)->calcage = MPI_GetValue(DATAFILE,"calcage","ReadProperties",myproc);
  (*prop)->agemethod = MPI_GetValue(DATAFILE,"agemethod","ReadProperties",myproc);
  (*prop)->calcaverage = MPI_GetValue(DATAFILE,"calcaverage","ReadProperties",myproc);
  if ((*prop)->calcaverage)
      (*prop)->ntaverage = (int)MPI_GetValue(DATAFILE,"ntaverage","ReadProperties",myproc);
  (*prop)->latitude = MPI_GetValue(DATAFILE,"latitude","ReadProperties",myproc);
  (*prop)->gmtoffset = MPI_GetValue(DATAFILE,"gmtoffset","ReadProperties",myproc);
  (*prop)->metmodel = (int)MPI_GetValue(DATAFILE,"metmodel","ReadProperties",myproc);
  (*prop)->varmodel = (int)MPI_GetValue(DATAFILE,"varmodel","ReadProperties",myproc);
  (*prop)->nugget = MPI_GetValue(DATAFILE,"nugget","ReadProperties",myproc);
  (*prop)->sill = MPI_GetValue(DATAFILE,"sill","ReadProperties",myproc);
  (*prop)->range = MPI_GetValue(DATAFILE,"range","ReadProperties",myproc);
  (*prop)->outputNetcdf = (int)MPI_GetValue(DATAFILE,"outputNetcdf","ReadProperties",myproc);
  (*prop)->netcdfBdy = (int)MPI_GetValue(DATAFILE,"netcdfBdy","ReadProperties",myproc);
  (*prop)->readinitialnc = (int)MPI_GetValue(DATAFILE,"readinitialnc","ReadProperties",myproc);
  (*prop)->Lsw = MPI_GetValue(DATAFILE,"Lsw","ReadProperties",myproc);
  (*prop)->Cda = MPI_GetValue(DATAFILE,"Cda","ReadProperties",myproc);
  (*prop)->Ce = MPI_GetValue(DATAFILE,"Ce","ReadProperties",myproc);
  (*prop)->Ch = MPI_GetValue(DATAFILE,"Ch","ReadProperties",myproc);
  if((*prop)->outputNetcdf > 0 || (*prop)->netcdfBdy > 0 || (*prop)->readinitialnc > 0){ 
      MPI_GetString((*prop)->starttime,DATAFILE,"starttime","ReadProperties",myproc);
      MPI_GetString((*prop)->basetime,DATAFILE,"basetime","ReadProperties",myproc);

      (*prop)->nstepsperncfile=(int)MPI_GetValue(DATAFILE,"nstepsperncfile","ReadProperties",myproc);
      (*prop)->ncfilectr=(int)MPI_GetValue(DATAFILE,"ncfilectr","ReadProperties",myproc);
  }
  
  if((*prop)->nonlinear==2) {
    (*prop)->laxWendroff = MPI_GetValue(DATAFILE,"laxWendroff","ReadProperties",myproc);
    if((*prop)->laxWendroff!=0)
      (*prop)->laxWendroff_Vertical = MPI_GetValue(DATAFILE,"laxWendroff_Vertical","ReadProperties",myproc);
    else
      (*prop)->laxWendroff_Vertical = 0;
  } else {
    (*prop)->laxWendroff = 0;
    (*prop)->laxWendroff_Vertical = 0;
  }

  (*prop)->hprecond = MPI_GetValue(DATAFILE,"hprecond","ReadProperties",myproc);

  // addition for interpolation methods
  switch((int)MPI_GetValue(DATAFILE,"interp","ReadProperties",myproc)) {
    case 0: //Perot
      (*prop)->interp = PEROT;
      break;
    case 1: //Quad
      (*prop)->interp = QUAD;
      break;
    case 2: //Least-squares
      (*prop)->interp = LSQ;
      break;
 
    default:
      printf("ERROR: Specification of interpolation type is incorrect!\n");
      MPI_Finalize();
      exit(EXIT_FAILURE);
      break;
  }
  (*prop)->kinterp=(int)MPI_GetValue(DATAFILE,"kinterp","ReadProperties",myproc);

  if((int)MPI_GetValue(DATAFILE,"kinterp","ReadProperties",myproc))
  {  
    (*prop)->interp=PEROT;
    //if(myproc==0)
      //printf("kinterp is used, so interp is set as perot automatically\n");
  }
  
  if((*prop)->interp==QUAD && grid->maxfaces>DEFAULT_NFACES) {
    //printf("Warning in ReadProperties...interp set to PEROT for use with quad or hybrid grid.\n");
    (*prop)->interp=PEROT;
  }

  // additional data for pretty plot methods
  (*prop)->prettyplot = MPI_GetValue(DATAFILE,"prettyplot","ReadProperties",myproc);
  if((*prop)->prettyplot!=0 && grid->maxfaces>DEFAULT_NFACES) {
    printf("Warning in ReadProperties...prettyplot set to zero for use with quad or hybrid grid.\n");
    (*prop)->prettyplot=0;
  }

  // addition for linearized free surface where dzz=dz
  (*prop)->linearFS = (int)MPI_GetValue(DATAFILE,"linearFS","ReadProperties",myproc);
}

/*
 * Function: InterpToFace
 * Usage: uface = InterpToFace(j,k,phys->uc,u,grid);
 * -------------------------------------------------
 * Linear interpolation of a Voronoi-centered value to the face, using the equation
 * 
 * uface = 1/Dj*(def1*u2 + def2*u1);
 *
 * Note that def1 and def2 are not the same as grid->def[] unless the
 * triangles have not been corrected.  This affects the Coriolis term as currently 
 * implemented.
 *
 */
REAL InterpToFace(int j, int k, REAL **phi, REAL **u, gridT *grid) {
  int nc1, nc2;
  REAL def1, def2, Dj;
  nc1 = grid->grad[2*j];
  nc2 = grid->grad[2*j+1];
  if(nc1==-1)
    nc1=nc2;
  if(nc2==-1)
    nc2=nc1;

  Dj = grid->dg[j];
  def1=grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
  def2=Dj-def1;

  if(def1==0 || def2==0) {
    return UpWind(u[j][k],phi[nc1][k],phi[nc2][k]);
  }
  else {
    return (phi[nc1][k]*def2+phi[nc2][k]*def1)/(def1+def2);
  }
}

/*
 * Function: UFaceFlux
 * Usage: UFaceFlux(j,k,phi,phys->u,grid,prop->dt,prop->nonlinear);
 * ---------------------------------------------------------------------
 * Interpolation to obtain the flux of the scalar field phi (of type REAL **) on 
 * face j, k;  method==2: Central-differencing, method==4: Lax-Wendroff.
 *
 */
// note that we should not compute def1 and def2 in this function as they don't change
// these should be computed in grid.c and just looked up for efficiency
static REAL UFaceFlux(int j, int k, REAL **phi, REAL **u, gridT *grid, REAL dt, int method) {
  int nc1, nc2;
  REAL def1, def2, Dj, C=0;
  nc1 = grid->grad[2*j];
  nc2 = grid->grad[2*j+1];
  if(nc1==-1) nc1=nc2;
  if(nc2==-1) nc2=nc1;
  Dj = grid->dg[j];
  def1=grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
  def2=Dj-def1;

  if(method==4) C = u[j][k]*dt/Dj;
  if(method==2) C = 0;
  if(def1==0 || def2==0 || method==1) {
    // this happens on a boundary cell
    return UpWind(u[j][k],phi[nc1][k],phi[nc2][k]);
  }
  else {
    // on an interior cell (orthogonal cell) we can just distance interpolation the 
    // value to the face)
    // basically Eqn 51
    return (phi[nc1][k]*def2+phi[nc2][k]*def1)/(def1+def2)
      -C/2*(phi[nc1][k]-phi[nc2][k]);
  }
}

/*
 * Function: SetDensity
 * Usage: SetDensity(grid,phys,prop);
 * ----------------------------------
 * Sets the values of the density in the density array rho and
 * at the boundaries.
 *
 */
void SetDensity(gridT *grid, physT *phys, propT *prop) {
  int i, j, k, jptr, ib;
  REAL z, p;

  for(i=0;i<grid->Nc;i++) {
    z=phys->h[i];
    for(k=grid->ctop[i];k<grid->Nk[i];k++) {
      z+=0.5*grid->dzz[i][k];
      p=RHO0*prop->grav*z;
      phys->rho[i][k]=StateEquation(prop,phys->s[i][k],phys->T[i][k],p);
      z+=0.5*grid->dzz[i][k];
    }
  }

  for(jptr=grid->edgedist[2];jptr<grid->edgedist[3];jptr++) {
    j=grid->edgep[jptr];
    ib=grid->grad[2*j];
    z=phys->h[ib];
    for(k=grid->ctop[ib];k<grid->Nk[ib];k++) {
      z+=0.5*grid->dzz[ib][k];
      p=RHO0*prop->grav*z;
      phys->boundary_rho[jptr-grid->edgedist[2]][k]=
        StateEquation(prop,phys->boundary_s[jptr-grid->edgedist[2]][k],
            phys->boundary_T[jptr-grid->edgedist[2]][k],p);
      z+=0.5*grid->dzz[ib][k];
    }
  }
}

/*
 * Function: SetFluxHeight
 * Usage: SetFluxHeight(grid,phys,prop);
 * -------------------------------------
 * Set the value of the flux height dzf at time step n for use
 * in continuity and scalar transport.
 *
 */
void SetFluxHeight(gridT *grid, physT *phys, propT *prop) {
  //static void SetFluxHeight(gridT *grid, physT *phys, propT *prop) {
  int i, j, k, nc1, nc2;
  REAL dz_bottom, dzsmall=grid->dzsmall,h_uw,utmp;

  //  // assuming upwinding
  //  for(j=0;j<grid->Ne;j++) {
  //    nc1 = grid->grad[2*j];
  //    nc2 = grid->grad[2*j+1];
  //    if(nc1==-1) nc1=nc2;
  //    if(nc2==-1) nc2=nc1;
  //
  //    for(k=0;k<grid->etop[j];k++)
  
  //      grid->dzf[j][k]=0;
  //    for(k=grid->etop[j];k<grid->Nke[j];k++) 
  //      grid->dzf[j][k]=UpWind(phys->u[j][k],grid->dzz[nc1][k],grid->dzz[nc2][k]);
  //
  //    k=grid->Nke[j]-1;
  //    /* This works with Wet/dry but not with cylinder case...*/
  //    if(grid->etop[j]==k)
  //      grid->dzf[j][k]=Max(0,UpWind(phys->u[j][k],phys->h[nc1],phys->h[nc2])+Min(grid->dv[nc1],grid->dv[nc2]));
  //    else 
  //      grid->dzf[j][k]=Min(grid->dzz[nc1][k],grid->dzz[nc2][k]);
  //    /*
  //       if(grid->Nk[nc1]!=grid->Nk[nc2])
  //       dz_bottom = Min(grid->dzz[nc1][k],grid->dzz[nc2][k]);
  //       else
  //       dz_bottom = 0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]);
  //       if(k==grid->etop[j])
  //       grid->dzf[j][k]=Max(0,dz_bottom + UpWind(phys->u[j][k],phys->h[nc1],phys->h[nc2]));
  //       else
  //       grid->dzf[j][k]=dz_bottom;
  //     */
  //
  //    for(k=grid->etop[j];k<grid->Nke[j];k++) 
  //      if(grid->dzf[j][k]<=DRYCELLHEIGHT)
  //        grid->dzf[j][k]=0;
  //  }
  // initialize dzf
  for(j=0;j<grid->Ne;j++) 
  {
    grid->hf[j]=0;
    for(k=0; k<grid->Nkc[j];k++)
      grid->dzf[j][k]=0;
  }

  // assuming central differencing
  if(grid->smoothbot && prop->vertcoord==1)
    for(i=0;i<grid->Nc;i++)
      grid->dzz[i][grid->Nk[i]-1]=Max(grid->dzbot[i],grid->smoothbot*grid->dz[grid->Nk[i]-1]); 

  for(j=0;j<grid->Ne;j++) {
    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];
    if(nc1==-1) nc1=nc2;
    if(nc2==-1) nc2=nc1;

    for(k=0;k<grid->etop[j];k++)
      grid->dzf[j][k]=0;
 
    for(k=grid->etop[j];k<grid->Nke[j];k++){
      grid->dzf[j][k]=UpWind(phys->u[j][k],grid->dzz[nc1][k],grid->dzz[nc2][k]);
      if(prop->vertcoord!=1 && prop->vertcoord!=5){
        if(grid->mark[j]==0)
        {
          if(phys->u[j][k]>0)
            grid->dzf[j][k]=phys->SfHp[j][k];
          else
            grid->dzf[j][k]=phys->SfHm[j][k];
        }
      }
    }
    k=grid->Nke[j]-1;

    /* This works with Wet/dry but not with cylinder case...*/
    if(grid->etop[j]==k) {
        grid->dzf[j][k]=Max(0,UpWind(phys->u[j][k],phys->h[nc1],phys->h[nc2])+Min(grid->dv[nc1],grid->dv[nc2]));
      // added part
     if(grid->mark[j]==2 && grid->dzf[j][k]<=0.01)
        grid->dzf[j][k]=0.01;
        
    } else{
      // this only works for the stair-like z-level coordinate
      if(prop->vertcoord==1 || prop->vertcoord==5)
        grid->dzf[j][k]=Min(grid->dzz[nc1][k],grid->dzz[nc2][k]);
    }
    /*
       if(grid->Nk[nc1]!=grid->Nk[nc2])
       dz_bottom = Min(grid->dzz[nc1][k],grid->dzz[nc2][k]);
       else
       dz_bottom = 0.5*(grid->dzz[nc1][k]+grid->dzz[nc2][k]);
       if(k==grid->etop[j])
       grid->dzf[j][k]=Max(0,dz_bottom + UpWind(phys->u[j][k],phys->h[nc1],phys->h[nc2]));
       else
       grid->dzf[j][k]=dz_bottom;
       */

    for(k=grid->etop[j];k<grid->Nke[j];k++) 
      if(grid->dzf[j][k]<=DRYCELLHEIGHT)
        grid->dzf[j][k]=0;
      
    for(k=grid->etop[j];k<grid->Nke[j];k++)
      grid->hf[j]+=grid->dzf[j][k];
  } 

  //set minimum dzz
  if(grid->smoothbot && prop->vertcoord==1)
    for(i=0;i<grid->Nc;i++)
      grid->dzz[i][grid->Nk[i]-1]=Max(grid->dzz[i][grid->Nk[i]-1],dzsmall*grid->dz[grid->Nk[i]-1]);
}

/*
 * Function: ComputeUC
 * Usage: ComputeUC(u,uc,vc,grid);
 * -------------------------------------------
 * Compute the cell-centered components of the velocity vector and place them
 * into uc and vc.  
 *
 */
//inline static void ComputeUC(physT *phys, gridT *grid, int myproc) {
void ComputeUC(REAL **ui, REAL **vi, physT *phys, gridT *grid, int myproc, interpolation interp,int kinterp, int subgridmodel) {

  switch(interp) {
    case QUAD:
      // using Wang et al 2011 methods
      ComputeUCRT(ui, vi, phys,grid, myproc);
      break;
    case PEROT:
      ComputeUCPerot(phys->u,ui,vi,phys->h,kinterp,subgridmodel,grid);
      break;
    case LSQ:
      ComputeUCLSQ(phys->u,ui,vi,grid,phys);
      break;
    default:
      break;
  }

}

/*
 * Function: ComputeUCRT
 * Usage: ComputeUCRT(u,uc,vc,grid);
 * -------------------------------------------
 * Compute the cell-centered components of the velocity vector and place them
 * into uc and vc.  This function estimates the velocity vector with
 * methods outlined in Wang et al, 2011
 *
 */
static void ComputeUCRT(REAL **ui, REAL **vi, physT *phys, gridT *grid, int myproc) {

  int k, n, ne, nf, iptr;
  REAL sum;

  // first we need to reconstruct the nodal velocities using the RT0 basis functions
  //  if(myproc==0) printf("ComputeNodalVelocity\n");
  ComputeNodalVelocity(phys, grid, nRT2, myproc);

  // now we can get the tangential velocities from these results
  //  if(myproc==0) printf("ComputeTangentialVelocity\n");
  ComputeTangentialVelocity(phys, grid, nRT2, tRT2, myproc);

  // for each computational cell (non-stage defined)
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    // get cell pointer transfering from boundary coordinates 
    // to grid coordinates
    n=grid->cellp[iptr];

    // initialize over all depths
    for(k=0;k<grid->Nk[n];k++) {
      phys->uc[n][k]=0;
      phys->vc[n][k]=0;
    }

    // over the entire depth (cell depth)
    // possible mistake here- make sure to the get the right value for the start
    // of the loop (etop?)
    //    if(myproc==0) printf("ComputeQuadraticInterp\n");
    if(grid->nfaces[n]==3){
      for(k=grid->ctop[n];k<grid->Nk[n];k++) {
        // now we can compute the quadratic interpolated velocity from these results
        ComputeQuadraticInterp(grid->xv[n], grid->yv[n], n, k, ui, 
            vi, phys, grid, nRT2, tRT2, myproc);
      }
    } else {
       // over the entire depth (cell depth)
       for(k=grid->ctop[n];k<grid->Nk[n];k++) {
        // over each face
        for(nf=0;nf<grid->nfaces[n];nf++) {
          ne = grid->face[n*grid->maxfaces+nf];
          if(!(grid->smoothbot) || k<grid->Nke[ne]){
            phys->uc[n][k]+=phys->u[ne][k]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
            phys->vc[n][k]+=phys->u[ne][k]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
          }
          else{	
            phys->uc[n][k]+=phys->u[ne][grid->Nke[ne]-1]*grid->n1[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
            phys->vc[n][k]+=phys->u[ne][grid->Nke[ne]-1]*grid->n2[ne]*grid->def[n*grid->maxfaces+nf]*grid->df[ne];
          }
        }

        phys->uc[n][k]/=grid->Ac[n];
        phys->vc[n][k]/=grid->Ac[n];
      }
    }
  } 
}

/*
 * Function: ComputeUCLSQ
 * Usage: ComputeUCRT(u,uc,vc,grid);
 * -------------------------------------------
 * Compute the cell-centered components of the velocity vector and place them
 * into uc and vc.  This function estimates the velocity vector with the least square method
 *
 */
static void ComputeUCLSQ(REAL **u, REAL **uc, REAL **vc, gridT *grid, physT *phys){
  int k, n, ne, nf, iptr;
  REAL sum;
  int ii,jj,kk;
  REAL **A = phys->A;
  REAL **AT = phys->AT;
  REAL **Apr = phys->Apr;
  REAL *bpr = phys->bpr;

  // for each computational cell (non-stage defined)
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    // get cell pointer transfering from boundary coordinates 
    // to grid coordinates
    n=grid->cellp[iptr];

    // initialize over all depths
    for(k=0;k<grid->Nk[n];k++) {
      uc[n][k]=0;
      vc[n][k]=0;
    }
    for(nf=0;nf<grid->nfaces[n];nf++) {

        ne = grid->face[n*grid->maxfaces+nf];
        
        // Construct the normal array - A
        A[nf][0] = grid->n1[ne];
        A[nf][1] = grid->n2[ne];

        // Construct A transpose
        AT[0][nf] = A[nf][0];
        AT[1][nf] = A[nf][1];
    }

    for(k=grid->ctop[n];k<grid->Nk[n];k++) {
        
        // Multiply A' = A^T*A 
        // This can't be moved outside of this for loop because A' is
        // modified by linsolve()
        for(ii=0;ii<2;ii++){
            for(jj=0;jj<2;jj++){
                sum=0;
                for(nf=0;nf<grid->nfaces[n];nf++) {
                    sum += AT[ii][nf]*A[nf][jj];
                }
                Apr[ii][jj]=sum;
            }
        }
        // Compute b' = A^T * b
        for(ii=0;ii<2;ii++){
            sum=0;
            for(nf=0;nf<grid->nfaces[n];nf++) {
                ne = grid->face[n*grid->maxfaces+nf];
                if(!(grid->smoothbot) || k<grid->Nke[ne]){
                    sum += AT[ii][nf]*u[ne][k];
                    //sum += AT[ii][nf]*u[ne][k]*grid->dzf[ne][k];
                } else{
                    sum += AT[ii][nf]*u[ne][grid->Nke[ne]-1];
                    //sum += AT[ii][nf]*u[ne][grid->Nke[ne]-1]*grid->dzf[ne][grid->Nke[ne]-1];
                }
            }
            bpr[ii]=sum;
        }
        // Now solve the problem (A'A)x = (A'b)
        linsolve(Apr,bpr,2);
       
        uc[n][k]=bpr[0];
        vc[n][k]=bpr[1];        
        //if (grid->dzz[n][k]  > 1e-3) {
        //    uc[n][k]=bpr[0]/grid->dzz[n][k];
        //    vc[n][k]=bpr[1]/grid->dzz[n][k];
        //}else{
        //    uc[n][k]=0;
        //    vc[n][k]=0;
        //}
    }

  }
}


/*
 * Function: ComputeQuadraticInterp
 * Usage: ComputeQuadraticInterp(U, V, un, grid, ninterp, tinterp, myproc)
 * -------------------------------------------
 * Compute the quadratic interpolation of the velocity based on
 * a choice for tinterp.
 * Two options presently exist based on choice of tinterp:
 *  1. tRT1
 *  2. tRT2
 * which are outlined in Wang et al, 2011.
 *
 */
static void  ComputeQuadraticInterp(REAL x, REAL y, int ic, int ik, REAL **uc, 
    REAL **vc, physT *phys, gridT *grid, interpolation ninterp, 
    interpolation tinterp, int myproc) {
  // this formulation considers a specific cell ic and a specific level ik
  // for computation of uc and vc

  // we need to have several pieces for this to work: 
  // we need to be able to quickly compute the area subsets in the triangle
  // A12, A13, A23; we need nodal velocity vectors (nRT2), and tangential
  // velocity vectors
  REAL points[2][grid->nfaces[ic]];
  REAL xt[3], yt[3];
  REAL SubArea[grid->nfaces[ic]];
  REAL nu[grid->nfaces[ic]], nv[grid->nfaces[ic]];
  REAL eu[grid->nfaces[ic]], ev[grid->nfaces[ic]];
  int np[grid->nfaces[ic]], ne[grid->nfaces[ic]], nf, ie, ip;
  const REAL TotalArea = grid->Ac[ic];

  // loop over each of the faces 
  // possible error if NUMEDGECOLUMNS != NFACES!!
  for( nf=0; nf < grid->nfaces[ic]; nf ++) {
    // get the index for the vertex of the cell
    np[nf] = grid->cells[grid->maxfaces*ic + nf];
    // get the index for the cell edge
    ne[nf] = grid->face[grid->maxfaces*ic + nf];

    // get the coordinates for the points for the vertex
    // to compute the area of the subtriangle
    points[0][nf] = grid->xp[np[nf]];
    points[1][nf] = grid->yp[np[nf]];
  }
  // now we have the points for each node: points[x/y][nf=0,1,2]
  // and also the edges via grid->face[NFACES*ic + nf=0,1,2] for
  // each cell ic

  // now all that remains is to store the velocities and then
  // get A12, A13, A23 and we can 
  // use the formula after getting the vector values for the 
  // velocities at each of the points

  for( nf=0; nf < grid->nfaces[ic]; nf++) {
    // get the points for each vertex of the triangle
    xt[0] = points[0][nf];
    yt[0] = points[1][nf];
    // wrap the next one if it goes out of bounds
    xt[1] = points[0][(nf+1)%grid->nfaces[ic]]; 
    yt[1] = points[1][(nf+1)%grid->nfaces[ic]];
    // the interpolated center
    xt[2] = x;
    yt[2] = y;

    // get the area from the points where 
    // nf=0 => A12, nf=1 => A23, nf=2 => A13 from Wang et al 2011
    // note that this is normalized
    SubArea[nf] = GetArea(xt, yt, 3)/TotalArea;
    //          printf("nf = %d Area=%e\n", nf, SubArea[nf]);
  }

  // get the nodal and edge velocities projected unto global x,y coords
  for (nf=0; nf < grid->nfaces[ic]; nf++) {
    // node and edge indexes
    ip = np[nf]; ie = ne[nf];
    // get the nodal velocity components 
    nu[nf] = phys->nRT2u[ip][ik];
    nv[nf] = phys->nRT2v[ip][ik];
    // get the tangential velocity components
    if(tinterp == tRT2) {
      eu[nf] = phys->u[ie][ik]*grid->n1[ie] + phys->tRT2[ie][ik]*grid->n2[ie];
      ev[nf] = phys->u[ie][ik]*grid->n2[ie] - phys->tRT2[ie][ik]*grid->n1[ie];
    }
    else if(tinterp == tRT1) {
      // need to have the specific cell neighbor here!  Current implementation 
      // is not correct.  This seems like the easiest way to make sure 
      // that this works
      eu[nf] = phys->u[ie][ik]*grid->n1[ie] + phys->tRT1[ie][ik]*grid->n2[ie];
      ev[nf] = phys->u[ie][ik]*grid->n2[ie] - phys->tRT1[ie][ik]*grid->n1[ie];
    }
  }

  // now perform interpolation from the results via Wang et al 2011 eq 12
  uc[ic][ik] = 
    (2*SubArea[1]-1)*SubArea[1]*nu[0] 
    + (2*SubArea[2]-1)*SubArea[2]*nu[1]
    + (2*SubArea[0]-1)*SubArea[0]*nu[2]
    + 4*SubArea[2]*SubArea[1]*eu[0]
    + 4*SubArea[0]*SubArea[2]*eu[1]
    + 4*SubArea[0]*SubArea[1]*eu[2];
  vc[ic][ik] = 
    (2*SubArea[1]-1)*SubArea[1]*nv[0] 
    + (2*SubArea[2]-1)*SubArea[2]*nv[1]
    + (2*SubArea[0]-1)*SubArea[0]*nv[2]
    + 4*SubArea[2]*SubArea[1]*ev[0]
    + 4*SubArea[0]*SubArea[2]*ev[1]
    + 4*SubArea[0]*SubArea[1]*ev[2];

}

/*
 * Function: ComputeTangentialVelocity
 * Usage: ComputeTangentialVelocity(phys, grid, ninterp, tinterp, myproc)
 * -------------------------------------------
 * Compute the tangential velocity from nodal velocities (interp determines type)
 * Two options presently exist based on choice of tinterp:
 *  1. tRT1
 *  2. tRT2
 * which are outlined in Wang et al, 2011.
 *
 */
static void  ComputeTangentialVelocity(physT *phys, gridT *grid, 
    interpolation ninterp, interpolation tinterp, int myproc) {
  int ie, in, ic, tn, ink;
  int nodes[2], cells[2];
  REAL tempu, tempv, tempA; 
  int tempnode, tempcell;


  if(tinterp == tRT2) {
    // for each edge, we must compute the value of its tangential velocity 
    for(ie = 0; ie < grid->Ne; ie++) {

      // get the nodes on either side of the edge
      nodes[0] = grid->edges[NUMEDGECOLUMNS*ie];
      nodes[1] = grid->edges[NUMEDGECOLUMNS*ie+1];
      //     printf("nodes = %d %d\n", nodes[0], nodes[1]);

      // for each layer at the edge
      for (ink = 0; ink < grid->Nkc[ie]; ink++) {
        // initialize temporary values
        tempu = tempv = tempA = 0;

        // for each node
        for (in = 0; in < 2; in++) {
          // get particular value for node 
          tempnode = nodes[in];

          // ensure that the node exists at the level 
          // before continuing
          if(ink < grid->Nkp[tempnode]) {
            // accumulate the velocity area-weighted values and 
            // cell area
            tempA += grid->Actotal[tempnode][ink];
            tempu += grid->Actotal[tempnode][ink]*phys->nRT2u[tempnode][ink];
            tempv += grid->Actotal[tempnode][ink]*phys->nRT2v[tempnode][ink];
          }
        }
        // area-average existing nodal velocities
        if(tempA == 0) {
          phys->tRT2[ie][ink] = 0;
        }
        else {
          tempu /= tempA;
          tempv /= tempA;
          // get the correct component in the tangential direction to store it
          phys->tRT2[ie][ink] = grid->n2[ie]*tempu - grid->n1[ie]*tempv;
        }
      }

    }

  }
}

/*
 * Function: ComputeNodalVelocity
 * Usage: ComputeNodalVelocity(phys, grid, interp, myproc)
 * -------------------------------------------
 * Compute the nodal velocity using RT0 basis functions.  
 * Two options presently exist based on choice of interp:
 *  1. nRT1
 *  2. nRT2
 * which are outlined in Wang et al, 2011.
 *
 */
static void ComputeNodalVelocity(physT *phys, gridT *grid, interpolation interp, int myproc) {
  //  int in, ink, e1, e2, cell, cp1, cp2;
  int in, ink, inpc, ie, intemp, cell, cp1, cp2,
      e1, e2, n1, n2, onode;
  REAL tempu, tempv, Atemp, tempAu, tempAv;


  /* compute the nodal velocity for RT1 elements */
  // for each node compute its nodal value
  for(in = 0; in < grid->Np; in++) {/*{{{*/
    // there are Nkp vertical layers for each node, so much compute over each 
    // of these

    for(ink = 0; ink < grid->Nkp[in]; ink++) {
      // there will be numpcneighs values for each node at a
      //particular layer so we must compute each separately

      if(interp == nRT2) { Atemp = tempAu = tempAv = 0; }
      for(inpc = 0; inpc < grid->numpcneighs[in]; inpc++) {

        // check to make sure that the cell under consideration exists at the given z-level
        if (ink < grid->Nk[grid->pcneighs[in][inpc]]) { 
          // if so get neighbors to the node
          e1 = grid->peneighs[in][2*inpc];
          e2 = grid->peneighs[in][2*inpc+1];

          // compute the RT0 reconstructed nodal value for the edges
          ComputeRT0Velocity(&tempu, &tempv, 
              grid->n1[e1], grid->n2[e1], grid->n1[e2], grid->n2[e2], phys->u[e1][ink], phys->u[e2][ink]);

          // store the computed values
          phys->nRT1u[in][ink][inpc] = tempu;
          phys->nRT1v[in][ink][inpc] = tempv;
          if(interp == nRT2) {
            // accumulate area and weighted values
            Atemp += grid->Ac[grid->pcneighs[in][inpc]];
            tempAu += grid->Ac[grid->pcneighs[in][inpc]]*tempu;
            tempAv += grid->Ac[grid->pcneighs[in][inpc]]*tempv;
          }

        } 
        else { // cell neighbor doesn't exist at this level
          phys->nRT1u[in][ink][inpc] = 0;
          phys->nRT1v[in][ink][inpc] = 0;
        }
      }
      // compute nRT2 from nRT1 values if desired  (area-weighted average of all cells)
      if(interp == nRT2) {
        if(Atemp == 0) {
          printf("Error as Atemp is 0 in nodal calc!! at in,ink,Nkp=%d,%d,%d\n",in, ink, grid->Nkp[in]);
          // print all nodal neighbors
          printf("cell neighbors = ");
          for(inpc = 0; inpc < grid->numpcneighs[in]; inpc++) {
            printf(" %d(%d)", grid->pcneighs[in][inpc], grid->Nk[grid->pcneighs[in][inpc]]);

          }
          printf("\n");

        }
        /* now we can compute the area-averaged values with nRT1 elements */
        phys->nRT2u[in][ink] = tempAu/Atemp;
        phys->nRT2v[in][ink] = tempAv/Atemp;
      }
    }
  }
}

/*
 * Function: ComputeRT0Velocity
 * Usage: ComputeRT0Velocity(REAL* tempu, REAL* tempv, int e1, int e2, physT* phys);
 * -------------------------------------------
 * Compute the nodal velocity using RT0 basis functions (Appendix B, Wang et al 2011)
 *
 */
static void ComputeRT0Velocity(REAL *tempu, REAL *tempv, REAL e1n1, REAL e1n2, 
    REAL e2n1, REAL e2n2, REAL Uj1, REAL Uj2) 
{
  const REAL det = e1n1*e2n2 - e1n2*e2n1;
  // compute using the basis functions directly with inverted 2x2 matrix
  *tempu = (e2n2*Uj1 - e1n2*Uj2)/det;
  *tempv = (e1n1*Uj2 - e2n1*Uj1)/det;
}

/*
 * Function: HFaceFlux
 * Usage: HFaceFlux(j,k,phi,phys->u,grid,prop->dt,prop->nonlinear);
 * ---------------------------------------------------------------------
 * Interpolation to obtain the flux of the scalar field phi (of type REAL **) on 
 * face j, k;  method==2: Central-differencing, method==4: Lax-Wendroff.
 *
 */
// note that we should not compute def1 and def2 in this function as they don't change
// these should be computed in grid.c and just looked up for efficiency
static REAL HFaceFlux(int j, int k, REAL *phi, REAL **u, gridT *grid, REAL dt, int method) {
  int nc1, nc2;
  REAL def1, def2, Dj, C=0;
  nc1 = grid->grad[2*j];
  nc2 = grid->grad[2*j+1];
  if(nc1==-1) nc1=nc2;
  if(nc2==-1) nc2=nc1;
  Dj = grid->dg[j];
  def1=grid->def[nc1*grid->maxfaces+grid->gradf[2*j]];
  def2=Dj-def1;

  if(method==4) C = u[j][k]*dt/Dj;

  if(def1==0 || def2==0) {
    // this happens on a boundary cell
    return UpWind(u[j][k],phi[nc1],phi[nc2]);
  }
  else {
    // on an interior cell (orthogonal cell) we can just distance interpolation the 
    // value to the face)
    // basically Eqn 51
    return (phi[nc1]*def2+phi[nc2]*def1)/(def1+def2)
      -C/2*(phi[nc1]-phi[nc2]);
  }
}

/*
 * Function: getTsurf
 * --------------------------------------------
 * Returns the temperature at the surface cell
 */
static void getTsurf(gridT *grid, physT *phys){
  int i, iptr, ktop;
  int Nc = grid->Nc;
  
  //for(i=0;i<Nc;i++) {
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    ktop = grid->ctop[i];
    phys->Tsurf[i] = phys->T[i][ktop];
  }
}

/*
 * Function: getchangeT
 * ----------------------------------------------------------------
 * Returns the change in temperature at the surface cell
 */
static void getchangeT(gridT *grid, physT *phys){
  int i, iptr, ktop;
  int Nc = grid->Nc;
  
  //for(i=0;i<Nc;i++) {
  for(iptr=grid->celldist[0];iptr<grid->celldist[1];iptr++) {
    i = grid->cellp[iptr];
    ktop = grid->ctop[i];
    phys->dT[i] = phys->T[i][ktop] - phys->Tsurf[i];
  }
}
 /*
 *
 */
static void GetMomentumFaceValues(REAL **uface, REAL **ui, REAL **boundary_ui, REAL **U, gridT *grid, physT *phys, propT *prop,
				  MPI_Comm comm, int myproc, int nonlinear) {
  int i, iptr, j, jptr, nc, nf, ne, nc1, nc2, k, kmin;
  REAL tempu;

  for(jptr=grid->edgedist[2];jptr<grid->edgedist[3];jptr++) {
    j = grid->edgep[jptr];
    i = grid->grad[2*j];

    for(k=grid->etop[j];k<grid->Nke[j];k++) {
      if(U[j][k]>0)
        uface[j][k]=boundary_ui[jptr-grid->edgedist[2]][k];
      else
        uface[j][k]=ui[i][k];
    }
  }

  // type 4 boundary conditions used for no slip 
  // but typically we think of no-slip boundary conditions as 
  // not having flux across them, so this is more general
  for(jptr=grid->edgedist[4];jptr<grid->edgedist[5];jptr++) {
    j = grid->edgep[jptr];
    
    for(k=grid->etop[j];k<grid->Nke[j];k++)
      uface[j][k]=boundary_ui[jptr-grid->edgedist[2]][k];
  }

  if(prop->nonlinear==5) //use tvd for advection of momemtum
    HorizontalFaceScalars(grid,phys,prop,ui,boundary_ui,prop->TVDmomentum,comm,myproc);

  // over each of the "computational" cells
  // Compute the u-component fluxes at the faces
  for(jptr=grid->edgedist[0];jptr<grid->edgedist[1];jptr++) {
    j = grid->edgep[jptr];
    
    nc1 = grid->grad[2*j];
    nc2 = grid->grad[2*j+1];

    // figure out which column adjacent to edge is limiter for the
    // top of the cell (cells much share face to have flux)
    if(grid->ctop[nc1]>grid->ctop[nc2])
      kmin = grid->ctop[nc1];
    else
      kmin = grid->ctop[nc2];
    
    for(k=0;k<kmin;k++)
      uface[j][k]=0;

    // compute mass flow rate by interpolating the velocity to the face flux
    // note that we can use UFaceFlux to just compute the value directly if 
    // we have quadratic upwinding available to us
    // basically Eqn 47
    for(k=kmin;k<grid->Nke[j];k++) {
      // compute upwinding data
      if(U[j][k]>0)
        nc=nc2;
      else
        nc=nc1;

      switch(prop->nonlinear) {
        case 1:
          uface[j][k]=ui[nc][k];
          break;
        case 2:
          uface[j][k]=UFaceFlux(j,k,ui,U,grid,prop->dt,nonlinear);
          break;
        case 4:
          uface[j][k]=UFaceFlux(j,k,ui,U,grid,prop->dt,nonlinear);
          break;
        case 5:
          if(U[j][k]>0)
            tempu=phys->SfHp[j][k];
          else
            tempu=phys->SfHm[j][k];
          uface[j][k]=tempu;
          break;
        default:
          uface[j][k]=ui[nc][k];
          break;
      }
    }
  }

  // Faces on type 3 cells are always updated with first-order upwind
  for(iptr=grid->celldist[1];iptr<grid->celldist[2];iptr++) {
    i = grid->cellp[iptr];

    for(nf=0;nf<grid->nfaces[i];nf++) {
      if((ne=grid->neigh[i*grid->maxfaces+nf])!=-1) {
        j=grid->face[i*grid->maxfaces+nf];

        nc1 = grid->grad[2*j];
        nc2 = grid->grad[2*j+1];

        if(grid->ctop[nc1]>grid->ctop[nc2])
          kmin = grid->ctop[nc1];
        else
          kmin = grid->ctop[nc2];

        for(k=0;k<kmin;k++)
          uface[j][k]=0;
        for(k=kmin;k<grid->Nke[j];k++) {
          if(U[j][k]>0)
            nc=nc2;
          else
            nc=nc1;
          uface[j][k]=ui[nc][k];
        }
      }
    }
  }
}
