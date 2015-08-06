/************************************************************************/
/* labao_tiptilt_data_socket.c						*/
/*                                                                      */
/* Routines to talk to the WFS data socket.				*/
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
/* Author : Theo ten Brummelaar			                        */
/* Date   : June 2015						*/
/************************************************************************/

static int labao_tiptilt_data_socket = -1;
static char displayed_comms_error_message = 0;

#include "labao.h"
#include <errno.h>
#include <stdio.h>
#include <curses.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>
#include <netdb.h>
#include <resolv.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/tcp.h>

/************************************************************************/
/* send_tiptilt_on()							*/
/************************************************************************/

int send_tiptilt_on(int argc, char **argv)
{
	if (call_open_labao_tiptilt_data_socket(0, NULL) != NOERROR)
		return ERROR;

	send_tiptilt = TRUE;

	message(system_window,"Tiptilt send is ON.");
	send_labao_text_message("Tiptilt send is ON.");
	
	return NOERROR;

} /* send_tiptilt_on() */

/************************************************************************/
/* send_tiptilt_off()							*/
/************************************************************************/

int send_tiptilt_off(int argc, char **argv)
{
	send_tiptilt = FALSE;

	message(system_window,"Tiptilt send is OFF.");
	send_labao_text_message("Tiptilt send is OFF.");
	
	return NOERROR;

} /* send_tiptilt_off() */

/************************************************************************/
/* call_open_labao_tiptilt_data_socket()				*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_open_labao_tiptilt_data_socket(int argc, char **argv)
{
	char	name[1234];

	sprintf(name,"wfs_%s", scope_types[this_labao]);

	return open_labao_tiptilt_data_socket(name);

} /* call_open_labao_tiptilt_data_socket() */

/************************************************************************/
/* open_labao_tiptilt_data_socket()					*/
/*									*/
/* Opens a socket so we can start sending tiptilt data to the outside   */
/* world.								*/
/* Returns error level.							*/
/************************************************************************/

int open_labao_tiptilt_data_socket(char *wfs_name)
{
	struct s_process_sockets process;
        struct sockaddr_in server;      /* Server address */
        struct hostent *hp;             /* Pointer to host data */
        int     sock;                   /* Socket number */
        int     nagle;
        struct timeval timeout;
	//char	name[567];

	message(system_window,
		"Trying to open data socket to %s Port %d.",
		wfs_name, TIPTILT_DATA_PORT);

	if (labao_tiptilt_data_socket != -1) close_labao_tiptilt_data_socket();

	/* Find out which machine and socket */

	if ((sock = socket_manager_get_process(wfs_name, &process)) != 0) 
        {
		labao_tiptilt_data_socket = -1;
		return error(ERROR,"Process %s does not seem to exist (%d).",
			wfs_name, sock);
        }

        /* First see if we can identify the host. */

        if((hp = gethostbyname(process.machine)) == 0)
        {
                return error(ERROR,"Host %s unknown.",process.machine);
        }

        /* Create a socket */

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
                return error(ERROR,"Failed to create socket.");
        }

        /* Force things to time out after 5 seconds */

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                (char *)&timeout, sizeof(timeout)) < 0)
        {
                close(sock);
                return error(ERROR,"Failed to change receive time out\n");
        }

        if (setsockopt(sock , SOL_SOCKET, SO_SNDTIMEO,
                (char *)&timeout, sizeof(timeout)) < 0)
        {
                close(sock);
                return error(ERROR,"Failed to change send time out\n");
        }

	/* Disable Nangles */

	nagle = 1;
	if (setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,
		(char *)&nagle, sizeof(nagle) ))
	{
		close(sock);
		return error(ERROR,"Failed to turn off Nagle\n");
	}

        /* Create server address */

        server.sin_family = AF_INET;
        memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);
        server.sin_port = htons(TIPTILT_DATA_PORT);

        /* Let's try and connect now.... */

        if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
        {
                close(sock);
                return error(ERROR,
                        "Failed to connect to %s port %d.",
			process.machine, TIPTILT_DATA_PORT);
        }

        /* All is well it seems */

	labao_tiptilt_data_socket = sock;
	message(system_window,
		"Opened tiptilt data socket to %s machine %s Port %d.",
		wfs_name, process.machine, TIPTILT_DATA_PORT);
	send_labao_text_message(
		"Opened tiptilt data socket to %s machine %s Port %d.",
		wfs_name, process.machine, TIPTILT_DATA_PORT);

	/* We have not displayed this message yet. */

	displayed_comms_error_message = 0;

	/* That is all */

	return send_labao_tiptilt_data(0.0, 0.0);

} /* open_labao_tiptilt_data_socket() */

/************************************************************************/
/* close_labao_tiptilt_data_socket()					*/
/*									*/
/* CLoses tiptilt data socket.						*/
/* Returns error level.							*/
/************************************************************************/

int close_labao_tiptilt_data_socket(void)
{
	if (labao_tiptilt_data_socket != -1)
	{
		close(labao_tiptilt_data_socket);
		labao_tiptilt_data_socket = -1;
		message(system_window,"Closed tip_tilt connection.");
	}

	/* We have not displayed this message yet. */

	displayed_comms_error_message = 0;

	return NOERROR;

} /* close_labao_tiptilt_data_socket() */

/************************************************************************/
/* send_labao_tiptilt_data()						*/
/*									*/
/* Sends tiptilt data to the right port.				*/
/* Returns error level.							*/
/************************************************************************/

int send_labao_tiptilt_data(float Az, float El)
{
	short int data[2];

	if (!send_tiptilt) return NOERROR;

	if (labao_tiptilt_data_socket == -1)
	{
		if (!displayed_comms_error_message)
		{
			error(ERROR,"Tiptilt data socket is not open.");
			displayed_comms_error_message = 1;
		}
		return ERROR;
	}

	data[0] = (int)(16382.0 * Az);
	data[1] = (int)(16382.0 * El);

	/* Send the data */

	if (!client_write_ready(labao_tiptilt_data_socket) ||
		write(labao_tiptilt_data_socket, data, 2*sizeof(short int)) !=
		2*sizeof(short int))
	{
		close_labao_tiptilt_data_socket();
		return error(ERROR,"Failed to send tiptilt data.");
	}

	return NOERROR;

} /* send_labao_tiptilt_data() */
