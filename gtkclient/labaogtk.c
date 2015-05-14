/************************************************************************/
/* labaogtk.c                                                           */
/*                                                                      */
/* Main routine for LABAO  GTK client. 		                        */
/************************************************************************/
/*                                                                      */
/*                    CHARA ARRAY USER INTERFACE                        */
/*                 Based on the SUSI User Interface                     */
/*              In turn based on the CHIP User interface                */
/*                                                                      */
/*            Center for High Angular Resolution Astronomy              */
/*              Mount Wilson Observatory, CA 91001, USA                 */
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
/* Date   : April 2009   		                                */
/************************************************************************/

#include "labaogtk.h"

/* Globals */

int server = -1;
char display[256];	/* Name of display to use */
int server_open = FALSE;
int display_delay_ms = 0;
char send_ready_for_display = TRUE;
char do_local_display = TRUE;
#ifdef GTK2
int main_page = -1;
int status_page = -1;
int config_page = -1;
int dm_page = -1;
int wfs_page = -1;
#endif
GtkWidget *notebook = NULL;
char	server_name[56];
#warning Think more about engineering mode
bool	engineering_mode = FALSE;
struct s_labao_usb_camera usb_camera;
float *x_centroid_offsets = NULL;
float *y_centroid_offsets = NULL;
float *xc = NULL;
float *yc = NULL;
float *avg_fluxes = NULL;
struct s_labao_wfs_results wfs_results;
GtkWidget *message_label;

/* Global for this file only */

static char myname[256];
static char	host[256];
static int	port = 0;
static char	skip_socket_manager = FALSE;
static struct astro_init_window_data init_info;
static GtkWidget *tilt_x_label;
static GtkWidget *tilt_y_label;
#ifdef SHOW_POS
static GtkWidget *pos_label;
#endif
static GtkWidget *focus_label;
static GtkWidget *a1_label;
static GtkWidget *a2_label;
static GtkWidget *fsm_state_label;
static char plot_aber = FALSE;
static scope aber_scope;
static GtkWidget *aber_button[5];

int main(int  argc, char *argv[] )
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *bigvbox;
	char	s[200];
	char	*p;
	char	*progname;
	GtkWidget *label;
	char	*c;
	GtkWidget *button;
	GtkWidget *hbox;

	/* Let GTK parse command line */

	gtk_init (&argc, &argv);

	/* See if we have any local arguments */

        progname = argv[0];
        p = s;
        while(--argc > 0 && (*++argv)[0] == '-')
        {
             for(p = argv[0]+1; *p != '\0'; p++)
             {
                switch(*p)
                {
                        case 'h': print_usage_message(progname);
                                  exit(0);

			case 'D':if (sscanf(p+1,"%d",&display_delay_ms) != 1)
                                  {
                                    print_usage_message(progname);
                                    exit(-1);
                                  }
                                  if (display_delay_ms < 0)
                                  {
                                        fprintf(stderr,
                                                "Display delay must be > 0.\n");
                                        exit(-1);
                                  }
                                  while(*p != '\0') p++; p--;
                                  break;

			case 'd': do_local_display = FALSE;
                                 break;

			case 'e': engineering_mode = TRUE;
                                 break;

			case 'p': strcpy(host, p+1);
				  for(c=host; *c!=0 && *c!=','; c++);
				  if (*c == 0 || sscanf(c+1,"%d", &port) != 1)
                                  {
                                    print_usage_message(progname);
                                    exit(-1);
                                  }
				  *c = 0;
				  skip_socket_manager = TRUE;
                                  while(*p != '\0') p++; p--;
                                  break;

			default: fprintf(stderr,"Unknown flag %c.\n",*p);
                                 print_usage_message(progname);
                                 exit(-1);
                                 break;
                }
	    }
	}

	/* Are there enough arguments left? */

	 if (argc != 1)
        {
                print_usage_message(progname);
                exit(-1);
        }

        /* What is the name of our server? */

        sprintf(server_name,"LABAO_%s", argv[0]);

	/* Which machine are we using? */

	if (gethostname(myname, 256) < 0)
	{
		fprintf(stderr,"Failed to get host name.\n");
		exit(-2);
	}
	if (strlen(display)==0) strcpy(myname,"localhost");

	/* Which display should we use? */

	if ((p = getenv("DISPLAY")) == NULL || *p == ':')
	{
		sprintf(display,"%s:0.0",myname);
	}
	else
	{
		strcpy(display, p);
	}

	/* Initialise mesage jobs */

	init_jobs();

	/* Set out name */

	set_client_name(server_name);

	/* Create a new window */

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	/* Set name of window */

	gtk_window_set_title (GTK_WINDOW (window), server_name);

	/* Set a handler for delete_event that immediately exits GTK. */

	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			GTK_SIGNAL_FUNC (delete_event), NULL);

	/* We will need a big vbox in which to place things */

	bigvbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (window), bigvbox);
        gtk_widget_show(bigvbox);

	/* Create a note book to use for this */

	notebook =  gtk_notebook_new();
        gtk_container_add (GTK_CONTAINER (bigvbox), notebook);
        gtk_widget_show(notebook);

	/* OK, let's make the main page */

	vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new("MAIN");
