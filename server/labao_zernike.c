/************************************************************************/
/* labao_zernike.c							*/
/*                                                                      */
/* Tries to impose Zernike terms on the actuators. This is uncalibrated.*/
/* This newer version uses the reconstructor to do it's work.		*/
/************************************************************************/
/*                                                                      */
/*                    CHARA ARRAY USER INTERFACE			*/
/*                 Based on the SUSI User Interface			*/
/*		In turn based on the CHIP User interface		*/
/*                                                                      */
/*            Center for High Angular Resolution Astronomy              */
/*              Mount Wilson Observatory, CA 91001, USA			*/
/*                                                                      */
/* Telephone: 1-626-796-5405                                            */
/* Fax      : 1-626-796-6717                                            */
/* email    : theo@chara.gsu.edu                                        */
/* WWW      : http://www.chara.gsu.edu			                */
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Theo ten Brummelaar 			                */
/* Date   : Oct 2002							*/
/************************************************************************/

#include "labao.h"
#include <zernike.h>

//#define TEST_ZERNIKE_1
#define TEST_ZERNIKE_1_DELAY  250000
//#define TEST_ZERNIKE_2

/* Where is each actuator? */

static float actuator_x[NUM_ACTUATORS+1] = {0, /* Numbering starts from 1 */
        0.0, -1, -0.5, 0.5, 1.0, 0.5, -0.5,
        -2, -1.5, -1.0, 0.0, 1.0, 1.5, 2.0, 1.5, 1.0, 0.0, -1.0, -1.5,
        -3.0, -2.5, -2.0, -1.5, -0.5, 0.5, 1.5, 2.0, 2.5, 3.0, 2.5,
        2.0, 1.5, 0.5 ,-0.5, -1.5, -2.0, -2.5};

static float actuator_y[NUM_ACTUATORS+1] = {0, /* Numbering starts from 1 */
        0, 0, 1, 1, 0, -1,-1, 0, 1, 2, 2, 2, 1, 0, -1, -2, -2, -2, -1,
        0, 1, 2, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -2, -1};

static float *actuator_zernike_values[NUM_ACTUATORS+1];

static float *zernike_values;

static float zernike_min, zernike_max;

static pthread_mutex_t zernike_mutex = PTHREAD_MUTEX_INITIALIZER;
static float rho[NUM_LENSLETS];
static float rho_size;
static float theta[NUM_LENSLETS];

/************************************************************************/
/* setup_zernike()							*/
/*									*/
/* Do the preliminary work on the zerniek stuff.			*/
/* See lab book 9 page 7.						*/
/************************************************************************/

void setup_zernike(void)
{
	int	i, j;
	float	rho, theta;

        /* Make the mutex so this will be thread safe */

        if (pthread_mutex_init(&zernike_mutex, NULL) != 0)
                error(FATAL, "Unable to create Zernike mutex.");

	/* Normalize the x and y */

        for (i=1; i<= NUM_ACTUATORS; i++)
        {
                actuator_x[i] /= 3.0;
                actuator_y[i] *= (sqrt(0.75)/3.0);
        }

	/* Setup the zernike stuff */

	set_up_zernike_calcs(maxJ);

	/* Allocate memory iand fill things in */

        actuator_zernike_values[0] = NULL;
        zernike_max = -1e32;
        zernike_min = 1e32;
        for (i=1; i<= NUM_ACTUATORS; i++)
        {
                actuator_zernike_values[i]=malloc(sizeof(float)*(maxJ+1));
                if (actuator_zernike_values[i] == NULL)
                        error(FATAL, "Not enough memory.");

                rho = sqrt(actuator_x[i]*actuator_x[i] +
                           actuator_y[i]*actuator_y[i]);
                if (rho > 1.0) rho = 1.0;

                theta = atan2(actuator_y[i], actuator_x[i]);
                if (theta < 0.0) theta += (2.0*M_PI);

                for(j=1; j<= maxJ; j++)
                {
                        actuator_zernike_values[i][j] = zernike(j,rho,theta);
                        if (actuator_zernike_values[i][j] > zernike_max)
                                zernike_max = actuator_zernike_values[i][j];
                        else if (actuator_zernike_values[i][j] < zernike_min)
                                zernike_min = actuator_zernike_values[i][j];
                }
        }

	/* Now we allocate the first version of the zernike values */

	zernike_values = malloc(sizeof(float)*(maxJ+1));
	if (zernike_values == NULL) error(FATAL, "Not enough memory.");
	
	zernike_values[0] = 1.0;
	zernike_values[1] = 0.0;
	for(j = 2; j <= maxJ; j++) zernike_values[j] = 0.0;

} /* setup_zernike() */
	
