/************************************************************************/
/* labao_edac40.c							*/
/*                                                                      */
/* Toutines to talk to EDAC40 device for controlling the DM.		*/
/* Note that, judging by their code, channel DAC numbers start at 0 	*/
/* We will, when refering to specific actuators, start from 1.		*/
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

#define EDAC40_MAXN 10
#define EDAC40_DISCOVER_TIMEOUT 1500 // milliseconds
#define EDAC40_DISCOVER_ATTEMPTS 1

static edac40_list_node edac40_list[EDAC40_MAXN];
static int edac40_num_device = 0;
static SOCKET edac40_socket = -1;
static edac40_channel_value dac_data[NUM_ACTUATORS];
static float *actuator_value = NULL; /* Indexed from 1 range 0.0 to 1.0 */

/* 
 * Maps (actuator number-1) to DAC channel number See Theo's Log 9 page 7.
 * This is for cable 1 being far away from mirror and cable 2 being close.
 * The manual says it is the other way around, so there is a second mapping,
 * see page 12 of log book 9.
 */

//#define CABLE_1_FAR_FROM_MIRROR
#ifndef TEST_ACTUATOR_MAPPING
#ifdef CABLE_1_FAR_FROM_MIRROR
static int edac40_addr[NUM_ACTUATORS] =  {30,
		25, 26, 34, 38, 33, 29,
		22, 24, 27, 28, 35, 36, 39, 37, 32, 31, 21, 23,
		4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 15, 3, 5, 1, 2};
#else
static int edac40_addr[NUM_ACTUATORS] =  {10,
		5, 6, 14, 18, 13, 9,
		2, 4, 7, 8, 15, 16, 19, 17, 12, 11, 1, 3,
	24, 26, 27, 28, 29, 30, 31, 32, 33, 34, 36, 37, 38, 35, 23, 25, 21, 22};
#endif
#else
static int edac40_addr[NUM_ACTUATORS] =  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
				10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
				20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
				30, 31, 32, 33, 34, 35, 36, 37, 38, 39};
#endif

static pthread_mutex_t edac40_mutex = PTHREAD_MUTEX_INITIALIZER;

#warning Should send LABAO_VALUE_ALL_CHANNELS messages on focus etc changes.

/************************************************************************/
/* open_edac40()							*/
/*									*/
/* Attempts to open the right edac40 unit for this scope.		*/
/* Returns error level.							*/
/************************************************************************/

int open_edac40(int argc, char **argv)
{
	char	*ip;
	int	i;

	clean_command_line();

	/* Make the mutex so this will be thread safe */

	if (pthread_mutex_init(&edac40_mutex, NULL) != 0)
                error(FATAL, "Unable to create EDAC40 mutex.");

	/* First make sure it is closed */

	close_edac40(0, NULL);
	message(system_window,"Trying to open EDAC40 device.");

	/* Initialize */

	pthread_mutex_lock(&edac40_mutex);
	edac40_init();

	/* Find out which devices are available */

	edac40_num_device = edac40_list_devices(edac40_list,
		EDAC40_MAXN, EDAC40_DISCOVER_TIMEOUT, EDAC40_DISCOVER_ATTEMPTS);

	if (edac40_num_device == 0)
	{
		clean_command_line();
		pthread_mutex_unlock(&edac40_mutex);
		close_edac40(0, NULL);
		return error(ERROR,
			"Failed to find any EDAC40 devices on network.");
	}
	message(system_window,"Found %d EDAC40 Device(s) on the network.",
		edac40_num_device);
	
	/* Now let us search for ours */

	if ((ip = edac40_find_device(dac40_mac_address[this_labao])) == NULL)
	{
		clean_command_line();
		pthread_mutex_unlock(&edac40_mutex);
		close_edac40(0, NULL);
		return error(ERROR,
			"Failed to find EDAC40 device for %s.",
				scope_types[this_labao]);
	}

	/* Now try and open a connection to that device */

	if ((edac40_socket = edac40_open(ip, FALSE)) < 0)
	{
		clean_command_line();
		pthread_mutex_unlock(&edac40_mutex);
		close_edac40(0, NULL);
		return error(ERROR,
			"Failed to open EDAC40 device for %s.",
				scope_types[this_labao]);
	}

	/* Set the time out */

        edac40_set_timeout(edac40_socket, 1000);

	/* Setup the translation between channels and DAC numbers */

	actuator_value = vector(1, NUM_ACTUATORS);
	for(i = 0 ; i < NUM_ACTUATORS; i++)
	{
		dac_data[i].channel = edac40_addr[i];
		dac_data[i].value = 0;
		actuator_value[i+1] = 0.0;
	}

	/* Set everything to zero state */

	pthread_mutex_unlock(&edac40_mutex);
#warning This commened out to stop changing DM flat when restarting
	//edac40_send_current_voltages();

	/* That will do for now */

	return NOERROR;

} /* open_edac40() */

