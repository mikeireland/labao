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

// #define TEST_ZERNIKE

static float *zernike_values;

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
        /* Make the mutex so this will be thread safe */

        if (pthread_mutex_init(&zernike_mutex, NULL) != 0)
                error(FATAL, "Unable to create Zernike mutex.");

	/* Setup the zernike stuff */

	set_up_zernike_calcs(maxJ);

	/* Now we allocate the first version of the zernike values */

	zernike_values = malloc(sizeof(float)*(maxJ+1));
	if (zernike_values == NULL) error(FATAL, "Not enough memory.");
	
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
	int	i;

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
	float	x, y;
	float	size;
	float	*a;

	size = max_offset + min_doffset/2.0;
	rho_size = min_doffset/(2.0 * size);

	for(i=0; i<NUM_LENSLETS; i++)
	{
		x = (x_centroid_offsets[i] - x_mean_offset)/size;
		y = (y_centroid_offsets[i] - y_mean_offset)/size;

		rho[i] = sqrt(x*x + y*y);
		if (rho[i] > 1.0) rho[i] = 1.0;

		theta[i] = atan2(y, x);
		if (theta[i] < 0.0) theta[i] += (2.0*M_PI);
	}

	/* We also need to create a new reconstructor for Zernikes */

 	for (i=0; i<NUM_ACTUATORS; i++)
        {
            for (j=0; j<2*NUM_LENSLETS; j++)
            {
                zernike_reconstructor[i][j]=0.0;
                fsm_actuator_to_sensor[i][j]=0.0;
            }
	}

	a = vector(1, maxJ);
	for (i=0; i<maxJ; i++)
	{
	    for(j=1; j<=maxJ; j++) a[j] = 0.0;
	    a[i+1] = 1.0;
	    compute_offsets_from_zernike(a, xc, yc);

	    for(j=0; j<NUM_LENSLETS; j++)
	    {
		    fsm_actuator_to_sensor[i][j] = xc[j];
		    fsm_actuator_to_sensor[i][NUM_LENSLETS + j] = yc[j];
	    }
	}
	free_vector(a, 1, maxJ);

	compute_reconstructor_new(0, NULL);

	for(i=0; i<NUM_ACTUATORS; i++)
            for (j=0; j<2*NUM_LENSLETS; j++)
		zernike_reconstructor[i][j] = fsm_reconstructor[i][j];

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
	aperture_pixels **pixels;
	float	**image;
	float	dx, dy, flux;


	for(i=0; i<NUM_LENSLETS; i++)
	{
		/* Build a subaperture */

		pixels = sub_aperture(CENTROID_WINDOW_WIDTH, maxJ,
				rho[j], theta[j], rho_size);

		/* 
 		 * Work out the image 
 	 	 * This is predicated on the assumption that 
 		 * the image size is about 1 pixel. The calculate_image
 		 * function makes the airy size 2^imbedp2/npix, so
 		 * we hope that this works...
 		 */

		image = calculate_image(pixels, 9, 3, maxJ, 0.45, a);

		/* Simmulate the WFS calculation */

		dx = 0.0;
		dy = 0.0;
		flux = 0.0;
		for(j = 1; j<=9; j++)
		for(k = 1; k<=9; k++)
		{
			flux += image[j][k];
			dx += (image[j][k] * (float)(j - 5));
			dy += (image[j][k] * (float)(k - 5));
		}
		dx /= flux;
		dy /= flux;

		/* That should be all */

		x[i] = dx;
		y[i] = dy
		
		/* Clear memory */

		free_sub_aperture(pixels,CENTROID_WINDOW_WIDTH);
		free_matrix(image, 1, 9, 1, 9);

	}

} /* compute_centroid_offset_rho_theta() */

/************************************************************************/
/* zernike_to_dm()							*/
/*									*/
/* Work out current DAC positions and DM positions for current		*/
/* value of Zernike coefficients.					*/
/* Returns 0 if all goes well.						*/
/* -1 if DAC fails.							*/
/************************************************************************/

int zernike_to_dm(void)
{
	float	*values;
	int	i, j;
	float	x[NUM_LENSLETS];
	float	y[NUM_LENSLETS];

	values = vector(1, NUM_ACTUATORS);

        pthread_mutex_lock(&zernike_mutex);

	/* What would the WFS see if this we happening? */

	compute_offsets_from_zernike(zernike_values, x, y);

	/* Apply the reconstructor to get actuator positions */

	for(i=0; i< NUM_ACTUATORS; i++)
	{
		values[i+1] = fsm_flat_dm[i];

		for(j=0; j<NUM_LENSLETS; j++)
			values[i] -=
                                ((fsm_reconstructor[i][j] * x[j]) +
                                (fsm_reconstructor[i][NUM_LENSLETS+j] * y[j]));
	}
	
        pthread_mutex_unlock(&zernike_mutex);

	if (edac40_set_all_channels(values) < 0) return -1;

	free_vector(values, 1, NUM_ACTUATORS);

	return 0;

} /* zernike_to_dm() */

/************************************************************************/
/* increment_zernike()							*/ /* 									*/
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
	
	edac40_send_current_voltages();

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
	
	edac40_send_current_voltages();

	return NOERROR;

} /* call_set_zernike() */

/************************************************************************/
/* test_set_all_zernike()						*/
/*									*/
/* Toggles through all zernike modes.					*/
/************************************************************************/

int test_set_all_zernike(int argc, char **argv)
{
	int	i, J, j;
	float	max;
	float	*values;

	values = vector(1, maxJ);

	for(J = 1; J <= maxJ; J++)
	{
		max = -1e32;
		for(i=1; i<= NUM_ACTUATORS; i++)
		{
			if (fabs(actuator_zernike_values[i][J]) > max)
				max = fabs(actuator_zernike_values[i][J]);
		}
		max *= 2.0;

		for (j=1; j<=maxJ; j++)
		{
			if (j == 1)
				values[j] = 0.5;
			else if (j == J)
				values[j] = 1.0/max;
			else
				values[j] = 0.0;
		}

		if (set_all_zernike(values) < 0)
			return error(ERROR,
			"Failed to set Zernike Mode %d to %.3f",
			J, 1.0/max);

		message(system_window,"Zernike Mode %d", J);

		edac40_send_current_voltages();

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

	edac40_send_current_voltages();

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
