/************************************************************************/
/* labao_fsm.c								*/
/*                                                                      */
/* The Finite State Machine for the Lab AO 				*/
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
/* WWW      : http://www.chara.gsu.edu					*/
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Michael Ireland 						*/
/* Date   : March 2014							*/
/************************************************************************/

#include "labao.h"

/* Default number of loops for different states */

#define NUM_LABAO_ZERO_AVG 100
#define NUM_RECON_WAIT 2
#define NUM_RECON_AVG 10
#define NUM_RECON_CYCLES 10
#define DM_DEFAULT  0.7
#define ACTUATOR_POKE  0.2
#define NUM_INNER_ACTUATORS 19
#define DEFAULT_AUTOALIGN_COUNT 20
#define LAB_ALIGN_LIMIT		0.01
#define LAB_ALIGN_STEP		50
#define SCOPE_ALIGN_LIMIT	0.05
#define SCOPE_ALIGN_STEP	100 
#define SCOPE_ALIGN_GAIN	300
#define ZERNIKE_ALIGN_LIMIT	0.075
#define ZERNIKE_ALIGN_STEP	0.005

/* Optionally enforce zero piston. With SERVO_DAMPING < 1 and a reasonable reconstructor,
   this shouldn't be needed */
#define ENFORCE_ZERO_PISTON

/* Centroid calculation number */

#define CENTROID_WINDOW_WIDTH 5
#define CENTROID_HW (CENTROID_WINDOW_WIDTH/2)
#define CLEAR_EDGE ((CENTROID_BOX_WIDTH/2) + (CENTROID_WINDOW_WIDTH/2))
#warning WE NEED TO THINK ABOUT THESE NUMBERS
#define ZERO_CLAMP 15
#define DENOM_CLAMP 30
#define FLUX_DAMPING 0.9

static const float centroid_window[][CENTROID_WINDOW_WIDTH] = 
	{{0,1,1,1,0}, 
	 {1,1,1,1,1},
	 {1,1,1,1,1},
	 {1,1,1,1,1},
	 {0,1,1,1,0}};

static float centroid_xw[CENTROID_WINDOW_WIDTH][CENTROID_WINDOW_WIDTH];
static float centroid_yw[CENTROID_WINDOW_WIDTH][CENTROID_WINDOW_WIDTH];
                         
static char *dichroic_types[] = {"GRAY ", "SPARE", "YSO  ", NULL};

/* Possible centroiding types */

#define CENTROID_WINDOW_ONLY 0
#define CENTROID_NONLINEAR   1

/* Maximum number of elements in a saved report */

#define MAX_ABERRATIONS_RECORD      100000
#define MAX_DATA_RECORD         100000

/*
 * Some important local globals. PHILOSOPHY: Using these is a little tricky, as
 * numerical recipes in C doesn't have basic vector and matrix multiplication
 * built in. We will therefore use standard C syntax rather than NRC syntax, 
 * and cast arrays to NRC syntax only when NRC routines are to be used.
 */

#warning We should be able to change this on the fly.

static int fsm_centroid_type=CENTROID_NONLINEAR;
static int fsm_ignore_tilt=TRUE;
static float fsm_reconstructor[NUM_ACTUATORS][2*NUM_LENSLETS];
static float fsm_actuator_to_sensor[NUM_ACTUATORS][2*NUM_LENSLETS];
static float fsm_xc_int[NUM_LENSLETS];
static float fsm_yc_int[NUM_LENSLETS];
static int fsm_cyclenum=0;
static float *fsm_flat_dm;
static float fsm_flat_dm_yso[NUM_ACTUATORS];
static float fsm_flat_dm_gray[NUM_ACTUATORS];
static float fsm_flat_dm_spare[NUM_ACTUATORS];
static float fsm_mean_dm[NUM_ACTUATORS];
static float fsm_calc_mean_dm[NUM_ACTUATORS];
static float fsm_dm_offset[NUM_ACTUATORS];
static float last_delta_dm[NUM_ACTUATORS];
static pthread_mutex_t fsm_mutex = PTHREAD_MUTEX_INITIALIZER;
static int actuator=1;
static int wfs_results_num = ((int)DEFAULT_FRAME_RATE);
static int wfs_results_n = 0;
static struct s_labao_wfs_results wfs_reference;
static bool autoalign_lab_dichroic = FALSE;
static bool autoalign_scope_dichroic = FALSE;

static bool scope_dichroic_mapping = FALSE;
static float target_telescope_az = 90.0;
static float initial_mapping_az = 90.0;
#define DICHROIC_MAPPING_ALIGN  0
#define DICHROIC_MAPPING_ROTATE 1
static int scope_dichroic_mapping_step = DICHROIC_MAPPING_ALIGN;
FILE *scope_dichroic_mapping_file;

static int current_total_flux = 0;
static bool autoalign_zernike = FALSE;
static int autoalign_count = 0;
static float autoalign_x_total = 0.0;
static float autoalign_y_total = 0.0;
static float zero_clamp = ZERO_CLAMP;
static float denom_clamp = DENOM_CLAMP;
static bool set_center = FALSE;
static bool new_aberrations = FALSE;
static int aberrations_record_num = 0;
static int aberrations_record_count = 0;
static struct s_labao_wfs_results aberrations_record[MAX_ABERRATIONS_RECORD];
static float x_mean_offset=0.0, y_mean_offset=0.0, max_offset=1.0;
static float aberration_xc[NUM_LENSLETS];
static float aberration_yc[NUM_LENSLETS];
static bool use_reference = FALSE;
static bool set_reference = FALSE;
static bool use_servo_flat = TRUE;

/* Globals. */

int fsm_state = FSM_CENTROIDS_ONLY;
int current_dichroic = AOB_DICHROIC_SPARE;
float x_centroid_offsets[NUM_LENSLETS];
float y_centroid_offsets[NUM_LENSLETS];
float xc[NUM_LENSLETS];
float yc[NUM_LENSLETS];
float avg_fluxes[NUM_LENSLETS];
struct s_labao_wfs_results wfs_results;
struct s_labao_wfs_results calc_labao_results;

/************************************************************************/
/* initialize_fsm()							*/
/*									*/
/* Initialize the finite state machine.					*/
/************************************************************************/

void initialize_fsm(void)
{
	int	i, j;

	calc_labao_results.xtilt = 0.0;
	calc_labao_results.ytilt = 0.0;
	calc_labao_results.xpos = 0.0;
	calc_labao_results.ypos = 0.0;
	calc_labao_results.focus = 0.0;
	calc_labao_results.a1 = 0.0;
	calc_labao_results.a2 = 0.0;
	calc_labao_results.c1 = 0.0;
	calc_labao_results.c2 = 0.0;
	

	wfs_results.xtilt = 0.0;
	wfs_results.ytilt = 0.0;
	wfs_results.xpos = 0.0;
	wfs_results.ypos = 0.0;
	wfs_results.focus = 0.0;
	wfs_results.a1 = 0.0;
	wfs_results.a2 = 0.0;
	wfs_results.c1 = 0.0;
	wfs_results.c2 = 0.0;

	wfs_results_n = 0;

	wfs_reference.xtilt = 0.0;
	wfs_reference.ytilt = 0.0;
	wfs_reference.xpos = 0.0;
	wfs_reference.ypos = 0.0;
	wfs_reference.focus = 0.0;
	wfs_reference.a1 = 0.0;
	wfs_reference.a2 = 0.0;
	wfs_reference.c1 = 0.0;
	wfs_reference.c2 = 0.0;

	for (i=0; i<NUM_ACTUATORS; i++)
	for (j=0; j<2*NUM_LENSLETS; j++)
	{
		fsm_reconstructor[i][j]=0.0;
		fsm_actuator_to_sensor[i][j]=0.0;
	}

	for (i=0;i<NUM_ACTUATORS;i++)
	{
		fsm_flat_dm_yso[i]=DM_DEFAULT;
		fsm_flat_dm_gray[i]=DM_DEFAULT;
		fsm_flat_dm_spare[i]=DM_DEFAULT;
		fsm_mean_dm[i]=DM_DEFAULT;
		fsm_calc_mean_dm[i]=DM_DEFAULT;
		fsm_dm_offset[i]=0.0;	
		last_delta_dm[i]=0.0;	
	}
	fsm_flat_dm = fsm_flat_dm_spare;
	current_dichroic = AOB_DICHROIC_SPARE;

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_centroid_offsets[i]=0.0;
		y_centroid_offsets[i]=0.0;
		aberration_xc[i]=0.0;
		aberration_yc[i]=0.0;
	}

	for (i=0;i<CENTROID_WINDOW_WIDTH;i++)
	for (j=0;j<CENTROID_WINDOW_WIDTH;j++)
	{
		centroid_xw[i][j] = (i-CENTROID_HW)*centroid_window[i][j];
		centroid_yw[i][j] = (j-CENTROID_HW)*centroid_window[i][j];
	}
	/* Create  Mutex for this */

	if (pthread_mutex_init(&fsm_mutex, NULL) != 0)
		error(FATAL, "Unable to create fsm mutex.");

} /* initialize_fsm() */
	
/************************************************************************/
/* compute_pupil_center()                                               */
/*                                                                      */
/* Compute the pupil center and scale directly from the centroid        */
/* offsets. Should be called wheneve rcentroid offsets are changed.     */
/************************************************************************/

void compute_pupil_center(void)
{
	int i;
	float x_offset, y_offset;

	/* Initialize variables */
	x_mean_offset = 0.0;
	y_mean_offset = 0.0;
	max_offset = 1.0;

 	/* To compute WFS aberrations, we need the current pupil center. */

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_mean_offset += x_centroid_offsets[i];
		y_mean_offset += y_centroid_offsets[i];
	}
	x_mean_offset /= NUM_LENSLETS;
	y_mean_offset /= NUM_LENSLETS;

	/* Find the maximum distance of a lenslet from the pupil center. */

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_offset = x_centroid_offsets[i] - x_mean_offset;
		y_offset = y_centroid_offsets[i] - y_mean_offset;

		if (fabs(x_offset) > max_offset) max_offset = fabs(x_offset);
		if (fabs(y_offset) > max_offset) max_offset = fabs(y_offset);
	}

} /* compute_pupil_center() */

/************************************************************************/
/* load_defaults()							*/
/*									*/
/* Load the subarray and the WFS targets.				*/
/* Returns								*/
/* 0 - All went well.							*/
/* -1 No file to open.							*/
/* -2 Failed to get AOI.						*/
/* -3 Failed to get X/Y position for centroid.				*/
/* -4 Failed to interpret X/Y position					*/
/* -5 Failed to set default AOI						*/
/* -6 Failed to get flat wavefront positions				*/
/* -7 Failed to interpret flat wavefront positionss			*/
/* -8 Centroid offsets too close to edge				*/
/* -9 Failed to send a flat wavefront to the DM				*/
/* -10 Failed to read pupil center					*/
/* -11 Failed to read reference information.				*/
/************************************************************************/

