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
#define BEACON_FOCUS_LIMIT 	0.05
#define BEACON_FOCUS_STEP  	10000
#define BEACON_FOCUS_GAIN   	100000

/* Optionally enforce zero piston. With SERVO_DAMPING < 1 and a reasonable reconstructor,
   this shouldn't be needed */
#define ENFORCE_ZERO_PISTON

/* Centroid calculation number */

#define CENTROID_HW (CENTROID_WINDOW_WIDTH/2)
#define CLEAR_EDGE ((CENTROID_BOX_WIDTH/2) + (CENTROID_WINDOW_WIDTH/2))
#warning WE NEED TO THINK ABOUT THESE NUMBERS
#define ZERO_CLAMP 15
#define DENOM_CLAMP 30
#define FLUX_MEMORY 0.9
//#define FLUX_MEMORY 0.0

static const float centroid_window[][CENTROID_WINDOW_WIDTH] = 
	{{0,1,1,1,0}, 
	 {1,1,1,1,1},
	 {1,1,1,1,1},
	 {1,1,1,1,1},
	 {0,1,1,1,0}};

static float centroid_xw[CENTROID_WINDOW_WIDTH][CENTROID_WINDOW_WIDTH];
static float centroid_yw[CENTROID_WINDOW_WIDTH][CENTROID_WINDOW_WIDTH];
                         
static char *dichroic_types[] = {"GRAY ", "SPARE", "YSO  ", NULL};

#define NUM_ABERRATIONS	9
static char *aberration_types[NUM_ABERRATIONS+1] = { "NULL", "NULL", 
	"Tilt X", "Tilt Y", "Focus ", 
	"Astig1", "Astig2", "Coma 1", "Coma 2", NULL};
		
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
static float fsm_actuator_to_aberration[NUM_ACTUATORS][NUM_ABERRATIONS];
static float fsm_xc_int[NUM_LENSLETS];
static float fsm_yc_int[NUM_LENSLETS];
static int fsm_cyclenum=0;
static float fsm_flat_dm_yso[NUM_ACTUATORS];
static float fsm_flat_dm_gray[NUM_ACTUATORS];
static float fsm_flat_dm_spare[NUM_ACTUATORS];
static float fsm_mean_dm[NUM_ACTUATORS];
static float fsm_calc_mean_dm[NUM_ACTUATORS];
static float fsm_dm_offset[NUM_ACTUATORS];
static float fsm_dm_sum[NUM_ACTUATORS];
static float max_dm_sum = SERVO_SUM_MAX;
static float fsm_dm_last_delta[NUM_ACTUATORS];
static pthread_mutex_t fsm_mutex = PTHREAD_MUTEX_INITIALIZER;
static int actuator=1;
static int wfs_results_num = ((int)DEFAULT_FRAME_RATE);
static int wfs_results_n = 0;
static bool autoalign_lab_dichroic = FALSE;
static bool autoalign_scope_dichroic = FALSE;
static bool autoalign_beacon_focus = FALSE;

static bool scope_dichroic_mapping = FALSE;
static float target_telescope_az = 90.0;
static float initial_mapping_az = 90.0;
#define DICHROIC_MAPPING_ALIGN  0
#define DICHROIC_MAPPING_ROTATE 1
static int scope_dichroic_mapping_step = DICHROIC_MAPPING_ALIGN;
FILE *scope_dichroic_mapping_file;

static float current_total_flux = 0.0;
static bool autoalign_zernike = FALSE;
static int autoalign_count = 0;
static int autoalign_x_total = 0;
static int autoalign_y_total = 0;
static float zero_clamp = ZERO_CLAMP;
static float denom_clamp = DENOM_CLAMP;
static bool set_center = FALSE;
static bool new_aberrations = FALSE;
static int aberrations_record_num = 0;
static int aberrations_record_count = 0;
static struct s_labao_wfs_results aberrations_record[MAX_ABERRATIONS_RECORD];
static float aberration_xc[NUM_LENSLETS];
static float aberration_yc[NUM_LENSLETS];
static bool use_reference = FALSE;
static bool last_coude_dichroic_corrections = FALSE;
static bool use_coude_dichroic_corrections = FALSE;
static float coude_dichroic_correction_x_amp = 0.0;
static float coude_dichroic_correction_x_phase = 0.0;
static float coude_dichroic_correction_y_amp = 0.0;
static float coude_dichroic_correction_y_phase = 0.0;
static float last_coude_correction_az = -1.0;
static float dm_impulse = 0.0;
static float	mean_dm_error, mean_dm_offset;
static int current_dichroic = AOB_DICHROIC_SPARE;
static struct s_labao_wfs_results calc_labao_results;

/* Globals. */

int fsm_state = FSM_CENTROIDS_ONLY;
float x_centroid_offsets_beacon[NUM_LENSLETS];
float y_centroid_offsets_beacon[NUM_LENSLETS];
float x_centroid_offsets_reference[NUM_LENSLETS];
float y_centroid_offsets_reference[NUM_LENSLETS];
float *x_centroid_offsets;
float *y_centroid_offsets;
float xc[NUM_LENSLETS];
float yc[NUM_LENSLETS];
float avg_fluxes[NUM_LENSLETS];
struct s_labao_wfs_results wfs_results;
float fsm_reconstructor[NUM_ACTUATORS][2*NUM_LENSLETS];
float zernike_reconstructor[NUM_ACTUATORS][2*NUM_LENSLETS];
float calculated_zernike[NUM_ACTUATORS]; /* Starts at 0 not 1 */
float fsm_actuator_to_sensor[NUM_ACTUATORS][2*NUM_LENSLETS];
float x_mean_offset=0.0, y_mean_offset=0.0, max_offset=1.0, min_doffset = 0.0;
float *fsm_flat_dm;

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

	for (i=0; i<NUM_ACTUATORS; i++)
	{
	    calculated_zernike[i] = 0;
	    for (j=0; j<2*NUM_LENSLETS; j++)
	    {
		fsm_reconstructor[i][j]=0.0; 
		zernike_reconstructor[i][j]=0.0; 
		fsm_actuator_to_sensor[i][j]=0.0;
	    }
	    for (j=0; j<NUM_ABERRATIONS; j++)
		fsm_actuator_to_aberration[i][j]=0.0;
	}
		

	for (i=0;i<NUM_ACTUATORS;i++)
	{
		fsm_flat_dm_yso[i]=DM_DEFAULT;
		fsm_flat_dm_gray[i]=DM_DEFAULT;
		fsm_flat_dm_spare[i]=DM_DEFAULT;
		fsm_mean_dm[i]=DM_DEFAULT;
		fsm_calc_mean_dm[i]=DM_DEFAULT;
		fsm_dm_offset[i]=0.0;	
		fsm_dm_sum[i]=0.0;	
		fsm_dm_last_delta[i]=0.0;	
	}
	fsm_flat_dm = fsm_flat_dm_spare;
	current_dichroic = AOB_DICHROIC_SPARE;

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_centroid_offsets_beacon[i]=0.0;
		y_centroid_offsets_beacon[i]=0.0;
		x_centroid_offsets_reference[i]=0.0;
		y_centroid_offsets_reference[i]=0.0;
		aberration_xc[i]=0.0;
		aberration_yc[i]=0.0;
		avg_fluxes[i] = 0.0;
	}
	x_centroid_offsets = x_centroid_offsets_beacon;
	y_centroid_offsets = y_centroid_offsets_beacon;

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
/* offsets. Should be called whenever centroid offsets are changed.     */
/************************************************************************/

