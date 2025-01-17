/**************************************************************************************************
*                                                                                                 *
* This file is part of HPIPM.                                                                     *
*                                                                                                 *
* HPIPM -- High-Performance Interior Point Method.                                                *
* Copyright (C) 2019 by Gianluca Frison.                                                          *
* Developed at IMTEK (University of Freiburg) under the supervision of Moritz Diehl.              *
* All rights reserved.                                                                            *
*                                                                                                 *
* The 2-Clause BSD License                                                                        *
*                                                                                                 *
* Redistribution and use in source and binary forms, with or without                              *
* modification, are permitted provided that the following conditions are met:                     *
*                                                                                                 *
* 1. Redistributions of source code must retain the above copyright notice, this                  *
*    list of conditions and the following disclaimer.                                             *
* 2. Redistributions in binary form must reproduce the above copyright notice,                    *
*    this list of conditions and the following disclaimer in the documentation                    *
*    and/or other materials provided with the distribution.                                       *
*                                                                                                 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND                 *
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED                   *
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE                          *
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR                 *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES                  *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;                    *
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND                     *
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT                      *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS                   *
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                    *
*                                                                                                 *
* Author: Gianluca Frison, gianluca.frison (at) imtek.uni-freiburg.de                             *
*                                                                                                 *
**************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <blasfeo_target.h>
#include <blasfeo_common.h>
#include <blasfeo_v_aux_ext_dep.h>
#include <blasfeo_d_aux_ext_dep.h>
#include <blasfeo_i_aux_ext_dep.h>
#include <blasfeo_d_aux.h>

#include <hpipm_d_ocp_qp_dim.h>
#include <hpipm_d_ocp_qp.h>
#include <hpipm_d_ocp_qp_sol.h>
#include <hpipm_d_ocp_qp_ipm.h>
#include <hpipm_d_ocp_qp_utils.h>
#include <hpipm_d_ocp_qp_red.h>
#include <hpipm_timing.h>

#include "d_tools.h"



#define KEEP_X0 0

// printing
#ifndef PRINT
#define PRINT 1
#endif



#if ! defined(EXT_DEP)
/* creates a zero matrix */
void d_zeros(double **pA, int row, int col)
	{
	*pA = malloc((row*col)*sizeof(double));
	double *A = *pA;
	int i;
	for(i=0; i<row*col; i++) A[i] = 0.0;
	}
/* frees matrix */
void d_free(double *pA)
	{
	free( pA );
	}
/* prints a matrix in column-major format */
void d_print_mat(int m, int n, double *A, int lda)
	{
	int i, j;
	for(i=0; i<m; i++)
		{
		for(j=0; j<n; j++)
			{
			printf("%9.5f ", A[i+lda*j]);
			}
		printf("\n");
		}
	printf("\n");
	}
/* prints the transposed of a matrix in column-major format */
void d_print_tran_mat(int row, int col, double *A, int lda)
	{
	int i, j;
	for(j=0; j<col; j++)
		{
		for(i=0; i<row; i++)
			{
			printf("%9.5f ", A[i+lda*j]);
			}
		printf("\n");
		}
	printf("\n");
	}
/* prints a matrix in column-major format (exponential notation) */
void d_print_exp_mat(int m, int n, double *A, int lda)
	{
	int i, j;
	for(i=0; i<m; i++)
		{
		for(j=0; j<n; j++)
			{
			printf("%e\t", A[i+lda*j]);
			}
		printf("\n");
		}
	printf("\n");
	}
/* prints the transposed of a matrix in column-major format (exponential notation) */
void d_print_exp_tran_mat(int row, int col, double *A, int lda)
	{
	int i, j;
	for(j=0; j<col; j++)
		{
		for(i=0; i<row; i++)
			{
			printf("%e\t", A[i+lda*j]);
			}
		printf("\n");
		}
	printf("\n");
	}
/* creates a zero matrix aligned */
void int_zeros(int **pA, int row, int col)
	{
	void *temp = malloc((row*col)*sizeof(int));
	*pA = temp;
	int *A = *pA;
	int i;
	for(i=0; i<row*col; i++) A[i] = 0;
	}
/* frees matrix */
void int_free(int *pA)
	{
	free( pA );
	}
/* prints a matrix in column-major format */
void int_print_mat(int row, int col, int *A, int lda)
	{
	int i, j;
	for(i=0; i<row; i++)
		{
		for(j=0; j<col; j++)
			{
			printf("%d ", A[i+lda*j]);
			}
		printf("\n");
		}
	printf("\n");
	}
#endif



/************************************************
Mass-spring system: nx/2 masses connected each other with springs (in a row), and the first and the last one to walls. nu (<=nx) controls act on the first nu masses. The system is sampled with sampling time Ts.
************************************************/
void mass_spring_system(double Ts, int nx, int nu, double *A, double *B, double *b, double *x0)
	{

	int nx2 = nx*nx;

	int info = 0;

	int pp = nx/2; // number of masses

/************************************************
* build the continuous time system
************************************************/

	double *T; d_zeros(&T, pp, pp);
	int ii;
	for(ii=0; ii<pp; ii++) T[ii*(pp+1)] = -2;
	for(ii=0; ii<pp-1; ii++) T[ii*(pp+1)+1] = 1;
	for(ii=1; ii<pp; ii++) T[ii*(pp+1)-1] = 1;

	double *Z; d_zeros(&Z, pp, pp);
	double *I; d_zeros(&I, pp, pp); for(ii=0; ii<pp; ii++) I[ii*(pp+1)]=1.0; // = eye(pp);
	double *Ac; d_zeros(&Ac, nx, nx);
	dmcopy(pp, pp, Z, pp, Ac, nx);
	dmcopy(pp, pp, T, pp, Ac+pp, nx);
	dmcopy(pp, pp, I, pp, Ac+pp*nx, nx);
	dmcopy(pp, pp, Z, pp, Ac+pp*(nx+1), nx);
	free(T);
	free(Z);
	free(I);

	d_zeros(&I, nu, nu); for(ii=0; ii<nu; ii++) I[ii*(nu+1)]=1.0; //I = eye(nu);
	double *Bc; d_zeros(&Bc, nx, nu);
	dmcopy(nu, nu, I, nu, Bc+pp, nx);
	free(I);

/************************************************
* compute the discrete time system
************************************************/

	double *bb; d_zeros(&bb, nx, 1);
	dmcopy(nx, 1, bb, nx, b, nx);

	dmcopy(nx, nx, Ac, nx, A, nx);
	dscal_3l(nx2, Ts, A);
	expm(nx, A);

	d_zeros(&T, nx, nx);
	d_zeros(&I, nx, nx); for(ii=0; ii<nx; ii++) I[ii*(nx+1)]=1.0; //I = eye(nx);
	dmcopy(nx, nx, A, nx, T, nx);
	daxpy_3l(nx2, -1.0, I, T);
	dgemm_nn_3l(nx, nu, nx, T, nx, Bc, nx, B, nx);
	free(T);
	free(I);

	int *ipiv = (int *) malloc(nx*sizeof(int));
	dgesv_3l(nx, nu, Ac, nx, ipiv, B, nx, &info);
	free(ipiv);

	free(Ac);
	free(Bc);
	free(bb);


/************************************************
* initial state
************************************************/

	if(nx==4)
		{
		x0[0] = 5;
		x0[1] = 10;
		x0[2] = 15;
		x0[3] = 20;
		}
	else
		{
		int jj;
		for(jj=0; jj<nx; jj++)
			x0[jj] = 1;
		}

	}