int load_defaults(char* filename_in)
{
	char filename[256], s[256];
	FILE *params_file;
	int x,y,dx,dy,i;
	float xoff, yoff;

	if (filename_in != NULL)
	{
 		sprintf(filename,"%s%s", get_etc_directory(s), filename_in);
	}
	else 
	{
    		sprintf(filename,"%s%s.dat", get_etc_directory(s), labao_name);
	}

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"r")) == NULL ) return -1;

  	if (GetDataLine(s, 80, params_file) == -1)
	{
  		fclose(params_file);
  		return -1;
  	}

	/* Get the default area of interest. */

  	if (sscanf(s, "%d %d %d %d", &x, &y, &dx, &dy) != 4) return -2;

	/* Get the default X/Y positions of the centroids */

  	for (i=0;i<NUM_LENSLETS;i++)
	{
		if (GetDataLine(s, 80, params_file) == -1)
		{
			fclose(params_file);
			return -3;
		}

		if (sscanf(s, "%f %f", &xoff, &yoff) != 2)
		{
			fclose(params_file);
			return -4;
		}
		/* Sanity check these numbers */
		if ((xoff < CLEAR_EDGE) || (yoff < CLEAR_EDGE) || 
		    (xoff >= dx-CLEAR_EDGE) || (yoff >= dy-CLEAR_EDGE)){
			return -8;
		}
		x_centroid_offsets[i] = xoff;
		y_centroid_offsets[i] = yoff;
	}
	/* Compute the new pupil center and scale. */
	compute_pupil_center();

	/* Load the "flat" wavefront definition */
	
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		if (GetDataLine(s, 80, params_file) == -1)
		{
			fclose(params_file);
			return -6;
		}

		if (sscanf(s, "%f %f %f", fsm_flat_dm_yso+i,
					  fsm_flat_dm_gray+i,
					  fsm_flat_dm_spare+i) != 3)
		{
			fclose(params_file);
			return -7;
		}
	}
	
	/* Load the pupil center */

	if (GetDataLine(s, 80, params_file) == -1)
	{
		fclose(params_file);
		return -10;
	}

	if (sscanf(s, "%f %f", &xpos_center, &ypos_center) != 2)
	{
		fclose(params_file);
		return -10;
	}

	/* Set a flat wavefront. */
	
	if (flatten_wavefront()) return -9;

	/* Tell clients */

	send_labao_value_all_channels(TRUE);

	/* 
  	 * Try to set the AOI.
	 * This was moved so that even if the camera isn't there
	 * we get a flat wavefront.
	 */

	if (set_usb_camera_aoi(x, y, dx, dy)) return -5;

	/* Get the reference beam information */

	wfs_reference.xtilt = 0.0;
	wfs_reference.ytilt = 0.0;
	wfs_reference.xpos = 0.0;
	wfs_reference.ypos = 0.0;
	wfs_reference.focus = 0.0;
	wfs_reference.a1 = 0.0;
	wfs_reference.a2 = 0.0;
	wfs_reference.c1 = 0.0;
	wfs_reference.c2 = 0.0;

	if (GetDataLine(s, 80, params_file) == -1)
	{
		fclose(params_file);
		return -11;
	}

	if (sscanf(s, "%f %f", &(wfs_reference.xtilt), &(wfs_reference.ytilt)) != 2)
	{
		fclose(params_file);
		return -11;
	}

	if (GetDataLine(s, 80, params_file) == -1)
	{
		fclose(params_file);
		return -11;
	}

	if (sscanf(s, "%f %f", &(wfs_reference.xpos), &(wfs_reference.ypos)) != 2)
	{
		fclose(params_file);
		return -11;
	}

	if (GetDataLine(s, 80, params_file) == -1)
	{
		fclose(params_file);
		return -11;
	}

	if (sscanf(s, "%f %f %f", &(wfs_reference.focus), &(wfs_reference.a1), &(wfs_reference.a2)) != 3)
	{
		fclose(params_file);
		return -11;
	}

#warning Coma terms need to be here too.

	fclose(params_file);

	return 0;

} /* load_defaults() */

/************************************************************************/
/* call_load_defaults()							*/
/*									*/
/* Load the subarray and the WFS targets.				*/
/************************************************************************/

int call_load_defaults(int argc, char **argv)
{
	int	retval = 0;

	if (argc > 1)
	{
		retval = load_defaults(argv[1]);
	}
	else
	{
		retval = load_defaults(NULL); 
	}

	switch (retval)
	{
		case 0: message(system_window,
			"Deafults succesfully loaded!");
			send_labao_text_message(
			"Deafults succesfully loaded!");
			return NOERROR;

		case -1: return error(ERROR,"No file to open.");

		case -2: return error(ERROR,"Failed to get AOI.");

		case -3: return error(ERROR,
				"Failed to get X/Y position for centroid.");

		case -4: return error(ERROR,"Failed to interpret X/Y position");

		case -5: if (usb_cammera_is_running())
				return ERROR;
			 else
				return error(ERROR,"Failed to set default AOI");

		case -6: return error(ERROR,"Failed to get actuator positions for flat wavefront.");
		
		case -7: return error(ERROR,"Failed to get interpret positions for flat wavefront.");

		case -8: return error(ERROR,"Offsets too close to edge.");

		case -9: return error(ERROR,"Failed to send flat to DM.");

		case -10: return error(ERROR,"Failed to read center position.");

		case -11: return error(ERROR,"Failed to read reference position.");

		default: return error(ERROR,
				"Unknown problem with loading defaults (%d).",
				retval);
	}

	return NOERROR;

} /* call_load_defaults() */

/************************************************************************/
/* save_defaults()							*/
/*									*/
/* Save the AOI and the WFS targets.					*/
/* Returns:								*/
/* 0 if all is OK.							*/
/* -1 if failed to open file.						*/
/************************************************************************/

int save_defaults(char* filename_in)
{
	char filename[256], s[256];
	FILE *params_file;
	int x,y,dx,dy,i;

	if (filename_in != NULL)
	{
 		sprintf(filename,"%s%s", get_etc_directory(s),filename_in);
	}
	else
	{
		sprintf(filename,"%s%s.dat", get_etc_directory(s), labao_name);
        }

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"w")) == NULL ) return -1;

  	/* Get the current AOI and save this */

  	get_usb_camera_aoi(&x,&y,&dx,&dy);

	fprintf(params_file, "# Default file %s\n", labao_name);
	fprintf(params_file, "# Default AOI\n");
  	fprintf(params_file, "%d %d %d %d\n", x, y, dx, dy);

	/* Now go through the lenslets offsets */

	fprintf(params_file, "# Default Lenslet X/Y positions.\n");
  	for (i=0;i<NUM_LENSLETS;i++)
	{
		fprintf(params_file, "%7.2f %7.2f\n",
			x_centroid_offsets[i], y_centroid_offsets[i]);
	}

	/* Save the "flat" wavefront definition */

	fprintf(params_file,
		"# Default actuator positions for a flat wavefront.\n");	
	fprintf(params_file, "# YSO      GRAY     SPARE.\n");	
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		fprintf(params_file, "%.5f %.5f %.5f\n", fsm_flat_dm_yso[i],
					  	   fsm_flat_dm_gray[i],
					  	   fsm_flat_dm_spare[i]);
	}

	/* Save the "Center" of the pupil */

	fprintf(params_file, "# Default center position for pupil\n");
	fprintf(params_file, "%f %f\n", xpos_center, ypos_center);

	/* Reference position */

	fprintf(params_file, "# Reference position Tilt\n");
	fprintf(params_file, "%f %f\n", wfs_reference.xtilt, wfs_reference.ytilt);

	fprintf(params_file, "# Reference position Position\n");
	fprintf(params_file, "%f %f\n", wfs_reference.xpos, wfs_reference.ypos);

	fprintf(params_file, "# Reference position Focus and Astigamtism\n");
	fprintf(params_file, "%f %f %f\n", wfs_reference.focus, wfs_reference.a1, wfs_reference.a2);

#warning Coma terms need to be here too.

	/* That is all. */

	fclose(params_file);

	return 0;
	
} /* save_defaults() */

/************************************************************************/
/* call_save_defaults()							*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_save_defaults(int argc, char **argv)
{
	if (argc > 1)
	{
		if (save_defaults(argv[1])) 
			return error (ERROR, "Error saveing defaults.");
	}
	else 
	{
		if (save_defaults(NULL)) 
			return error (ERROR, "Error saveing defaults.");
	}

	message(system_window,  "Centroid offsets and AOI succesfully saved!");
	send_labao_text_message("Centroid offsets and AOI succesfully saved!");

	return NOERROR;

}	/* call_save_defaults() */

/************************************************************************/
/* load_reconstructor()							*/
/*									*/
/* Load the reconstructor (space-separated, N_ACTUATORS rows,           */
/*  2*N_LENSLETS columns)						*/
/* Errors: -1: No file							*/
/* -2: Not enough data lines						*/
/* -3: Not a floating point number. 					*/
/* -4: Not enough numbers in one of the lines.				*/
/************************************************************************/

int load_reconstructor(char* filename_in)
{
	char filename[256], s[256];
	char *substr;
	int i,j;
	FILE *params_file;

	if (filename_in != NULL)
	{
 		strcpy(filename, filename_in);
	}
	else 
	{
    		sprintf(filename,"%s%s_recon.dat", get_etc_directory(s), 
			labao_name);
	}

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"r")) == NULL ) return -1;
	
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		if (GetDataLine(s, 1024, params_file) == -1)
		{
  			fclose(params_file);
  			return -2;
  		}
  		substr = s;

#warning There has to be a library way to do this bit... 
#warning but what library function? A bug in this bit originally wasted a day...

		for (j=0;j<2*NUM_LENSLETS;j++)
		{
			/* Remove opening spaces */
			while (substr[0] == ' ') substr++;
			/* Find the next floating point number */
			if (sscanf(substr,"%f",&(fsm_reconstructor[i][j])) != 1)
			{
				fclose(params_file);
				return -3;
			}	
			/* Move to the the next float */
			if ( (substr = strstr(substr," ")) == NULL)
			{
				fclose(params_file);
				return -4;
			}
//			fprintf(stderr, "%6.3f ", fsm_reconstructor[i][j]);
		}
//		fprintf(stderr, "\n");
	}

	fclose(params_file);

	return 0;

} /* load_reconstructor() */

/************************************************************************/
/* call_load_reconstructor()						*/
/*									*/
/* User callable for load_reconstructor.				*/
/************************************************************************/

int call_load_reconstructor(int argc, char **argv)
{
	int	retval = 0;

	if (argc > 1)
	{
		retval = load_reconstructor(argv[1]);
	}
	else
	{
		retval = load_reconstructor(NULL); 
	}

	switch (retval)
	{
		case 0: message(system_window,
			"Reconstructor successfully loaded!");
			send_labao_text_message(
			"Reconstructor successfully loaded!");
			return NOERROR;

		case -1: return error(ERROR,"No file to open.");

		case -2: return error(ERROR,"Not enough data lines.");

		case -3: return error(ERROR,
				"Non-floating point number (or not enough).");

		case -4: return error(ERROR,
				"Not enough space-separated floats per line.");

		default: return error(ERROR,
				"Unknown problem with loading reconstructor.");
	}

	return NOERROR;

} /* call_load_reconstructor() */

/************************************************************************/
/* save_actuator_to_sensor()						*/
/*									*/
/* Save the actuator to sensor matrix, used to create the reconstructor */ 
/************************************************************************/

int save_actuator_to_sensor(char* filename_in)
{
	char filename[256], s[256];
	FILE *params_file;
	int i,j;

	if (filename_in != NULL)
	{
 		strcpy(filename, filename_in);
	}
	else
	{
		sprintf(filename,"%s%s_actuator_to_sensor.dat", 
			get_data_directory(s), labao_name);
        }

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"w")) == NULL ) return -1;

  	/* Get the current AOI and save this */

	fprintf(params_file, "# Actuator to sensor matrix %s\n", labao_name);
	fprintf(params_file, "# One line of sensor movement per actuator \n");
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		for (j=0;j<2*NUM_LENSLETS;j++)
			fprintf(params_file, "%6.2f ", 
				fsm_actuator_to_sensor[i][j]);
		fprintf(params_file, "\n");
	}

	/* That is all. */

	fclose(params_file);

	return 0;
	
} /* save_actuator_to_sensor() */