void compute_pupil_center(void)
{
	int i,j ;
	float x_offset, y_offset, offset;

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
		offset = sqrt(x_offset*x_offset + y_offset*y_offset);

#warning Talk to MIKE about this
		//if (fabs(x_offset) > max_offset) max_offset = fabs(x_offset);
		//if (fabs(y_offset) > max_offset) max_offset = fabs(y_offset);
		if (offset > max_offset) max_offset = offset;
	}

	/* Find the minimum separration between subapertures */

	min_doffset = 1e32;
	for(i=0; i<NUM_LENSLETS-1; i++)
	{
	    for(j=i+1; j<NUM_LENSLETS; j++)
	    {
		x_offset = x_centroid_offsets[i] - x_centroid_offsets[j];
		y_offset = y_centroid_offsets[i] - y_centroid_offsets[j];
		offset = sqrt(x_offset*x_offset + y_offset*y_offset);
		if (offset < min_doffset) min_doffset = offset;
	    }
	}

	/* This will change the Rho and Theta of these subapertures */

	compute_centroid_offset_rho_theta();

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
	float xoff_beacon, yoff_beacon;
	float xoff_reference, yoff_reference;
	int	retval = 0;

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

  	if (GetDataLine(s, 256, params_file) == -1)
	{
  		fclose(params_file);
  		return -1;
  	}

	/* Get the default area of interest. */

  	if (sscanf(s, "%d %d %d %d", &x, &y, &dx, &dy) != 4) return -2;

	/* Get the default X/Y positions of the centroids */

  	for (i=0;i<NUM_LENSLETS;i++)
	{
		if (GetDataLine(s, 256, params_file) == -1)
		{
			fclose(params_file);
			return -3;
		}

		if (sscanf(s, "%f %f %f %f", &xoff_beacon, &yoff_beacon,
			&xoff_reference, &yoff_reference) != 4)
		{
			fclose(params_file);
			return -4;
		}

		/* Sanity check these numbers */

		if ((xoff_beacon < CLEAR_EDGE) || 
		    (yoff_beacon < CLEAR_EDGE) || 
		    (xoff_beacon >= dx-CLEAR_EDGE) || 
		    (yoff_beacon >= dy-CLEAR_EDGE)) retval =  -8;

		if ((xoff_reference < CLEAR_EDGE) || 
		    (yoff_reference < CLEAR_EDGE) || 
		    (xoff_reference >= dx-CLEAR_EDGE) || 
		    (yoff_reference >= dy-CLEAR_EDGE)) retval =  -8;

		x_centroid_offsets_beacon[i] = xoff_beacon;
		y_centroid_offsets_beacon[i] = yoff_beacon;
		x_centroid_offsets_reference[i] = xoff_reference;
		y_centroid_offsets_reference[i] = yoff_reference;
	}

	/* Compute the new pupil center and scale. */

	compute_pupil_center();

	/* Load the "flat" wavefront definition */
	
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		if (GetDataLine(s, 256, params_file) == -1)
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

	if (GetDataLine(s, 256, params_file) == -1)
	{
		fclose(params_file);
		return -10;
	}

	if (sscanf(s, "%f %f", &xpos_center, &ypos_center) != 2)
	{
		fclose(params_file);
		return -10;
	}

        /* Load the coude/dichroic correction values */

        if (GetDataLine(s, 256, params_file) == -1)
        {
                fclose(params_file);
                return -11;
        }

        if (sscanf(s, "%f %f %f %f",
                &coude_dichroic_correction_x_amp,
                &coude_dichroic_correction_x_phase,
                &coude_dichroic_correction_y_amp,
                &coude_dichroic_correction_y_phase) != 4)
        {
                fclose(params_file);
                return -11;
        }

	/* Set a flat wavefront. */
	
	if (flatten_wavefront()) retval -9;

	/* 
  	 * Try to set the AOI.
	 */

	if (set_usb_camera_aoi(FALSE, x, y, dx, dy)) retval -5;

	/* Tell clients */

	send_labao_value_all_channels(TRUE);

	fclose(params_file);

	return retval;

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

		case -6: return 
	error(ERROR,"Failed to get actuator positions for flat wavefront.");
		
		case -7: return 
	error(ERROR,"Failed to get interpret positions for flat wavefront.");

		case -8: return error(ERROR,"Boxes are too close to edge.");

		case -9: return error(ERROR,"Failed to send flat to DM.");

		case -10: return error(ERROR,"Failed to read center position.");

		case -11: return error(ERROR,
                    "Failed to read Coude/Dichroic corrections.");

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

	fprintf(params_file, 
		"# Default Lenslet X/Y positions. Beacon then Reference\n");
  	for (i=0;i<NUM_LENSLETS;i++)
	{
		fprintf(params_file, "%7.2f %7.2f %7.2f %7.2f\n",
			x_centroid_offsets_beacon[i], 
			y_centroid_offsets_beacon[i],
			x_centroid_offsets_reference[i],
			y_centroid_offsets_reference[i]);
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

        /* Save Coude/Dichroic correction values */

        fprintf(params_file,
                "# Coude/Dichroic Correction X Amp/Phase Y Amp/Phase\n");
        fprintf(params_file, "%.2f %.2f %.2f %.2f\n",
                coude_dichroic_correction_x_amp,
                coude_dichroic_correction_x_phase,
                coude_dichroic_correction_y_amp,
                coude_dichroic_correction_y_phase);

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
	char filename[256], s[1024];
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

  	/* Save the current matrix */

	fprintf(params_file, "# Actuator to sensor matrix %s\n", labao_name);
	fprintf(params_file, "# One line of sensor movement per actuator \n");
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		for (j=0;j<2*NUM_LENSLETS;j++)
			fprintf(params_file, "%10.3e ", 
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
/* save_actuator_to_aberration()					*/
/*									*/
/* Save the actuator to aberration matrix, used to create the 		*/
/* reconstructor 							*/ 
/************************************************************************/

int save_actuator_to_aberration(char* filename_in)
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
		sprintf(filename,"%s%s_actuator_to_aberration.dat", 
			get_data_directory(s), labao_name);
        }

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"w")) == NULL ) return -1;

  	/* Save the current matrix and save this */

	fprintf(params_file, "# Actuator to aberration matrix %s\n", 
		labao_name);
	fprintf(params_file,"# One line of aberration movement per actuator\n");
	for (i=0;i<NUM_ACTUATORS;i++)
	{
		for (j=0;j<NUM_ABERRATIONS;j++)
			fprintf(params_file, "%10.3e ", 
				fsm_actuator_to_aberration[i][j]);
		fprintf(params_file, "\n");
	}

	/* That is all. */

	fclose(params_file);

	return 0;
	
} /* save_actuator_to_aberration() */

/************************************************************************/
/* call_save_actuator_to_aberration()					*/
/*									*/
/* User callable version of save_actuator_to_aberration.		*/
/************************************************************************/

int call_save_actuator_to_aberration(int argc, char **argv)
{
	if (argc > 1)
	{
		if (save_actuator_to_aberration(argv[1])) 
			return error (ERROR,
				"Error saving actuator to aberration matrix.");
	}
	else 
	{
		if (save_actuator_to_aberration(NULL)) 
			return error (ERROR,
				"Error saving actuator to aberration matrix.");
	}

	message(system_window, "Acuator to aberration matrix succesfully saved!");
	send_labao_text_message("Acuator to aberration matrix succesfully saved!");

	return NOERROR;

}	/* call_save_actuator_to_aberration() */

/************************************************************************/
/* run_centroids_and_fsm()						*/
/*									*/
/* The callback function that computes centroids and runs the FSM.	*/
/************************************************************************/