/************************************************************************/
/* close_edac40()							*/
/*									*/
/* Attempts to close the right edac40 unit for this scope.		*/
/* Returns error level.							*/
/************************************************************************/

int close_edac40(int argc, char **argv)
{
	/* Close down */

	pthread_mutex_lock(&edac40_mutex);
	if (edac40_socket > 0) edac40_close(edac40_socket);
	edac40_finish();

	/* That will do for now */

	edac40_socket = -1;

	pthread_mutex_unlock(&edac40_mutex);

	message(system_window,"EDAC40 connection is closed.");

	return NOERROR;

} /* close_edac40() */

/************************************************************************/
/* close_edac40_and_zero_dm()						*/
/*									*/
/* Forces the system to zero dm *and* close the edac40 connection.	*/
/************************************************************************/

int close_edac40_and_zero_dm(int argc, char **argv)
{
	int	i;

	/* Go for zero everywhere */

	if (actuator_value != NULL)
	{
		message(system_window,"Setting EDAC40 to all zeros.");
		usleep(500000);
		pthread_mutex_lock(&edac40_mutex);
		for(i = 1 ; i <= NUM_ACTUATORS; i++) actuator_value[i] = 0.0;
		pthread_mutex_unlock(&edac40_mutex);
		edac40_send_current_voltages();
		free_vector(actuator_value, 1, NUM_ACTUATORS);
		actuator_value = NULL;
	}

	return close_edac40(argc, argv);

} /* close_edac40_and_zero_dm() */

/************************************************************************/
/* edac40_is_open()							*/
/*									*/
/* Returns TRUE if the system thinks the conenction is open.		*/
/************************************************************************/

bool edac40_is_open(void)
{
	return (edac40_socket > 0);

} /* edac40_is_open() */

/************************************************************************/
/* edac40_send_current_voltages()					*/
/*									*/
/* Sends current voltages to the DAC. 					*/
/************************************************************************/

void edac40_send_current_voltages(void)
{
	char *buf;
	int  buf_len;
	int  i;
	float value;

	/* Do nothing if we are not open */

	if (edac40_socket < 0) return;

	if (actuator_value == NULL) return;

	/* Fill the data values */

	pthread_mutex_lock(&edac40_mutex);
	for(i=0 ; i < NUM_ACTUATORS; i++)
	{
		value = nearbyint(actuator_value[i+1] * 65535 + 0.5);
		if (value < 0.0)
			value = 0.0;
		else if (value > 65535.0)
			value = 65535.0;

		dac_data[i].value = (uint16_t)value;
	}

	/* Do it */

	buf_len = edac40_prepare_packet(dac_data, NUM_ACTUATORS, &buf);
	edac40_send_packet(edac40_socket, buf, buf_len);
	pthread_mutex_unlock(&edac40_mutex);
	free(buf);

} /* edac40_send_current_voltages() */