/************************************************************************/
/* call_save_actuator_to_sensor()					*/
/*									*/
/* User callable version of save_actuator_to_sensor.			*/
/************************************************************************/

int call_save_actuator_to_sensor(int argc, char **argv)
{
	if (argc > 1)
	{
		if (save_actuator_to_sensor(argv[1])) 
			return error (ERROR,
				"Error saving actuator to sensor matrix.");
	}
	else 
	{
		if (save_actuator_to_sensor(NULL)) 
			return error (ERROR,
				"Error saving actuator to sensor matrix.");
	}

	message(system_window, "Acuator to sensor matrix succesfully saved!");
	send_labao_text_message("Acuator to sensor matrix succesfully saved!");

	return NOERROR;

}	/* call_save_actuator_to_sensor() */


/************************************************************************/
/* run_centroids_and_fsm()						*/
/*									*/
/* The callback function that computes centroids and runs the FSM.	*/
/************************************************************************/

void run_centroids_and_fsm(CHARA_TIME time_stamp,
		float **data, int aoi_width, int aoi_height)
{
	int i,j,l,left,bottom,box_left, box_bottom, poke_sign;
	float last_servo_gain_flat;
	float new_xc[NUM_LENSLETS];
	float new_yc[NUM_LENSLETS];
	float max;
	float fluxes[NUM_LENSLETS];
	float new_dm[NUM_ACTUATORS+1];
	float delta_dm[NUM_ACTUATORS+1];
	int x,y,dx,dy;
	bool is_new_actuator=TRUE;
	float outer_dm_mean = 0.0;
	float x_offset = 0.0, y_offset = 0.0, total_flux = 0.0;
	float	xtilt = 0.0, ytilt = 0.0, focus = 0.0, a1 = 0.0, a2 = 0.0, c1=0.0, c2=0.0;
	float	xpos = 0.0, ypos = 0.0;

	/* First, compute the centroids. */

	for (l = 0; l < NUM_LENSLETS; l++)
	{
		/* 
 		 * If we are doing "nonlinear" centroiding, 
 		 * we compute the centroid
		 * only after finding the maximum pixel in a box.
		 */

		if (fsm_centroid_type == CENTROID_NONLINEAR)
		{
			box_left = (int)(rintf(x_centroid_offsets[l] -
					CENTROID_BOX_WIDTH/2));
			box_bottom = (int)(rintf(y_centroid_offsets[l] -
					CENTROID_BOX_WIDTH/2));
			max = 0;
			left = CENTROID_BOX_WIDTH/2;
			bottom = CENTROID_BOX_WIDTH/2;
			for (i=0;i<CENTROID_BOX_WIDTH;i++)
			for (j=0;j<CENTROID_BOX_WIDTH;j++)
			{
				if (data[box_left+i][box_bottom+j] > max)
				{
					max = data[box_left+i][box_bottom+j];
					left = i;
					bottom = j;
				}
			}
			left += box_left - CENTROID_HW;
			bottom += box_bottom - CENTROID_HW;
		}
		else
		{
			left = (int)(rintf(x_centroid_offsets[l]-CENTROID_HW));
			bottom =(int)(rintf(y_centroid_offsets[l]-CENTROID_HW));
		}

		/* 
 		 * Now that we've defined our box, 
 		 * compute the weighted averages in each box
 		 */

		fluxes[l]=0.0;
		new_xc[l]=0.0;
		new_yc[l]=0.0;
		for (i=0;i<CENTROID_WINDOW_WIDTH;i++)
		for (j=0;j<CENTROID_WINDOW_WIDTH;j++)
		{
			fluxes[l] += centroid_window[i][j] *
					data[left+i][bottom+j];
			new_xc[l] += centroid_xw[i][j] *
					data[left+i][bottom+j];
			new_yc[l] += centroid_yw[i][j] *
					data[left+i][bottom+j];
		}

		/* Correct for low fluxes (denominator clamping) */

		if (fluxes[l] < denom_clamp)
		{
			new_xc[l] /= denom_clamp;
			new_yc[l] /= denom_clamp;
		}
		else
		{
			new_xc[l] /= fluxes[l];
			new_yc[l] /= fluxes[l];
		}

		/* Correct for box offsets, including half-pixels */

		new_xc[l] -= (x_centroid_offsets[l] -
				(float)(left + CENTROID_HW));
		new_yc[l] -= (y_centroid_offsets[l] - 
				(float)(bottom + CENTROID_HW));

		if (fluxes[l] < zero_clamp)
		{
			new_xc[l] = 0.0;
			new_yc[l] = 0.0;
		}

	}

	/* Reset the integral if needed */

	if (fsm_cyclenum==0)
	{
		for (l=0;l<NUM_LENSLETS;l++)
		{
			fsm_xc_int[l]=0.0;
			fsm_yc_int[l]=0.0;
			avg_fluxes[l]=0.0;
		}			
	}

	/* Compute the integral */

	for (l=0;l<NUM_LENSLETS;l++)
	{
		fsm_xc_int[l]+=new_xc[l];
		fsm_yc_int[l]+=new_yc[l];
		avg_fluxes[l]+=fluxes[l];
	}

	/* Lock the mutex */

	pthread_mutex_lock(&fsm_mutex);

	/* Specialised code for different states...  */

	switch (fsm_state)
	{
	case FSM_CENTROIDS_ONLY: fsm_cyclenum = 0;
				 break;

	case FSM_ZERO_CENTROIDS:
	  	if (fsm_cyclenum == NUM_LABAO_ZERO_AVG)
		{	
		    get_usb_camera_aoi(&x,&y,&dx,&dy);
		    for (l=0;l<NUM_LENSLETS;l++)
		    {
			x_centroid_offsets[l]+=fsm_xc_int[l]/NUM_LABAO_ZERO_AVG;
			y_centroid_offsets[l]+=fsm_yc_int[l]/NUM_LABAO_ZERO_AVG;

			/* Sanity check the numbers */

			if (x_centroid_offsets[l] < CLEAR_EDGE)
				x_centroid_offsets[l] = CLEAR_EDGE;
			if (y_centroid_offsets[l] < CLEAR_EDGE)
				y_centroid_offsets[l] = CLEAR_EDGE;
			if (x_centroid_offsets[l] >= dx-CLEAR_EDGE)
				x_centroid_offsets[l] = dx-CLEAR_EDGE;
			if (y_centroid_offsets[l] >= dy-CLEAR_EDGE)
				y_centroid_offsets[l] = dy-CLEAR_EDGE;
		    }
		    /* Compute the new pupil center... */
		    compute_pupil_center();
		    /* Return the fsm to default */
		    fsm_cyclenum=0;
		    fsm_state = FSM_CENTROIDS_ONLY;
	  	}
	  	break;
	  	
	case FSM_MEASURE_RECONSTRUCTOR:
		if (fsm_cyclenum == 0)
		{
		    for (i=0;i<NUM_ACTUATORS;i++)
		    for (l=0;l<NUM_LENSLETS;l++)
		    {
		    	fsm_actuator_to_sensor[i][l] = 0.0;
		    	fsm_actuator_to_sensor[i][NUM_LENSLETS + l] = 0.0;
		    }
	  	}
	  	    
		/* 
		 * "actuator" starts at 1, incrementing every
		 *  (NUM_RECON_WAIT + NUM_RECON_AVG)*NUM_RECON_CYCLES*2 cycles
		 */
		
		actuator = 1 + fsm_cyclenum/(NUM_RECON_WAIT + NUM_RECON_AVG)/
				NUM_RECON_CYCLES/2;
		 
		message(system_window,"Poking actuator %d", actuator);
		send_labao_text_message("Poking actuator %d", actuator);

		/* 
		 * "poke_sign" starts at 1, changing sign every 
		 *  NUM_RECON_WAIT + NUM_RECON_AVG cycles
		 */

		poke_sign = 1-2*( (fsm_cyclenum/(NUM_RECON_WAIT + 
				NUM_RECON_AVG)) % 2 );
		
		/* To make the next code more readable is this a new actuator?*/
		
		is_new_actuator = 1 - 
		  ( (fsm_cyclenum/(NUM_RECON_WAIT + NUM_RECON_AVG)/
				NUM_RECON_CYCLES) % 2);
		
		/* 
		 * We cycle through the actuators one by one, waiting 
		 * NUM_RECON_WAIT after a move and averaging for NUM_RECON_AVG,
		 * reversing the sign
		 * NUM_RECON_CYCLES*2 times then moving on to the next actuator.
		 */
		
		if (fsm_cyclenum % (NUM_RECON_WAIT + NUM_RECON_AVG) == 0)
		{
		    if (fsm_cyclenum > 0)
		    {
			/* Remove the previous poke */
				if (is_new_actuator)
					edac40_delta_single_channel(actuator-1,
					poke_sign*ACTUATOR_POKE);
				else
					edac40_delta_single_channel(actuator,
					poke_sign*ACTUATOR_POKE);
		    }

		    /* Poke the current actuator in the poke direction */

		    if (actuator <= NUM_ACTUATORS) 
				edac40_delta_single_channel(actuator,
					poke_sign*ACTUATOR_POKE);
		}

		/* If we've finished, then finish up! */

		if (fsm_cyclenum == 2*NUM_ACTUATORS*NUM_RECON_CYCLES *
					(NUM_RECON_WAIT + NUM_RECON_AVG))
		{
		    for (i=0;i<NUM_ACTUATORS;i++)
		    for (l=0;l<2*NUM_LENSLETS;l++) 
			fsm_actuator_to_sensor[i][l] /=
			(2*ACTUATOR_POKE*NUM_RECON_AVG*NUM_RECON_CYCLES);
		    fsm_state = FSM_CENTROIDS_ONLY;
		    fsm_cyclenum = 0;
		    actuator = 1;
		    werase(system_window);
		    send_labao_text_message(
			"Reonstructor measurment complete.");
		} 

		/* If we've finished waiting, integrate the sensor positions */ 

		else if (fsm_cyclenum % (NUM_RECON_WAIT + NUM_RECON_AVG) >= 
				NUM_RECON_WAIT) 
		{
		    for (l=0;l<NUM_LENSLETS;l++)
		    {
			fsm_actuator_to_sensor[actuator-1][l] += 
				poke_sign*new_xc[l];
			fsm_actuator_to_sensor[actuator-1][NUM_LENSLETS + l] +=
				poke_sign*new_yc[l];
		    }
		}
		break;
	
	case FSM_APPLY_RECON_ONCE: /* Run the servo loop once with gain=1.0 */
		last_servo_gain_flat = servo_gain_flat;
		servo_gain_flat = 1.0;
		fsm_cyclenum = 0;

	case FSM_SERVO_LOOP:

		for (i=0;i<NUM_ACTUATORS;i++)
		{
		    /* 
 		     * The most likely wavefront is flat, 
 		     * so damp the DM position to flat
 		     */

		    if (fsm_cyclenum > 0 && 
			use_servo_flat && 
			fsm_state ==  FSM_SERVO_LOOP)
		  	    fsm_dm_offset[i] *= servo_damping_flat;
		    else
			    fsm_dm_offset[i] = 0.0;

		    /* 
 		     * If needed, find the overall tilt. 
 		     * NB This requires that all 
		     * lenslets are illuminated!
		     * It probably needs to be a weighted average.
		     */

		    if (fsm_ignore_tilt)
		    {
			for (l=0;l<NUM_LENSLETS;l++)
			{
				xtilt += new_xc[l];
				ytilt += new_xc[l];
			}
			xtilt /= NUM_LENSLETS;
			ytilt /= NUM_LENSLETS;
		    }

		    /* Apply the reconstructor */

		    if (use_servo_flat)
		    {
		      for (l=0;l<NUM_LENSLETS;l++)
		      {
			fsm_dm_offset[i] -= 
			 servo_gain_flat * fsm_reconstructor[i][l]*(new_xc[l]
				- xtilt + aberration_xc[l]);
			fsm_dm_offset[i] -= 
			  servo_gain_flat*fsm_reconstructor[i][NUM_LENSLETS+l] *
				(new_yc[l] - ytilt + aberration_yc[l]);
		      }
#ifdef ENFORCE_ZERO_PISTON
		      if (i >= NUM_INNER_ACTUATORS)
			outer_dm_mean += fsm_dm_offset[i];
#endif
		    }
		    else
		    {
		      for (l=0;l<NUM_LENSLETS;l++)
		      {
			fsm_dm_offset[i] -= 
			 fsm_reconstructor[i][l]*(new_xc[l] - xtilt + 
				aberration_xc[l]);
			fsm_dm_offset[i] -= 
			  fsm_reconstructor[i][NUM_LENSLETS+l] *
				(new_yc[l] - ytilt + aberration_yc[l]);
		      }
		    }
		}

		/* Add in the flat wavefront. */

	        if (use_servo_flat)
		{
		    /* Work out avergae on the edge */

#ifdef ENFORCE_ZERO_PISTON
		    outer_dm_mean /= (NUM_ACTUATORS - NUM_INNER_ACTUATORS);
#endif

		    for (i=0;i<NUM_ACTUATORS;i++)
		    {
		        new_dm[i+1] = fsm_flat_dm[i] + fsm_dm_offset[i];

#ifdef ENFORCE_ZERO_PISTON
			if (i >= NUM_INNER_ACTUATORS)
				new_dm[i+1] -= outer_dm_mean;
#endif
		    }
		    
		}
		else
		{
		    for (i=0;i<NUM_ACTUATORS;i++)
		    {
			delta_dm[i] = servo_gain_delta * fsm_dm_offset[i] -
				servo_damping_delta * last_delta_dm[i];
			last_delta_dm[i] = delta_dm[i];
			new_dm[i+1] = edac40_current_value(i+1) + delta_dm[i];

#ifdef ENFORCE_ZERO_PISTON
			if (i > NUM_INNER_ACTUATORS)
			  outer_dm_mean += (new_dm[i] - fsm_flat_dm[i]);
#endif
		    }
		    
#ifdef ENFORCE_ZERO_PISTON
		    outer_dm_mean /= (NUM_ACTUATORS - NUM_INNER_ACTUATORS);

		    for (i=NUM_INNER_ACTUATORS;i<NUM_ACTUATORS;i++)
		    {
			new_dm[i] -= outer_dm_mean;
		    }
#endif
		}

	        /* Limit positions to the range 0 to 1 */

	        if (new_dm[i+1] > 1)
		{
			if (fsm_state != FSM_APPLY_RECON_ONCE)
			 	fsm_dm_offset[i] -= new_dm[i+1]-1;
			new_dm[i+1] = 1.0;
		}

		if (new_dm[i+1] < 0)
		{
			if (fsm_state != FSM_APPLY_RECON_ONCE)
			 	fsm_dm_offset[i] -= new_dm[i+1];
			new_dm[i+1] = 0.0;
		}

		/* Send off the DM command! */

		if (edac40_set_all_channels(new_dm))
		{
		       	message(system_window, 
				"Problem controlling the DM! Stopping servo.");
		       	send_labao_text_message( 
				"Problem controlling the DM! Stopping servo.");
			fsm_state = FSM_CENTROIDS_ONLY;
		}
		if (fsm_state == FSM_APPLY_RECON_ONCE)
		{
			fsm_state = FSM_CENTROIDS_ONLY;
			fsm_cyclenum = 0;
			servo_gain_flat = last_servo_gain_flat;
		}
		break;

		default: error(FATAL,
			"We should never reach this point in the switch");
	}

	/* 
 	 * Done with fancy servoing etc. 
 	 * Save the centroids and fluxes to our globals
 	 */

	for (l=0;l<NUM_LENSLETS;l++)
	{
		xc[l] = new_xc[l];
		yc[l] = new_yc[l];
		avg_fluxes[l] = fluxes[l] *
				(1-FLUX_DAMPING) + FLUX_DAMPING*avg_fluxes[l];
	}

	/* OK, next cycle */

	fsm_cyclenum++;

	/* Compute the tiptilt, focus and astigmatism terms */

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_offset = x_centroid_offsets[i] - x_mean_offset;
		y_offset = y_centroid_offsets[i] - y_mean_offset;

		xtilt += xc[i];
		ytilt += yc[i];

		total_flux += avg_fluxes[i];

		/* To compute the pupil position, reference everything
		to the center of the pupil. */
		xpos += x_offset * avg_fluxes[i];
		ypos += y_offset * avg_fluxes[i];

#warning This is not normalised correctly. Should give 1 for e.g. peak to valley slopes of 1 arcsec.

		focus += (xc[i]*x_offset + yc[i]*y_offset);
		a1 +=    (xc[i]*x_offset - yc[i]*y_offset);
		a2 +=    (yc[i]*x_offset + xc[i]*y_offset);
		c1 +=  xc[i]*(9*x_offset*x_offset + 3*y_offset*y_offset - 2)/9.0 + 
			yc[i]*2*x_offset*y_offset/3.0;
		c2 +=  yc[i]*(9*y_offset*y_offset + 3*x_offset*x_offset - 2)/9.0 + 
			xc[i]*2*x_offset*y_offset/3.0;
	}
	current_total_flux = total_flux;
	/* This is the old method.... */

	/*
	xtilt /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	ytilt /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	focus /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	a1 /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	a2 /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	xpos /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	ypos /= (NUM_LENSLETS/ARCSEC_PER_PIX);
	*/

	/* We do it this way so the numbers are the same as for the WFS */

	xtilt /= ((float)NUM_LENSLETS*((float)CENTROID_BOX_WIDTH/2.0));
	ytilt /= ((float)NUM_LENSLETS*((float)CENTROID_BOX_WIDTH/2.0));
	focus /= (float)NUM_LENSLETS;
	a1 /= (float)NUM_LENSLETS;
	a2 /= (float)NUM_LENSLETS;
	c1 /= (float)NUM_LENSLETS;
	c2 /= (float)NUM_LENSLETS;
	

	if (total_flux > ZERO_CLAMP)
	{
		xpos /= total_flux;
		ypos /= total_flux;
	}
	else
	{
		xpos = 0.0;
		ypos = 0.0;
	}

	/* 
	 * So that all terms are at least in units of "pixels" 
	 * (i.e. max tilt per sub-aperture), we divide by the
	 * radius of the pupil.
	 */

	focus /= max_offset;
	a1 /= max_offset;
	a2 /= max_offset;
	c1 /= max_offset*max_offset;
	c2 /= max_offset*max_offset;
	xpos /= max_offset;
	ypos /= max_offset;

	/* Are we saving the aberrations for a report? */

	if (aberrations_record_num > 0 &&
	    aberrations_record_count < aberrations_record_num)
	{
		aberrations_record[aberrations_record_count].xtilt = xtilt;
		aberrations_record[aberrations_record_count].ytilt = ytilt;
		aberrations_record[aberrations_record_count].xpos = xpos;
		aberrations_record[aberrations_record_count].ypos = ypos;
		aberrations_record[aberrations_record_count].focus = focus;
		aberrations_record[aberrations_record_count].a1 = a1;
		aberrations_record[aberrations_record_count].a2 = a2;
		aberrations_record[aberrations_record_count].c1 = c1;
		aberrations_record[aberrations_record_count].c2 = c2;
		aberrations_record[aberrations_record_count].fsm_state = 
								fsm_state;
		aberrations_record[aberrations_record_count].current_dichroic = 
							current_dichroic;
		aberrations_record[aberrations_record_count].time_stamp = 
								time_stamp;

		aberrations_record_count++;
	}

	/* Now calculate the mean */

	calc_labao_results.xtilt += xtilt;
	calc_labao_results.ytilt += ytilt;
	calc_labao_results.focus += focus;
	calc_labao_results.xpos += xpos;
	calc_labao_results.ypos += ypos;
	calc_labao_results.a1 += a1;
	calc_labao_results.a2 += a2;
	calc_labao_results.c1 += c1;
	calc_labao_results.c2 += c2;
	 for (i=0;i<NUM_ACTUATORS;i++)
                fsm_calc_mean_dm[i] += edac40_current_value(i+1);

	if (++wfs_results_n >= wfs_results_num)
	{
		wfs_results.xtilt = calc_labao_results.xtilt/wfs_results_num;
		wfs_results.ytilt = calc_labao_results.ytilt/wfs_results_num;
		wfs_results.focus = calc_labao_results.focus/wfs_results_num;
		wfs_results.xpos = calc_labao_results.xpos/wfs_results_num;
		wfs_results.ypos = calc_labao_results.ypos/wfs_results_num;
		wfs_results.fsm_state = fsm_state;
		wfs_results.current_dichroic = current_dichroic;

		if (set_center)
		{
			xpos_center = wfs_results.xpos;
			ypos_center = wfs_results.ypos;
			set_center = FALSE;
		}

		wfs_results.xpos -= xpos_center;
		wfs_results.ypos -= ypos_center;
		wfs_results.a1 = calc_labao_results.a1/wfs_results_num;
		wfs_results.a2 = calc_labao_results.a2/wfs_results_num;
		wfs_results.c1 = calc_labao_results.c1/wfs_results_num;
		wfs_results.c2 = calc_labao_results.c2/wfs_results_num;

		for (i=0;i<NUM_ACTUATORS;i++)
                {
                        fsm_mean_dm[i] = fsm_calc_mean_dm[i]/wfs_results_num;
                        fsm_calc_mean_dm[i] = 0.0;
                }

		if (set_reference)
		{
			wfs_reference.xtilt = wfs_results.xtilt;
			wfs_reference.ytilt = wfs_results.ytilt;
			wfs_reference.xpos = wfs_results.xpos;
			wfs_reference.ypos = wfs_results.ypos;
			wfs_reference.focus = wfs_results.focus;
			wfs_reference.a1 = wfs_results.a1;
			wfs_reference.a2 = wfs_results.a2;
			wfs_reference.c1 = wfs_results.c1;
			wfs_reference.c2 = wfs_results.c2;
			set_reference = FALSE;
		}
		calc_labao_results.xtilt = 0.0;

		if (use_reference)
		{
			wfs_results.xtilt -= wfs_reference.xtilt;
			wfs_results.ytilt -= wfs_reference.ytilt;
			wfs_results.xpos -= wfs_reference.xpos;
			wfs_results.ypos -= wfs_reference.ypos;
			wfs_results.focus -= wfs_reference.focus;
			wfs_results.a1 -= wfs_reference.a1;
			wfs_results.a2 -= wfs_reference.a2;
			wfs_results.c1 -= wfs_reference.c1;
			wfs_results.c2 -= wfs_reference.c2;
		}
		calc_labao_results.xtilt = 0.0;
		calc_labao_results.ytilt = 0.0;
		calc_labao_results.xpos = 0.0;
		calc_labao_results.ypos = 0.0;
		calc_labao_results.focus = 0.0;
		calc_labao_results.a1 = 0.0;
		calc_labao_results.a2 = 0.0;
		calc_labao_results.c1 = 0.0;
		calc_labao_results.c2 = 0.0;

		wfs_results_n = 0;

		new_aberrations = TRUE;
	}

	/* Unlock the Mutex */
 
	pthread_mutex_unlock(&fsm_mutex);

} /* run_centroids_and_fsm() */

