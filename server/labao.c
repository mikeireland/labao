/************************************************************************/
/* labao.c								*/
/*                                                                      */
/* User interface for labao system.					*/ 
/* 1.0 - First attempt.							*/
/* 1.1 - Few changes, mostly to turning boxes on and off.		*/
/* 1.2 - Can now turn off tip/tilt correction.				*/
/* 2.0 - Has messages added.						*/
/* 2.1 - Added messages for wfs status.					*/
/* 2.2 - Added more messages so we know what is going on.		*/
/* 2.3 - Added Dichroic autoalignment.					*/
/* 3.0 - Changed aberration calculations to match the WFS.		*/
/* 4.0 - Aberration calculations in RT thread. Can save data.		*/
/* 4.1 - Some small corrections after testing 2014_10_01		*/
/* 4.2 - Added offsets for reference beam focus etc. 2015 02 02		*/
/* 4.3 - Recompiled for Centos 7 and doesn't try to close USB cam.	*/
/* 4.4 - Added new servo type, and averaging for flats.			*/
/* 4.5 - Frame summing added						*/
/* 5.0 - Different DM shapes for dichroics and different centroids	*/
/*	 for reference and beacon.					*/
/* 5.1 - More functionality added for tiptilt messages to WFS.          */
/* 5.2 - Automated Coude/Dichroic corrections added                     */
/* 6.0 - No changes but wanted to make the version clearly new.		*/
/* 6.1 - Added Gail's new reconstructor calculation code.		*/
/* 7.0 - Trying to make the Zernike stuff make sense.			*/
/* 7.1 - It did not work.						*/
/* 7.2 - Added flux total, mean , min and max				*/
/* 7.3 - Small bug fix for when boxes are close to the edge.		*/
/************************************************************************/
/*                                                                      */
/*                    CHARA ARRAY USER INTERFACE			*/
/*                 Based on the SUSI User Interface			*/
/*		In turn based on the CHIP User interface		*/
/*                                                                      */
/*            Center for High Angular Resolution Astronomy              */
/*              Mount Wilson Observatory, CA 91023, USA			*/
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
/* Author : Theo ten Brummelaar			                        */
/* Date   : Feb 2014							*/
/************************************************************************/

#include "labao.h"

/* Globals */

char title[100];
char labao_name[80];
char default_display[256];
char *dac40_mac_address[NUM_SCOPES] =
		//{"00-1E-C0-BB-6F-BF", /* 15.13 Originally S1 faulty? */
		{"00-1E-C0-BB-72-1A", /* 15.07 S1 */
		 "00-04-A3-13-DA-90", /* S2 */
		 "Undefined", /* E2 */
		 "Undefined", /* E2 */
		 "Undefined", /* W1 */
		 "Undefined"}; /* W2 */

int camera_id[NUM_SCOPES] =
		{ 0x2,  /* S1 */
		  0x1,  /* S2 */
		  0x3,  /* E1 */
		  0x4,  /* E2 */
		  0x5,  /* W1 */
		  0x6}; /* W2 */
int this_labao;
int maxJ = DEFAULT_MAXJ;
struct s_labao_usb_camera usb_camera;
int     pico_server = -1;
char    *pico_servers[NUM_PICO_SERVERS] = {"PICO_1", "PICO_2", "PICO_3",
                "PICO_4", "PICO_5", "PICO_6", "PICO_7"};
int     telescope_server = -1;
int     iris_server = -1;
int	dich_mirror = -1;
struct SMIRROR_LIST mirror_list[MAX_SERVERS];
float servo_memory = SERVO_MEMORY;
float servo_gain = SERVO_GAIN;
float servo_damping = SERVO_DAMPING;
float servo_integration = SERVO_INTEGRATION;
float    xpos_center = 0.0;
float    ypos_center = 0.0;
struct s_scope_status telescope_status;
bool send_tiptilt = FALSE;
bool iris_at_beam_size = FALSE;
bool include_old_S2_code = FALSE;

