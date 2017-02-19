/*
 * File: uservertcoordinate.c
 * Author: Yun Zhang
 * Institution: Stanford University
 * --------------------------------
 * This file include a function to user defined vertical coordinate
 * 
 */

#include "suntans.h"
#include "grid.h"
#include "phys.h"
#include "initialization.h"
#include "boundaries.h"
#include "util.h"
#include "tvd.h"
#include "mympi.h"
#include "scalars.h"
#include "vertcoordinate.h"
#include "physio.h"
#include "subgrid.h"

/*
 * Function: UserDefinedVerticalCoordinate
 * User define vertical coordinate 
 * basically it is a user-defined function to calculate the layer thickness based on 
 * different criterion
 * ----------------------------------------------------
 * the original code has already include 1 z-level, 2 isopycnal, 3 sigma, 4 variational 
 */
void UserDefinedVerticalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
	// one for other update scheme
	
}

/*
 * Function: InitializeVerticalCoordinate
 * to setup the initial condition of dzz for user defined vertical coordinate
 * ----------------------------------------------------
 */
void InitializeVerticalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
	// one for other update scheme
	
}

/*
 * Function: InitializeIsopycnalCoordinate
 * User define isopycnal coordinate 
 * define the initial dzz for each cell under isopycnal coordinate
 * ----------------------------------------------------
 */
void InitializeIsopycnalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
  int i,k,Nkmax=grid->Nkmax;
  REAL ratio=1.0/Nkmax;
  for(i=0;i<grid->Nc;i++)
    for(k=0;k<grid->Nk[i];k++)
      grid->dzz[i][k]=ratio*(phys->h[i]+grid->dv[i]);
}

/*
 * Function: InitializeVariationalCoordinate
 * Initialize dzz for variational vertical coordinate
 * --------zz--------------------------------------------
 */
void InitializeVariationalCoordinate(gridT *grid, propT *prop, physT *phys,int myproc)
{
  int i,k;
  REAL ratio=1.0/grid->Nkmax;

  for(i=0;i<grid->Nc;i++)
  {
    for(k=grid->ctop[i];k<grid->Nk[i];k++)
    {
      grid->dzz[i][k]=ratio*(grid->dv[i]+phys->h[i]);
      grid->dzzold[i][k]=grid->dzz[i][k];
    }
  }
}

/*
 * Function: UserDefinedSigmaCoordinate
 * User define sigma coordinate 
 * basically to define the dsigma for each layer
 * ----------------------------------------------------
 */
void InitializeSigmaCoordinate(gridT *grid, propT *prop, physT *phys, int myproc)
{
  int i,k;
  for(k=0;k<grid->Nkmax;k++){

  	vert->dsigma[k]=1.0/grid->Nkmax;
  }

  for(i=0;i<grid->Nc;i++)
  {
  	for(k=grid->ctop[i];k<grid->Nk[i];k++)
  	{
  	  grid->dzz[i][k]=vert->dsigma[k]*(grid->dv[i]+phys->h[i]);
  	  grid->dzzold[i][k]=grid->dzz[i][k];
    }
  }
}

/*
 * Function: MonitorFunctionForVariationalMethod
 * calculate the value of monitor function for the variational approach
 * to update layer thickness when nonlinear==4
 * ----------------------------------------------------
 * Mii=sqrt(1-alphaM*(drhodz)^2)
 */
void MonitorFunctionForAverageMethod(gridT *grid, propT *prop, physT *phys, int myproc)
{
   int i,k;
   REAL alphaM=160,minM=0.15,max;
   // nonlinear=1 or 5 stable with alpham=320
   // nonlinear=2 stable with alpham=60
   // nonlinear=4 stable with alpham=60
 
   for(i=0;i<grid->Nc;i++)
   {
     max=0;
     vert->Msum[i]=0;
     for(k=grid->ctop[i]+1;k<grid->Nk[i]-1;k++){
       vert->M[i][k]=1000*(phys->rho[i][k-1]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k-1]+grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
       if(fabs(vert->M[i][k])>max)
         max=fabs(vert->M[i][k]);
     }
     
     // top boundary
     k=grid->ctop[i];
     vert->M[i][k]=1000*(phys->rho[i][k]-phys->rho[i][k+1])/(0.5*grid->dzz[i][k]+0.5*grid->dzz[i][k+1]);
     if(fabs(vert->M[i][k])>max)
       max=fabs(vert->M[i][k]);   
     // bottom boundary
     k=grid->Nk[i]-1;
     vert->M[i][k]=1000*(phys->rho[i][k-1]-phys->rho[i][k])/(0.5*grid->dzz[i][k-1]+0.5*grid->dzz[i][k]);
     if(fabs(vert->M[i][k])>max)
       max=fabs(vert->M[i][k]);   
     if(max<1)
       max=1;
     
     for(k=grid->ctop[i];k<grid->Nk[i];k++){ 
       vert->M[i][k]=1/sqrt(1+alphaM*vert->M[i][k]/max*vert->M[i][k]/max);
       if(vert->M[i][k]<minM)
         vert->M[i][k]=minM;     
       vert->Msum[i]+=vert->M[i][k];
     }
   }
}