/************************************************************************/
/* zero_centroids()							*/
/*                                                                      */
/* Change the FSM state to ZERO_CENTROIDS.                              */
/************************************************************************/

void zero_centroids(void)
{
	pthread_mutex_lock(&fsm_mutex);
	fsm_cyclenum=0;
	fsm_state = FSM_ZERO_CENTROIDS;
	set_center = TRUE;
	pthread_mutex_unlock(&fsm_mutex);
}

/************************************************************************/
/* call_zero_centroids()                                                */
/*                                                                      */
/* The callback function for this.                                      */
/************************************************************************/

int call_zero_centroids(int argc, char **argv)
{
	zero_centroids();
	return NOERROR;
} 

/************************************************************************/
/* close_servo()							*/
/*                                                                      */
/* Change the FSM state to FSM_SERVO_LOOP.                              */
/************************************************************************/

void close_servo(void)
{
	pthread_mutex_lock(&fsm_mutex);
	fsm_cyclenum=0;
	fsm_state = FSM_SERVO_LOOP;
	pthread_mutex_unlock(&fsm_mutex);
}

/************************************************************************/
/* call_close_servo()                                                   */
/*                                                                      */
/* The callback function for this.                                      */
/************************************************************************/

int call_close_servo(int argc, char **argv)
{
	close_servo();
	return NOERROR;
} 