/************************************************************************/
/* cleanup_zernike()							*/
/*									*/
/* Frees memory and so on. 						*/
/************************************************************************/

void cleanup_zernike(void)
{
	clean_up_zernike_calcs();

	/* Now we allocate the first version of the zernike values */

	free(zernike_values);

} /* cleanup_zernike() */

/************************************************************************/
/* compute_centroid_offset_rho_theta()					*/
/*									*/
/* We need a rho and theta for each subaperture.			*/
/************************************************************************/

void compute_centroid_offset_rho_theta(void)
{
	int	i, j;
	float	size;
	float	a[NUM_ACTUATORS+1];
	float	norm[NUM_ACTUATORS+1];
	float	x[NUM_LENSLETS];
	float	y[NUM_LENSLETS];
	float	x1, y1;
	float	old_reconstructor[NUM_ACTUATORS][2*NUM_LENSLETS];

	size = max_offset + min_doffset/2.0;
	rho_size = 0.95*min_doffset/(2.0 * size);

	for(i=0; i<NUM_LENSLETS; i++)
	{
		x1 = (x_centroid_offsets[i] - x_mean_offset)/size;
		y1 = (y_centroid_offsets[i] - y_mean_offset)/size;

		rho[i] = sqrt(x1*x1 + y1*y1);
		if (rho[i] > 1.0) rho[i] = 1.0;

		theta[i] = atan2(y1, x1);
		if (theta[i] < 0.0) theta[i] += (2.0*M_PI);
	}

	/* We also need to create a new reconstructor for Zernikes */

 	for (i=0; i<NUM_ACTUATORS; i++)
        {
            for (j=0; j<2*NUM_LENSLETS; j++)
            {
                zernike_reconstructor[i][j] = 0.0;
                fsm_actuator_to_sensor[i][j] = 0.0;
		old_reconstructor[i][j] = fsm_reconstructor[i][j];
            }
	}

	for (i=1; i<maxJ; i++)
	{
	    for(j=0; j<=maxJ; j++) a[j] = 0.0;
	    a[0] = a[i+1] = 2.0;
	    compute_offsets_from_zernike(a, x, y);

	    for(j=0; j<NUM_LENSLETS; j++)
	    {
		    fsm_actuator_to_sensor[i][j] = x[j]/2.0;
		    fsm_actuator_to_sensor[i][NUM_LENSLETS + j] = y[j]/2.0;
	    }
	}

	compute_reconstructor_new(0, NULL);

	for(i=0; i<NUM_ACTUATORS; i++)
	{
            for (j=0; j<2*NUM_LENSLETS; j++)
	    {
		zernike_reconstructor[i][j] = fsm_reconstructor[i][j];
		fsm_reconstructor[i][j] = old_reconstructor[i][j];
	    }
	}

#warning TALK TO MIKE ABOUT THIS

	/* Now, we need to normalize the reconstructor - not sure why */

	for (i=1; i<maxJ; i++)
	{
	    for(j=0; j<NUM_ACTUATORS; j++) a[j] = 0.0;
	    a[0] = a[i+1] = 2.0;
	    compute_offsets_from_zernike(a, x, y);

	    norm[i+1] = 0.0;
	    for(j=0; j<NUM_LENSLETS; j++)
	    {
                norm[i+1] += (zernike_reconstructor[i][j] * x[j] +
                           zernike_reconstructor[i][NUM_LENSLETS+j] * y[j]);
	    }
	    norm[i+1] /= 2.0;
	}

	a[0] = a[1] = 1.0;
        for (i=0;i<NUM_ACTUATORS;i++) 
	{
#ifdef TEST_ZERNIKE_2
	    error(MESSAGE,"%d %f",i+1, norm[i+1]);
#endif
            for (j=0;j<2*NUM_LENSLETS;j++)
	    {
		zernike_reconstructor[i][j] /= norm[i+1];
	    }
	}

} /* compute_centroid_offset_rho_theta() */