/*
 * Function: MonitorFunctionForVariationalMethod
 * calculate the value of monitor function for the variational approach
 * to update layer thickness when nonlinear==4
 * solve the elliptic equation using iteration method
 * ----------------------------------------------------
 * Mii=sqrt(1-alphaM*(drhodz)^2)
 */
void MonitorFunctionForVariationalMethod(gridT *grid, propT *prop, physT *phys, int myproc)
{
  int i,k,j,nf,neigh,ne,kk;
  REAL alphaH=1, alphaV=160, minM=0.15,max,tmp;

  // clean values
  for(i=0;i<grid->Nc;i++)
  {
    for(k=0;k<grid->Nk[i]+1;k++)
      vert->Mw[i][k]=0;
    for(k=0;k<grid->Nk[i];k++)
      vert->M[i][k]=0;
  }

  for(i=0;i<grid->Nc;i++)
  {
    // calculate Monitor function value at cell face
    // to calculate A value
    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++)
      vert->Mw[i][k]=1000*(phys->rho[i][k-1]-phys->rho[i][k])/(0.5*grid->dzz[i][k]+0.5*grid->dzz[i][k-1]);      
    
    // top surface 
    k=grid->ctop[i];
    vert->Mw[i][k]=grid->Mw[i][k+1];

    // bottom surface
    k=grid->Nk[i];
    vert->Mw[i][k]=grid->Mw[i][k-1];

    for(k=grid->ctop[i];k<grid->Nk[i]+1;k++)
      vert->Mw[i][k]=sqrt(1+alphaV*vert->Mw[i][k]*vert->Mw[i][k]);

    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++)
      vert->Mw[i][k]/=0.5*(grid->dzzold[i][k-1]+grid->dzzold[i][k]);

    k=grid->ctop[i];
    vert->Mw[i][k]/=grid->dzzold[i][k];
    k=grid->Nk[i];
    vert->Mw[i][k]/=grid->dzzold[i][k-1];

    // Mw stores A_k to solve dz
    for(k=grid->ctop[i];k<grid->Nk[i]+1;k++)
      vert->Mw[i][k]=phys->Mw[i][grid->Nk[i]]/phys->Mw[i][k];

    // calculate the effects from horizontal gradient
    for(k=grid->ctop[i],k<grid->Nk[i];k++)
    {
      for(nf=0;nf<grid->nfaces[i];nf++)
      {
        tmp=0;
        neigh=grid->neigh[i*grid->maxfaces+nf];
        ne=grid->face[i*grid->maxfaces+nf];
        if(neigh!=-1){
          tmp=1000*(phys->rho[i][k]-phys->rho[neigh][k])/grid->dg[ne];
          vert->M[i][k]+=grid->dzzold[i][k]*sqrt(1+alphaH*tmp*tmp)*(vert->zc[i][k]-vert->zc[neigh][k])/
          grid->dg[ne]*grid->df[ne];
        }
      }  
    }

    // calculate B_k stores in M[i][k]
    for(k=grid->ctop[i];k<grid->Nk[i];k++)
      for(kk=k+1;k<grid->Nk[i];k++)
       vert->M[i][k]+=vert->M[i][kk];
    for(k=grid->ctop[i]+1;k<grid->Nk[i];k++)
      vert->M[i][k]=vert->M[i][k]/grid->Ac[i]/vert->Mw[i][k]*(grid->dzzold[i][k-1]+grid->dzzold[i][k])/2;
    k=grid->ctop[i];
    vert->M[i][k]=vert->M[i][k]/grid->Ac[i]/vert->Mw[i][k]*grid->dzzold[i][k];
  }
}