void run_centroids_and_fsm(CHARA_TIME time_stamp,
		float **data, int aoi_width, int aoi_height)
{
	int i,j,l,left,bottom,box_left, box_bottom, poke_sign;
	float last_servo_gain = 0.0;
	float new_xc[NUM_LENSLETS];
	float new_yc[NUM_LENSLETS];
	float max;
	float fluxes[NUM_LENSLETS];
	float new_dm[NUM_ACTUATORS+1];
	float fsm_dm_error[NUM_ACTUATORS];
	float fsm_dm_delta[NUM_ACTUATORS];
	int x,y,dx,dy;
	bool is_new_actuator=TRUE;
	float outer_dm_mean = 0.0;
	float x_offset = 0.0, y_offset = 0.0, total_flux = 0.0;
	float	xtilt = 0.0, ytilt = 0.0, focus = 0.0;
	float   a1 = 0.0, a2 = 0.0, c1=0.0, c2=0.0;
	float	xpos = 0.0, ypos = 0.0;
	float	xwfs, ywfs, theta, sintheta, costheta;

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
#warning Should this be avg_fluxes[l] instead??
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

	/* 
 	 * Add aberration and save the centroids and fluxes to our globals
 	 */

	pthread_mutex_lock(&fsm_mutex);
	for (l=0;l<NUM_LENSLETS;l++)
	{
		xc[l] = (new_xc[l] -= aberration_xc[l]);
		yc[l] = (new_yc[l] -= aberration_yc[l]);
		avg_fluxes[l] = fluxes[l] * (1.0 - FLUX_MEMORY) + 
				FLUX_MEMORY*avg_fluxes[l];
	}
	pthread_mutex_unlock(&fsm_mutex);

	/* Compute the Zernike terms */

	for (i=0;i<NUM_ACTUATORS;i++)
	{
	    calculated_zernike[i] = 0.0;
	    for (l=0;l<NUM_LENSLETS;l++)
	    {
		calculated_zernike[i] += 
			(zernike_reconstructor[i][l] * new_xc[l] +
			zernike_reconstructor[i][NUM_LENSLETS+l] * new_yc[l]);
	    }
	}

	/* Compute the tiptilt, focus, astigmatism, and coma terms */

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_offset = x_centroid_offsets[i] - x_mean_offset;
		y_offset = y_centroid_offsets[i] - y_mean_offset;

		xtilt += new_xc[i];
		ytilt += new_yc[i];

		total_flux += avg_fluxes[i];

		/* To compute the pupil position, reference everything
		to the center of the pupil. */

		xpos += x_offset * avg_fluxes[i];
		ypos += y_offset * avg_fluxes[i];

#warning This is not normalised correctly. Should give 1 for e.g. peak to valley slopes of 1 arcsec.

		focus += (new_xc[i]*x_offset + new_yc[i]*y_offset);
		a1 +=    (new_xc[i]*x_offset - new_yc[i]*y_offset);
		a2 +=    (new_yc[i]*x_offset + new_xc[i]*y_offset);
		c1 +=  new_xc[i] *
			(9*x_offset*x_offset + 3*y_offset*y_offset-2)/9.0 + 
			new_yc[i]*2*x_offset*y_offset/3.0;
		c2 +=  new_yc[i] *
			(9*y_offset*y_offset + 3*x_offset*x_offset-2)/9.0 + 
			new_xc[i]*2*x_offset*y_offset/3.0;
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
	
	/* Calculate the position.... this doesn't work well */

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

	/* Reset the integral if needed */

	if (fsm_cyclenum==0)
	{
		for (l=0;l<NUM_LENSLETS;l++)
		{
			fsm_xc_int[l]=0.0;
			fsm_yc_int[l]=0.0;
		}			
	}

	/* Compute the integral */

	for (l=0;l<NUM_LENSLETS;l++)
	{
		fsm_xc_int[l] += new_xc[l];
		fsm_yc_int[l] += new_yc[l];
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
#warning Added Zernike matrix here as well.
		if (fsm_cyclenum == 0)
		{
		    for (i=0;i<NUM_ACTUATORS;i++)
		    {
		        for (l=0;l<NUM_LENSLETS;l++)
		        {
		    	    fsm_actuator_to_sensor[i][l] = 0.0;
		    	    fsm_actuator_to_sensor[i][NUM_LENSLETS + l] = 0.0;
		        }
		        for (l=0;l<NUM_ABERRATIONS;l++)
		    	    fsm_actuator_to_aberration[i][l] = 0.0;
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
		    {
		    	for (l=0;l<2*NUM_LENSLETS;l++) 
			    fsm_actuator_to_sensor[i][l] /=
			    (2*ACTUATOR_POKE*NUM_RECON_AVG*NUM_RECON_CYCLES);
		        for (l=0;l<NUM_ABERRATIONS;l++)
		    	    fsm_actuator_to_aberration[i][l] /= 
			    (2*ACTUATOR_POKE*NUM_RECON_AVG*NUM_RECON_CYCLES);
		    }
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
		    fsm_actuator_to_aberration[actuator-1][0] += 0;
		    fsm_actuator_to_aberration[actuator-1][1] += 0;
		    fsm_actuator_to_aberration[actuator-1][2] += xtilt;
		    fsm_actuator_to_aberration[actuator-1][3] += ytilt;
		    fsm_actuator_to_aberration[actuator-1][4] += focus;
		    fsm_actuator_to_aberration[actuator-1][5] += a1;
		    fsm_actuator_to_aberration[actuator-1][6] += a2;
		    fsm_actuator_to_aberration[actuator-1][7] += c1;
		    fsm_actuator_to_aberration[actuator-1][8] += c2;
		}
		break;
	
	case FSM_APPLY_RECON_ONCE: /* Run the servo loop once with gain=1.0 */
		last_servo_gain = servo_gain;
		servo_gain = 1.0;
		fsm_cyclenum = 0;

	case FSM_SERVO_LOOP:

		mean_dm_error = 0.0;
		mean_dm_offset = 0.0;
		for (i=0;i<NUM_ACTUATORS;i++)
		{
 		    /*
 		     * Here fsm_cyclenum will only ever be 0 when
 		     * the servo is first switched on.
 		     * If this is the first time, we set a bunch of things
 		     * to zero.
 		     */

		    if(fsm_cyclenum == 0)
		    {
			fsm_dm_offset[i] = 0.0;
			fsm_dm_sum[i] = 0.0;
			fsm_dm_last_delta[i] = 0.0;
		    }

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

		    /* Apply the reconstructor and get the delta for the DM */

		    fsm_dm_error[i] = 0.0;
		    for (l=0;l<NUM_LENSLETS;l++)
		    {
			fsm_dm_error[i] -= 
				(fsm_reconstructor[i][l] *
				(new_xc[l] - xtilt) +
				fsm_reconstructor[i][NUM_LENSLETS+l] *
				(new_yc[l] - ytilt));
		    }
		    mean_dm_error += fsm_dm_error[i];

		    /* 
 		     * The most likely wavefront is flat, 
 		     * so damp the DM position to flat.
 		     * If this memory is 1.0 this behaves like a standard
 		     * servo. If it is less than one things always relax
 		     * towards the "flat" DM.
 		     */

		    if (fsm_state == FSM_SERVO_LOOP)
			fsm_dm_offset[i] *= servo_memory;

		    /* Now apply the servo to the error, adding impulse  */

		    fsm_dm_delta[i] = servo_gain * fsm_dm_error[i] -
			servo_damping * fsm_dm_last_delta[i] +
			servo_integration * fsm_dm_sum[i] +
			dm_impulse;

		    /* We will need these for the next round */

		    fsm_dm_last_delta[i] = fsm_dm_delta[i];
		    fsm_dm_sum[i] += fsm_dm_error[i];
    
    		    if (fabs(fsm_dm_sum[i]) > max_dm_sum)
		    {
			fsm_dm_sum[i] = max_dm_sum * 
					fsm_dm_sum[i]/fabs(fsm_dm_sum[i]);
		    }

		    /* We now have a final offset from the flat */

		    fsm_dm_offset[i] += fsm_dm_delta[i];
		    mean_dm_offset += fsm_dm_offset[i];

#ifdef ENFORCE_ZERO_PISTON
		    if (i >= NUM_INNER_ACTUATORS)
			outer_dm_mean += fsm_dm_offset[i];
#endif
		}
		mean_dm_error /= ((float)NUM_ACTUATORS);
		mean_dm_offset /= ((float)NUM_ACTUATORS);

		/* The impulse should only happen once */

		dm_impulse = 0.0;

		/* Work out average on the edge */

#ifdef ENFORCE_ZERO_PISTON
		outer_dm_mean /= (NUM_ACTUATORS - NUM_INNER_ACTUATORS);
#endif

		for (i=0;i<NUM_ACTUATORS;i++)
		{
			/* Add in the flat */

		        new_dm[i+1] = fsm_flat_dm[i] + fsm_dm_offset[i];
		
#ifdef ENFORCE_ZERO_PISTON
			/* Remove any piston */

			if (i >= NUM_INNER_ACTUATORS)
				new_dm[i+1] -= outer_dm_mean;
#endif
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
			servo_gain = last_servo_gain;
		}
		break;

		default: error(FATAL,
			"We should never reach this point in the switch");
	}

	/* OK, next cycle */

	fsm_cyclenum++;

	/* Unlock the Mutex */
 
	pthread_mutex_unlock(&fsm_mutex);

	/* And now we send the tiptilt away */

	theta = M_PI*(telescope_status.az - 65.5)/180.0;
	sintheta = sin(theta);
	costheta = cos(theta);
	xwfs = xtilt * costheta + ytilt * sintheta;
	ywfs = xtilt * sintheta - ytilt * costheta;

	send_labao_tiptilt_data(xwfs, ywfs);

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
		aberrations_record[aberrations_record_count].use_reference = 
							use_reference;
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
		wfs_results.use_reference = use_reference;

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
	float	flux_mean, flux_stddev, flux_min, flux_max;
	int	i;

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
			wprintw(status_window, " Servo ON   ");
		default:
			wprintw(status_window, "UNKNOWN");
	}

	wstandout(status_window);
	mvwaddstr(status_window,3,20,"IntN    : ");
	wstandend(status_window);
	wprintw(status_window, "%9d", wfs_results_num);

	wstandout(status_window);
	mvwaddstr(status_window,4,20,"Beam/TT : ");
	wstandend(status_window);
	if (use_reference)
		sprintf(s, "%6s", "Ref");
	else
		sprintf(s, "%6s", "Beacon");

	if (send_tiptilt)
		strcat(s,"/ON ");
	else
		strcat(s,"/OFF ");
	wprintw(status_window, s );

	/* Check on the scope_dichroic_mapping function */

	if (scope_dichroic_mapping) 
	{
	    if ( (scope_dichroic_mapping_step == DICHROIC_MAPPING_ALIGN) &&
		 (!autoalign_scope_dichroic))
	    {
		/* Have we moved all the way around? */

		if (fmod(telescope_status.az - initial_mapping_az + 
					15  + 360,360) < 10.0)
		{
		    scope_dichroic_mapping = FALSE;
		    use_coude_dichroic_corrections =
			last_coude_dichroic_corrections;
		    fclose(scope_dichroic_mapping_file);
		    message(system_window,"Dichroic mapping complete!");
		    send_labao_text_message("%s", "Dichroic mapping complete!");
		} 
		else 
		{
		    /* Write out the autoalign total from the last alignment */
	
		    fprintf(scope_dichroic_mapping_file,"1 %5.1f %8d %8d\n",
			    telescope_status.az, autoalign_x_total,
			    autoalign_y_total);
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
	         (fabs(fmod(telescope_status.az - 
			target_telescope_az+180+360,360)-180)<0.1) )
	    {
		/* 
 		 * This is only a rough sanity check... !!!
  		 * the number should be checked
  		 */

		if (current_total_flux < 200)
		{
		    scope_dichroic_mapping = FALSE;
		    use_coude_dichroic_corrections =
			last_coude_dichroic_corrections;
		    fclose(scope_dichroic_mapping_file);
		    message(system_window,
			"Giving up on scope dichroic mapping - low flux.");
		    send_labao_text_message("%s",
			"Giving up on scope dichroic mapping - low flux.");
		} 
		else
		{
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
				pthread_mutex_unlock(&fsm_mutex);
				use_reference_off();
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
				use_reference_off();
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
			last_coude_correction_az = telescope_status.az;
			pthread_mutex_unlock(&fsm_mutex);
			return NOERROR;
		    }

		    /* Have we tried too many times? */

		    if (--autoalign_count < 0)
		    {
			autoalign_scope_dichroic = FALSE;
			autoalign_count = 0;
			if (scope_dichroic_mapping)
			{
			    scope_dichroic_mapping = FALSE;
			    use_coude_dichroic_corrections =
				last_coude_dichroic_corrections;
			    fclose(scope_dichroic_mapping_file);
			    message(system_window,
		"Giving up on scope dichroic mapping - autoalignment fail.");
			    send_labao_text_message("%s",
		"Giving up on scope dichroic mapping - autoalignment fail.");
			}
			else
			{
			    message(system_window,
					"Giving up on scope autoalignment.");
			    send_labao_text_message("%s",
					"Giving up on scope autoalignment.");
			}
			fprintf(scope_dichroic_mapping_file,
			    "0 %5.1f %8d %8d\n", 
			    telescope_status.az, autoalign_x_total, 
			    autoalign_y_total);
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
			    if (include_old_S2_code && this_labao == S2)
			          motor_move.motor = AOB_S2_DICHR_2;
			    else
			          motor_move.motor = AOB_DICHR_2;
			    motor_move.position = SCOPE_ALIGN_STEP + 
				(int)(fabs(x)*SCOPE_ALIGN_GAIN);
			    autoalign_x_total += SCOPE_ALIGN_STEP + 
				(int)(fabs(x)*SCOPE_ALIGN_GAIN);
			    send_message(telescope_server, &mess);
			    strcat(s," Moving RIGHT");
			}
			else if (x < -1.0*SCOPE_ALIGN_LIMIT)
			{
			    if (include_old_S2_code && this_labao == S2)
			          motor_move.motor = AOB_S2_DICHR_2;
			    else
			          motor_move.motor = AOB_DICHR_2;
			    motor_move.position = -1.0*(SCOPE_ALIGN_STEP  + 
				(int)(fabs(x)*SCOPE_ALIGN_GAIN));
			    autoalign_x_total -= SCOPE_ALIGN_STEP  + 
				(int)(fabs(x)*SCOPE_ALIGN_GAIN);
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
			    if (include_old_S2_code && this_labao == S2)
			        motor_move.motor = AOB_S2_DICHR_1;
			    else
			        motor_move.motor = AOB_DICHR_1;
			    motor_move.position = SCOPE_ALIGN_STEP + 
				(int)(fabs(y)*SCOPE_ALIGN_GAIN);
			    autoalign_y_total += SCOPE_ALIGN_STEP + 
				(int)(fabs(y)*SCOPE_ALIGN_GAIN); 
			    send_message(telescope_server, &mess);
			    strcat(s," Moving UP");
			}
			else if (y < -1.0 * SCOPE_ALIGN_LIMIT)
			{
			    if (include_old_S2_code && this_labao == S2)
			        motor_move.motor = AOB_S2_DICHR_1;
			    else
			        motor_move.motor = AOB_DICHR_1;
			    motor_move.position = -1.0*(SCOPE_ALIGN_STEP + 
				(int)(fabs(y)*SCOPE_ALIGN_GAIN));
			    autoalign_y_total -= SCOPE_ALIGN_STEP + 
				(int)(fabs(y)*SCOPE_ALIGN_GAIN);
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

		if (autoalign_beacon_focus && time(NULL) > last_time+3)
		{
		    /* Is it done? */
        
		    if (fabs(wfs_results.focus) < BEACON_FOCUS_LIMIT)
		    {
			autoalign_beacon_focus = FALSE;
			autoalign_count = 0;
			message(system_window,
				"Beacon Focus Autoalignment is complete.");
			send_labao_text_message("%s", 
				"Beacon Focus Autoalignment is complete.");
			pthread_mutex_unlock(&fsm_mutex);
			return NOERROR;
		    }

		    /* Have we tried too many times? */

		    if (--autoalign_count < 0)
		    {
			autoalign_beacon_focus = FALSE;
			autoalign_count = 0;
			message(system_window,
				"Giving up on beacon focus autoalignment.");
			send_labao_text_message("%s",
				"Giving up on beacon focus autoalignment.");
			pthread_mutex_unlock(&fsm_mutex);
			return NOERROR;
		    }

		    /* OK, move things in appropriate ways */

		    mess.type = HUT_AOB_MOVE_RELATIVE;
		    mess.length = sizeof(motor_move);
		    mess.data = (unsigned char *)&motor_move;
		    if (include_old_S2_code && this_labao == S2)
			  motor_move.motor = AOB_S2_BEACON_FOC;
		    else
			  motor_move.motor = AOB_BEACON_FOC;
		    motor_move.position = BEACON_FOCUS_STEP + 
			  (int)(fabs(wfs_results.focus)*BEACON_FOCUS_GAIN);

		    if (wfs_results.focus < 0) motor_move.position *= -1.0;
		    send_message(telescope_server, &mess);

		    /* Tell the user */

		    sprintf(s, "Tries = %d Focus = %.2f Move = %d", 
			autoalign_count+1, wfs_results.focus, 
			motor_move.position);
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
		mvwaddstr(status_window,8,44,"Coma   : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f %6.3f", 
			wfs_results.c1, wfs_results.c2);
	
		/* These to help us learn about the servo */

		wstandout(status_window);
		mvwaddstr(status_window,0,44,"Error  : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f", mean_dm_error);
	
		wstandout(status_window);
		mvwaddstr(status_window,1,44,"Offset : ");
		wstandend(status_window);
		wprintw(status_window, "%6.3f", mean_dm_offset);
	
		flux_mean = 0.0;
		flux_stddev = 0.0;
		flux_min = 1e32;
		flux_max = -1e32;

		for (i=0;i<NUM_LENSLETS;i++)
		{
			flux_mean += avg_fluxes[i];
			flux_stddev += (avg_fluxes[i] *  avg_fluxes[i]);
			if (avg_fluxes[i] > flux_max)
				flux_max = avg_fluxes[i];
			if (avg_fluxes[i] < flux_min)
				flux_min = avg_fluxes[i];
		}
		flux_mean /= NUM_LENSLETS;
		flux_stddev /= NUM_LENSLETS;
		flux_stddev = sqrt(flux_stddev - flux_mean*flux_mean);

		wstandout(status_window);
		mvwaddstr(status_window,2,44,"Flux To: ");
		wstandend(status_window);
		wprintw(status_window, " %6.1f", current_total_flux);

		wstandout(status_window);
		mvwaddstr(status_window,3,44,"Flux Mn: ");
		wstandend(status_window);
		wprintw(status_window, " %5.1f+-%5.1f", 
			flux_mean, flux_stddev);

		wstandout(status_window);
		mvwaddstr(status_window,4,44,"Flux Rg: ");
		wstandend(status_window);
		wprintw(status_window, " %5.1f->%5.1f", 
			flux_min, flux_max);
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
        for (i=0;i<NUM_ACTUATORS;i++) fsm_flat_dm[i] = fsm_mean_dm[i];

        message(system_window,"New DM flat set");
        send_labao_text_message("New DM flat set");

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

	if (scope_dichroic_mapping) return error(ERROR,
	   "Already mapping the scope dichroic positions. stopmap to stop.");

	/* Make sure we start off close to 0 azimuth */

	if ((telescope_status.az > 85) && (telescope_status.az < 275))
		return error(ERROR,
	"Telescope azimuth must be within 85 degrees of North to start!");

	sprintf(filename, "%s/%s_dich_map.dat", get_data_directory(data_dir),
		labao_name);

	if ( (scope_dichroic_mapping_file = fopen(filename, "w")) == NULL ) 
		return error(ERROR, "Could not open dichroic mapping file");

	/* Start with a dichroic alignment */

	args[0] = "adich";
	args[1] = "50"; /* A fixed number of tries.*/
	start_autoalign_scope_dichroic(2, args);

	/* 
         * Set the initial azimuth... apart from that it 
         * is just a matter of seeting some local globals
 	 */

	initial_mapping_az = telescope_status.az;
	last_coude_dichroic_corrections = use_coude_dichroic_corrections;
	use_coude_dichroic_corrections = FALSE;
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
	struct smessage mess;
	time_t	start;

	if (autoalign_scope_dichroic || autoalign_zernike || 
		autoalign_beacon_focus)
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

	/* First, let's make sure the pico and iris connections ares there */

#warning This stops the PICO server If it is switched off.
	
	if (pico_server != -1) close_server_socket(pico_server);

	if (open_pico_connection(0, NULL) != NOERROR) return ERROR;

	if (iris_server != -1) close_server_socket(iris_server);

	if (open_iris_connection(0, NULL) != NOERROR) return ERROR;

	message(system_window,"Lab autoalignment begins Trys = %d",
		autoalign_count);

	/* The servo must not be on. */

	open_servo();

#warning Do we need to add shutter things here? 

	/* We need the reference offsets */

	use_reference_on();

	/* We must ensure the iris is at beam size */

	if (!iris_at_beam_size)
	{
	    mess.type = IRIS_COMMAND_BEAM;
	    mess.data = NULL;
	    mess.length = 0;
	
            if (!send_message(iris_server, &mess))
	    {
		iris_server = -1;
	    }
	    else
	    {
		start = time(NULL);

		while (!iris_at_beam_size)
		{
		    process_server_socket(iris_server);
		    message(system_window,
			"Waiting for Iris to go to beam size %d.",
			15 + (int)(start - time(NULL)));
		    send_labao_text_message(
			"Waiting for Iris to go to beam size %d.",
			15 + (int)(start - time(NULL)));
    
    		    if (time(NULL) > start+15)
		    {
			error(ERROR,"Timed out waiting for IRIS.");
			break;
		    }
		}

		werase(system_window);
		wrefresh(system_window);
	    }
	}

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
	struct smessage mess;
        struct s_aob_move_motor motor_move;

	if (autoalign_lab_dichroic || autoalign_zernike ||
		autoalign_beacon_focus)
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

        /* First has there been a large jump? last_coude_correction_az is negative only if
	there hasn't been any corrections yet. */

        if (last_coude_correction_az >= 0.0 && use_coude_dichroic_corrections)
        {
                mess.type = HUT_AOB_MOVE_RELATIVE;
                mess.length = sizeof(motor_move);
                mess.data = (unsigned char *)&motor_move;

	        if (include_old_S2_code && this_labao == S2)
                    motor_move.motor = AOB_S2_DICHR_2;
	        else
                    motor_move.motor = AOB_DICHR_2;
                motor_move.position =
                        coude_dichroic_correction_x_amp * (
                        sin((telescope_status.az  +
                             coude_dichroic_correction_x_phase)/180.0*M_PI) -
                        sin((last_coude_correction_az  +
                             coude_dichroic_correction_x_phase)/180.0*M_PI));
                message(system_window,"Moving large offset dX = %d",
			motor_move.position);
                send_message(telescope_server, &mess);

                sleep(1);

	        if (include_old_S2_code && this_labao == S2)
                    motor_move.motor = AOB_S2_DICHR_1;
		else
                    motor_move.motor = AOB_DICHR_1;
                motor_move.position =
                        coude_dichroic_correction_y_amp * (
                        sin((telescope_status.az  +
                             coude_dichroic_correction_y_phase)/180.0*M_PI) -
                        sin((last_coude_correction_az  +
                             coude_dichroic_correction_y_phase)/180.0*M_PI));
                message(system_window,"Moving large offset dY = %d",
			motor_move.position);
                send_message(telescope_server, &mess);
                sleep(1);
	}

	message(system_window,"Scope autoalignment begins Trys = %d",
		autoalign_count);

	autoalign_x_total = 0;
	autoalign_y_total = 0;
	use_reference_off();
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

	if (autoalign_lab_dichroic || autoalign_scope_dichroic ||
		autoalign_beacon_focus)
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
	use_reference_off();
	call_load_zernike(0, NULL);
	autoalign_zernike = TRUE;

	return NOERROR;

} /* start_autoalign_zernike() */

/************************************************************************/
/* start_autoalign_beacon_focus()					*/
/*									*/
/* Starts the beacon focus alignment routine.				*/
/************************************************************************/

int start_autoalign_beacon_focus(int argc, char **argv)
{
        char    s[100];

	if (autoalign_lab_dichroic || autoalign_scope_dichroic ||
		autoalign_beacon_focus)
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

	message(system_window,"Beacon focus autoalignment begins Trys = %d",
		autoalign_count);

	fsm_state = FSM_CENTROIDS_ONLY;
	use_reference_off();
	autoalign_beacon_focus = TRUE;

	return NOERROR;

} /* start_autoalign_beacon_focus() */

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
	autoalign_beacon_focus = FALSE;
	use_reference_off();
        if (scope_dichroic_mapping)
	{
	    use_coude_dichroic_corrections = last_coude_dichroic_corrections;
	    scope_dichroic_mapping = FALSE;
	    fclose(scope_dichroic_mapping_file);
	}

	if (open_telescope_connection(0, NULL) != NOERROR) return ERROR;

	message(system_window, "Auto alignment has been stopped.");
        send_labao_text_message("Auto alignment has been stopped.");

	return NOERROR;

} /* stop_autoalign() */

/************************************************************************/
/* edit_servo_parameters()						*/
/*									*/
/* Edit gain etx of servo.						*/
/************************************************************************/

int edit_servo_parameters(int argc, char **argv)
{
        char    s[100];
	float	memory, gain, damping, integration, max_sum;

        /* Check out the command line */

	memory = servo_memory;
	gain = servo_gain;
	damping = servo_damping;
	integration = servo_integration;
	max_sum = max_dm_sum;

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&memory);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", memory);
                if (quick_edit("Servo Memory",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&memory);
        }

	if (memory < 0.0 || memory > 1.0) return 
		error(ERROR,"Memory must be between 0 and 1.");

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&gain);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", gain);
                if (quick_edit("Servo Gain",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&gain);
        }

	if (gain < 0.0 || gain > 2.0) return 
		error(ERROR,"Gain must be between 0 and 2.");

        if (argc > 3)
        {
                sscanf(argv[3],"%f",&damping);
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

        if (argc > 4)
        {
                sscanf(argv[4],"%f",&integration);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", integration);
                if (quick_edit("Servo Integration",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&integration);
        }

	if (integration < 0.0 || integration > 1.0) return 
		error(ERROR,"Integration must be between 0 and 1.");

        if (argc > 5)
        {
                sscanf(argv[5],"%f",&max_sum);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", max_sum);
                if (quick_edit("Maximum DM Sum",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&max_sum);
        }

	if (max_sum < 0.0) return 
		error(ERROR,"Maximum DM sum must be greater than 0.");

	servo_gain = gain;
	servo_damping = damping;
	servo_memory = memory;
	servo_integration = integration;
	max_dm_sum = max_sum;

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

	move_centroids(dx, dy);

	return NOERROR;

} /* move_boxes() */

/************************************************************************/
/* expand_boxes()							*/
/*									*/
/* Expand centroid boxes by a small amount.				*/
/************************************************************************/

int expand_boxes(int argc, char **argv)
{
        char    s[100];
	int i;
	float	factor,x_mean_offset, y_mean_offset; 

	/* Initialize variables */

	x_mean_offset = 0.0;
	y_mean_offset = 0.0;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&factor);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9.2f", 0.1);
                if (quick_edit("factor",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&factor);
        }

	/* To compute expansion, we need the current pupil center. */

	for (i=0;i<NUM_LENSLETS;i++)
	{
		x_mean_offset += x_centroid_offsets[i];
		y_mean_offset += y_centroid_offsets[i];
	}
	x_mean_offset /= NUM_LENSLETS;
	y_mean_offset /= NUM_LENSLETS;
	
	for (i=0;i<NUM_LENSLETS;i++)
        {
                x_centroid_offsets_beacon[i] = x_mean_offset + factor*(x_centroid_offsets_beacon[i] - x_mean_offset);
		y_centroid_offsets_beacon[i] = y_mean_offset + factor*(y_centroid_offsets_beacon[i] - y_mean_offset);
		x_centroid_offsets_reference[i] = x_mean_offset + factor*(x_centroid_offsets_reference[i] - x_mean_offset);
		y_centroid_offsets_reference[i] = y_mean_offset + factor*(y_centroid_offsets_reference[i] - y_mean_offset);
        }


	compute_pupil_center();
        

	return NOERROR;

} /* move_boxes() */

/************************************************************************/
/* move_centroids()                                                     */
/*                                                                      */
/* Move centroid boxes by a small amount.                               */
/************************************************************************/

void move_centroids(float dx, float dy)
{
        int     i;

        for (i=0;i<NUM_LENSLETS;i++)
        {
                x_centroid_offsets_beacon[i] += dx;
                y_centroid_offsets_beacon[i] += dy;
                x_centroid_offsets_reference[i] += dx;
                y_centroid_offsets_reference[i] += dy;
        }
	compute_pupil_center();

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
		fprintf(fp,
		"%9d %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f\n",
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
		zernike = 4;
                if (quick_edit("Zernike Term",aberration_types[zernike],
                        &zernike,aberration_types,ENUMERATED) == KEY_ESC)
                                return NOERROR;
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
			return NOERROR;

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
		amplitude /= 0.773;
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				max_offset;
		    aberration_yc[i] += amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				max_offset;
		}
		break;

	    case 5: /* a1 (first astigmatism) term */
		amplitude /= 0.773;
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				max_offset;
		    aberration_yc[i] -= amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				max_offset;
		}
		break;

	    case 6: /* a2 (second astigmatism) term */
		amplitude /= 0.773;
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    aberration_xc[i] += amplitude *
			(y_centroid_offsets[i] - y_mean_offset)/
				max_offset;
		    aberration_yc[i] += amplitude *
			(x_centroid_offsets[i] - x_mean_offset)/
				max_offset;
		}
		break;

	    case 7: /* c1 (first coma) term */
		amplitude /= 0.310;
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    xx = (x_centroid_offsets[i] - x_mean_offset)/
				max_offset;
		    yy = (y_centroid_offsets[i] - y_mean_offset)/
				max_offset;
		    aberration_xc[i] += amplitude *
			(9*xx*xx + 3*yy*yy - 2)/9.0;
		    aberration_yc[i] += amplitude *
			2*xx*yy/3.0;
		}
		break;

	    
	    case 8: /* c2 (second coma) term */
		amplitude /= 0.310;
		for (i=0;i<NUM_LENSLETS;i++)
		{
		    xx = (x_centroid_offsets[i] - x_mean_offset)/
				max_offset;
		    yy = (y_centroid_offsets[i] - y_mean_offset)/
				max_offset;
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

	if (!use_reference)
	{
		use_reference_on();
		message(system_window, 
			"Using reference mirror offsets.");
		send_labao_text_message(
			"Using reference mirror offsets.");
	}
	else
	{
		use_reference_off();
		message(system_window, 
			"Not using reference mirror offsets.");
		send_labao_text_message(
			"Not using reference mirror offsets.");
	}

	return NOERROR;

} /* toggle_use_reference() */

/************************************************************************/
/* use_reference_on()							*/
/*									*/
/* Force us to use teh reference centroids.				*/
/************************************************************************/

void use_reference_on(void)
{
	use_reference = TRUE;
	x_centroid_offsets = x_centroid_offsets_reference;
	y_centroid_offsets = y_centroid_offsets_reference;
	compute_pupil_center();

} /* use_reference_on */

/************************************************************************/
/* use_reference_off()							*/
/*									*/
/* Force us to use the beacon centroids.				*/
/************************************************************************/

void use_reference_off(void)
{
	use_reference = FALSE;
	x_centroid_offsets = x_centroid_offsets_beacon;
	y_centroid_offsets = y_centroid_offsets_beacon;
	compute_pupil_center();

} /* use_reference_off */

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

/************************************************************************/
/* toggle_coude_dichroic_corrections() 		    			*/
/*									*/
/* Toggle if we are going to use the coude rotation based dichroic	*/
/* position correctors.							*/
/************************************************************************/

int toggle_coude_dichroic_corrections(int argc, char **argv)
{
	if (use_coude_dichroic_corrections)
	{
		use_coude_dichroic_corrections=FALSE;
		message(system_window, 
			"Not using Coude based Dichroic corrections.");
		send_labao_text_message(
			"Not using Coude based Dichroic corrections.");
	}
	else
	{
		use_coude_dichroic_corrections=TRUE;
		message(system_window, 
			"Using Coude based Dichroic corrections.");
		send_labao_text_message(
			"Using Coude based Dichroic corrections.");
	}

	return NOERROR;

} /* toggle_coude_dichroic_corrections */

/************************************************************************/
/* edit_coude_dichroic_corrections()					*/
/*									*/
/* Let's you change the coude cdichroic correction values.		*/
/************************************************************************/

int edit_coude_dichroic_corrections(int argc, char **argv)
{
        char    s[100];
	float	new_x_amp, new_x_phase;
	float	new_y_amp, new_y_phase;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&new_x_amp);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9f", coude_dichroic_correction_x_amp);
                if (quick_edit("Coude/Dichroic X Amplitude",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_x_amp);
        }

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&new_x_phase);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9f", coude_dichroic_correction_x_phase);
                if (quick_edit("Coude/Dichroic X Phase",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_x_phase);
        }

        if (argc > 3)
        {
                sscanf(argv[3],"%f",&new_y_amp);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9f", coude_dichroic_correction_y_amp);
                if (quick_edit("Coude/Dichroic Y Amplitude",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_y_amp);
        }

        if (argc > 4)
        {
                sscanf(argv[4],"%f",&new_y_phase);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%9f", coude_dichroic_correction_y_phase);
                if (quick_edit("Coude/Dichroic Y Phase",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_y_phase);
        }

	coude_dichroic_correction_x_amp = new_x_amp;
	coude_dichroic_correction_x_phase = new_x_phase;
	coude_dichroic_correction_y_amp = new_y_amp;
	coude_dichroic_correction_y_phase = new_y_phase;

	return NOERROR;

} /* edit_coude_dichroic_corrections() */

/************************************************************************/
/* edit_dm_impulse()							*/
/*									*/
/* Let's you add a single impules to the DM for servo tuning.		*/
/************************************************************************/

int edit_dm_impulse(int argc, char **argv)
{
        char    s[100];
	float	new_impulse;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&new_impulse);
        }
        else
        {
                clean_command_line();
                sprintf(s,"      0.05");
                if (quick_edit("DM Impulse amplitude",s,s,NULL,FLOAT)
                   == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&new_impulse);
        }

	if (new_impulse < 0.0 || new_impulse > 1.0)
		return error(ERROR,"Impulse must be between 0 and 1.");

	pthread_mutex_lock(&fsm_mutex);
	dm_impulse = new_impulse;
	pthread_mutex_unlock(&fsm_mutex);

	return NOERROR;

} /* edit_dm_impulse() */