/************************************************************************/
/* open_servo() 							*/
/*                                                                      */
/* Change the FSM state to FSM_SERVO_LOOP.                              */
/************************************************************************/

void open_servo(void)
{
	pthread_mutex_lock(&fsm_mutex);
	fsm_cyclenum=0;
	fsm_state = FSM_CENTROIDS_ONLY;
	pthread_mutex_unlock(&fsm_mutex);
}

/************************************************************************/
/* call_open_servo()                                                    */
/*                                                                      */
/* The callback function for this.                                      */
/************************************************************************/

int call_open_servo(int argc, char **argv)
{
	open_servo();
	return NOERROR;
} 

/************************************************************************/
/* apply_reconstructor() 						*/
/*                                                                      */
/* Apply the reconstructor once.                                        */
/************************************************************************/

void apply_reconstructor(void)
{
	pthread_mutex_lock(&fsm_mutex);
	fsm_cyclenum=0;
	fsm_state = FSM_APPLY_RECON_ONCE;
	pthread_mutex_unlock(&fsm_mutex);
}

/************************************************************************/
/* call_apply_reconstructor()                                           */
/*                                                                      */
/* The callback function for this.                                      */
/************************************************************************/

int call_apply_reconstructor(int argc, char **argv)
{
	apply_reconstructor();
	return NOERROR;
} 

/************************************************************************/
/* measure_reconstructor()						*/
/*									*/
/* Change the FSM state to measure_reconstructor.			*/
/************************************************************************/

void measure_reconstructor(void)
{
	pthread_mutex_lock(&fsm_mutex);
	fsm_cyclenum=0;
	fsm_state = FSM_MEASURE_RECONSTRUCTOR;
	pthread_mutex_unlock(&fsm_mutex);
}

/************************************************************************/
/* call_measure_reconstructor()						*/
/*									*/
/* User callable version.						*/
/************************************************************************/


int call_measure_reconstructor(int argc, char **argv)
{
	measure_reconstructor();
	return NOERROR;
}

/************************************************************************/
/* fsm_status()						 		*/
/*									*/
/* Can be called as a background process.				*/
/************************************************************************/

int fsm_status(void)
{
	struct smessage mess;
	struct s_elaz pos;
	struct s_aob_move_motor motor_move;
	mirror_move mirror_move;
	char	s[123];
	char	*args[2];
	static time_t  last_time = 0;
	float	theta, x, y;

	/* The overall state. */

	wstandout(status_window);
	mvwaddstr(status_window,1,20,"Dichroic: ");
	wstandend(status_window);
	wprintw(status_window," %s", dichroic_types[current_dichroic - 1]);

	wstandout(status_window);
	mvwaddstr(status_window,2,20,"FSM     : ");
	wstandend(status_window);
	switch (fsm_state)
	{
		case FSM_CENTROIDS_ONLY:
			wprintw(status_window, " WFS only     ");
			break;
		case FSM_ZERO_CENTROIDS:
			wprintw(status_window,"Zero Centroids");
			break;
		case FSM_MEASURE_RECONSTRUCTOR:
			wprintw(status_window, " Recon %2d  ", actuator);
			break;
		case FSM_SERVO_LOOP:
			wprintw(status_window, " Servo Active ");
			break;
		default:
			wprintw(status_window, "UNKNOWN");
	}

	wstandout(status_window);
	mvwaddstr(status_window,3,20,"IntN    : ");
	wstandend(status_window);
	wprintw(status_window, "%9d", wfs_results_num);

	wstandout(status_window);
	mvwaddstr(status_window,4,20,"Beam    : ");
	wstandend(status_window);
	if (use_reference)
		wprintw(status_window, "%9s", "Reference");
	else
		wprintw(status_window, "%9s", "Beacon");

	/* Check on the scope_dichroic_mapping function */
	if (scope_dichroic_mapping) {
		if ( (scope_dichroic_mapping_step == DICHROIC_MAPPING_ALIGN) && (!autoalign_scope_dichroic) ){
			/* Have we moved all the way around? */
			if (fmod(telescope_status.az - initial_mapping_az + 15  + 360,360) < 10.0){
				scope_dichroic_mapping = FALSE;
				fclose(scope_dichroic_mapping_file);
				message(system_window,
				"Dichroic mapping complete!");
				send_labao_text_message("%s",
				"Dichroic mapping complete!");
			} else {
				/* Write out the autoalign total from the last alignment */
				fprintf(scope_dichroic_mapping_file, "1 %5.1f %6.2f %6.2f\n", 
				 telescope_status.az, autoalign_x_total, autoalign_y_total);
				fflush(scope_dichroic_mapping_file);
				/* Time to rotate the telescope */
				scope_dichroic_mapping_step = DICHROIC_MAPPING_ROTATE;
				pos.el = telescope_status.el;
				target_telescope_az = telescope_status.az + 10;
				pos.az = target_telescope_az;
				mess.type = TELESCOPE_ELAZ;
				mess.data = (unsigned char *)&pos;
				mess.length = sizeof(pos);
				send_message(telescope_server, &mess);
			}
		}
		
		if ( (scope_dichroic_mapping_step == DICHROIC_MAPPING_ROTATE) && 
		     (fabs(telescope_status.az - target_telescope_az)<0.1) ){
		/* This is only a rough sanity check... !!! the number should be checked */
			if (current_total_flux < 200){
				scope_dichroic_mapping = FALSE;
				fclose(scope_dichroic_mapping_file);
				message(system_window,
				"Giving up on scope dichroic mapping - low flux.");
				send_labao_text_message("%s",
				"Giving up on scope dichroic mapping - low flux.");
			} else {
				args[0] = "adich";
				args[1] = "50"; /* A fixed number of tries.*/
				start_autoalign_scope_dichroic(2, args);
				scope_dichroic_mapping_step = DICHROIC_MAPPING_ALIGN;
			}
		}
	}


	/* Is there a new mean we can work with? */

	pthread_mutex_lock(&fsm_mutex);

	if (new_aberrations)
	{
		new_aberrations = FALSE;

		/* Let the user know about this */

		mess.type = LABAO_WFS_RESULTS;
		mess.length = sizeof(wfs_results);
		mess.data = (unsigned char *)&wfs_results;
		ui_send_message_all(&mess);

		/* Are we trying to align things? */

		if (autoalign_lab_dichroic && time(NULL) > last_time)
		{
			/* Is it done? */

			if (fabs(wfs_results.xtilt) < LAB_ALIGN_LIMIT &&
			    fabs(wfs_results.ytilt) < LAB_ALIGN_LIMIT)
			{
				autoalign_lab_dichroic = FALSE;
				autoalign_count = 0;
				message(system_window,
					"Lab Autoalignment is complete.");
				send_labao_text_message("%s", 
					"Lab Autoalignment is complete.");
				use_reference = FALSE;
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* Have we tried too many times? */

			if (--autoalign_count < 0)
			{
				autoalign_lab_dichroic = FALSE;
				autoalign_count = 0;
				message(system_window,
					"Giving up on lab autoalignment.");
				send_labao_text_message("%s",
					"Giving up on lab autoalignment.");
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* OK, move things in appropriate ways */

			mess.type = PICO_MOVE_MIRROR;
			mess.length = sizeof(mirror_move);
			mess.data = (unsigned char *)&mirror_move;
			mirror_move.mirror = dich_mirror;
			mirror_move.pulses = LAB_ALIGN_STEP;

			sprintf(s, "%d X = %.2f Y = %.2f -", 
			autoalign_count+1, 
			wfs_results.xtilt, wfs_results.ytilt);

			if (autoalign_count % 2)
			{
			    if (wfs_results.xtilt >= LAB_ALIGN_LIMIT)
			    {
				mirror_move.direction = DOWN;
				send_message(pico_server, &mess);
				strcat(s," Moving DOWN");
			    }
			    else if (wfs_results.xtilt <= -1.0*LAB_ALIGN_LIMIT)
			    {
				mirror_move.direction = UP;
				send_message(pico_server, &mess);
				strcat(s," Moving UP");
			    }
			    else
			    {
				strcat(s, "X OK.");
			    }
			}
			else
			{
			    if (wfs_results.ytilt >= LAB_ALIGN_LIMIT)
			    {
				mirror_move.direction = RIGHT;
				send_message(pico_server, &mess);
				strcat(s," Moving RIGHT");
			    }
			    else if (wfs_results.ytilt <= -1.0*LAB_ALIGN_LIMIT)
			    {
				mirror_move.direction = LEFT;
				send_message(pico_server, &mess);
				strcat(s," Moving LEFT");
			    }
			    else
			    {
				strcat(s, "Y OK.");
			    }
			}
			message(system_window,s);
			send_labao_text_message("%s", s);

			/* OK, this is now done */

			last_time = time(NULL);
		}

		if (autoalign_scope_dichroic && time(NULL) > last_time+2)
		{
			/* Is it done? */

			if (fabs(wfs_results.xtilt) < SCOPE_ALIGN_LIMIT &&
			    fabs(wfs_results.ytilt) < SCOPE_ALIGN_LIMIT)
			{
				autoalign_scope_dichroic = FALSE;
				autoalign_count = 0;
				message(system_window,
					"Scope Autoalignment is complete.");
				send_labao_text_message("%s", 
					"Scope Autoalignment is complete.");
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* Have we tried too many times? */

			if (--autoalign_count < 0)
			{
				autoalign_scope_dichroic = FALSE;
				autoalign_count = 0;
				if (scope_dichroic_mapping){
					scope_dichroic_mapping = FALSE;
					fclose(scope_dichroic_mapping_file);
					message(system_window,
					"Giving up on scope dichroic mapping - autoalignment fail.");
					send_labao_text_message("%s",
					"Giving up on scope dichroic mapping - autoalignment fail.");
				} else {
					message(system_window,
					"Giving up on scope autoalignment.");
					send_labao_text_message("%s",
					"Giving up on scope autoalignment.");
				}
				fprintf(scope_dichroic_mapping_file, "0 %5.1f %6.2f %6.2f\n", 
				 telescope_status.az, autoalign_x_total, autoalign_y_total);
				fflush(scope_dichroic_mapping_file);
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* OK, move things in appropriate ways */

			mess.type = HUT_AOB_MOVE_RELATIVE;
			mess.length = sizeof(motor_move);
			mess.data = (unsigned char *)&motor_move;

			/* First we need to rotate into the right frame */
#warning Despite this being totally wrong in principle (120 degree offset between axes) there used to be an 8 degrees below82
			theta = -telescope_status.az/180.0*M_PI;
			x = - cos(theta) * wfs_results.xtilt +
			    sin(theta) * wfs_results.ytilt;
			y = - sin(theta) * wfs_results.xtilt -
			    cos(theta) * wfs_results.ytilt;

			/* Tell the user */

			sprintf(s, "Az %5.1f %d X = %.2f/%.2f Y = %.2f/%.2f -", 
			telescope_status.az, autoalign_count+1, 
			wfs_results.xtilt, x, wfs_results.ytilt, y);
			if (autoalign_count % 2)
			{
			    if (x > SCOPE_ALIGN_LIMIT)
			    {
				motor_move.motor = AOB_DICHR_2;
				motor_move.position = SCOPE_ALIGN_STEP + (int)(fabs(x)*SCOPE_ALIGN_GAIN);
				autoalign_x_total += SCOPE_ALIGN_STEP + (int)(fabs(x)*SCOPE_ALIGN_GAIN);
				send_message(telescope_server, &mess);
				strcat(s," Moving RIGHT");
			    }
			    else if (x < -1.0*SCOPE_ALIGN_LIMIT)
			    {
				motor_move.motor = AOB_DICHR_2;
				motor_move.position = -1.0*(SCOPE_ALIGN_STEP  + (int)(fabs(x)*SCOPE_ALIGN_GAIN));
				autoalign_x_total -= SCOPE_ALIGN_STEP  + (int)(fabs(x)*SCOPE_ALIGN_GAIN);
				send_message(telescope_server, &mess);
				strcat(s," Moving LEFT");
			    }
			    else
			    {
				strcat(s, "X OK.");
			    }
			}
			else
			{
			    if (y > SCOPE_ALIGN_LIMIT)
			    {
				motor_move.motor = AOB_DICHR_1;
				motor_move.position = SCOPE_ALIGN_STEP + (int)(fabs(y)*SCOPE_ALIGN_GAIN);
				autoalign_y_total += SCOPE_ALIGN_STEP + (int)(fabs(y)*SCOPE_ALIGN_GAIN); 
				send_message(telescope_server, &mess);
				strcat(s," Moving UP");
			    }
			    else if (y < -1.0 * SCOPE_ALIGN_LIMIT)
			    {
				motor_move.motor = AOB_DICHR_1;
				motor_move.position = -1.0*(SCOPE_ALIGN_STEP + (int)(fabs(y)*SCOPE_ALIGN_GAIN));
				autoalign_y_total -= SCOPE_ALIGN_STEP + (int)(fabs(y)*SCOPE_ALIGN_GAIN);
				send_message(telescope_server, &mess);
				strcat(s," Moving DOWN");
			    }
			    else
			    {
				strcat(s, "Y OK.");
			    }
			}

			message(system_window,s);
			send_labao_text_message("%s", s);

			/* OK, this is now done */

			last_time = time(NULL);
		}

#warning This bit needs coma also.
		if (autoalign_zernike && time(NULL) > last_time)
		{
			/* Is it done? */

			if (fabs(wfs_results.focus) < ZERNIKE_ALIGN_LIMIT &&
			    fabs(wfs_results.a1) < ZERNIKE_ALIGN_LIMIT &&
			    fabs(wfs_results.a1) < ZERNIKE_ALIGN_LIMIT)
			{
				autoalign_zernike = FALSE;
				autoalign_count = 0;
				message(system_window,
					"Zernike Autoalignment is complete.");
				send_labao_text_message("%s", 
					"Zernike Autoalignment is complete.");
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* Have we tried too many times? */

			if (--autoalign_count < 0)
			{
				autoalign_zernike = FALSE;
				autoalign_count = 0;
				message(system_window,
					"Giving up on Zernike autoalignment.");
				send_labao_text_message("%s",
					"Giving up on Zernike autoalignment.");
				pthread_mutex_unlock(&fsm_mutex);
				return NOERROR;
			}

			/* Tell the user */

			sprintf(s, "%d F = %.3f A1 = %.3f A2 = %.3f -", 
				autoalign_count+1, 
				wfs_results.focus, 
				wfs_results.a1, wfs_results.a2);

			if (wfs_results.focus > ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," F ++");
				increment_zernike(1, ZERNIKE_ALIGN_STEP);
			}
			else if (wfs_results.focus < -1.0 * ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," F --");
				increment_zernike(1, -1.0 * ZERNIKE_ALIGN_STEP);
			}
			else
			{
				strcat(s, "F OK.");
			}

			if (wfs_results.a1 > ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," A1 ++");
				increment_zernike(6, -1.0 * ZERNIKE_ALIGN_STEP);
			}
			else if (wfs_results.focus < -1.0 * ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," A1 --");
				increment_zernike(6,ZERNIKE_ALIGN_STEP);
			}
			else
			{
				strcat(s, "A1 OK.");
			}

			if (wfs_results.a2 > ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," A2 ++");
				increment_zernike(5, ZERNIKE_ALIGN_STEP);
			}
			else if (wfs_results.focus < -1.0 * ZERNIKE_ALIGN_LIMIT)
			{
				strcat(s," A2 --");
				increment_zernike(5,-1.0 * ZERNIKE_ALIGN_STEP);
			}
			else
			{
				strcat(s, "A2 OK.");
			}

			message(system_window,s);
			send_labao_text_message("%s", s);

			/* OK, this is now done */

			last_time = time(NULL);
		}

		/* Now output these variables to the status window */

		wstandout(status_window);
		mvwaddstr(status_window,5,20,"Tilt    : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f %6.3f", 
			wfs_results.xtilt, wfs_results.ytilt);
		wstandout(status_window);
		mvwaddstr(status_window,6,20,"Pos     : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f %6.3f", 
			wfs_results.xpos, wfs_results.ypos);
		wstandout(status_window);
		mvwaddstr(status_window,7,20,"Focus   : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f", wfs_results.focus);
		wstandout(status_window);
		mvwaddstr(status_window,8,20,"Astig   : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f %6.3f", 
			wfs_results.a1, wfs_results.a2);
		wstandout(status_window);
		mvwaddstr(status_window,8,41,"Coma   : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f %6.3f", 
			wfs_results.c1, wfs_results.c2);
	
	}

	pthread_mutex_unlock(&fsm_mutex);

	return NOERROR;

} /* fsm_status() */