int main(int argc, char **argv)
{
	char	*name, *p;

	name = argv[0];
        while(--argc > 0 && (*++argv)[0] == '-')
        {
            for(p = argv[0]+1; *p != '\0'; p++)
            {
                switch(*p)
                {
			case 'o': include_old_S2_code = !include_old_S2_code;
				  break;

                        case 's': use_socket_manager = !use_socket_manager;
                                  break;

			case 'j': if (sscanf(p+1,"%d",&maxJ) != 1 ||
					maxJ < 3 || maxJ > NUM_LENSLETS)
                                  {
                                    print_usage_message(name);
                                    exit(-1);
                                  }
                                  while(*p != '\0') p++; p--;
                                  break;

			default:
                        case 'h': print_usage_message(name);
                                  exit(0);
		}
	    }
	}

	if (argc < 1)
	{
		print_usage_message(name);
		exit(-1);
	}

	if (argc > 1)
	{
		/* Is there a ':' in the name somewhere? */

		for(p=default_display; *p != ':' && *p != 0; p++);
		if (*p == 0)
			sprintf(default_display,"%s:0.0", argv[1]);
		else
			strcpy(default_display, argv[1]);
	}
	else
	{
		*default_display = 0;
	}

	/* Make sure the name is one we know */

	if (strcmp(argv[0], "S1") == 0)
		this_labao = S1;
	else if (strcmp(argv[0], "S2") == 0)
		this_labao = S2;
	else if (strcmp(argv[0], "E1") == 0)
		this_labao = E1;
	else if (strcmp(argv[0], "E2") == 0)
		this_labao = E2;
	else if (strcmp(argv[0], "W1") == 0)
		this_labao = W1;
	else if (strcmp(argv[0], "W2") == 0)
		this_labao = W2;
	else
	{
		fprintf(stderr,"Unknown labao %s.\n",labao_name);
		exit(-1);
	}
	sprintf(labao_name,"LABAO_%2s",argv[0]);

	/* Title page. */

	ui_clear_screen();
	put_line("");

	sprintf(title,"%s 7.3",labao_name);
	center_line(title);
	put_line("");
	center_line("The CHARA Array");
	center_line("Center for High Angular Resolution Astronomy");
	center_line("Mount Wilson Observatory, CA 91023, USA");
	put_line("");
	center_line("Telephone: 1-626-796-5405");
	center_line("Fax: 1-626-796-6717");
	center_line("email: theo@chara.gsu.edu");
	center_line("WWW: http://www.chara.gsu.edu");
	put_line("");
	center_line("(C) This executable program is copyright.");
	wait_for_title();

	/* Initialize the user interface */

	TITLE = title;
	initialise_ui("labao_menus.ini","labao_help.ini",
		labao_name);
	set_user_close_function(labao_close);
	set_user_open_function(labao_open);

	/* We only ever use the NTP based clock */

	set_bypass_clock_driver(TRUE);

        /* Try and open up the clock */

	if (!open_clock_device()) error(FATAL,"Failed to open clock device.");

	/* Setup background job(s) */

	setup_background_jobs();

        /* Set up the client port */

        setup_standard_clock_messages();
	setup_standard_labao_messages();
	setup_astromod_messages();

	/* Let's go! */

	start_ui(); /* Should never return from here. */

	exit(0);

} /* main() */

/************************************************************************/
/* labao_open()								*/
/*									*/
/************************************************************************/

void labao_open(void) 
{
	/* Open display */

	if (initX(NULL) < 0) error(ERROR,"Failed to open X windows display.");

	/* Initialize the zernike stuff */

	setup_zernike();

	/* Setup the astromod with default values */

        astromod_init(NULL, NULL);
        set_no_star(0, NULL);
        use_astromod_lib = TRUE;

#warning We may one day need a callback for this
	//set_star_callback(labao_star_callback);

	/* Start the background */

	background_start(0, NULL);

	/* Clear lost ticks etc */

	clear_lost_counts(0, NULL);

	/* Open connection to DM controller */

	open_edac40(0, NULL);
	
	/* Can we find a USB camera to talk to? */

	open_usb_camera();

	/* Initialize the finite state machine */

	initialize_fsm();

	/* Make sure we have the right defaults */

	call_load_defaults(0, NULL);

	/* Load the reconstructor */

	call_load_reconstructor(0,NULL);

	/* Set the USB callback function */

	set_usb_camera_callback(run_centroids_and_fsm);

        /* Turns out we need a fast response */

#warning Not sure we really need this yet
        //set_process_all_clients_in_background(TRUE, TRUE);
        
	open_pico_connection(0, NULL);

	/* Open telescope server if we can */

	open_telescope_connection(0, NULL);

	/* Open iris server if we can */

	open_iris_connection(0, NULL);

	/* Set default frame rate */

	set_frame_rate(DEFAULT_FRAME_RATE);

	/* Try and open the tiptilt comms */

	call_open_labao_tiptilt_data_socket(0, NULL);

}

/************************************************************************/
/* labao_close()							*/
/*									*/
/************************************************************************/

void labao_close(void) 
{
	background_off(0, NULL);
	message(system_window,"Closing down");
	send_labao_text_message("Closing down");
	background_stop(0, NULL);
	cleanup_zernike();
	message(system_window,"Stopping display");
	send_labao_text_message("Stopping display");
	stop_usb_camera_display(0, NULL);
	sleep(1);
	message(system_window,"Closing USB camera");
	send_labao_text_message("Closing USB camera");
	close_usb_camera();
	sleep(1);
	message(system_window,"Quiting X");
	send_labao_text_message("Quiting X");
	if (display_is_open) quitX();
	message(system_window,"Closing EDAC40");
	send_labao_text_message("Closing EDAC40");
	close_edac40(0, NULL);
	if (pico_server != -1) close_server_socket(pico_server);
	if (telescope_server != -1) close_server_socket(telescope_server);
	if (iris_server != -1) close_server_socket(iris_server);
}

/************************************************************************/
/* print_usage_message()                                                */
/*                                                                      */
/* Tells the user about any possible flags and arguments.               */
/************************************************************************/