/************************************************************************/
/* sort_eigen()                                                         */
/*                                                                      */
/* Sort eigenvalues and eigenvectors in descending order.               */
/* Largest eigenvalue and corresponding eigenvector go first,           */
/* then the next largest, and so forth.                                 */
/* Input is array of eigenvalues [1...n], matrix of                     */
/* eigenvectors [1...n][1..n].  Output is sorted arrays.                */
/* Note that the arrays are in Numerical Recipes format with            */
/* indices going from 1 to n (rather than 0 to n-1).                    */
/************************************************************************/

void sort_eigen(float eigenval[], float **eigenvect, int n)
{
	float atemp, btemp[n];
	int i,j,k;

	for (j=2; j<=n; j++)
	{
		atemp = eigenval[j];
		for (k=1; k<=n; k++) btemp[k] = eigenvect[k][j];
		i=j-1;
		while (i>0 && eigenval[i] < atemp)
		{
			eigenval[i+1] = eigenval[i];
			for (k=1; k<=n; k++) eigenvect[k][i+1] =eigenvect[k][i];
			i--;
		}
		eigenval[i+1] = atemp;
		for (k=1; k<=n; k++) eigenvect[k][i+1] = btemp[k];
	}

} /* sort_eigen() */

/************************************************************************/
/* compute reconstructor_new()						*/
/*									*/
/* Compute the reconstructor.  Save fsm_actuator_to_sensor matrix to    */
/* file.  Perform singular value decomposition to compute the           */
/* pseudo-inverse of fsm_actuator_to_sensor matrix.  The inverse        */
/* will be output as fsm_reconstructor and also saved to file.          */
/*									*/
/* ARGUMENTS:                                                           */
/*   argc: number of parameters in argv                                 */
/*   argv: optional character array containing the following parameters */
/*         num_modes in_filename out_filename                           */
/*         where num_modes is the number of eigenmodes to keep,         */
/*         in_filename is the name of the file to save the              */
/*         fsm_actuator_to_sensor matrix, and out_filename is the name  */
/*         of the file to save the fsm_reconstructor matrix             */
/************************************************************************/