/************************************************************************/
/* compute_offsets_from_zernike()					*/
/*									*/
/* Given an aray of zernike's assumed setup for mircons of path, returns*/
/* the offsets theory says the lenslet array should see.		*/
/************************************************************************/

void compute_offsets_from_zernike(float *a, float *x, float *y)
{
	int i,j,k;
	aperture_pixel **pixels;
	float	**image;
	float	dx, dy, flux;
	float	test_a[NUM_ACTUATORS+1];
	float	cen_x, cen_y;
#ifdef TEST_ZERNIKE_1
	Window	phaseWindow;
	Window	imageWindow;

	phaseWindow = openWindow("Phase", 10, 10, 128, 128);
	imageWindow = openWindow("Image", 148, 10, 128, 128);
#endif

	/* First we need to know where the "center" is */

	test_a[0] = 1.0;
	for(i=1; i<=maxJ; i++) test_a[i] = 0.0;
	pixels = sub_aperture(15, maxJ, 0.0, 0.0, rho_size);
#ifdef TEST_ZERNIKE_1
	display_phase(phaseWindow, pixels, 2.0*M_PI, test_a, maxJ, 15, 
		8, LIN);
	display_image(imageWindow, pixels, 2.0*M_PI, test_a, maxJ, 15, 
		8, 5, LIN);
#endif
	image = calculate_image(pixels, 15, 5, maxJ, 2.0*M_PI, test_a);
	cen_x = 0.0;
	cen_y = 0.0;
	flux = 0.0;
	for(j = 1; j<=15; j++)
	{
	    for(k = 1; k<=15; k++)
	    {
		flux += image[j][k];
		cen_x += (image[j][k] * (float)j);
		cen_y += (image[j][k] * (float)k);
	    }
	}
	cen_x /= flux;
	cen_y /= flux;
#ifdef TEST_ZERNIKE_1
	message(system_window,"Center %.2f %.2f", cen_x, cen_y);
	usleep(TEST_ZERNIKE_1_DELAY);
#endif

	for(i=0; i<NUM_LENSLETS; i++)
	{
		/* Build a subaperture */

		pixels = sub_aperture(16, maxJ, rho[i], theta[i], rho_size);

#ifdef TEST_ZERNIKE_1
		display_phase(phaseWindow, pixels, 2.0*M_PI, a, maxJ, 15, 
				8, LIN);
		display_image(imageWindow, pixels, 2.0*M_PI, a, maxJ, 15, 
				8, 5, LIN);
#endif

		/* 
 		 * Work out the image 
 	 	 * This is predicated on the assumption that 
 		 * the image size is about 1 pixel. The calculate_image
 		 * function makes the airy size 2^imbedp2/npix, so
 		 * we hope that this works...
 		 */

		image = calculate_image(pixels, 15, 5, maxJ, 2.0*M_PI, a);

		/* Simmulate the WFS calculation */

		dx = 0.0;
		dy = 0.0;
		flux = 0.0;
		for(j = 1; j<=15; j++)
		{
		    for(k = 1; k<=15; k++)
		    {
			flux += image[j][k];
			dx += (image[j][k] * ((float)j - cen_x));
			dy += (image[j][k] * ((float)k - cen_y));
		    }
		}
		dx /= flux;
		dy /= flux;

		/* That should be all */

		x[i] = dx;
		y[i] = dy;

		/* Clear memory */

		free_sub_aperture(pixels,CENTROID_WINDOW_WIDTH);
		free_matrix(image, 1, 9, 1, 9);
#ifdef TEST_ZERNIKE_1
		message(system_window,"Lenslet %d Pos %.2f %.2f Flux %.2f",i, dx, dy, flux);
		usleep(TEST_ZERNIKE_1_DELAY);
#endif
	}

#ifdef TEST_ZERNIKE_1
	XDestroyWindow(theDisplay, phaseWindow);
#endif

} /* compute_centroid_offset_rho_theta() */

