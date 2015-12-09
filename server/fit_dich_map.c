/************************************************************************/
/* fit_dich_map.c VERSION 1.0						*/
/*									*/
/* Fits Gaussian to a data set.						*/
/*									*/
/* fit_dich_map <input <output>>					*/
/*									*/
/* If no input file is given uses .					*/
/* If output file is given a file of x and real and fitted data		*/
/* is generated.							*/
/************************************************************************/
/*                                        	                      	*/
/*                    CHARA ARRAY USER INTERFACE                        */
/*                 Based on the SUSI User Interface                     */
/*              In turn based on the CHIP User interface                */
/*                                                                      */
/*            Center for High Angular Resolution Astronomy              */
/*              Mount Wilson Observatory, CA 91023, USA                 */
/*                                                                      */
/* Telephone: 1-626-796-5405                                            */
/* Fax      : 1-626-796-6717                                            */
/* email    : theo@chara.gsu.edu                                        */
/* WWW      : http://www.chara.gsu.edu                                  */
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Theo ten Brummelaar                                         */
/* Date   : July 2015							*/
/************************************************************************/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <nrc.h>

#define MAX_DATA 	2000

void fit_sin_func(float *ftn, float *var, float *param)
{
	ftn[1] = 1.0;
	ftn[2] = sin(param[1] + var[3]);
	ftn[3] = var[2] * cos(param[1] * var[3]);
	ftn[0] = var[1] + var[2] * ftn[2];
}

int main(int argc, char **argv)
{
	/* Locals */

	FILE	*input;		/* data file */
	FILE	*output;	/* data file */
	char	s[256];		/* Buffer */
	register int i;
	float	rmsres;
	float	maxres;
	int	nparam = 1;	/* Motor movement */
	float	**param = NULL; /* 1..ndata, 1..param */
	int	ndata = 0;
	float	*var;		/* 1..nvar, Offset, Amplitude and Phase */
	float	*fit_data;	/* 1..ndata */
	int	nvar = 3;
	float	theta[MAX_DATA];
	float	x[MAX_DATA];
	float	y[MAX_DATA];
	float dx,dy;
	float	x_off, x_amp, x_phase;
	float	y_off, y_amp, y_phase;
	float	temp;
	

	/* Open the input file */

	if (argc > 1)
	{
		if ((input = fopen(argv[1],"r")) == NULL)
		{
			fprintf(stderr,"Could not open file %s.\n",argv[1]);
			exit(-2);
		}
	}
	else
	{
		input = stdin;
	}

	/* Open the output file */

	if (argc > 2)
	{
		if ((output = fopen(argv[2],"r")) != NULL)
		{
			fprintf(stderr,"File %s already exists.\n",argv[2]);
			exit(-3);
		}
		if ((output = fopen(argv[2],"w")) == NULL)
		{
			fprintf(stderr,"Could not open file %s.\n",argv[2]);
			exit(-4);
		}
	}
	else
	{
		output = NULL;
	}

	if (input == stdin)
	{
		printf("# file : stdin\n");
	}
	else	
	{
		printf("# file : %s \n\n",argv[1]);
	}

	/* Read in the data and doit */

	ndata = 0;
	x[0]=0;
	y[0]=0;
	while(fgets(s,256,input) != NULL)
	{
		if (sscanf(s,"%f %f %f %f",
			&temp, theta+ndata,&dx,&dy) != 4) continue;
		if (ndata > 0){
			x[ndata] = x[ndata-1] + dx;
			y[ndata] = y[ndata-1] + dy;
		}
		theta[ndata] *= (M_PI/180.0);
		if (++ndata > MAX_DATA) break;
	}
	if (input != stdin) fclose(input);

	printf("# Number of data points : %d\n\n",ndata);

	/* Allocate the memory */

	var = vector(1, nvar);
	param = matrix(1, ndata, 1, nparam);
	fit_data = vector(1, ndata);

	/* Do fit for X including first guess */

	var[1] = 0.0;
	for(i=0; i<ndata; i++)
	{
		param[i+1][1] = theta[i];
		var[1] += (fit_data[i+1] = x[i]);
	}

	var[1] /= ndata;
	var[2] = -1e32;
	for(i=0; i<ndata; i++)
	{
		if (fabs(x[i] - var[1]) > var[2])
		{
			var[2] = fabs(x[i] - var[1]);
			var[3] = theta[i] - M_PI/2.0;
		}
	}
	
	non_linear_fit(nvar, var, ndata, fit_data, nparam, param, 0,
		fit_sin_func, &rmsres, &maxres, 2.0, 100, SHOW_NOTHING);

	printf("# Results of X fit : \n");
	printf("# RMS residue %le\n",rmsres);
	printf("# MAX residue %le\n",maxres);
	printf("# Offset    = %f\n",var[1]);
	printf("# Amplitude = %f\n",var[2]);
	printf("# Phase     = %f\n\n",var[3]*180.0/M_PI);

	x_off = var[1];
	x_amp = var[2];
	x_phase = var[3];

	/* Do fit for Y including first guess */

	var[1] = 0.0;
	for(i=0; i<ndata; i++)
	{
		param[i+1][1] = theta[i];
		var[1] += (fit_data[i+1] = y[i]);
	}

	var[1] /= ndata;
	var[2] = -1e32;
	for(i=0; i<ndata; i++)
	{
		if (fabs(y[i] - var[1]) > var[2])
		{
			var[2] = fabs(y[i] - var[1]);
			var[3] = theta[i] - M_PI/2.0;
		}
	}
	
	non_linear_fit(nvar, var, ndata, fit_data, nparam, param, 0,
		fit_sin_func, &rmsres, &maxres, 2.0, 100, SHOW_NOTHING);

	printf("# Results of Y fit : \n");
	printf("# RMS residue %le\n",rmsres);
	printf("# MAX residue %le\n",maxres);
	printf("# Offset    = %f\n",var[1]);
	printf("# Amplitude = %f\n",var[2]);
	printf("# Phase     = %f\n",var[3]*180.0/M_PI);

	y_off = var[1];
	y_amp = var[2];
	y_phase = var[3];

	for(i=0; i<ndata; i++)
	{
		printf("%.1f\t %.1f\t %.1f %.1f %.1f\n",
			theta[i]/M_PI*180.0,
			x[i],
			x_off + x_amp * sin(theta[i] + x_phase),
			y[i],
			y_off + y_amp * sin(theta[i] + y_phase));
	}
			
	exit(0);
}