/************************************************************************/
/* compute reconstructor()						*/
/*									*/
/* Compute the reconstructor using python, and save as the "working"    */
/* reconstructor version.						*/
/************************************************************************/

int compute_reconstructor(int argc, char **argv)
{
	char command[256], in_filename[256], out_filename[256], s[256];
	int num_modes;

	/* Setup the defaults */

	sprintf(in_filename,"%s%s_actuator_to_sensor.tmp", 
		get_data_directory(s), labao_name);
	sprintf(out_filename,"%s%s_reconstructor.tmp", 
		get_data_directory(s), labao_name);

	/* See which arguments are there. */

	if (argc < 2)
	{
		if (save_actuator_to_sensor(in_filename))
		    return error(ERROR, 
		    "Could not create temporary actuator_to_sensor file %s.",
			in_filename);
		sprintf(command, "compute_reconstructor %s %s",
			in_filename, out_filename);
	}
	else 
	{
		if (sscanf(argv[1], "%d", &num_modes) != 1)
		{
		    return error(ERROR, 
		      "Usage: recon [n_modes] [infile (opt)] [outfile (opt)]");
		}
		if (argc > 2) strcpy(in_filename, argv[2]);
		if (argc > 3) strcpy(out_filename, argv[3]);

		if (save_actuator_to_sensor(in_filename))
		    return error(ERROR, 
		    "Could not create temporary actuator_to_sensor file %s.",
			in_filename);
		sprintf(command, "compute_reconstructor %s %s %d",
			in_filename, out_filename, num_modes);
	}

	/* Run the code */

       /* fprintf(stderr, "python line: %s\n", command); */

	if (system(command))
		return error(ERROR,
			"Failed to run python reconstructor computation.");

	/* Load the reconstructor */

	if (load_reconstructor(out_filename))
		return error(ERROR,
			"Problems loading the temporary reconstructor file.");

	return NOERROR;	

} /* compute_reconstructor() */

/************************************************************************/
/* save reconstructor()							*/
/*									*/
/* Save the temporary reconstructor file to the etc directory or	*/
/* elsewhere 								*/
/************************************************************************/

int save_reconstructor(int argc, char **argv)
{
	char tmp_filename[256], etc_filename[256], command[256], s[256];

	sprintf(tmp_filename, "%s%s_actuator_to_sensor.tmp",
		get_data_directory(s), labao_name);
	sprintf(etc_filename, "%s%s_actuator_to_sensor.dat",
		get_etc_directory(s), labao_name);
	sprintf(command, "cp %s %s", tmp_filename, etc_filename);
	if (system(command))
		return error(ERROR,
		"Could not save actuator to sensor in the etc directory.");

	sprintf(tmp_filename, "%s%s_reconstructor.tmp",
		get_data_directory(s), labao_name);
	sprintf(etc_filename, "%s%s_recon.dat",
		get_etc_directory(s), labao_name);
	sprintf(command, "cp %s %s", tmp_filename, etc_filename);
	if (system(command))
		return error(ERROR,
		"Could not save reconstructor in the etc directory.");

	return NOERROR;

} /* save_reconstructor() */

/************************************************************************/
/* set_flat_dm()                                                        */
/*                                                                      */
/* Set the current DM shape as "flat"                                   */
/************************************************************************/

int set_flat_dm(void)
{
	int i;	
        time_t start;

        start = time(NULL);
        new_aberrations = FALSE;
        while(!new_aberrations)
        {
                if (time(NULL) > start+120) return ERROR;
                message(system_window,"Waiting for new mean DM %d",
                        (int)time(NULL) - start);
                send_labao_text_message("Waiting for new mean DM %d",
                        (int)time(NULL) - start);
        }
        message(system_window,"New DM flat set");
        send_labao_text_message("New DM flat set");

        for (i=0;i<NUM_ACTUATORS;i++) fsm_flat_dm[i] = fsm_mean_dm[i];

	return NOERROR;

} /* set_flat_dm() */

/************************************************************************/
/* call_set_flat_dm()                                                   */
/*                                                                      */
/* Set the current DM shape as "flat"                                   */
/************************************************************************/

int call_set_flat_dm(int argc, char **argv)
{
	set_flat_dm();

	return NOERROR;

} /* call_set_flat_dm() */

/************************************************************************/
/* flatten_wavefront()                                                  */
/*                                                                      */
/* Flatten the wavefront.                                               */
/************************************************************************/

int flatten_wavefront(void)
{
        int i;
        float actuators_out[NUM_ACTUATORS+1];

        for (i=0;i<NUM_ACTUATORS;i++) actuators_out[i+1]=fsm_flat_dm[i];

        if (edac40_set_all_channels(actuators_out)) return -1;

        return NOERROR;

} /* flatten_wavefront() */