/************************************************************************/
/* edac40_set_single_channel()						*/
/*									*/
/* Tried to set a specific channel 1..37 to a specific value.		*/
/* Returns 0 if OK							*/
/* -1 if channel is out of range.					*/
/* -2 if EDAC conneciton is not open.					*/
/************************************************************************/

int edac40_set_single_channel(int channel, float value)
{
	if (channel < 1 || channel > NUM_ACTUATORS) return -1;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	actuator_value[channel] = value;
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	return 0;

} /* edac40_set_single_channel() */

/************************************************************************/
/* edac40_delta_single_channel()					*/
/*									*/
/* Tries to change one channel (1..37) by a delta.			*/
/* Returns 0 if OK							*/
/* -1 if channel is out of range.					*/
/* -2 if EDAC conneciton is not open.					*/
/************************************************************************/

int edac40_delta_single_channel(int channel, float delta)
{
	if (channel < 1 || channel > NUM_ACTUATORS) return -1;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	actuator_value[channel] += delta;
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	return 0;

} /* edac40_set_single_channel() */

/************************************************************************/
/* edac40_set_all_channels()						*/
/*									*/
/* Tried to set all channels to a specific value.			*/
/* Returns 0 if OK							*/
/* values array indexed from 1!						*/
/* -2 if no conection to edac unit.					*/
/************************************************************************/

int edac40_set_all_channels(float *values)
{
	int	i;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	for (i=1; i<=NUM_ACTUATORS; i++) actuator_value[i] = values[i];
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	return 0;

} /* edac40_set_all_channels() */

/************************************************************************/
/* edac40_delta_all_channels()						*/
/*									*/
/* Returns 0 if OK							*/
/* Delta array indexed from 1!						*/
/* -1 if channel is out of range.					*/
/* -2 if no conneciton to edac unit.					*/
/************************************************************************/

int edac40_delta_all_channels(float *deltas)
{
	int i;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	for (i = 1; i <= NUM_ACTUATORS; i++) actuator_value[i] += deltas[i];
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	return 0;

} /* edac40_set_all_channels() */

/************************************************************************/
/* call_edac40_set_single_channel()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_set_single_channel(int argc, char **argv)
{
	int	channel;
	float	value;
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&channel);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    1");
                if (quick_edit("DM Channel",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&channel);
        }

	if (channel < 1 || channel > NUM_ACTUATORS)
		return error(ERROR,"Channel %d is out of range 1..%d",
			channel, NUM_ACTUATORS);

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&value);
        }
        else
        {
                clean_command_line();
                sprintf(s,"%7.4f", actuator_value[channel]);
                if (quick_edit("Value",s,s,NULL,FLOAT)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&value);
        }

	if (edac40_set_single_channel(channel, value) < 0)
		return error(ERROR,"Failed to set channel %d to %d.",
			channel, value);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_set_single_channel() */

/************************************************************************/
/* call_edac40_delta_single_channel()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_delta_single_channel(int argc, char **argv)
{
	int	channel;
	float	value;
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&channel);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    1");
                if (quick_edit("DM Channel",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&channel);
        }

	if (channel < 1 || channel > NUM_ACTUATORS)
		return error(ERROR,"Channel %d is out of range 1..%d",
			channel, NUM_ACTUATORS);

        if (argc > 2)
        {
                sscanf(argv[2],"%f",&value);
        }
        else
        {
                clean_command_line();
                sprintf(s,"     0.0");
                if (quick_edit("Value",s,s,NULL,FLOAT)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&value);
        }

	if (edac40_delta_single_channel(channel, value) < 0)
		return error(ERROR,"Failed to delta channel %d by %d.",
			channel, value);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_delta_single_channel() */

/************************************************************************/
/* call_edac40_set_all_channels()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_set_all_channels(int argc, char **argv)
{
	int	i;
	float	value;
	float   values[NUM_ACTUATORS+1];
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&value);
        }
        else
        {
                clean_command_line();
                sprintf(s,"     0.0");
                if (quick_edit("Value",s,s,NULL,FLOAT)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&value);
        }

	for(i=1; i<= NUM_ACTUATORS; i++) 
		values[i] = value;

	if (edac40_set_all_channels(values) < 0)
		return error(ERROR,"Failed to set all channels %d.",
			value);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_set_all_channels() */