void print_usage_message(char *name)
{

        fprintf(stderr,"usage: %s [-flags] name {display}\n",name);
        fprintf(stderr,"Flags:\n");
        fprintf(stderr,"-h\t\tPrint this message\n");
	fprintf(stderr,"-j\t\tSet maximum Zernike mode 1..%d (%d)\n", 
		NUM_LENSLETS, DEFAULT_MAXJ);
        fprintf(stderr,"-o\t\tToggle use outdate S2 HUT code (OFF)\n");
        fprintf(stderr,"-s\t\tToggle use of socketmanager (ON)\n");

} /* print_usage_message() */

/************************************************************************/
/* open_pico_connection()						*/
/*									*/
/* Open connection to PICO controller for M10.				*/
/************************************************************************/

int open_pico_connection(int argc, char **argv)
{
	int	i,j;
	char	dich[20];

	if (pico_server != -1) close_server_socket(pico_server);

	process_server_socket(pico_server);

	/* Go through the pico servers and try and find M10 mirror */

	sprintf(dich,"%sDICH", scope_types[this_labao]);
	for(i=0; i<NUM_PICO_SERVERS; i++)
	{
		pico_server = open_pico_server(pico_servers[i]);

		if (pico_server < 0)
		{
			message(system_window,
				"Failed to open picomotor server %s",
				pico_servers[i]);
			continue;
		}

		for(j=0; j < mirror_list[pico_server].n &&
			strcmpi(dich, mirror_list[pico_server].mirrors[j].name);
			j++);

		if (j  < mirror_list[pico_server].n) 
		{
			dich_mirror = j;
			break;
		}

		close_server_socket(pico_server);
		pico_server = -1;
	}

	if (pico_server == -1)
		return error(ERROR,"Failed to find pico motor server for %s",
			dich);

	message(system_window,"Using pico server %s.",pico_servers[i]);

	return NOERROR;

} /* int open_pico_connection() */

/************************************************************************/
/* open_telescope_connection()						*/
/*									*/
/* Open connection to telescope server.					*/
/************************************************************************/

int open_telescope_connection(int argc, char **argv)
{
	struct smessage mess;

	if (telescope_server != -1) close_server_socket(telescope_server);

        telescope_server = open_server_by_name(scope_types[this_labao]);

        if (telescope_server < 0)
        {
            telescope_server = -1;
           return error(ERROR, "Failed to open socket connect to telescope %s",
                        scope_types[this_labao]);
        }

	if (!add_message_job(telescope_server, 
		TELESCOPE_STATUS, message_telescope_status))
        {
                fprintf(stderr,"Failed to add server TELESCOPE_STATUS, job.\n");
                exit(-8);
        }

	if (!add_message_job(telescope_server, 
		HUT_AOB_CHANGE_DICHROIC, message_telescope_change_dichroic))
        {
                fprintf(stderr,"Failed to add server TELESCOPE_STATUS, job.\n");
                exit(-8);
        }

	message(system_window, "Opened connection to Telescope Server.");
	send_labao_text_message("Opened connection to Telescope Server.");

	/* Make sure we know which dichroic is in */

	mess.type = HUT_AOB_WHICH_DICHROIC;
	mess.length = 0;
	mess. data = NULL;

	send_message(telescope_server, &mess);

	return NOERROR;

} /* int open_telescope_connection() */

/************************************************************************/
/* message_telescope_status()                                           */
/*                                                                      */
/************************************************************************/

int message_telescope_status(int server, struct smessage *mess)
{
        struct s_scope_status *status;

        if (mess->length != sizeof(*status))
        {
            error(ERROR,"Got TELESCOPE_STATUS with bad data.");
            return TRUE;
        }

        status = (struct s_scope_status *)mess->data;

         telescope_status = *status;

	return TRUE;

} /* message_telescope_status() */

/************************************************************************/
/* open_iris_connection()						*/
/*									*/
/* Returns error level.							*/
/************************************************************************/

int open_iris_connection(int argc, char **argv)
{
	if (iris_server != -1) close_server_socket(iris_server);

        iris_server = open_server_by_name("iris");

        if (iris_server < 0)
        {
            iris_server = -1;
           return error(ERROR, "Failed to open socket connect to Iris.");
        }

	if (!add_message_job(iris_server, 
			IRIS_STATUS_MESSAGE, message_iris_status))
        {
                error(ERROR,"Failed to add server IRIS_STATUS_MESSAGE, job.\n");
                exit(-8);
        }

	message(system_window, "Opened connection to Iris Server.");
	send_labao_text_message("Opened connection to Iris Server.");

	return NOERROR;

} /* start_autoalign_lab_dichroic() */

/************************************************************************/
/* message_iris_status()                                                */
/*                                                                      */
/************************************************************************/

int message_iris_status(int server, struct smessage *mess)
{
	int	*status;

        if (mess->length != sizeof(*status))
        {
            error(ERROR,"Got IRIS_STATUS with bad data.");
            return TRUE;
        }

	status = (int *) mess->data;

	iris_at_beam_size = (*status == IRIS_STATUS_BEAM_SIZE);

	return TRUE;

} /* message_iris_status() */