int compute_reconstructor_new(int argc, char **argv)
{
	float **amatrix, **a_inverse, **c_inverse;
	float **v_eigenvector, *v_eigenvalue, *offdiag_v;
	float sum, ave_tilt, median_w;
	float UNSENSED_THRESHOLD;
	int num_modes;
	int i, j, k;
	char in_filename[256], out_filename[256], s[256];
	bool	found_nan = FALSE;

	/* Setup the defaults */

	num_modes = 12;
	sprintf(in_filename,"%s%s_actuator_to_sensor.tmp", 
		get_data_directory(s), labao_name);
	sprintf(out_filename,"%s%s_reconstructor.tmp", 
		get_data_directory(s), labao_name);

	/* Check which arguments are defined in the call */

	if (argc > 1)
	  if (sscanf(argv[1], "%d", &num_modes) != 1)
	    return error(ERROR, 
	      "Usage: recon [n_modes] [infile (opt)] [outfile (opt)]");
	if (argc > 2) strcpy(in_filename, argv[2]);
	if (argc > 3) strcpy(out_filename, argv[3]);

	/* Save fsm_actuator_to_sensor matrix */

	if (save_actuator_to_sensor(in_filename))
	  return error(ERROR, 
	    "Could not create temporary actuator_to_sensor file %s.",
            in_filename);

	/* Initialize matrices to be used with Numerical Recipes */

	amatrix = matrix(1,2*NUM_LENSLETS,1,NUM_ACTUATORS);
	a_inverse = matrix(1,NUM_ACTUATORS,1,2*NUM_LENSLETS);
	v_eigenvector = matrix(1,NUM_ACTUATORS,1,NUM_ACTUATORS);
	c_inverse = matrix(1,NUM_ACTUATORS,1,NUM_ACTUATORS);

	/* Initialize vectors to be used with Numerical Recipes */

	v_eigenvalue = vector(1,NUM_ACTUATORS);
	offdiag_v = vector(1,NUM_ACTUATORS);

	/* Shift indices of fsm_actuator_to_sensor matrix to be */
	/* consistent with Numerical Recipes (indices go from   */
        /* 1 to n instead of 0 to n-1).  Also switch rows and   */
	/* columns to prepare for singular value decomposition. */

	for (i=0; i<2*NUM_LENSLETS; i++)
	  for (j=0; j<NUM_ACTUATORS; j++) 
	    amatrix[i+1][j+1] = fsm_actuator_to_sensor[j][i];

	/* Optionally remove tilt terms */
	/* Mike's python code removed the tilts if num_modes > 0 */
	/* However, could we also do this with fsm_ignore_tilt variable? */

	if (num_modes > 0)
        {
	  for (i=1; i<=NUM_ACTUATORS; i++)
          {
	    /* Compute x tilt - sum over lenslets for each actuator */
	    ave_tilt = 0.0;
	    for (j=1; j<=NUM_LENSLETS; j++) 
	      ave_tilt += amatrix[j][i];
	    ave_tilt /= NUM_LENSLETS;
	    for (j=1; j<=NUM_LENSLETS; j++) 
	      amatrix[j][i] -= ave_tilt;
	    /* Compute y tilt - sum over lenslets for each actuator */
	    ave_tilt = 0.0;
	    for (j=1; j<=NUM_LENSLETS; j++) 
	      ave_tilt += amatrix[j+NUM_LENSLETS][i];
	    ave_tilt /= NUM_LENSLETS;
	    for (j=1; j<=NUM_LENSLETS; j++) 
	      amatrix[j+NUM_LENSLETS][i] -= ave_tilt;
	  }	
	}

	/* Prepare matrices for singular value decomposition to       */
	/* determine the pseudo-inverse of matrix A.  A rectangular   */
	/* matrix can be broken down into 3 components:               */
	/* A = U*W*Vtranspose                                         */
	/* - columns of U represent eigenvectors of A*Atranspose      */ 
	/*     [2*NUM_LENSLETS][2*NUM_LENSLETS]                       */
	/* - columns of V represent eigenvectors of Atranspose*A      */
	/*     [NUM_ACTUATORS][NUM_ACTUATORS]                         */
	/* - diagonals of W represent non-zero eigenvalues of U and V */
	/*     [2*NUM_LENSLETS][NUM_ACTUATORS]                        */

	/* Compute dot product of Atranspose*A                        */
	/* (set temporarily to v_eigenvector)                         */
	for (i=1; i<=NUM_ACTUATORS; i++)
	{
	  for (j=1; j<=NUM_ACTUATORS; j++) 
	  { 
	    sum = 0.0;
	    for (k=1; k<=2*NUM_LENSLETS; k++)
	      sum += amatrix[k][i]*amatrix[k][j];
	    v_eigenvector[i][j] = sum;
	  }
	}

	/* Compute eigenvalues and eigenvectors of Atranspose*A       */
	/* Note: Atranspose*A is a real, symmetric matrix             */
	/* Step 1: Use householder reduction to convert nxn real,     */
	/*         symmetric matrix to tridiagonal form (tred2)       */
	/* Step 2: Compute eigenvectors and eigenvalues of the        */
	/*         real, symmetric matrix in tridiagonal form         */
	/*         using tqli in numerical recipes                    */
	/* Step 3: Sort eigenvalues and eigenvectors in descending    */
	/*         order according to the eigenvalue.  Largest        */
	/*         eigenvalue and corresponding eigenvector first,    */
	/*         then next largest eigenvalue, and so forth.        */
	/*                                                            */
	/* If the eigenvectors in U and V are computed independently, */
	/* there exists an ambiguity in the sign (+/-) assigned to    */
	/* each eigenvector.  To ensure that the eigenvectors of U    */
	/* and V are on the same basis, it is possible to compute     */
	/* either V (or U) from U (or V).                             */
	/*    A = U*W*Vtranspose                                      */
	/*    U = A*V*Winverse                                        */
	/*    Vtranspose = Winverse*Utranspose*A                      */
	/*    (where U*Utranspose=I, V*Vtranspose=I, W*Winverse=I     */

	/* Compute eigenvalues and eigenvectors of Atranspose*A       */

	tred2(v_eigenvector,NUM_ACTUATORS,v_eigenvalue,offdiag_v);
	tqli(v_eigenvalue,offdiag_v,NUM_ACTUATORS,v_eigenvector); 
	sort_eigen(v_eigenvalue,v_eigenvector,NUM_ACTUATORS);

	/* Compute median weight: */

	median_w = v_eigenvalue[(int)(rintf(NUM_ACTUATORS/2))+1];

	/* Apply threshold to the eigenmodes based on num_modes.   */
	/* If the number of modes is non-zero, then set threshold  */
	/* to the eigenvalue of that mode.  If the number of modes */
	/* is set to 0, then use default threshold of 2.1*median.  */
	/* In python script it says, "This stays a long way from   */
        /* the DM range when applied (max 0.92 for mean 0.77), and */
	/* has 10 modes."                                          */

	if (num_modes > 0) UNSENSED_THRESHOLD = v_eigenvalue[num_modes];
	else UNSENSED_THRESHOLD = 2.1*median_w;

	for (i=1; i<=NUM_ACTUATORS; i++)
	{
	  if (fabs(v_eigenvalue[i]) < UNSENSED_THRESHOLD)
	    v_eigenvalue[i] = 0.0;

	  /* The following is from Mike's code, but I do not */
	  /* understand why these weights are being raised   */

	  else if (fabs(v_eigenvalue[i]) < 2*UNSENSED_THRESHOLD) 
	    v_eigenvalue[i] = 2*UNSENSED_THRESHOLD; 
	}

	/* Compute the reconstructor - pseudo-inverse of matrix A */
	/* A = U*W*Vtranspose                                     */
 	/* A^-1 = V*Winverse*Utranspose                           */
	/* Substitute Utranspose = Winverse*Vtranspose*Atranspose */
	/* A^-1 = V*[diag(1/wj^2)]*Vtranspose*Atranspose          */

	/* Step 1: Compute C_inverse = V*[diag(1/wj^2)]*Vtranspose */

	for (i=1; i<=NUM_ACTUATORS; i++)
	{
	  for (j=1; j<=NUM_ACTUATORS; j++)
	  {
	    sum = 0.0;
	    for (k=1; k<=NUM_ACTUATORS; k++)
	      if (fabs(v_eigenvalue[k]) > 0.0)
		sum += v_eigenvector[i][k]*v_eigenvector[j][k]/v_eigenvalue[k];
	    c_inverse[i][j] = sum;
	  }
	}

	/* Step 2: Compute A_inverse = C_inverse*Atranspose */

	for (i=1; i<=NUM_ACTUATORS; i++)
	{
	  for (j=1; j<=2*NUM_LENSLETS; j++)
	  {
	    sum = 0.0;
	    for (k=1; k<=NUM_ACTUATORS; k++)
	      sum += c_inverse[i][k]*amatrix[j][k];
	    a_inverse[i][j] = sum;
	  }
	}

	/* Step 3: Transfer a_inverse to fsm_reconstructor */
	/* Shift indices from [1...n] to [0...n-1]         */

	for (i=0; i<NUM_ACTUATORS; i++)
	{
	  for (j=0; j<2*NUM_LENSLETS; j++)
	  {
	    if (isnan(a_inverse[i+1][j+1]))
	    {	
		found_nan = TRUE;
	    	fsm_reconstructor[i][j] = 0.0;
	    }
	    else
	    {
	    	fsm_reconstructor[i][j] = a_inverse[i+1][j+1];
	    }
	  }
	}

	/* Save the reconstructor matrix */

	if (!found_nan && save_reconstructor_new(out_filename))
		return error(ERROR,
			"Problems loading the temporary reconstructor file.");

	free_matrix(amatrix,1,2*NUM_LENSLETS,1,NUM_ACTUATORS);
	free_matrix(a_inverse,1,NUM_ACTUATORS,1,2*NUM_LENSLETS);
	free_matrix(v_eigenvector,1,NUM_ACTUATORS,1,NUM_ACTUATORS);
	free_matrix(c_inverse,1,NUM_ACTUATORS,1,NUM_ACTUATORS);
	free_vector(v_eigenvalue,1,NUM_ACTUATORS);
	free_vector(offdiag_v,1,2*NUM_LENSLETS);

	if (found_nan)
	   message(system_window,"New reconstructor contains NAN!");
	else
	   message(system_window,"New reconstructor saved in %s", out_filename);
	
	return NOERROR;	

} /* compute_reconstructor_new() */