#ifdef GTK2
	main_page = 
#endif
		gtk_notebook_append_page((GtkNotebook *)notebook, 
		vbox, label);
        gtk_widget_show(vbox);

	/* A place for error messages */

	fill_status_view((GtkWidget *)vbox, 0, 100);

	/* This means we will see messages as they come in. */

        set_status_page(notebook, main_page);

	/* Now some labels we see on all pages */

	vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (bigvbox), vbox);
        gtk_widget_show(vbox);

	message_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(vbox), message_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (message_label, LABAO_WIDTH, LABAO_HEIGHT);
        gtk_widget_show(message_label);

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (vbox), hbox);
        gtk_widget_show(hbox);

	wfs_results.xtilt = 0.0;
	wfs_results.ytilt = 0.0;
	wfs_results.xpos = 0.0;
	wfs_results.ypos = 0.0;
	wfs_results.focus = 0.0;
	wfs_results.a1 = 0.0;
	wfs_results.a2 = 0.0;
	wfs_results.fsm_state = 0;

	aber_button[0] = gtk_check_button_new_with_label("");
	gtk_box_pack_start(GTK_BOX(hbox), aber_button[0], TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (aber_button[0]), 1);
	gtk_widget_set_usize (aber_button[0],LABAO_WIDTH/20, LABAO_HEIGHT);
        gtk_toggle_button_set_active((GtkToggleButton *)aber_button[0], 1);
	gtk_widget_show (aber_button[0]);

	tilt_x_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), tilt_x_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (tilt_x_label, 3*LABAO_WIDTH/20, LABAO_HEIGHT);
        gtk_widget_show(tilt_x_label);

	aber_button[1] = gtk_check_button_new_with_label("");
	gtk_box_pack_start(GTK_BOX(hbox), aber_button[1], TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (aber_button[1]), 1);
	gtk_widget_set_usize (aber_button[1],LABAO_WIDTH/20, LABAO_HEIGHT);
        gtk_toggle_button_set_active((GtkToggleButton *)aber_button[1], 1);
	gtk_widget_show (aber_button[1]);

	tilt_y_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), tilt_y_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (tilt_y_label, 3*LABAO_WIDTH/20, LABAO_HEIGHT);
        gtk_widget_show(tilt_y_label);

#ifdef SHOW_POS
	pos_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), pos_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (pos_label, LABAO_WIDTH/20, LABAO_HEIGHT);
        gtk_widget_show(pos_label);