/************************************************************************/
/* zernike_to_dm()                                                      */
/*                                                                      */
/* Work out current DAC positions and DM positions for current          */
/* value of Zernike coefficients.                                       */
/* Returns 0 if all goes well.                                          */
/* -1 if DAC fails.                                                     */
/************************************************************************/

int zernike_to_dm(void)
{
        float   *values;
        int     i, j;

        values = vector(1, NUM_ACTUATORS);

        pthread_mutex_lock(&zernike_mutex);
        for(i=1; i<= NUM_ACTUATORS; i++)
        {
                values[i] = 0.0;

                for(j=1; j <= maxJ; j++)
                {
                        values[i] += (zernike_values[j] *
                                actuator_zernike_values[i][j]);
                }
        }
        pthread_mutex_unlock(&zernike_mutex);

        if (edac40_set_all_channels(values) < 0) return -1;

        free_vector(values, 1, NUM_ACTUATORS);

        return 0;

} /* zernike_to_dm() */

/************************************************************************/
/* new_zernike_to_dm()							*/
/*									*/
/* Work out current DAC positions and DM positions for current		*/
/* value of Zernike coefficients.					*/
/* Returns 0 if all goes well.						*/
/* -1 if DAC fails.							*/
/************************************************************************/

int new_zernike_to_dm(void)
{
	float	*values;
	int	i, j;
	float	x[NUM_LENSLETS];
	float	y[NUM_LENSLETS];
#ifdef TEST_ZERNIKE_2
	float	a[NUM_ACTUATORS+1];
	FILE	*fp;
#endif

	values = vector(1, NUM_ACTUATORS);

        pthread_mutex_lock(&zernike_mutex);

	/* What would the WFS see if this we happening? */

	compute_offsets_from_zernike(zernike_values, x, y);

	/* Apply the reconstructor to get actuator positions */

	for(i=0; i< NUM_ACTUATORS; i++)
	{
		values[i] = fsm_flat_dm[i];

		for(j=0; j<NUM_LENSLETS; j++)
			values[i] -=
                                ((fsm_reconstructor[i][j] * x[j]) +
                                (fsm_reconstructor[i][NUM_LENSLETS+j] * y[j]));
	}
	
        pthread_mutex_unlock(&zernike_mutex);

#ifdef TEST_ZERNIKE_2
	for(j=0; j<NUM_LENSLETS; j++)
	{
		xc[j] = x[j];
		yc[j] = y[j];
	}
	a[0] = 1.0;
        for (i=0;i<NUM_ACTUATORS;i++)
        {
            a[i+1] = 0.0;
            for (j=0;j<NUM_LENSLETS;j++)
            {
                a[i+1] += (zernike_reconstructor[i][j] * x[j] +
                           zernike_reconstructor[i][NUM_LENSLETS+j] * y[j]);
            }
        }
	fp = fopen("zernike.dat", "a");
        for (i=0;i<=maxJ;i++) fprintf(fp, "%.2f ", zernike_values[i]);
	fprintf(fp,"\n");
        for (i=0;i<=maxJ;i++) fprintf(fp, "%.2f ", a[i]);
	fprintf(fp,"\n");
	fflush(fp);
	fclose(fp);
#else
	if (edac40_set_all_channels(values) < 0) return -1;
#endif

	free_vector(values, 1, NUM_ACTUATORS);

	return 0;

} /* new_zernike_to_dm() */

/************************************************************************/
/* call_new_zernike_to_dm()						*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_new_zernike_to_dm(int argc, char **argv)
{
	zernike_to_dm();

	return NOERROR;

} /* call_new_zernike_to_dm() */

/************************************************************************/
/* increment_zernike()							*/
/* 									*/
/* Incriment a sinlge zernike mode by a given amount.			*/
/* Returns:								*/
/* 0 - If all goes well.						*/
/* -1 if J out of range.						*/
/* -2 if zernike_to_dm() fails.						*/
/************************************************************************/

int increment_zernike(int J, float delta)
{
	if (J < 1 || J > maxJ) return -1;

        pthread_mutex_lock(&zernike_mutex);
	zernike_values[J] += delta;
        pthread_mutex_unlock(&zernike_mutex);

	if (zernike_to_dm() < 0) return -2;

	return 0;

} /* increment_zernike() */