/************************************************************************/
/* save_reconstructor_new()						*/
/*									*/
/* Save the reconstructor matrix to a file                              */ 
/************************************************************************/

int save_reconstructor_new(char* filename_in)
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
		sprintf(filename,"%s%s_reconstructor.dat", 
			get_data_directory(s), labao_name);
        }

  	/* Start with the defaults, in case there is a problem loading */
  
  	if ((params_file = fopen(filename,"w")) == NULL ) return -1;

  	/* Save the reconstructor matrix */

	for (i=0;i<NUM_ACTUATORS;i++)
	{
		for (j=0;j<2*NUM_LENSLETS;j++)
			fprintf(params_file, "%10.3e ", 
				fsm_reconstructor[i][j]);
		fprintf(params_file, "\n");
	}

	/* That is all. */

	fclose(params_file);

	return 0;
	
} /* save_reconstructor_new() */

/************************************************************************/
/* read_actuator_to_sensor()                                            */
/*                                                                      */
/* Read the actuator to sensor matrix, used to create the reconstructor */
/************************************************************************/

int read_actuator_to_sensor(int argc, char **argv)
{
	char	filename[3456];
	char	s[3456];
	char 	*p;
	FILE	*fp;
	int	i = 0, j = 0;

	/* Has there been an argument on the command line? */

	if (argc > 1)
	{
		strcpy(filename, argv[1]);
	}
	else
	{
		sprintf(filename,"%s%s_actuator_to_sensor.tmp",
                        get_data_directory(s), labao_name);
	}

	/* Open the file */

	if ((fp = fopen(filename, "r")) == NULL)
		return error(ERROR, "Failed to open file %s", filename);

	/* Get the data */

	for (i=0; i<NUM_ACTUATORS; i++)
	{
		/* Get a line of data */

		if (GetDataLine(s, 3456, fp) == -1)
		{
			fclose(fp);
			return error(ERROR,"We ran out lines of data (%d, %d).",
				i, j);
		}
		p = s;

		for(j=0;j<2*NUM_LENSLETS;j++)
		{
			/* Find first non white space */

			while(*p == ' ' && *p != 0) p++;

			/* Read in a number */

			if (sscanf(p, "%f", &(fsm_actuator_to_sensor[i][j]))!=1)
			{
				fclose(fp);
				return error(ERROR,
					"We ran out data (%d, %d).", i, j);
			}

			/* Find white space */

			while (*p != ' ' && *p != '\t' && *p != 0) p++;
		}
	}

	fclose(fp);

	message(system_window,"Read in matrix from file %s.", filename);
	return NOERROR;

} /* read_actuator_to_sensor() */