#endif

	aber_button[2] = gtk_check_button_new_with_label("");
	gtk_box_pack_start(GTK_BOX(hbox), aber_button[2], TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (aber_button[2]), 1);
	gtk_widget_set_usize (aber_button[2],LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_toggle_button_set_active((GtkToggleButton *)aber_button[2], 1);
	gtk_widget_show (aber_button[2]);

	focus_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), focus_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (focus_label, 3*LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_widget_show(focus_label);

	aber_button[3] = gtk_check_button_new_with_label("");
	gtk_box_pack_start(GTK_BOX(hbox), aber_button[3], TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (aber_button[3]), 1);
	gtk_widget_set_usize (aber_button[3],LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_toggle_button_set_active((GtkToggleButton *)aber_button[3], 1);
	gtk_widget_show (aber_button[3]);

	a1_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), a1_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (a1_label, 3*LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_widget_show(a1_label);

	aber_button[4] = gtk_check_button_new_with_label("");
	gtk_box_pack_start(GTK_BOX(hbox), aber_button[4], TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (aber_button[4]), 1);
	gtk_widget_set_usize (aber_button[4],LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_toggle_button_set_active((GtkToggleButton *)aber_button[4], 1);
	gtk_widget_show (aber_button[4]);

	a2_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), a2_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (a2_label, 3*LABAO_WIDTH/24, LABAO_HEIGHT);
        gtk_widget_show(a2_label);

	fsm_state_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), fsm_state_label , TRUE, TRUE, 0);
        gtk_widget_set_usize (fsm_state_label, LABAO_WIDTH/6, LABAO_HEIGHT);
        gtk_widget_show(fsm_state_label);

	update_wfs_results();

	/* Now some buttons we see on all pages */

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (vbox), hbox);
        gtk_widget_show(hbox);

	/* Creates a ping button */

	button = gtk_button_new_with_label ("PING");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (ping_callback), (gpointer) &server);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (button), 1);
	gtk_widget_set_usize (button, LABAO_WIDTH/8, LABAO_HEIGHT);
	gtk_widget_show(button);

        /* Creates a reopen button */

        button = gtk_button_new_with_label ("REOPEN");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (reopen_socket_callback), (gpointer) &server);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (button), 1);
        gtk_widget_set_usize(button, LABAO_WIDTH/8, LABAO_HEIGHT);
        gtk_widget_show(button);

        /* Creates a clear display button */

        button = gtk_button_new_with_label ("CLEAR DISP");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (clear_display_callback), (gpointer) &server);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (button), 1);
        gtk_widget_set_usize(button, LABAO_WIDTH/8, LABAO_HEIGHT);
        gtk_widget_show(button);

        /* Add a load defaults button */

        button = gtk_button_new_with_label ("LOAD DEFAULTS");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_LOAD_DEFAULTS));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize(button, LABAO_WIDTH/8, LABAO_HEIGHT);
        gtk_widget_show(button);

        /* Add a quit button */

        button = gtk_button_new_with_label ("QUIT");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                        GTK_SIGNAL_FUNC (delete_event), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize(button, LABAO_WIDTH/8, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Open the sockets must happen after this and before everything else.*/

        reopen_socket_callback(NULL, NULL);

	/* make the DM page */

	vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new("DM");
#ifdef GTK2
	dm_page = 
#endif
		gtk_notebook_append_page((GtkNotebook *)notebook, 
		vbox, label);
	fill_dm_page(vbox);

	/* make the WFS page */

	vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new("WFS");
#ifdef GTK2
	wfs_page = 
#endif
		gtk_notebook_append_page((GtkNotebook *)notebook, 
		vbox, label);
	fill_wfs_page(vbox);

	/* make the status page */

	vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new("STATUS");
#ifdef GTK2
	status_page = 
#endif
		gtk_notebook_append_page((GtkNotebook *)notebook, 
		vbox, label);
	fill_ui_status_page(vbox);
	set_ui_status_callback(status_page_callback);

	/* Hbox for a few buttons on the status page */

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (vbox), hbox);
        gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("START UI STATUS");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (start_ui_status), 
                (gpointer)(&server));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/2, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("STOP UI STATUS");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (stop_ui_status), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/2, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* make the configuration page */

	vbox = gtk_vbox_new(FALSE, 0);
	label = gtk_label_new("CONFIGURE");
#ifdef GTK2
	config_page = 
#endif
		gtk_notebook_append_page((GtkNotebook *)notebook, 
		vbox, label);

	/* Both the star and config need to go in here */

        hbox = gtk_hbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (vbox), hbox);
        gtk_widget_show(hbox);
        gtk_widget_show(vbox);

	/* First the star */

	vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (hbox), vbox);
	fill_astrogtk_control_page(vbox, server);

	/* We put the reference cart at the bottom here */

	fill_astrogtk_ref(vbox, (gpointer) &init_info);

	/* Now the configuration */

	vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (hbox), vbox);
	fill_astrogtk_beams_pops(vbox, (gpointer) &init_info);

	/* We need buttons to force getting and sending all the information */

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add (GTK_CONTAINER (hbox), vbox);
        gtk_widget_show(vbox);

	/* Get configuration */

        button = gtk_button_new_with_label ("GET");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (get_and_display_star_and_config_callback), 
		(gpointer)&server);
        gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 0, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("SEND");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (send_star_and_config_callback), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 0, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Try and ensure we're up to date */

	get_and_display_star_and_config_callback(NULL, (gpointer)&server);

	/* and show the window */

	gtk_widget_show (window);

	/* Now start up the background process. */

        gtk_timeout_add(10, background_code, 0);