int main()
	{

	int rep, nrep=1000;

	hpipm_timer timer0;


	// local variables

	int ii, jj;



	// problem size

	int nx_ = 8; // number of states (it has to be even for the mass-spring system test problem)
	int nu_ = 3; // number of inputs (controllers) (it has to be at least 1 and at most nx/2 for the mass-spring system test problem)
	int N  = 8; // horizon lenght



	// stage-wise variant size

	int *nx = (int *) malloc((N+1)*sizeof(int));
	nx[0] = nx_;
	for(ii=1; ii<=N; ii++)
		nx[ii] = nx_;

	int *nu = (int *) malloc((N+1)*sizeof(int));
	for(ii=0; ii<N; ii++)
		nu[ii] = nu_;
	nu[N] = 0;

	int *nbu = (int *) malloc((N+1)*sizeof(int));
	for (ii=0; ii<N; ii++)
		nbu[ii] = nu[ii];
	nbu[N] = 0;
#if 1
	int *nbx = (int *) malloc((N+1)*sizeof(int));
	nbx[0] = nx[0];
	for(ii=1; ii<=N; ii++)
		nbx[ii] = nx[ii]/2;

	int *nb = (int *) malloc((N+1)*sizeof(int));
	for (ii=0; ii<=N; ii++)
		nb[ii] = nbu[ii]+nbx[ii];

	int *ng = (int *) malloc((N+1)*sizeof(int));
	ng[0] = 0;
	for(ii=1; ii<=N; ii++)
		ng[ii] = 0;

	int *nsbx = (int *) malloc((N+1)*sizeof(int));
	nsbx[0] = 0;
	for(ii=1; ii<=N; ii++)
		nsbx[ii] = nbx[ii];

	int *nsbu = (int *) malloc((N+1)*sizeof(int));
	for(ii=0; ii<=N; ii++)
		nsbu[ii] = 0;

	int *nsg = (int *) malloc((N+1)*sizeof(int));
	for(ii=0; ii<=N; ii++)
		nsg[ii] = 0;

	int *ns = (int *) malloc((N+1)*sizeof(int));
	for(ii=0; ii<=N; ii++)
		ns[ii] = nsbx[ii] + nsbu[ii] + nsg[ii];

	int *nbxe = (int *) malloc((N+1)*sizeof(int));
	nbxe[0] = nx_;
	for(ii=1; ii<=N; ii++)
		nbxe[ii] = 0;

#elif 0
	int nb[N+1];
	nb[0] = 0;
	for(ii=1; ii<N; ii++)
		nb[ii] = 0;
	nb[N] = 0;

	int ng[N+1];
#if KEEP_X0
	ng[0] = nu[0]+nx[0]/2;
#else
	ng[0] = nu[0];
#endif
	for(ii=1; ii<N; ii++)
		ng[ii] = nu[1]+nx[1]/2;
	ng[N] = nx[N]/2;
#else
	int nb[N+1];
	nb[0] = nu[0] + nx[0]/2;
	for(ii=1; ii<N; ii++)
		nb[ii] = nu[ii] + nx[ii]/2;
	nb[N] = nu[N] + nx[N]/2;

	int ng[N+1];
#if KEEP_X0
	ng[0] = nx[0]/2;
#else
	ng[0] = 0;
#endif
	for(ii=1; ii<N; ii++)
		ng[ii] = nx[1]/2;
	ng[N] = nx[N]/2;
#endif

/************************************************
* dynamical system
************************************************/

	double *A; d_zeros(&A, nx_, nx_); // states update matrix

	double *B; d_zeros(&B, nx_, nu_); // inputs matrix

	double *b; d_zeros(&b, nx_, 1); // states offset
	double *x0; d_zeros(&x0, nx_, 1); // initial state

	double Ts = 0.5; // sampling time
	mass_spring_system(Ts, nx_, nu_, A, B, b, x0);

	for(jj=0; jj<nx_; jj++)
		b[jj] = 0.0;

	for(jj=0; jj<nx_; jj++)
		x0[jj] = 0;
	x0[0] = 2.5;
	x0[1] = 2.5;

#if PRINT
	d_print_mat(nx_, nx_, A, nx_);
	d_print_mat(nx_, nu_, B, nu_);
	d_print_mat(1, nx_, b, 1);
	d_print_mat(1, nx_, x0, 1);
#endif

/************************************************
* cost function
************************************************/

	double *Q; d_zeros(&Q, nx_, nx_);
	for(ii=0; ii<nx_; ii++) Q[ii*(nx_+1)] = 0.0;

	double *R; d_zeros(&R, nu_, nu_);
	for(ii=0; ii<nu_; ii++) R[ii*(nu_+1)] = 2.0;

	double *S; d_zeros(&S, nu_, nx_);

	double *q; d_zeros(&q, nx_, 1);
	for(ii=0; ii<nx_; ii++) q[ii] = 0.0;

	double *r; d_zeros(&r, nu_, 1);
	for(ii=0; ii<nu_; ii++) r[ii] = 0.0;

#if 0
	double *QN; d_zeros(&QN, nx_, nx_);
	for(ii=0; ii<2; ii++) QN[ii*(nx_+1)] = 1e15;
	for(ii=0; ii<nx_; ii++) QN[ii*(nx_+1)] += Q[ii*(nx_+1)];
	double *qN; d_zeros(&qN, nx_, 1);
	qN[0] = - 0.1;
	qN[1] = - 0.1;
	for(ii=0; ii<2; ii++) qN[ii] *= 1e15;
	for(ii=0; ii<nx_; ii++) qN[ii] += q[ii];
#endif

#if PRINT
	d_print_mat(nx_, nx_, Q, nx_);
	d_print_mat(nu_, nu_, R, nu_);
	d_print_mat(nu_, nx_, S, nu_);
	d_print_mat(1, nx_, q, 1);
	d_print_mat(1, nu_, r, 1);
//	d_print_mat(nx_, nx_, QN, nx_);
//	d_print_mat(1, nx_, qN, 1);
#endif

	// maximum element in cost functions
	double mu0;
	if(ns[1]>0 | ns[N]>0)
		mu0 = 1000.0;
	else
		mu0 = 2.0;

/************************************************
* box & general constraints
************************************************/

	int *idxbx0; int_zeros(&idxbx0, nbx[0], 1);
	double *d_lbx0; d_zeros(&d_lbx0, nbx[0], 1);
	double *d_ubx0; d_zeros(&d_ubx0, nbx[0], 1);
	int *idxbu0; int_zeros(&idxbu0, nbu[0], 1);
	double *d_lbu0; d_zeros(&d_lbu0, nbu[0], 1);
	double *d_ubu0; d_zeros(&d_ubu0, nbu[0], 1);
	double *d_lg0; d_zeros(&d_lg0, ng[0], 1);
	double *d_ug0; d_zeros(&d_ug0, ng[0], 1);
	for(ii=0; ii<nbu[0]; ii++)
		{
		d_lbu0[ii] = - 0.5; // umin
		d_ubu0[ii] =   0.5; // umax
		idxbu0[ii] = ii;
		}
	for(ii=0; ii<nbx[0]; ii++)
		{
		d_lbx0[ii] = x0[ii]; //- 4.0; // xmin
		d_ubx0[ii] = x0[ii]; //  4.0; // xmax
		idxbx0[ii] = ii;
		}
	for(ii=0; ii<ng[0]; ii++)
		{
		if(ii<nu[0]-nb[0]) // input
			{
			d_lg0[ii] = - 0.5; // umin
			d_ug0[ii] =   0.5; // umax
			}
		else // state
			{
			d_lg0[ii] = - 4.0; // xmin
			d_ug0[ii] =   4.0; // xmax
			}
		}

	int *idxbx1; int_zeros(&idxbx1, nbx[1], 1);
	double *d_lbx1; d_zeros(&d_lbx1, nbx[1], 1);
	double *d_ubx1; d_zeros(&d_ubx1, nbx[1], 1);
	int *idxbu1; int_zeros(&idxbu1, nbu[1], 1);
	double *d_lbu1; d_zeros(&d_lbu1, nbu[1], 1);
	double *d_ubu1; d_zeros(&d_ubu1, nbu[1], 1);
	double *d_lg1; d_zeros(&d_lg1, ng[1], 1);
	double *d_ug1; d_zeros(&d_ug1, ng[1], 1);
	for(ii=0; ii<nbu[1]; ii++)
		{
		d_lbu1[ii] = - 0.5; // umin
		d_ubu1[ii] =   0.5; // umax
		idxbu1[ii] = ii;
		}
	for(ii=0; ii<nbx[1]; ii++)
		{
		d_lbx1[ii] = - 1.0; // xmin
		d_ubx1[ii] =   1.0; // xmax
		idxbx1[ii] = ii;
		}
	for(ii=0; ii<ng[1]; ii++)
		{
		if(ii<nu[1]-nb[1]) // input
			{
			d_lg1[ii] = - 0.5; // umin
			d_ug1[ii] =   0.5; // umax
			}
		else // state
			{
			d_lg1[ii] = - 4.0; // xmin
			d_ug1[ii] =   4.0; // xmax
			}
		}


	int *idxbxN; int_zeros(&idxbxN, nbx[N], 1);
	double *d_lbxN; d_zeros(&d_lbxN, nbx[N], 1);
	double *d_ubxN; d_zeros(&d_ubxN, nbx[N], 1);
	double *d_lgN; d_zeros(&d_lgN, ng[N], 1);
	double *d_ugN; d_zeros(&d_ugN, ng[N], 1);
	for(ii=0; ii<nbx[N]; ii++)
		{
		d_lbxN[ii] = - 1.0; // xmin
		d_ubxN[ii] =   1.0; // xmax
		idxbxN[ii] = ii;
		}
	for(ii=0; ii<ng[N]; ii++)
		{
		d_lgN[ii] = - 4.0; // dmin
		d_ugN[ii] =   4.0; // dmax
		}

	double *C0; d_zeros(&C0, ng[0], nx[0]);
	double *D0; d_zeros(&D0, ng[0], nu[0]);
	for(ii=0; ii<nu[0]-nb[0] & ii<ng[0]; ii++)
		D0[ii+(nb[0]+ii)*ng[0]] = 1.0;
	for(; ii<ng[0]; ii++)
		C0[ii+(nb[0]+ii-nu[0])*ng[0]] = 1.0;

	double *C1; d_zeros(&C1, ng[1], nx[1]);
	double *D1; d_zeros(&D1, ng[1], nu[1]);
	for(ii=0; ii<nu[1]-nb[1] & ii<ng[1]; ii++)
		D1[ii+(nb[1]+ii)*ng[1]] = 1.0;
	for(; ii<ng[1]; ii++)
		C1[ii+(nb[1]+ii-nu[1])*ng[1]] = 1.0;

	double *CN; d_zeros(&CN, ng[N], nx[N]);
	double *DN; d_zeros(&DN, ng[N], nu[N]);
	for(ii=0; ii<nu[N]-nb[N] & ii<ng[N]; ii++)
		DN[ii+(nb[N]+ii)*ng[N]] = 1.0;
	for(; ii<ng[N]; ii++)
		CN[ii+(nb[N]+ii-nu[N])*ng[N]] = 1.0;

#if PRINT
	// box constraints
	int_print_mat(1, nbx[0], idxbx0, 1);
	d_print_mat(1, nbx[0], d_lbx0, 1);
	d_print_mat(1, nbx[0], d_ubx0, 1);
	int_print_mat(1, nbu[0], idxbu0, 1);
	d_print_mat(1, nbu[0], d_lbu0, 1);
	d_print_mat(1, nbu[0], d_ubu0, 1);
	int_print_mat(1, nbx[1], idxbx1, 1);
	d_print_mat(1, nbx[1], d_lbx1, 1);
	d_print_mat(1, nbx[1], d_ubx1, 1);
	int_print_mat(1, nbu[1], idxbu1, 1);
	d_print_mat(1, nbu[1], d_lbu1, 1);
	d_print_mat(1, nbu[1], d_ubu1, 1);
	int_print_mat(1, nbx[N], idxbxN, 1);
	d_print_mat(1, nbx[N], d_lbxN, 1);
	d_print_mat(1, nbx[N], d_ubxN, 1);
	// general constraints
	d_print_mat(1, ng[0], d_lg0, 1);
	d_print_mat(1, ng[0], d_ug0, 1);
	d_print_mat(ng[0], nu[0], D0, ng[0]);
	d_print_mat(ng[0], nx[0], C0, ng[0]);
	d_print_mat(1, ng[1], d_lg1, 1);
	d_print_mat(1, ng[1], d_ug1, 1);
	d_print_mat(ng[1], nu[1], D1, ng[1]);
	d_print_mat(ng[1], nx[1], C1, ng[1]);
	d_print_mat(1, ng[N], d_lgN, 1);
	d_print_mat(1, ng[N], d_ugN, 1);
	d_print_mat(ng[N], nu[N], DN, ng[N]);
	d_print_mat(ng[N], nx[N], CN, ng[N]);
#endif

/************************************************
* soft constraints
************************************************/

	double *Zl0; d_zeros(&Zl0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		Zl0[ii] = 0e3;
	double *Zu0; d_zeros(&Zu0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		Zu0[ii] = 0e3;
	double *zl0; d_zeros(&zl0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		zl0[ii] = 1e2;
	double *zu0; d_zeros(&zu0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		zu0[ii] = 1e2;
	int *idxs0; int_zeros(&idxs0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		idxs0[ii] = nu[0]+ii;
	double *d_ls0; d_zeros(&d_ls0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		d_ls0[ii] = 0.0; //-1.0;
	double *d_us0; d_zeros(&d_us0, ns[0], 1);
	for(ii=0; ii<ns[0]; ii++)
		d_us0[ii] = 0.0;

	double *Zl1; d_zeros(&Zl1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		Zl1[ii] = 0e3;
	double *Zu1; d_zeros(&Zu1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		Zu1[ii] = 0e3;
	double *zl1; d_zeros(&zl1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		zl1[ii] = 1e2;
	double *zu1; d_zeros(&zu1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		zu1[ii] = 1e2;
	int *idxs1; int_zeros(&idxs1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		idxs1[ii] = nu[1]+ii;
	double *d_ls1; d_zeros(&d_ls1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		d_ls1[ii] = 0.0; //-1.0;
	double *d_us1; d_zeros(&d_us1, ns[1], 1);
	for(ii=0; ii<ns[1]; ii++)
		d_us1[ii] = 0.0;

	double *ZlN; d_zeros(&ZlN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		ZlN[ii] = 0e2;
	double *ZuN; d_zeros(&ZuN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		ZuN[ii] = 0e2;
	double *zlN; d_zeros(&zlN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		zlN[ii] = 1e2;
	double *zuN; d_zeros(&zuN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		zuN[ii] = 1e2;
	int *idxsN; int_zeros(&idxsN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		idxsN[ii] = nu[N]+ii;
	double *d_lsN; d_zeros(&d_lsN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		d_lsN[ii] = 0.0; //-1.0;
	double *d_usN; d_zeros(&d_usN, ns[N], 1);
	for(ii=0; ii<ns[N]; ii++)
		d_usN[ii] = 0.0;

#if PRINT
	// soft constraints
	int_print_mat(1, ns[0], idxs0, 1);
	d_print_mat(1, ns[0], Zl0, 1);
	d_print_mat(1, ns[0], Zu0, 1);
	d_print_mat(1, ns[0], zl0, 1);
	d_print_mat(1, ns[0], zu0, 1);
	d_print_mat(1, ns[0], d_ls0, 1);
	d_print_mat(1, ns[0], d_us0, 1);
	int_print_mat(1, ns[1], idxs1, 1);
	d_print_mat(1, ns[1], Zl1, 1);
	d_print_mat(1, ns[1], Zu1, 1);
	d_print_mat(1, ns[1], zl1, 1);
	d_print_mat(1, ns[1], zu1, 1);
	d_print_mat(1, ns[1], d_ls1, 1);
	d_print_mat(1, ns[1], d_us1, 1);
	int_print_mat(1, ns[N], idxsN, 1);
	d_print_mat(1, ns[N], ZlN, 1);
	d_print_mat(1, ns[N], ZuN, 1);
	d_print_mat(1, ns[N], zlN, 1);
	d_print_mat(1, ns[N], zuN, 1);
	d_print_mat(1, ns[N], d_lsN, 1);
	d_print_mat(1, ns[N], d_usN, 1);
#endif

/************************************************
* array of matrices
************************************************/

	double **hA = (double **) malloc((N)*sizeof(double *));
	double **hB = (double **) malloc((N)*sizeof(double *));
	double **hb = (double **) malloc((N)*sizeof(double *));
	double **hQ = (double **) malloc((N+1)*sizeof(double *));
	double **hS = (double **) malloc((N+1)*sizeof(double *));
	double **hR = (double **) malloc((N+1)*sizeof(double *));
	double **hq = (double **) malloc((N+1)*sizeof(double *));
	double **hr = (double **) malloc((N+1)*sizeof(double *));
	int **hidxbx = (int **) malloc((N+1)*sizeof(int *));
	double **hd_lbx = (double **) malloc((N+1)*sizeof(double *));
	double **hd_ubx = (double **) malloc((N+1)*sizeof(double *));
	int **hidxbu = (int **) malloc((N+1)*sizeof(int *));
	double **hd_lbu = (double **) malloc((N+1)*sizeof(double *));
	double **hd_ubu = (double **) malloc((N+1)*sizeof(double *));
	double **hC = (double **) malloc((N+1)*sizeof(double *));
	double **hD = (double **) malloc((N+1)*sizeof(double *));
	double **hd_lg = (double **) malloc((N+1)*sizeof(double *));
	double **hd_ug = (double **) malloc((N+1)*sizeof(double *));
	double **hZl = (double **) malloc((N+1)*sizeof(double *));
	double **hZu = (double **) malloc((N+1)*sizeof(double *));
	double **hzl = (double **) malloc((N+1)*sizeof(double *));
	double **hzu = (double **) malloc((N+1)*sizeof(double *));
	int **hidxs = (int **) malloc((N+1)*sizeof(int *));
	double **hd_ls = (double **) malloc((N+1)*sizeof(double *));
	double **hd_us = (double **) malloc((N+1)*sizeof(double *));

	hA[0] = A;
	hB[0] = B;
	hb[0] = b;
	hQ[0] = Q;
	hS[0] = S;
	hR[0] = R;
	hq[0] = q;
	hr[0] = r;
	hidxbx[0] = idxbx0;
	hd_lbx[0] = d_lbx0;
	hd_ubx[0] = d_ubx0;
	hidxbu[0] = idxbu0;
	hd_lbu[0] = d_lbu0;
	hd_ubu[0] = d_ubu0;
	hC[0] = C0;
	hD[0] = D0;
	hd_lg[0] = d_lg0;
	hd_ug[0] = d_ug0;
	hZl[0] = Zl0;
	hZu[0] = Zu0;
	hzl[0] = zl0;
	hzu[0] = zu0;
	hidxs[0] = idxs0;
	hd_ls[0] = d_ls0;
	hd_us[0] = d_us0;
	for(ii=1; ii<N; ii++)
		{
		hA[ii] = A;
		hB[ii] = B;
		hb[ii] = b;
		hQ[ii] = Q;
		hS[ii] = S;
		hR[ii] = R;
		hq[ii] = q;
		hr[ii] = r;
		hidxbx[ii] = idxbx1;
		hd_lbx[ii] = d_lbx1;
		hd_ubx[ii] = d_ubx1;
		hidxbu[ii] = idxbu1;
		hd_lbu[ii] = d_lbu1;
		hd_ubu[ii] = d_ubu1;
		hd_lg[ii] = d_lg1;
		hd_ug[ii] = d_ug1;
		hC[ii] = C1;
		hD[ii] = D1;
		hZl[ii] = Zl1;
		hZu[ii] = Zu1;
		hzl[ii] = zl1;
		hzu[ii] = zu1;
		hidxs[ii] = idxs1;
		hd_ls[ii] = d_ls1;
		hd_us[ii] = d_us1;
		}
	hQ[N] = Q;
	hS[N] = S;
	hR[N] = R;
	hq[N] = q;
	hr[N] = r;
	hidxbx[N] = idxbxN;
	hd_lbx[N] = d_lbxN;
	hd_ubx[N] = d_ubxN;
	hd_lg[N] = d_lgN;
	hd_ug[N] = d_ugN;
	hC[N] = CN;
	hD[N] = DN;
	hZl[N] = ZlN;
	hZu[N] = ZuN;
	hzl[N] = zlN;
	hzu[N] = zuN;
	hidxs[N] = idxsN;
	hd_ls[N] = d_lsN;
	hd_us[N] = d_usN;

/************************************************
* ocp qp dim
************************************************/

	hpipm_size_t dim_size = d_ocp_qp_dim_memsize(N);
#if PRINT
	printf("\ndim size = %ld\n", dim_size);
#endif
	void *dim_mem = malloc(dim_size);

	struct d_ocp_qp_dim dim;
	d_ocp_qp_dim_create(N, &dim, dim_mem);
	d_ocp_qp_dim_set_all(nx, nu, nbx, nbu, ng, nsbx, nsbu, nsg, &dim);

	for(ii=0; ii<=N; ii++)
		d_ocp_qp_dim_set_nbxe(ii, nbxe[ii], &dim);

#if PRINT
	d_ocp_qp_dim_print(&dim);
#endif

/************************************************
* ocp qp
************************************************/

	hpipm_size_t qp_size = d_ocp_qp_memsize(&dim);
#if PRINT
	printf("\nqp size = %ld\n", qp_size);
#endif
	void *qp_mem = malloc(qp_size);

	struct d_ocp_qp qp;
	d_ocp_qp_create(&dim, &qp, qp_mem);
	d_ocp_qp_set_all(hA, hB, hb, hQ, hS, hR, hq, hr, hidxbx, hd_lbx, hd_ubx, hidxbu, hd_lbu, hd_ubu, hC, hD, hd_lg, hd_ug, hZl, hZu, hzl, hzu, hidxs, hd_ls, hd_us, &qp);

	// dynamic constraints removal
	double *d_lbu_mask; d_zeros(&d_lbu_mask, nbu[0], 1);
	double *d_ubu_mask; d_zeros(&d_ubu_mask, nbu[0], 1);
	double *d_lbx_mask; d_zeros(&d_lbx_mask, nbx[N], 1);
	double *d_ubx_mask; d_zeros(&d_ubx_mask, nbx[N], 1);
//	d_ocp_qp_set("lbu_mask", 0, d_lbu_mask, &qp);
//	d_ocp_qp_set("ubu_mask", 0, d_ubu_mask, &qp);
//	d_ocp_qp_set("lbx_mask", N, d_lbx_mask, &qp);
//	d_ocp_qp_set("ubx_mask", N, d_ubx_mask, &qp);
	
	int *idxbxe0 = (int *) malloc(nx_*sizeof(int));
	for(ii=0; ii<=nx_; ii++)
		idxbxe0[ii] = ii;
	
	d_ocp_qp_set_idxbxe(0, idxbxe0, &qp);

#if PRINT
	d_ocp_qp_print(qp.dim, &qp);
//	exit(1);
#endif

#if 0
	printf("\nN = %d\n", qp.dim->N);
	for(ii=0; ii<N; ii++)
		d_print_strmat(qp.dim->nu[ii]+qp.dim->nx[ii]+1, qp.dim->nx[ii+1], qp.BAbt+ii, 0, 0);
	for(ii=0; ii<N; ii++)
		blasfeo_print_tran_dvec(qp.nx[ii+1], qp.b+ii, 0);
	for(ii=0; ii<=N; ii++)
		d_print_strmat(qp.nu[ii]+qp.nx[ii]+1, qp.nu[ii]+qp.nx[ii], qp.RSQrq+ii, 0, 0);
	for(ii=0; ii<=N; ii++)
		blasfeo_print_tran_dvec(qp.nu[ii]+qp.nx[ii], qp.rq+ii, 0);
	for(ii=0; ii<=N; ii++)
		int_print_mat(1, nb[ii], qp.idxb[ii], 1);
	for(ii=0; ii<=N; ii++)
		blasfeo_print_tran_dvec(qp.nb[ii], qp.d_lb+ii, 0);
	for(ii=0; ii<=N; ii++)
		blasfeo_print_tran_dvec(qp.nb[ii], qp.d_ub+ii, 0);
	for(ii=0; ii<=N; ii++)
		d_print_strmat(qp.nu[ii]+qp.nx[ii], qp.ng[ii], qp.DCt+ii, 0, 0);
	for(ii=0; ii<=N; ii++)
		blasfeo_print_tran_dvec(qp.ng[ii], qp.d_lg+ii, 0);
	for(ii=0; ii<=N; ii++)
		blasfeo_print_tran_dvec(qp.ng[ii], qp.d_ug+ii, 0);
	return;
#endif

/************************************************
* ocp qp reduce equation dof
************************************************/

#if KEEP_X0

	struct d_ocp_qp_dim dim2 = dim;
	struct d_ocp_qp qp2 = qp;

	double time_red_eq_dof = 0.0;

#else // keep x0

	hpipm_size_t dim_size2 = d_ocp_qp_dim_memsize(N);
#if PRINT
	printf("\ndim size = %ld\n", dim_size2);
#endif
	void *dim_mem2 = malloc(dim_size2);

	struct d_ocp_qp_dim dim2;
	d_ocp_qp_dim_create(N, &dim2, dim_mem2);

	d_ocp_qp_dim_reduce_eq_dof(&dim, &dim2);

#if PRINT
	d_ocp_qp_dim_print(&dim2);
#endif


	hpipm_size_t qp_size2 = d_ocp_qp_memsize(&dim2);
#if PRINT
	printf("\nqp size = %ld\n", qp_size2);
#endif
	void *qp_mem2 = malloc(qp_size2);

	struct d_ocp_qp qp2;
	d_ocp_qp_create(&dim2, &qp2, qp_mem2);

//	d_ocp_qp_print(qp2.dim, &qp2);


	hpipm_size_t qp_red_arg_size = d_ocp_qp_reduce_eq_dof_arg_memsize();
#if PRINT
	printf("\nqp red arg size = %ld\n", qp_red_arg_size);
#endif
	void *qp_red_arg_mem = malloc(qp_red_arg_size);

	struct d_ocp_qp_reduce_eq_dof_arg qp_red_arg;
	d_ocp_qp_reduce_eq_dof_arg_create(&qp_red_arg, qp_red_arg_mem);

	d_ocp_qp_reduce_eq_dof_arg_set_default(&qp_red_arg);
	d_ocp_qp_reduce_eq_dof_arg_set_alias_unchanged(&qp_red_arg, 1);
	d_ocp_qp_reduce_eq_dof_arg_set_comp_dual_sol_eq(&qp_red_arg, 1);
	d_ocp_qp_reduce_eq_dof_arg_set_comp_dual_sol_ineq(&qp_red_arg, 1);


	hpipm_size_t qp_red_work_size = d_ocp_qp_reduce_eq_dof_ws_memsize(&dim);
#if PRINT
	printf("\nqp red work size = %ld\n", qp_red_work_size);
#endif
	void *qp_red_work_mem = malloc(qp_red_work_size);

	struct d_ocp_qp_reduce_eq_dof_ws qp_red_work;
	d_ocp_qp_reduce_eq_dof_ws_create(&dim, &qp_red_work, qp_red_work_mem);


	hpipm_tic(&timer0);

	for(rep=0; rep<nrep; rep++)
		{
		d_ocp_qp_reduce_eq_dof(&qp, &qp2, &qp_red_arg, &qp_red_work);
		}

	double time_red_eq_dof = hpipm_toc(&timer0) / nrep;

#if PRINT
	d_ocp_qp_print(qp2.dim, &qp2);
#endif

#endif // keep x0

/************************************************
* ocp qp sol
************************************************/

	hpipm_size_t qp_sol_size = d_ocp_qp_sol_memsize(&dim);
#if PRINT
	printf("\nqp sol size = %ld\n", qp_sol_size);
#endif
	void *qp_sol_mem = malloc(qp_sol_size);

	struct d_ocp_qp_sol qp_sol;
	d_ocp_qp_sol_create(&dim, &qp_sol, qp_sol_mem);

#if KEEP_X0
	
	struct d_ocp_qp_sol qp_sol2 = qp_sol;

#else // keep x0

	hpipm_size_t qp_sol_size2 = d_ocp_qp_sol_memsize(&dim2);
#if PRINT
	printf("\nqp sol size = %ld\n", qp_sol_size2);
#endif
	void *qp_sol_mem2 = malloc(qp_sol_size2);

	struct d_ocp_qp_sol qp_sol2;
	d_ocp_qp_sol_create(&dim2, &qp_sol2, qp_sol_mem2);

#endif // keep x0

/************************************************
* ipm arg
************************************************/

	hpipm_size_t ipm_arg_size = d_ocp_qp_ipm_arg_memsize(&dim2);
	void *ipm_arg_mem = malloc(ipm_arg_size);

	struct d_ocp_qp_ipm_arg arg;
	d_ocp_qp_ipm_arg_create(&dim2, &arg, ipm_arg_mem);

//	enum hpipm_mode mode = SPEED_ABS;
	enum hpipm_mode mode = SPEED;
//	enum hpipm_mode mode = BALANCE;
//	enum hpipm_mode mode = ROBUST;
	d_ocp_qp_ipm_arg_set_default(mode, &arg);

	mu0 = 1e2;
	int iter_max = 30;
	double alpha_min = 1e-8;
	double tol_stat = 1e-6;
	double tol_eq = 1e-8;
	double tol_ineq = 1e-8;
	double tol_comp = 1e-8;
	double reg_prim = 1e-12;
	int warm_start = 0;
	int pred_corr = 1;
	int ric_alg = 0;
	int comp_res_exit = 1;
//	double tau_min = 1e-12;

	d_ocp_qp_ipm_arg_set_mu0(&mu0, &arg);
	d_ocp_qp_ipm_arg_set_iter_max(&iter_max, &arg);
	d_ocp_qp_ipm_arg_set_alpha_min(&alpha_min, &arg);
	d_ocp_qp_ipm_arg_set_tol_stat(&tol_stat, &arg);
	d_ocp_qp_ipm_arg_set_tol_eq(&tol_eq, &arg);
	d_ocp_qp_ipm_arg_set_tol_ineq(&tol_ineq, &arg);
	d_ocp_qp_ipm_arg_set_tol_comp(&tol_comp, &arg);
	d_ocp_qp_ipm_arg_set_reg_prim(&reg_prim, &arg);
	d_ocp_qp_ipm_arg_set_warm_start(&warm_start, &arg);
	d_ocp_qp_ipm_arg_set_pred_corr(&pred_corr, &arg);
	d_ocp_qp_ipm_arg_set_ric_alg(&ric_alg, &arg);
	d_ocp_qp_ipm_arg_set_comp_res_exit(&comp_res_exit, &arg);
//	d_ocp_qp_ipm_arg_set_tau_min(&tau_min, &arg);

#if PRINT
	d_ocp_qp_ipm_arg_print(&dim2, &arg);
#endif

/************************************************
* ipm
************************************************/

	hpipm_size_t ipm_size = d_ocp_qp_ipm_ws_memsize(&dim2, &arg);
#if PRINT
	printf("\nipm size = %ld\n", ipm_size);
#endif
	void *ipm_mem = malloc(ipm_size);

	struct d_ocp_qp_ipm_ws workspace;
	d_ocp_qp_ipm_ws_create(&dim2, &arg, &workspace, ipm_mem);

	int hpipm_status; // 0 normal; 1 max iter

	hpipm_tic(&timer0);

	for(rep=0; rep<nrep; rep++)
		{
		d_ocp_qp_ipm_solve(&qp2, &qp_sol2, &arg, &workspace);
		d_ocp_qp_ipm_get_status(&workspace, &hpipm_status);
		}

	double time_ipm = hpipm_toc(&timer0) / nrep;

#if PRINT
	d_ocp_qp_sol_print(&dim2, &qp_sol2);
#endif

/************************************************
* ocp qp restore equation dof
************************************************/

#if KEEP_X0

	double time_res_eq_dof = 0.0;

#else // keep x0

	hpipm_tic(&timer0);

	for(rep=0; rep<nrep; rep++)
		{
		d_ocp_qp_restore_eq_dof(&qp, &qp_sol2, &qp_sol, &qp_red_arg, &qp_red_work);
		}

	double time_res_eq_dof = hpipm_toc(&timer0) / nrep;

#if PRINT
	d_ocp_qp_sol_print(&dim, &qp_sol);
#endif

//	exit(1);

#endif // keep x0

/************************************************
* extract and print solution
************************************************/
	
#if 1
	double **u = (double **) malloc((N+1)*sizeof(double *));
	double **x = (double **) malloc((N+1)*sizeof(double *));
	double **ls = (double **) malloc((N+1)*sizeof(double *));
	double **us = (double **) malloc((N+1)*sizeof(double *));
	double **pi = (double **) malloc(N*sizeof(double *));
	double **lam_lb = (double **) malloc((N+1)*sizeof(double *));
	double **lam_ub = (double **) malloc((N+1)*sizeof(double *));
	double **lam_lg = (double **) malloc((N+1)*sizeof(double *));
	double **lam_ug = (double **) malloc((N+1)*sizeof(double *));
	double **lam_ls = (double **) malloc((N+1)*sizeof(double *));
	double **lam_us = (double **) malloc((N+1)*sizeof(double *));

	for(ii=0; ii<=N; ii++) d_zeros(u+ii, nu[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(x+ii, nx[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(ls+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(us+ii, ns[ii], 1);
	for(ii=0; ii<N; ii++) d_zeros(pi+ii, nx[ii+1], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_lb+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_ub+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_lg+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_ug+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_ls+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(lam_us+ii, ns[ii], 1);

	d_ocp_qp_sol_get_all(&qp_sol, u, x, ls, us, pi, lam_lb, lam_ub, lam_lg, lam_ug, lam_ls, lam_us);
#endif

#if PRINT
	printf("\nsolution\n\n");
	d_ocp_qp_sol_print(&dim, &qp_sol);
#if 0
	printf("\nu\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nu[ii], u[ii], 1);
	printf("\nx\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nx[ii], x[ii], 1);
	printf("\nls\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], ls[ii], 1);
	printf("\nus\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], us[ii], 1);
	printf("\npi\n");
	for(ii=0; ii<N; ii++)
		d_print_mat(1, nx[ii+1], pi[ii], 1);
	printf("\nlam_lb\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nb[ii], lam_lb[ii], 1);
	printf("\nlam_ub\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nb[ii], lam_ub[ii], 1);
	printf("\nlam_lg\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ng[ii], lam_lg[ii], 1);
	printf("\nlam_ug\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ng[ii], lam_ug[ii], 1);
	printf("\nlam_ls\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], lam_ls[ii], 1);
	printf("\nlam_us\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], lam_us[ii], 1);

	printf("\nt_lb\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nb[ii], (qp_sol.t+ii)->pa, 1);
	printf("\nt_ub\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, nb[ii], (qp_sol.t+ii)->pa+nb[ii]+ng[ii], 1);
	printf("\nt_lg\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ng[ii], (qp_sol.t+ii)->pa+nb[ii], 1);
	printf("\nt_ug\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ng[ii], (qp_sol.t+ii)->pa+2*nb[ii]+ng[ii], 1);
	printf("\nt_ls\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], (qp_sol.t+ii)->pa+2*nb[ii]+2*ng[ii], 1);
	printf("\nt_us\n");
	for(ii=0; ii<=N; ii++)
		d_print_mat(1, ns[ii], (qp_sol.t+ii)->pa+2*nb[ii]+2*ng[ii]+ns[ii], 1);
#endif
#endif

/************************************************
* extract and print residuals
************************************************/

#if 1
	double **res_r = (double **) malloc((N+1)*sizeof(double *));
	double **res_q = (double **) malloc((N+1)*sizeof(double *));
	double **res_ls = (double **) malloc((N+1)*sizeof(double *));
	double **res_us = (double **) malloc((N+1)*sizeof(double *));
	double **res_b = (double **) malloc(N*sizeof(double *));
	double **res_d_lb = (double **) malloc((N+1)*sizeof(double *));
	double **res_d_ub = (double **) malloc((N+1)*sizeof(double *));
	double **res_d_lg = (double **) malloc((N+1)*sizeof(double *));
	double **res_d_ug = (double **) malloc((N+1)*sizeof(double *));
	double **res_d_ls = (double **) malloc((N+1)*sizeof(double *));
	double **res_d_us = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_lb = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_ub = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_lg = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_ug = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_ls = (double **) malloc((N+1)*sizeof(double *));
	double **res_m_us = (double **) malloc((N+1)*sizeof(double *));

	for(ii=0; ii<=N; ii++) d_zeros(res_r+ii, nu[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_q+ii, nx[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_ls+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_us+ii, ns[ii], 1);
	for(ii=0; ii<N; ii++) d_zeros(res_b+ii, nx[ii+1], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_lb+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_ub+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_lg+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_ug+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_ls+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_d_us+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_lb+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_ub+ii, nb[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_lg+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_ug+ii, ng[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_ls+ii, ns[ii], 1);
	for(ii=0; ii<=N; ii++) d_zeros(res_m_us+ii, ns[ii], 1);

	d_ocp_qp_res_get_all(workspace.res, res_r, res_q, res_ls, res_us, res_b, res_d_lb, res_d_ub, res_d_lg, res_d_ug, res_d_ls, res_d_us, res_m_lb, res_m_ub, res_m_lg, res_m_ug, res_m_ls, res_m_us);

#if PRINT
	printf("\nresiduals\n\n");
	printf("\nres_r\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nu[ii], res_r[ii], 1);
	printf("\nres_q\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nx[ii], res_q[ii], 1);
	printf("\nres_ls\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_ls[ii], 1);
	printf("\nres_us\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_us[ii], 1);
	printf("\nres_b\n");
	for(ii=0; ii<N; ii++)
		d_print_exp_mat(1, nx[ii+1], res_b[ii], 1);
	printf("\nres_d_lb\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nb[ii], res_d_lb[ii], 1);
	printf("\nres_d_ub\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nb[ii], res_d_ub[ii], 1);
	printf("\nres_d_lg\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ng[ii], res_d_lg[ii], 1);
	printf("\nres_d_ug\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ng[ii], res_d_ug[ii], 1);
	printf("\nres_d_ls\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_d_ls[ii], 1);
	printf("\nres_d_us\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_d_us[ii], 1);
	printf("\nres_m_lb\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nb[ii], res_m_lb[ii], 1);
	printf("\nres_m_ub\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, nb[ii], res_m_ub[ii], 1);
	printf("\nres_m_lg\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ng[ii], res_m_lg[ii], 1);
	printf("\nres_m_ug\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ng[ii], res_m_ug[ii], 1);
	printf("\nres_m_ls\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_m_ls[ii], 1);
	printf("\nres_m_us\n");
	for(ii=0; ii<=N; ii++)
		d_print_exp_mat(1, ns[ii], res_m_us[ii], 1);
#endif
#endif

/************************************************
* print ipm statistics
************************************************/

	int iter; d_ocp_qp_ipm_get_iter(&workspace, &iter);
	double res_stat; d_ocp_qp_ipm_get_max_res_stat(&workspace, &res_stat);
	double res_eq; d_ocp_qp_ipm_get_max_res_eq(&workspace, &res_eq);
	double res_ineq; d_ocp_qp_ipm_get_max_res_ineq(&workspace, &res_ineq);
	double res_comp; d_ocp_qp_ipm_get_max_res_comp(&workspace, &res_comp);
	double obj; d_ocp_qp_ipm_get_obj(&workspace, &obj);
	double *stat; d_ocp_qp_ipm_get_stat(&workspace, &stat);
	int stat_m; d_ocp_qp_ipm_get_stat_m(&workspace, &stat_m);

#if PRINT
	printf("\nipm return = %d\n", hpipm_status);
	printf("\nipm residuals max: res_g = %e, res_b = %e, res_d = %e, res_m = %e\n", res_stat, res_eq, res_ineq, res_comp);
	printf("\nipm objective = %e\n", obj);

	printf("\nipm iter = %d\n", iter);
	printf("\nalpha_aff\tmu_aff\t\tsigma\t\talpha_prim\talpha_dual\tmu\t\tres_stat\tres_eq\t\tres_ineq\tres_comp\tobj\t\tlq fact\t\titref pred\titref corr\tlin res stat\tlin res eq\tlin res ineq\tlin res comp\n");
	d_print_exp_tran_mat(stat_m, iter+1, stat, stat_m);

	printf("\nred eq for time     = %e [s]\n", time_red_eq_dof);
	printf("\nocp ipm time        = %e [s]\n", time_ipm);
	printf("\nres eq for time     = %e [s]\n\n", time_res_eq_dof);
	printf("\ntotal solution time = %e [s]\n\n", time_red_eq_dof+time_ipm+time_res_eq_dof);
#endif

/************************************************
* codegen QP data
************************************************/

//	d_ocp_qp_dim_codegen("examples/c/data/test_d_ocp_data.c", "w", &dim);
//	d_ocp_qp_codegen("examples/c/data/test_d_ocp_data.c", "a", &dim, &qp);
//	d_ocp_qp_ipm_arg_codegen("examples/c/data/test_d_ocp_data.c", "a", &dim, &arg);

/************************************************
* free memory
************************************************/

	// TODO update the frees
	free(nx);
	free(nu);
	free(nbu);
	free(nbx);
	free(nb);
	free(ng);
	free(nsbx);
	free(nsbu);
	free(nsg);
	free(ns);
	free(nbxe);

	d_free(A);
	d_free(B);
	d_free(b);
	d_free(x0);
	d_free(Q);
//	d_free(QN);
	d_free(R);
	d_free(S);
	d_free(q);
//	d_free(qN);
	d_free(r);
	int_free(idxbx0);
	d_free(d_lbx0);
	d_free(d_ubx0);
	int_free(idxbu0);
	d_free(d_lbu0);
	d_free(d_ubu0);
	int_free(idxbx1);
	d_free(d_lbx1);
	d_free(d_ubx1);
	int_free(idxbu1);
	d_free(d_lbu1);
	d_free(d_ubu1);
	int_free(idxbxN);
	d_free(d_lbxN);
	d_free(d_ubxN);
	d_free(C0);
	d_free(D0);
	d_free(d_lg0);
	d_free(d_ug0);
	d_free(C1);
	d_free(D1);
	d_free(d_lg1);
	d_free(d_ug1);
	d_free(CN);
	d_free(DN);
	d_free(d_lgN);
	d_free(d_ugN);
	d_free(d_lbu_mask);
	d_free(d_ubu_mask);
	d_free(d_lbx_mask);
	d_free(d_ubx_mask);

	d_free(Zl0);
	d_free(Zu0);
	d_free(zl0);
	d_free(zu0);
	int_free(idxs0);
	d_free(d_ls0);
	d_free(d_us0);
	d_free(Zl1);
	d_free(Zu1);
	d_free(zl1);
	d_free(zu1);
	int_free(idxs1);
	d_free(d_ls1);
	d_free(d_us1);
	d_free(ZlN);
	d_free(ZuN);
	d_free(zlN);
	d_free(zuN);
	int_free(idxsN);
	d_free(d_lsN);
	d_free(d_usN);

	free(hA);
	free(hB);
	free(hb);
	free(hQ);
	free(hS);
	free(hR);
	free(hq);
	free(hr);
	free(hidxbx);
	free(hd_lbx);
	free(hd_ubx);
	free(hidxbu);
	free(hd_lbu);
	free(hd_ubu);
	free(hC);
	free(hD);
	free(hd_lg);
	free(hd_ug);
	free(hZl);
	free(hZu);
	free(hzl);
	free(hzu);
	free(hidxs);
	free(hd_ls);
	free(hd_us);

	for(ii=0; ii<=N; ii++) d_free(u[ii]);
	for(ii=0; ii<=N; ii++) d_free(x[ii]);
	for(ii=0; ii<=N; ii++) d_free(ls[ii]);
	for(ii=0; ii<=N; ii++) d_free(us[ii]);
	for(ii=0; ii<N; ii++) d_free(pi[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_lb[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_ub[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_lg[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_ug[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_ls[ii]);
	for(ii=0; ii<=N; ii++) d_free(lam_us[ii]);

	free(u);
	free(x);
	free(ls);
	free(us);
	free(pi);
	free(lam_lb);
	free(lam_ub);
	free(lam_lg);
	free(lam_ug);
	free(lam_ls);
	free(lam_us);

#if 0
	for(ii=0; ii<N; ii++)
		{
		d_free(u[ii]);
		d_free(x[ii]);
		d_free(ls[ii]);
		d_free(us[ii]);
		d_free(pi[ii]);
		d_free(lam_lb[ii]);
		d_free(lam_ub[ii]);
		d_free(lam_lg[ii]);
		d_free(lam_ug[ii]);
		d_free(lam_ls[ii]);
		d_free(lam_us[ii]);
		d_free(res_r[ii]);
		d_free(res_q[ii]);
		d_free(res_ls[ii]);
		d_free(res_us[ii]);
		d_free(res_b[ii]);
		d_free(res_d_lb[ii]);
		d_free(res_d_ub[ii]);
		d_free(res_d_lg[ii]);
		d_free(res_d_ug[ii]);
		d_free(res_d_ls[ii]);
		d_free(res_d_us[ii]);
		d_free(res_m_lb[ii]);
		d_free(res_m_ub[ii]);
		d_free(res_m_lg[ii]);
		d_free(res_m_ug[ii]);
		d_free(res_m_ls[ii]);
		d_free(res_m_us[ii]);
		}
	d_free(u[ii]);
	d_free(x[ii]);
	d_free(ls[ii]);
	d_free(us[ii]);
	d_free(lam_lb[ii]);
	d_free(lam_ub[ii]);
	d_free(lam_lg[ii]);
	d_free(lam_ug[ii]);
	d_free(lam_ls[ii]);
	d_free(lam_us[ii]);
	d_free(res_r[ii]);
	d_free(res_q[ii]);
	d_free(res_ls[ii]);
	d_free(res_us[ii]);
	d_free(res_d_lb[ii]);
	d_free(res_d_ub[ii]);
	d_free(res_d_lg[ii]);
	d_free(res_d_ug[ii]);
	d_free(res_d_ls[ii]);
	d_free(res_d_us[ii]);
	d_free(res_m_lb[ii]);
	d_free(res_m_ub[ii]);
	d_free(res_m_lg[ii]);
	d_free(res_m_ug[ii]);
	d_free(res_m_ls[ii]);
	d_free(res_m_us[ii]);
#endif

	free(dim_mem);
	free(qp_mem);
	free(qp_sol_mem);
	free(ipm_arg_mem);
	free(ipm_mem);
#if KEEP_X0
#else // keep x0
	free(dim_mem2);
	free(qp_mem2);
	free(qp_sol_mem2);
	free(qp_red_arg_mem);
	free(qp_red_work_mem);
#endif // keep x0

/************************************************
* return
************************************************/

	return hpipm_status;

	}