/************************************************************************/
/* read_actuator_to_aberration()                                        */
/*                                                                      */
/* Read the actuator to aberration matrix				*/
/************************************************************************/

int read_actuator_to_aberration(int argc, char **argv)
{
	char	filename[3456];
	char	s[3456];
	char 	*p;
	FILE	*fp;
	int	i = 0, j = 0;

	/* Has there been an argument on the command line? */

	if (argc > 1)
	{
		strcpy(filename, argv[1]);
	}
	else
	{
		sprintf(filename,"%s%s_actuator_to_aberration.dat",
                        get_data_directory(s), labao_name);
	}

	/* Open the file */

	if ((fp = fopen(filename, "r")) == NULL)
		return error(ERROR, "Failed to open file %s", filename);

	/* Get the data */

	for (i=0; i<NUM_ACTUATORS; i++)
	{
		/* Get a line of data */

		if (GetDataLine(s, 3456, fp) == -1)
		{
			fclose(fp);
			return error(ERROR,"We ran out lines of data (%d, %d).",
				i, j);
		}
		p = s;

		for(j=0;j<NUM_ABERRATIONS;j++)
		{
			/* Find first non white space */

			while(*p == ' ' && *p != 0) p++;

			/* Read in a number */

			if (sscanf(p,"%f",&(fsm_actuator_to_aberration[i][j]))
				!=1)
			{
				fclose(fp);
				return error(ERROR,
					"We ran out data (%d, %d).", i, j);
			}

			/* Find white space */

			while (*p != ' ' && *p != '\t' && *p != 0) p++;
		}
	}

	fclose(fp);

	message(system_window,"Read in matrix from file %s.", filename);
	return NOERROR;

} /* read_actuator_to_aberration() */