/*
	if (strcmp(display, "localhost:0.0")?initX(display):initX(NULL) == 0)
*/
	if (initX(NULL) == 0)
	{
		fprintf(stderr,"Failed to open X windows on %s.\n", display);
		exit(-10);
	}

	/* Rest in gtk_main and wait for the fun to begin! */

	gtk_main ();

	return(0);
}

/************************************************************************/
/* delete_event()							*/
/*									*/
/* Callback function for program exit.					*/
/************************************************************************/

gint delete_event( GtkWidget *widget, GdkEvent  *event, gpointer   data )
{
	gtk_main_quit();
	close_server_socket(server);
	quitX();
	return(FALSE);

} /* delete_event() */

/************************************************************************/
/* background_code()							*/
/*									*/
/* Background code, deals with incomming messages.			*/
/************************************************************************/

gint background_code(gpointer data)
{
        struct timeval stime; /* System and RT time. */
        long  time_now_ms = 0;
        static long time_next_display = 0;
        static long start_time_sec = 0;

	/* Process the message queue */

	process_server_socket_all_messages(server);

        /* What time is it now? */

        gettimeofday(&stime, NULL);
        if (start_time_sec == 0) start_time_sec = stime.tv_sec;
        stime.tv_sec -= start_time_sec;

        time_now_ms = stime.tv_sec*1000 + stime.tv_usec/1000;

        /* Let the server know we are ready to display things */

        if (do_local_display &&
                time_now_ms >= time_next_display && send_ready_for_display)
        {
                send_ui_signal(server, LABAO_SIGNAL_READY_FOR_DISPLAY);
                time_next_display = time_now_ms + display_delay_ms;
                send_ready_for_display = FALSE;
        }

	return TRUE;
}

/************************************************************************/
/* reopen_socket_callback()                                             */
/*                                                                      */
/* Tries to reopen the socket to the server.                            */
/************************************************************************/

void reopen_socket_callback(GtkButton *button, gpointer user_data)
{
	struct smessage mess;

        /* Close the port */

        close_server_socket(server);

        /* Try and open the port */

	if (skip_socket_manager)
	{
            if ((server = open_server_socket(host, port, server_name)) == -1)
            {
                if (!server_open)
                {
                        fprintf(stderr,"Failed to open %s on %s/%d.\n",
				server_name, host, port);
                        exit(-2);
                }
                else
                        print_status(ERROR,"Failed to open socket %s.\n",
				server_name);
            }
	}
	else
	{
            if ((server = open_server_by_name(server_name)) == -1)
            {
                if (!server_open)
                {
                        fprintf(stderr,"Failed to open socket %s.\n",
				server_name);
                        exit(-2);
                }
                else
                        print_status(ERROR,"Failed to open socket %s.\n",
				server_name);
            }
	}

        /* Setup up clock message servers */

        if (!setup_clock_messages(server)) 
        {
                if (!server_open)
                {
                        fprintf(stderr,"Could not setup clock messages.");
                        exit(-7);
                }
                else
                        print_status(ERROR,"Failed to open socket.\n");
        }

        /* Setup astromod messages */

        if (!setup_astro_messages(server, FALSE))
        {
                if (!server_open)
                {
                        fprintf(stderr,"Could not setup astro messages.");
                        exit(-7);
                }
                else
                        print_status(ERROR,"Failed to open socket.\n");
        }

        /* Setup our other messages */

        set_labao_messages();

	/* OK, ask for the current DM actuator positions */

	mess.type = LABAO_VALUE_ALL_CHANNELS;
	mess.length = 0;
	mess.data = NULL;
	if (!send_message(server, &mess))
	{
	    if (!server_open)
		fprintf(stderr,"Failed to send message.\n");
	    else
		print_status(ERROR,"Failed to send message.\n");
	}
	
	/* OK, ask for the current usb camera setup */

	usb_camera.num_lenslets = 0;
	mess.type = LABAO_GET_USB_CAMERA;
	mess.length = 0;
	mess.data = NULL;
	if (!send_message(server, &mess))
	{
	    if (!server_open)
		fprintf(stderr,"Failed to send message.\n");
	    else
		print_status(ERROR,"Failed to send message.\n");
	}
	
        /* OK, if display is on tell server we are ready */

        if (do_local_display) send_ready_for_display = TRUE;

        /* OK, we're done */

        server_open = TRUE;

} /* reopen_socket_callback() */