/************************************************************************/
/* call_edac40_delta_all_channels()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_delta_all_channels(int argc, char **argv)
{
	int	i;
	float	value;
	float	deltas[NUM_ACTUATORS+1];
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&value);
        }
        else
        {
                clean_command_line();
                sprintf(s,"     0.0");
                if (quick_edit("Delta",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&value);
        }

	for(i = 1; i <= NUM_ACTUATORS; i++) deltas[i] = value;

	if (edac40_delta_all_channels(deltas) < 0)
	       return error(ERROR,"Failed to delta all channels by %d.", value);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_delta_all_channels() */

/************************************************************************/
/* edac40_current_value(int channel)					*/
/*									*/
/* Returns the current value of a specific channel (1..37).		*/
/* Returns -1 on error.							*/
/************************************************************************/

float edac40_current_value(int channel)
{
	if (actuator_value == NULL) return -1;

	if (channel < 1 || channel > NUM_ACTUATORS) return -2;

	return actuator_value[channel];

} /* edac40_current_value() */

#warning Fix locked up display from GUI.

/************************************************************************/
/* show_edac40_values()							*/
/*									*/
/* Shows the current values and will also let you type a command.	*/
/************************************************************************/

int show_edac40_values(int argc, char **argv)
{
	bool 	loop = TRUE;
	bool	command = FALSE;
	int	keystroke;
	float	values[NUM_ACTUATORS+1];
	int	i;
	struct smessage mess;

	if (active_socket != -1) 
		return error(NOERROR,"Only possible from main window.");

	if (actuator_value == NULL) 
		return error(ERROR,"Conenction to EDAC40 isn't open.");

	active_window = sub_main_window;

	clear_client_signal(LABAO_SIGNAL_EXIT);

	while(loop)
	{
		heading(heading_window,"EDAC40 VALUES", "");

		background();
		process_command_socket();
		data_connected();

		werase(sub_main_window);
		message(system_window,CONT_TEXT);

		mvwprintw(sub_main_window, 0, 26,"%+6.2f",
			edac40_current_value(23));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(24));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(25));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(26));

		mvwprintw(sub_main_window, 1, 23,"%+6.2f",
			edac40_current_value(22));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(10));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(11));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(12));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(27));

		mvwprintw(sub_main_window, 2, 20,"%+6.2f",
			edac40_current_value(21));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(9));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(3));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(4));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(13));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(28));

		mvwprintw(sub_main_window, 3, 17,"%+6.2f",
			edac40_current_value(20));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(8));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(2));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(1));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(5));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(14));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(29));

		mvwprintw(sub_main_window, 4, 20,"%+6.2f",
			edac40_current_value(37));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(19));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(7));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(6));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(15));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(30));

		mvwprintw(sub_main_window, 5, 23,"%+6.2f",
			edac40_current_value(36));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(18));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(17));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(16));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(31));

		mvwprintw(sub_main_window, 6, 26,"%+6.2f",
			edac40_current_value(35));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(34));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(33));
		wprintw(sub_main_window, "%+6.2f",
			edac40_current_value(32));

		wrefresh(sub_main_window);

		/* Send things to the outside world */

		process_message_socket_all_clients(TRUE);
		if (client_signal_set(LABAO_SIGNAL_READY_FOR_DISPLAY))
		{
			for(i=1; i<= NUM_ACTUATORS; i++) values[i] = 
				edac40_current_value(i);

			mess.type = LABAO_VALUE_ALL_CHANNELS;
			mess.length = (NUM_ACTUATORS+1) * sizeof(float);
			mess.data = (unsigned char *)values;

			for(i=0; i<MAX_CONNECTIONS; i++)
			{
				if (client_signal_set_single_client(i,
                                        LABAO_SIGNAL_READY_FOR_DISPLAY))
                            	{
					ui_send_message_by_index(i, &mess);
				}
			}
			clear_client_signal(LABAO_SIGNAL_READY_FOR_DISPLAY);
		}

		/* Was a key hit ? */

		process_message_socket_all_clients(TRUE);
		keystroke = -1;
		command = FALSE;
		if (kbhit())
		{
			keystroke = get_command();
			command = TRUE;
		}
		else
		{
			if (client_signal_set(LABAO_SIGNAL_EXIT))
                	{
				clear_client_signal(LABAO_SIGNAL_EXIT);
				keystroke = KEY_ESC;
                	}

		}
		if (keystroke == KEY_ESC)
			loop = FALSE;
		else if (command)
			command_processor(keystroke);
	}

	werase(system_window);
	wrefresh(system_window);

	return NOERROR;

} /* show_edac40_values() */