/************************************************************************/
/* call_flatten_wavefront()                                             */
/*                                                                      */
/* The callback function for this.                                      */
/************************************************************************/

int call_flatten_wavefront(int argc, char **argv)
{
	if (flatten_wavefront()) 
		return error(ERROR, "Error flattening wavefront.");
	return NOERROR;

} /* call_flatten_wavefront */

/************************************************************************/
/* toggle_ignore_tilt()   	 		    			*/
/*									*/
/* User Callable: toggle ignoring tilt when applying the reconstructor	*/
/************************************************************************/

int toggle_ignore_tilt(int argc, char **argv)
{
	if (fsm_ignore_tilt)
	{
		fsm_ignore_tilt=FALSE;
		message(system_window, 
			"NOT ignoring WFS tilt when applying reconstructor.");
		send_labao_text_message(
			"NOT ignoring WFS tilt when applying reconstructor.");
	}
	else
	{
		fsm_ignore_tilt=TRUE;
		message(system_window, 
			"Ignoring WFS tilt when applying reconstructor.");
		send_labao_text_message(
			"Ignoring WFS tilt when applying reconstructor.");
	}

	return NOERROR;

} /* toggle_ignore_tilt() */

/************************************************************************/
/* dump_dm_offset()							*/
/*									*/
/************************************************************************/

int dump_dm_offset(int argc, char **argv)
{
	char tmp_filename[256], s[256];
	FILE *dumpfile;
	int i;
	sprintf(tmp_filename, "%s%s_dm.tmp", get_data_directory(s), labao_name);
	if ((dumpfile = fopen(tmp_filename,"w")) == NULL ) 
		return error(ERROR, "Error opening file");
	for (i=0;i<NUM_ACTUATORS;i++)
		fprintf(dumpfile, "%6.3f\n", fsm_dm_offset[i]);
	fclose(dumpfile);
	return NOERROR;
}

/************************************************************************/
/* dump_centroid_offsets()						*/
/************************************************************************/

int dump_centroid_offsets(int argc, char **argv)
{
	char tmp_filename[256], s[256];
	FILE *dumpfile;
	int i;
	sprintf(tmp_filename, "%s%s_co.tmp", get_data_directory(s), labao_name);
	if ((dumpfile = fopen(tmp_filename,"w")) == NULL ) 
		return error(ERROR, "Error opening file");
	for (i=0;i<NUM_LENSLETS;i++)
		fprintf(dumpfile, "%6.3f\n", xc[i]);
	for (i=0;i<NUM_LENSLETS;i++)
		fprintf(dumpfile, "%6.3f\n", yc[i]);
	fclose(dumpfile);
	return NOERROR;
}

/************************************************************************/
/* edit_wfs_results_num()						*/
/*									*/
/* Let's you change the bnumber over which results are averaged		*/
/************************************************************************/

int edit_wfs_results_num(int argc, char **argv)
{
        char    s[100];
        int 	n;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&n);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9d", wfs_results_num);
                if (quick_edit("Samples for integration",s,s,NULL,INTEGER)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&n);
        }

	if (n > 0) wfs_results_num = n;

	
	send_labao_text_message("Samples for integration = %d.", n);

	return NOERROR;

} /* edit_wfs_results_num() */

/************************************************************************/
/* start_scope_dichroic_mapping()					*/
/*									*/
/* Starts the automated mapping routine.				*/
/************************************************************************/

int start_scope_dichroic_mapping(int argc, char **argv)
{
	char *args[2], filename[80], data_dir[80];
	/* Sanity check here so we don't get ourselves stuck. */
	if (scope_dichroic_mapping) return error(ERROR, "Already mapping the scope dichroic positions.");

	/* Make sure we start off close to 0 azimuth */
	if ((telescope_status.az > 85) && (telescope_status.az < 275))
	 return error(ERROR, "Telescope azimuth must be within 85 degrees of North to start!");

	sprintf(filename, "%s/%s_dich_map.dat", get_data_directory(data_dir), labao_name);
	if ( (scope_dichroic_mapping_file = fopen(filename, "w")) == NULL ) 
		return error(ERROR, "Could not open dichroic mapping file");

	/* Start with a dichroic alignment */
	args[0] = "adich";
	args[1] = "50"; /* A fixed number of tries.*/
	start_autoalign_scope_dichroic(2, args);
	/* Set the initial azimuth... apart from that it is just a matter of seeting some local globals */
	initial_mapping_az = telescope_status.az;
	scope_dichroic_mapping_step = DICHROIC_MAPPING_ALIGN;
	scope_dichroic_mapping = TRUE;

	return NOERROR;
}


/************************************************************************/
/* start_autoalign_lab_dichroic()					*/
/*									*/
/* Starts the Dichroic Automated alignment routine.			*/
/************************************************************************/

int start_autoalign_lab_dichroic(int argc, char **argv)
{
        char    s[100];

	if (autoalign_scope_dichroic || autoalign_zernike)
		return error(ERROR,"Already running Auto Alignment.");

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&autoalign_count);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9d", DEFAULT_AUTOALIGN_COUNT);
                if (quick_edit("Maximum number of steps",s,s,NULL,INTEGER)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&autoalign_count);
        }

	/* First, let's make sure the pico connection is there */

#warning This stops the PICO server If it is switched off.
	
	if (pico_server != -1) close_server_socket(pico_server);

	if (open_pico_connection(0, NULL) != NOERROR) return ERROR;

	message(system_window,"Lab autoalignment begins Trys = %d",
		autoalign_count);

	/* The servo must not be on. */

	open_servo();

#warning Do we need to add shutter things here? 

	/* We need the reference offsets */

	use_reference = TRUE;

	/* Go */

	autoalign_lab_dichroic = TRUE;

	return NOERROR;

} /* start_autoalign_lab_dichroic() */

/************************************************************************/
/* start_autoalign_scope_dichroic()					*/
/*									*/
/* Starts the Dichroic Automated alignment routine.			*/
/************************************************************************/

int start_autoalign_scope_dichroic(int argc, char **argv)
{
        char    s[100];

	if (autoalign_lab_dichroic || autoalign_zernike)
		return error(ERROR,"Already running Auto Alignment.");

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&autoalign_count);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9d", DEFAULT_AUTOALIGN_COUNT);
                if (quick_edit("Maximum number of steps",s,s,NULL,INTEGER)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&autoalign_count);
	} 

	if (open_telescope_connection(0, NULL) != NOERROR) return ERROR;

	message(system_window,"Scope autoalignment begins Trys = %d",
		autoalign_count);

	autoalign_x_total = 0.0;
	autoalign_y_total = 0.0;
	use_reference = FALSE;
	autoalign_scope_dichroic = TRUE;

	return NOERROR;

} /* start_autoalign_scope_dichroic() */

/************************************************************************/
/* start_autoalign_zernike()						*/
/*									*/
/* Starts the Zernike Automated alignment routine.			*/
/************************************************************************/

int start_autoalign_zernike(int argc, char **argv)
{
        char    s[100];

	if (autoalign_lab_dichroic || autoalign_scope_dichroic)
		return error(ERROR,"Already running Auto Alignment.");

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&autoalign_count);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9d", DEFAULT_AUTOALIGN_COUNT);
                if (quick_edit("Maximum number of steps",s,s,NULL,INTEGER)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&autoalign_count);
	} 

	message(system_window,"Zernike autoalignment begins Trys = %d",
		autoalign_count);

	fsm_state = FSM_CENTROIDS_ONLY;
	use_reference = FALSE;
	call_load_zernike(0, NULL);
	autoalign_zernike = TRUE;

	return NOERROR;

} /* start_autoalign_zernike() */

/************************************************************************/
/* stop_autoalign()							*/
/*									*/
/* Stops the Dichroic Automated alignment routine.			*/
/************************************************************************/

int stop_autoalign(int argc, char **argv)
{
	autoalign_lab_dichroic = FALSE;
	autoalign_scope_dichroic = FALSE;
	autoalign_zernike = FALSE;
	use_reference = FALSE;
    if (scope_dichroic_mapping){
			scope_dichroic_mapping = FALSE;
			fclose(scope_dichroic_mapping_file);
	}

	message(system_window, "Auto alignment has been stopped.");

	return NOERROR;

} /* stop_autoalign() */

/************************************************************************/
/* edit_servo_parameters()						*/
/*									*/
/* Edit gain and damping of servo.					*/
/************************************************************************/

int edit_servo_parameters(int argc, char **argv)
{
        char    s[100]; float	gain, damping;

        /* Check out the command line */

	if (use_servo_flat)
	{
		gain = servo_gain_flat;
		damping = servo_damping_flat;
	}
	else
	{
		gain = servo_gain_delta;
		damping = servo_damping_delta;
	}

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&gain);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", gain);
                if (quick_edit("Servo Gain",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&gain);
        }

	if (gain < 0.0 || gain > 1.0) return 
		error(ERROR,"Gain must be between 0 and 1.");

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&damping);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", damping);
                if (quick_edit("Servo Damping",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&damping);
        }

	if (damping < 0.0 || damping > 1.0) return 
		error(ERROR,"Damping must be between 0 and 1.");

	if (use_servo_flat)
	{
		servo_gain_flat = gain;
		servo_damping_flat = damping;
	}
	else
	{
		servo_gain_delta = gain;
		servo_damping_delta = damping;
	}

	return NOERROR;

} /* edit_servo_parameters() */

/************************************************************************/
/* move_boxes()								*/
/*									*/
/* Move centroid boxes by a small amount.				*/
/************************************************************************/

int move_boxes(int argc, char **argv)
{
        char    s[100];
	float	dx, dy;
	int	i;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&dx);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", 0.1);
                if (quick_edit("dX",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&dx);
        }

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&dy);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", 0.1);
                if (quick_edit("dY",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&dy);
        }

  	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_centroid_offsets[i] += dx;
		y_centroid_offsets[i] += dy;
	}

	return NOERROR;

} /* move_boxes() */

/************************************************************************/
/* edit_zero_clamp()							*/
/*									*/
/* Change value of zero clamp.						*/
/************************************************************************/

int edit_zero_clamp(int argc, char **argv)
{
        char    s[100];
	float	new_clamp;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&new_clamp);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", zero_clamp);
                if (quick_edit("Zero clamp",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_clamp);
        }

	if (new_clamp < 0.0) return error(ERROR,
		"Zero clamp must be >= 0.0");

	zero_clamp = new_clamp;

	return NOERROR;

} /* edit_zero_clamp() */

/************************************************************************/
/* edit_denom_clamp()							*/
/*									*/
/* Change value of denom clamp.						*/
/************************************************************************/

int edit_denom_clamp(int argc, char **argv)
{
        char    s[100];
	float	new_clamp;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&new_clamp);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", denom_clamp);
                if (quick_edit("Denom clamp",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_clamp);
        }

	if (new_clamp < 0.0) return error(ERROR,
		"Zero clamp must be >= 0.0");

	denom_clamp = new_clamp;

	return NOERROR;

} /* edit_denom_clamp() */

/************************************************************************/
/* message_labao_save_aberrations()					*/
/*									*/
/* Allocate memory, setup globals for recording aberrations data.	*/
/************************************************************************/