/************************************************************************/
/* labao_message_callback()                                            */
/*                                                                      */
/* A callback that sends a signal with no data. 			*/
/************************************************************************/

void labao_message_callback(GtkButton *widget, gpointer type)
{
        struct smessage mess;

	mess.type = *((short int *)type);
	mess.length = 0;
	mess.data = NULL;
	if (!send_message(server, &mess))
		print_status(ERROR,"Failed to send message.\n");

} /* labao_message_callback() */

/************************************************************************/
/* labao_signal_callback()                                              */
/*                                                                      */
/* Send a signal to the server.                                         */
/************************************************************************/

void labao_signal_callback(GtkButton *button, gpointer signal)
{
        /* Send message to server to start doing things */

        send_ui_signal(server, *((unsigned char *)signal));

} /* labao_signal_callback() */

/************************************************************************/
/* clear_display_callback()                                             */
/*                                                                      */
/* Forces current page to be status page when new status information    */
/* arrives.                                                             */
/************************************************************************/

void clear_display_callback(void)
{
        send_ready_for_display = TRUE;

} /* clear_display_callback() */

/************************************************************************/
/* status_page_callback()                                               */
/*                                                                      */
/* Forces current page to be status page when new status information    */
/* arrives.                                                             */
/************************************************************************/

void status_page_callback(void)
{
#ifdef GTK2
        gtk_notebook_set_current_page((GtkNotebook *)notebook, status_page);
#endif

} /* status_page_callback() */

/************************************************************************/
/* print_usage_message()                                                */
/*                                                                      */
/************************************************************************/

void print_usage_message(char *name)
{
        fprintf(stderr,"usage: %s [-flags] SCOPE\n",name);
        fprintf(stderr,"Flags:\n");
        fprintf(stderr,"-d\t\tTurn off local displays\n");
        fprintf(stderr,"-D[mS]\t\tSet delay in mS between displays (0)\n");
        fprintf(stderr,"-e\t\tUse engineering mode\n");
        fprintf(stderr,"-h\t\tPrint this message\n");
        fprintf(stderr,"-p[host,port]\tSkip socket manager\n");

} /* print_usage_message() */

/************************************************************************/
/* update_wfs_results()							*/
/*									*/
/************************************************************************/