/************************************************************************/
/* fit_sin_func()							*/
/*									*/
/* ROutine for use in the fitting program.				*/
/************************************************************************/

void fit_sin_func(float *ftn, float *var, float *param)
{
	ftn[1] = 1.0;
	ftn[2] = sin(param[1] + var[3]);
	ftn[3] = var[2] * cos(param[1] * var[3]);
	ftn[0] = var[1] + var[2] * ftn[2];
}

/************************************************************************/
/* fit_dichroic_map()							*/
/*									*/
/* Fits cosines to the dichroic map to a data set.			*/
/*									*/
/************************************************************************/

int fit_dichroic_map(int argc, char **argv)
{
	/* Locals */

	FILE	*input;		/* data file */
	char	s[256];		/* Buffer */
	char	filename[256];		/* Buffer */
	register int i;
	float	rmsres;
	float	maxres;
	int	nparam = 1;	/* Motor movement */
	float	**param = NULL; /* 1..ndata, 1..param */
	int	ndata = 0;
	float	*var;		/* 1..nvar, Offset, Amplitude and Phase */
	float	*fit_data;	/* 1..ndata */
	int	nvar = 3;
	float	theta[1000];
	float	x[1000];
	float	y[1000];
	float dx,dy;
	float	x_off, x_amp, x_phase;
	float	y_off, y_amp, y_phase;
	float	temp;
	

	/* Open the input file */

	if (argc > 1)
	{
		strcpy(filename, argv[1]);
	}
	else
	{
		sprintf(filename,
			"%s/%s_dich_map.dat", get_data_directory(s),
			labao_name);
	}

	/* Open the file */

	if ((input = fopen(filename,"r")) == NULL)
	{
		return error(ERROR,"Could not open file %s.\n",filename);
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
		if (++ndata > 1000) break;
	}
	fclose(input);

	message(system_window,"# Number of data points : %d\n\n",ndata);

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

	sprintf(s,"Results of X fit : \n");
	sprintf(filename, "RMS residue %le\n",rmsres);
	strcat(s, filename);
	sprintf(filename, "MAX residue %le\n",maxres);
	strcat(s, filename);
	sprintf(filename, "Offset    = %f\n",var[1]);
	strcat(s, filename);
	sprintf(filename, "Amplitude = %f\n",var[2]);
	strcat(s, filename);
	sprintf(filename, "Phase     = %f",var[3]*180.0/M_PI);
	strcat(s, filename);
	error(MESSAGE, s);

	x_off = var[1];
	x_amp = var[2];
	x_phase = var[3]*180.0/M_PI;

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

	sprintf(s,"Results of Y fit : \n");
	sprintf(filename, "RMS residue %le\n",rmsres);
	strcat(s, filename);
	sprintf(filename, "MAX residue %le\n",maxres);
	strcat(s, filename);
	sprintf(filename, "Offset    = %f\n",var[1]);
	strcat(s, filename);
	sprintf(filename, "Amplitude = %f\n",var[2]);
	strcat(s, filename);
	sprintf(filename, "Phase     = %f",var[3]*180.0/M_PI);
	strcat(s, filename);
	error(MESSAGE, s);

	y_off = var[1];
	y_amp = var[2];
	y_phase = var[3]*180.0/M_PI;

	if (ask_yes_no("Do you wish to try these values?",""))
	{
		coude_dichroic_correction_x_amp = x_amp;
		coude_dichroic_correction_x_phase = x_phase;
		coude_dichroic_correction_y_amp = y_amp;
		coude_dichroic_correction_y_phase = x_phase;
	}

	return NOERROR;
}