/************************************************************************/
/* edac40_single_square()						*/
/*									*/
/* Tries to send a sine wave to a single actuator.			*/
/************************************************************************/

int edac40_single_square(int argc, char **argv)
{
	int	channel;
	int	delay;
        char    s[81];
	bool	loop;
        rt_time_struct now;
	CHARA_TIME start;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&channel);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    1");
                if (quick_edit("DM Channel",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&channel);
        }

	if (channel < 1 || channel > NUM_ACTUATORS)
		return error(ERROR,"Channel %d is out of range 1..%d",
			channel, NUM_ACTUATORS);

        if (argc > 2)
        {
                sscanf(argv[2],"%d",&delay);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    2000");
                if (quick_edit("Delay in mS",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&delay);
        }

	loop = TRUE;
	while(loop)
	{
		edac40_set_single_channel(channel, 1.0);
	        message(system_window,
			"Value 1.0 on channel %d Hit <ESC> to stop.", channel);
		if (get_rt_time(&now) != 0) return error(ERROR,"No time!");
		start = now.time_now;
		do {
			if (get_rt_time(&now) != 0)
				return error(ERROR,"No time!");
			background();
		} while (now.time_now < start+delay);

		edac40_set_single_channel(channel, 0.0);
	        message(system_window,
			"Value 0.0 on channel %d Hit <ESC> to stop.", channel);

		if (kbhit() && get_command() == KEY_ESC) loop = FALSE;

		if (get_rt_time(&now) != 0) return error(ERROR,"No time!");
		start = now.time_now;
		do {
			if (get_rt_time(&now) != 0)
				return error(ERROR,"No time!"); background();
			background();
		} while (now.time_now < start+delay);
	}
	werase(system_window);
	wrefresh(system_window);

	return NOERROR;

} /* edac40_single_square() */

/************************************************************************/
/* edac40_cycle_square()						*/
/*									*/
/* Tries to send a sine wave to all actuators in turn.			*/
/************************************************************************/