/************************************************************************/
/* set_zernike()							*/
/* 									*/
/* Set a single zernike mode by a given amount.				*/
/* Returns:								*/
/* 0 - If all goes well.						*/
/* -1 if J out of range.						*/
/* -2 if zernike_to_dm() fails.						*/
/************************************************************************/

int set_zernike(int J, float value)
{
	if (J < 1 || J > maxJ) return -1;

        pthread_mutex_lock(&zernike_mutex);
	zernike_values[J] = value;
        pthread_mutex_unlock(&zernike_mutex);

	if (zernike_to_dm() < 0) return -2;

	return 0;

} /* set_zernike() */

/************************************************************************/
/* set_all_zernike()							*/
/* 									*/
/* Set all zernike modes to values[1..maxJ]				*/
/* Returns:								*/
/* 0 - If all goes well.						*/
/* -2 if zernike_to_dm() fails.						*/
/************************************************************************/

int set_all_zernike(float *values)
{
	int	j;

        pthread_mutex_lock(&zernike_mutex);
	for(j = 1; j <= maxJ; j++) zernike_values[j] = values[j];
        pthread_mutex_unlock(&zernike_mutex);

	if (zernike_to_dm() < 0) return -2;

	return 0;

} /* set_all_zernike() */

/************************************************************************/
/* call_increment_zernike()						*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_increment_zernike(int argc, char **argv)
{
	char	s[81];
	int	J;
	float	delta;

	/* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&J);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    1");
                if (quick_edit("Zernike Mode",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&J);
        }

        if (J < 1 || J > maxJ)
                return error(ERROR,"Mode %d is out of range 1..%d", J, maxJ);

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&delta);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%7.4f", 0.0);
                if (quick_edit("Mode Delta",s,s,NULL,FLOAT)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&delta);
        }

	if (increment_zernike(J, delta) < 0)
		return error(ERROR,
			"Failed to incirment Zernike Mode %d by %.3f",
			J, delta);
	
#ifndef TEST_ZERNIKE_2
	edac40_send_current_voltages();
#endif

	return NOERROR;

} /* call_increment_zernike() */

/************************************************************************/
/* call_set_zernike()							*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_set_zernike(int argc, char **argv)
{
	char	s[81];
	int	J;
	float	value;

	/* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&J);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    1");
                if (quick_edit("Zernike Mode",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&J);
        }

        if (J < 1 || J > maxJ)
                return error(ERROR,"Mode %d is out of range 1..%d", J, maxJ);

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&value);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%7.4f", zernike_values[J]);
                if (quick_edit("Mode Delta",s,s,NULL,FLOAT)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&value);
        }

	if (set_zernike(J, value) < 0)
		return error(ERROR,
			"Failed to set Zernike Mode %d to %.3f",
			J, value);
	
#ifndef TEST_ZERNIKE_2
	edac40_send_current_voltages();
#endif

	return NOERROR;

} /* call_set_zernike() */

/************************************************************************/
/* test_set_all_zernike()						*/
/*									*/
/* Toggles through all zernike modes.					*/
/************************************************************************/

int test_set_all_zernike(int argc, char **argv)
{
	int	J, j;
	float	*values;

	values = vector(1, maxJ);

	for(J = 1; J <= maxJ; J++)
	{
		for (j=1; j<=maxJ; j++)
		{
			if (j == 1)
				values[j] = 0.5;
			else if (j == J)
				values[j] = 1.0;
			else
				values[j] = 0.0;
		}

		if (set_all_zernike(values) < 0)
			return error(ERROR,
			"Failed to set Zernike Mode %d to %.3f",
			J, 1.0);

		message(system_window,"Zernike Mode %d", J);

#ifndef TEST_ZERNIKE_2
		edac40_send_current_voltages();
#endif

		show_edac40_values(0, NULL);
	}
		
	values[1] = 0.5;
	for (j=2; j<=maxJ; j++) values[j] = 0.0;
	if (set_all_zernike(values) < 0)
	{
		free_vector(values, 1, maxJ);
		return error(ERROR, "Failed to reset Zernike Modes");
	}
	free_vector(values, 1, maxJ);

	werase(system_window);
	wrefresh(system_window);

	return NOERROR;

} /* test_set_all_zernike() */