int message_labao_save_aberrations(struct smessage *mess)
{
	int	num = 0;

        if (mess->length != sizeof(int))
        {
                return error(ERROR,
                "Wrong number of bytes in LABAO_SETUP_ABERRATIONS_RECORD.");
        }

	/* Are we already recording data? */

	if (aberrations_record_num != 0)
	{
		send_labao_text_message(
		"We are already recording aberrations data.");
		return NOERROR;
	}

	num = *((int *)mess->data);

	if (num <= 0) return NOERROR;
	if (num > MAX_ABERRATIONS_RECORD) num = MAX_ABERRATIONS_RECORD;

	pthread_mutex_lock(&fsm_mutex);
	aberrations_record_count = 0;
	aberrations_record_num = num;
	pthread_mutex_unlock(&fsm_mutex);

	send_labao_text_message(
		"Trying to save %d aberration measurements", num);
	message(system_window,  
		"Trying to save %d aberration measurements", num);

	return NOERROR;

}  /* message_labao_save_aberrations() */

/************************************************************************/
/* complete_aberrations_record()					*/
/*									*/
/* Should be called periodically to see if there is a complete set	*/
/* of aberrations data recorded we need to save.			*/
/************************************************************************/

void complete_aberrations_record(void)
{
	int	year, month, day;
	char	filename[345];
	char	s[345];
	FILE	*fp;
	time_t	now;
	struct tm *gmt_now;
	int	i;

	/* Have we finished? */

	pthread_mutex_lock(&fsm_mutex);
	if (aberrations_record_num == 0 ||
	    aberrations_record_count < aberrations_record_num)
	{
		pthread_mutex_unlock(&fsm_mutex);
		return;
	}
	pthread_mutex_unlock(&fsm_mutex);

	/* We have data, let's find a good filename */

	time(&now);
	gmt_now = gmtime(&now);
	year = gmt_now->tm_year + 1900;
	month = gmt_now->tm_mon+1;
	day = gmt_now->tm_mday;
	
	if (year < 1950) year+=100; /* Y2K.... */

	for(i=1; i<1000; i++)
	{
		sprintf(filename,"%s/%4d_%02d_%02d_%s_aberrations_%03d.dat",
			get_data_directory(s), year, month, day, labao_name, i);

		if ((fp = fopen(filename, "r")) == NULL) break;
		fclose(fp);
	}

	if (i > 999) 
	{
		error(ERROR,"Too many aberrations files.");
		return;
	}

	/* OK, save the data */

	if ((fp = fopen(filename, "w")) == NULL)
	{
		error(ERROR, "Failed to open file %s", filename );
		return;
	}

	fprintf(fp,"# FILENAME        : %s\n",filename);
	fprintf(fp,"# GMT YEAR        : %d\n",year);
	fprintf(fp,"# GMT MONTH       : %d\n",month);
	fprintf(fp,"# GMT DAY         : %d\n",day);
	fprintf(fp,"# FSM STATE       : %d\n",aberrations_record[0].fsm_state);
	fprintf(fp,"# DICHROIC        : %s\n",
			dichroic_types[current_dichroic - 1]);
	fprintf(fp,
	"# Time     Tilt X  Tilt Y   Pos X   Pos Y   Focus    A1      A2      C1     C2\n");
	
	for(i=0; i< aberrations_record_num; i++)
		fprintf(fp, "%9d %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4 %7.4f %7.4f\n",
			aberrations_record[i].time_stamp,			
			aberrations_record[i].xtilt,			
			aberrations_record[i].ytilt,			
			aberrations_record[i].xpos, 
			aberrations_record[i].ypos,
			aberrations_record[i].focus,
			aberrations_record[i].a1,
			aberrations_record[i].a2,
			aberrations_record[i].c1,
			aberrations_record[i].c2);

	fclose(fp);
	pthread_mutex_lock(&fsm_mutex);
	aberrations_record_num = 0;
	aberrations_record_count = 0;
	pthread_mutex_unlock(&fsm_mutex);

	send_labao_text_message("Saved file %s", filename);
	message(system_window,  "Saved file %s", filename);

} /* complete_aberrations_record() */

/************************************************************************/
/* call_zero_aberrations()						*/
/*									*/
/* Zero the aberrations to be added to the "zero" position of the WFS.	*/
/************************************************************************/

int call_zero_aberrations(int argc, char **argv)
{
	int i;

	for (i=0;i<NUM_LENSLETS;i++)
	{
		aberration_xc[i]=0.0;
		aberration_yc[i]=0.0;
	}
	
	message(system_window, "Zeroed aberrrations.");
	send_labao_text_message("Zeroed aberrrations.");

	return NOERROR;

} /* call_zero_aberrations() */

/************************************************************************/
/* call_add_wfs_aberration()						*/
/*									*/
/* Add a low-order Zernike term to the aberration.			*/
/************************************************************************/

int call_add_wfs_aberration(int argc, char **argv)
{
	int zernike=0;
	float amplitude=0.0;
	char	s[80];
	
        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&zernike);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9d", 1);
                if (quick_edit("Zernike term to change",s,s,NULL,INTEGER)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&zernike);
        }

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&amplitude);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9f", 0.0);
                if (quick_edit("AMplitude of change",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&amplitude);
        }

	return add_wfs_aberration(zernike, amplitude);

} /* call_add_wfs_aberration() */

/************************************************************************/
/* add_wfs_aberration()							*/
/*									*/
/* Add a low-order Zernike term to the aberration.			*/
/* Returns error level.							*/
/************************************************************************/

int add_wfs_aberration(int zernike, float amplitude)
{
	int	i;
	float yy, xx;

#warning Have a close look at the normalization of this compared to calculation
	switch (zernike)
	{
	    case 0:
	    case 1:
			return error(ERROR, 
			   "WFS can't measure piston. zernikes start at 2");	
	    case 2:
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += (amplitude * CENTROID_BOX_WIDTH/2.0);
		}
		break;

	    case 3:
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_yc[i] += (amplitude * CENTROID_BOX_WIDTH/2.0);
		}
		break;

	    case 4: /* Focus */
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    /* The 0.778 ensures we get the right amount */

		    aberration_xc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				(0.778*max_offset);
		    aberration_yc[i] += amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				(0.778*max_offset);
		}
		break;

	    case 5: /* a1 (first astigmatism) term */
			for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				(0.778*max_offset);
		    aberration_yc[i] -= amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				(0.778*max_offset);
		}
		break;

	    case 6: /* a2 (second astigmatism) term */
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				(0.778*max_offset);
		    aberration_yc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				(0.778*max_offset);
		}
		break;

	    case 7: /* c1 (first coma) term */
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    xx = (x_centroid_offsets[i] - x_mean_offset)/
				(0.778*max_offset);
		    yy = (y_centroid_offsets[i] - y_mean_offset)/
				(0.778*max_offset);
		    aberration_xc[i] += amplitude *
			(9*xx*xx + 3*yy*yy - 2)/9.0;
		    aberration_yc[i] += amplitude *
			2*xx*yy/3.0;
		}
		break;

	    
	    case 8: /* c2 (second coma) term */
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    xx = (x_centroid_offsets[i] - x_mean_offset)/
				(0.778*max_offset);
		    yy = (y_centroid_offsets[i] - y_mean_offset)/
				(0.778*max_offset);
		    aberration_xc[i] += amplitude *
			2*xx*yy/3.0;
		    aberration_yc[i] += amplitude *
			(3*xx*xx + 9*yy*yy - 2)/9.0;;
		}
		break;

	    default:
			return error(ERROR, "Zernike term out of range!");		

	}

	message(system_window, 
			"Added Zernike %d magnitude %.3f to aberrrations.",
			zernike, amplitude);
	send_labao_text_message(
			"Added Zernike %d magnitude %.3f to aberrrations.",
			zernike, amplitude);

	return NOERROR;

} /* add_wfs_aberration() */

/************************************************************************/
/* toggle_use_reference()						*/
/*									*/
/* Toggles global use_reference. On for when looking at reference	*/
/* mirror.								*/
/************************************************************************/

int toggle_use_reference(int argc, char **argv)
{
	use_reference = !use_reference;

	if (use_reference)
	{
		message(system_window, 
			"Using reference mirror offsets.");
		send_labao_text_message(
			"Using reference mirror offsets.");
	}
	else
	{
		message(system_window, 
			"Not using reference mirror offsets.");
		send_labao_text_message(
			"Not using reference mirror offsets.");
	}

	return NOERROR;

} /* toggle_use_reference() */

/************************************************************************/
/* set_reference_now()							*/
/*									*/
/* Tells system to set current position to reference position.		*/
/************************************************************************/

int set_reference_now(int argc, char **argv)
{
	set_reference = TRUE;

	message(system_window, 
		"Reference mirror offsets set to current position.");
	send_labao_text_message(
		"Reference mirror offsets set to current position.");

	return NOERROR;

} /* set_reference_now() */

/************************************************************************/
/* toggle_use_servo_flat()						*/
/*									*/
/* Switch between servo types.						*/
/************************************************************************/

int toggle_use_servo_flat(int argc, char **argv)
{
	use_servo_flat = !use_servo_flat;

	if (use_servo_flat)
	{
	    message(system_window, "Using servo FLAT.");
	    send_labao_text_message( "Using servo FLAT.");
	}
	else
	{
	    message(system_window, "Using servo DELTA.");
	    send_labao_text_message( "Using servo DELTA.");
	}

	return NOERROR;

} /* toggle_use_servo_flat() */

/************************************************************************/
/* select_dichroic().							*/
/*									*/
/* Attempts to select a dichroic.					*/
/************************************************************************/

int select_dichroic(int argc, char **argv)
{
	int	dich;
	struct smessage mess;

        /* Check out the command line */

        clean_command_line();
        if (argc > 1)
        {
                for (dich=0; dich<3; dich++)
                {
                        if (strcmpi(argv[1],dichroic_types[dich]) == 0)
                        {
                                break;
                        }
                }

                if (dich >= 3)
                {
                        error(ERROR,"Unkown dichroic %s.",argv[1]);
                        return -2;
                }
        }
        else
        {
		dich = current_dichroic - 1;
                if (quick_edit("dichroic mirror",dichroic_types[dich],
                        &dich,dichroic_types,ENUMERATED) == KEY_ESC)
                                return NOERROR;
	}

	switch(dich+1)
	{
		case AOB_DICHROIC_GRAY: fsm_flat_dm = fsm_flat_dm_gray;
					current_dichroic = AOB_DICHROIC_GRAY;
					break;

		case AOB_DICHROIC_SPARE: fsm_flat_dm = fsm_flat_dm_spare;
					current_dichroic = AOB_DICHROIC_SPARE;
					break;

		case AOB_DICHROIC_YSO: fsm_flat_dm = fsm_flat_dm_yso;
					current_dichroic = AOB_DICHROIC_YSO;
					break;

		default: return error(ERROR,"Unknown dichroic.");
	}

	mess.type = HUT_AOB_CHANGE_DICHROIC;
	mess.length = sizeof(int);
	mess.data = (unsigned char *)&current_dichroic;

	if (!send_message(telescope_server, &mess))
	{
		return error(ERROR,"Failed ot send DICHROIC message to scope.");
	}

	return NOERROR;

} /* select_dichroic() */

/************************************************************************/
/* message_aob_change_dichroic() 					*/
/*									*/
/************************************************************************/

int message_aob_change_dichroic(struct smessage *message)
{
	int	data;
	char	*argv[2];

	if (message->length != sizeof(int))
	{
		return error(ERROR,"Got AOB_CHANGE_DICHROIC with wrong data.");
	}

	data = *((int *)message->data);

	if (data < 1 || data > 3)
		return error(ERROR,"Got non-existant Dichroic");

	argv[0] = "dich";
	argv[1] = dichroic_types[data-1];

	return select_dichroic(2, argv);

} /* message_aob_change_dichroic() */