int edac40_cycle_square(int argc, char **argv)
{
	int	channel;
	int	delay;
        char    s[81];
	char	*largv[3];
	char	argv2[34], argv1[34];

        if (argc > 1)
        {
                sscanf(argv[1],"%d",&delay);
        }
        else
        {
                clean_command_line();
                sprintf(s,"    2000");
                if (quick_edit("Delay in mS",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%d",&delay);
        }

	sprintf(argv2,"%d", delay);
	largv[0] = "sqedac";
	largv[1] = argv1;
	largv[2] = argv2;
	for(channel = 1; channel <= NUM_ACTUATORS; channel++)
	{
		sprintf(argv1,"%d", channel);
		edac40_single_square(3, largv);
		if (kbhit() && get_command() == KEY_ESC) break;
	}
	werase(system_window);
	wrefresh(system_window);

	return NOERROR;

} /* edac40_cycle_square() */

/************************************************************************/
/* send_labao_value_all_channels()					*/
/*									*/
/* Send current actuator values to one client, or all if argument is	*/
/* TRUE;								*/
/************************************************************************/

int send_labao_value_all_channels(bool send_to_all_clients)
{
	float	values[LABAO_NUM_ACTUATORS+1];
	int	i;
	struct smessage mess;

	if (actuator_value == NULL) return ERROR;

	for(i=1; i<= NUM_ACTUATORS; i++) values[i] = actuator_value[i];

	mess.type = LABAO_VALUE_ALL_CHANNELS;
	mess.length = (NUM_ACTUATORS+1) * sizeof(float);
	mess.data   = (unsigned char *)values;

	if (send_to_all_clients)
	{
		ui_send_message_all(&mess);
	}
	else
	{
		if (client_socket != -1) ui_send_message(client_socket, &mess);
	}

	return NOERROR;

} /* send_labao_value_all_channels() */

/************************************************************************/
/* exit_and_zero_dm()							*/
/*									*/
/* Sets DM to zero state and then exits.				*/
/************************************************************************/

int exit_and_zero_dm(int argc, char **argv)
{
	char	*sargv[2];

        if (argc > 1)
        {
                if (*argv[1]!='y' && *argv[1]!='Y') return NOERROR;
        }
        else if (ask_yes_no("Do you wish to exit the programme?","")
                        ==NO)
        {
                return NOERROR;
        }

	sargv[0] = "exit";
	sargv[1] = "y";

	close_edac40_and_zero_dm(0, NULL);

	return ui_end(2, sargv);

} /* exit_and_zero_dm() */

/************************************************************************/
/* edac40_mult_all_channels()						*/
/*									*/
/* Returns 0 if OK							*/
/************************************************************************/

int edac40_mult_all_channels(float mult)
{
	int i;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	for (i = 1; i <= NUM_ACTUATORS; i++) actuator_value[i] *= mult;
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	send_labao_text_message("Multiuplied DM position by %.3f.", mult);
	return 0;

} /* edac40_set_all_channels() */

/************************************************************************/
/* call_edac40_mult_all_channels()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_mult_all_channels(int argc, char **argv)
{
	float	mult;
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&mult);
        }
        else
        {
                clean_command_line();
                sprintf(s,"     1.0");
                if (quick_edit("Multiplier",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&mult);
        }

	if (edac40_mult_all_channels(mult) < 0)
	       return error(ERROR,
		"Failed to multiply all channels by %d.", mult);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_mult_all_channels() */

/************************************************************************/
/* edac40_add_all_channels()						*/
/*									*/
/* Returns 0 if OK							*/
/************************************************************************/

int edac40_add_all_channels(float add)
{
	int i;

	if (actuator_value == NULL) return -2;

	pthread_mutex_lock(&edac40_mutex);
	for (i = 1; i <= NUM_ACTUATORS; i++) actuator_value[i] += add;
	pthread_mutex_unlock(&edac40_mutex);
	edac40_send_current_voltages();

	send_labao_text_message("Added %.3f to DM position", add);

	return 0;

} /* edac40_set_all_channels() */

/************************************************************************/
/* call_edac40_add_all_channels()					*/
/*									*/
/* Calls the appropriate function. For testing.				*/
/************************************************************************/

int call_edac40_add_all_channels(int argc, char **argv)
{
	float	add;
        char    s[81];

	if (actuator_value == NULL) return NOERROR;

        /* Check out the command line */

        if (argc > 1)
        {
                sscanf(argv[1],"%f",&add);
        }
        else
        {
                clean_command_line();
                sprintf(s,"     1.0");
                if (quick_edit("Addition",s,s,NULL,INTEGER)
                        == KEY_ESC) return NOERROR;
                sscanf(s,"%f",&add);
        }

	if (edac40_add_all_channels(add) < 0)
	       return error(ERROR,
		"Failed to add %d to all channels.", add);

	return send_labao_value_all_channels(TRUE);

} /* call_edac40_add_all_channels() */