void update_wfs_results(void)
{
	char	s[1234];
	GtkStyle* style;
        GdkColor color_green = {4L,0,65535,0};
        GdkColor color_red = {4L,65535,0,0};
        GdkColor color_dark_blue = {4L,0,0,65535};
        GdkColor color_purple = {4L,65535,0,65535};
        GdkColor color_light_blue = {4L,0,65535,65535};
	float	*values;

	style = gtk_style_copy (gtk_widget_get_style (tilt_x_label));
	style->fg[GTK_STATE_NORMAL] = color_red;
        style->fg[GTK_STATE_PRELIGHT] = color_red;
        gtk_widget_set_style (tilt_x_label, style);
	sprintf(s,"X: %+6.3f", wfs_results.xtilt);
	gtk_label_set_text((GtkLabel *) tilt_x_label, s);

	style = gtk_style_copy (gtk_widget_get_style (tilt_y_label));
	style->fg[GTK_STATE_NORMAL] = color_green;
        style->fg[GTK_STATE_PRELIGHT] = color_green;
        gtk_widget_set_style (tilt_y_label, style);
	sprintf(s,"Y: %+6.3f", wfs_results.ytilt);
	gtk_label_set_text((GtkLabel *) tilt_y_label, s);

#ifdef SHOW_POS
	sprintf(s,"%+6.3f/%+6.3f", wfs_results.xpos, wfs_results.ypos);
	gtk_label_set_text((GtkLabel *) pos_label, s);
#endif

	style = gtk_style_copy (gtk_widget_get_style (focus_label));
	style->fg[GTK_STATE_NORMAL] = color_purple;
        style->fg[GTK_STATE_PRELIGHT] = color_purple;
        gtk_widget_set_style (focus_label, style);
	sprintf(s,"Foc: %+6.3f", wfs_results.focus);
	gtk_label_set_text((GtkLabel *) focus_label, s);

	style = gtk_style_copy (gtk_widget_get_style (a1_label));
	style->fg[GTK_STATE_NORMAL] = color_light_blue;
        style->fg[GTK_STATE_PRELIGHT] = color_light_blue;
        gtk_widget_set_style (a1_label, style);
	sprintf(s,"A1: %+6.3f", wfs_results.a1);
	gtk_label_set_text((GtkLabel *) a1_label, s);

	style = gtk_style_copy (gtk_widget_get_style (a2_label));
	style->fg[GTK_STATE_NORMAL] = color_dark_blue;
        style->fg[GTK_STATE_PRELIGHT] = color_dark_blue;
        gtk_widget_set_style (a2_label, style);
	sprintf(s,"A2: %+6.3f", wfs_results.a2);
	gtk_label_set_text((GtkLabel *) a2_label, s);

	switch (wfs_results.fsm_state)
        {
                case FSM_CENTROIDS_ONLY:
                        gtk_label_set_text((GtkLabel *)fsm_state_label,
				"WFS only");
                        break;
                case FSM_ZERO_CENTROIDS:
                        gtk_label_set_text((GtkLabel *)fsm_state_label,
				"Zero Cent");
                        break;
                case FSM_MEASURE_RECONSTRUCTOR:
			gtk_label_set_text((GtkLabel *) fsm_state_label, 
				"Reconst");
                        break;
                case FSM_SERVO_LOOP:
                        gtk_label_set_text((GtkLabel *)fsm_state_label,
				"Servo ON");
                        break;
                default:
                        gtk_label_set_text((GtkLabel *)fsm_state_label,
				"UNKNOWN");
        }

	if (plot_aber)
	{
		values = vector(1, 6);

		values[6] = 0.0;

		if (gtk_toggle_button_get_active(
		   (GtkToggleButton *)aber_button[0]))
		{
			values[1] =  wfs_results.xtilt;
		}
		else
			values[1] = 0.0;

		if (gtk_toggle_button_get_active(
		   (GtkToggleButton *)aber_button[1]))
			values[2] =  wfs_results.ytilt;
		else
			values[2] = 0.0;

		if (gtk_toggle_button_get_active(
		   (GtkToggleButton *)aber_button[2]))
			values[3] =  wfs_results.focus;
		else
			values[3] = 0.0;

		if (gtk_toggle_button_get_active(
		   (GtkToggleButton *)aber_button[3]))
			values[4] =  wfs_results.a1;
		else
			values[4] = 0.0;

		if (gtk_toggle_button_get_active(
		   (GtkToggleButton *)aber_button[4]))
			values[5] =  wfs_results.a2;
		else
			values[5] = 0.0;

		update_scope(aber_scope, values);
		free_vector(values, 1, 6);
	}

} /* update_wfs_results() */

/************************************************************************/
/* labao_plot_aber_callback()                                           */
/*                                                                      */
/************************************************************************/

void labao_plot_aber_callback(GtkButton *button, gpointer user_data)
{

	if (plot_aber)
	{
		/* We turn things off */

		close_scope(&aber_scope);
		XFlush(theDisplay);
		plot_aber = FALSE;
	}
	else
	{
		aber_scope = open_scope(server_name, 100, 100, 400, 200);

		add_signal(&aber_scope, PLOT_RED, -0.75, 0.75);
		add_signal(&aber_scope, PLOT_GREEN, -0.75, 0.75);
		add_signal(&aber_scope, PLOT_PURPLE, -0.75, 0.75);
		add_signal(&aber_scope, PLOT_LIGHT_BLUE, -0.75, 0.75);
		add_signal(&aber_scope, PLOT_DARK_BLUE, -0.75, 0.75);
		add_signal(&aber_scope, PLOT_WHITE, -0.75, 0.75);

		plot_aber = TRUE;
	}

} /* labao_plot_aber_callback() */