/************************************************************************/
/* reset_all_zernike()							*/
/*									*/
/* Resets all zernike modes to zero except mode 1 to 0.0.		*/
/************************************************************************/

int reset_all_zernike(int argc, char **argv)
{
	int	j;
	float	*values;

	values = vector(1, maxJ);
	values[1] = 0.5;
	for (j=2; j<=maxJ; j++) values[j] = 0.0;
	if (set_all_zernike(values) < 0)
	{
		free_vector(values, 1, maxJ);
		return error(ERROR, "Failed to reset Zernike Modes");
	}
	free_vector(values, 1, maxJ);

#ifndef TEST_ZERNIKE_2
	edac40_send_current_voltages();
#endif

	return NOERROR;

} /* test_set_all_zernike() */

/************************************************************************/
/* save_zernike()							*/
/*									*/
/* Saves current set of Zernikes to a file.				*/
/************************************************************************/

int save_zernike(char* filename_in)
{
        char filename[256], s[256];
	FILE	*fp;
	int	j;

        if (filename_in != NULL)
        {
                sprintf(filename,"%s%s", get_etc_directory(s), filename_in);
        }
        else
        {
                sprintf(filename,"%s%s.zern", get_etc_directory(s), labao_name);
        }

	if ((fp = fopen(filename,"w")) == NULL)
	{
		return ERROR;
	}

	fprintf(fp,"# Zernike values for %s\n", labao_name);

	for(j=1; j <= maxJ; j++) fprintf(fp,"%.5f\n",zernike_values[j]);

	fclose(fp);

	message(system_window,  "Zernike modes succesfully saved!");
        send_labao_text_message("Zernike modes succesfully saved!");

	return NOERROR;

} /* save_zernike() */

/************************************************************************/
/* call_save_zernike()							*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_save_zernike(int argc, char **argv)
{
	int	ret; 

	if (argc > 1)
		ret = save_zernike(argv[1]);
	else
		ret = save_zernike(NULL);

	if (ret != NOERROR)
		return error(ERROR,"Failed to save Zenrike file.");

	return NOERROR;

} /* call_save_zernike() */

/************************************************************************/
/* load_zernike()							*/
/*									*/
/* Saves current set of Zernikes to a file.				*/
/************************************************************************/

int load_zernike(char* filename_in)
{
        char filename[256], s[256];
	FILE	*fp;
	int	j;

        if (filename_in != NULL)
        {
                sprintf(filename,"%s%s", get_etc_directory(s), filename_in);
        }
        else
        {
                sprintf(filename,"%s%s.zern", get_etc_directory(s), labao_name);
        }

	if ((fp = fopen(filename,"r")) == NULL)
	{
		return -1;
	}

	for(j=1; j <= maxJ; j++) zernike_values[j] = 0.0;

	for(j=1; j <= maxJ; j++)
	{
		if (GetDataLine(s, 256, fp) == -1)
		{
			fclose(fp);
			return -2;	
		}
		if (sscanf(s,"%f",zernike_values+j) != 1) 
			return -3;

	}

	fclose(fp);

	message(system_window,  "Zernike modes succesfully loaded!");
        send_labao_text_message("Zernike modes succesfully loaded!");

	return NOERROR;

} /* load_zernike() */

/************************************************************************/
/* call_load_zernike()							*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_load_zernike(int argc, char **argv)
{
	int	ret; 

	if (argc > 1)
		ret = load_zernike(argv[1]);
	else
		ret = load_zernike(NULL);

	if (ret != 0)
	{
		if (ret == -1)
		    return error(ERROR,"Failed to open Zenrike file.");
		else if (ret == -2)
		    return error(ERROR,"Failed to read all Zernike terms.");
		else
		    return error(ERROR,"Unknown error reading Zernike modes.");
	}

	if (set_all_zernike(zernike_values) != 0)
	{
		return error(ERROR,"Failed to set zernike values.");
	}

        return send_labao_value_all_channels(TRUE);

} /* call_load_zernike() */
