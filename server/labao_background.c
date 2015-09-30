/************************************************************************/
/* labao_background.c	 						*/
/*                                                                      */
/* Background jobs for labao user interface.	 			*/ 
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

/************************************************************************/
/* setup_background_jobs()						*/
/*									*/
/* Guess what this does.						*/
/************************************************************************/

void setup_background_jobs(void)
{
	set_top_job(labao_top_job);
	background_add_name(linux_time_status,"Linux Time");
	background_add_name(rt_time_status, "RT Time");
	background_add_name(edac40_status,"EDAC40");
	background_add_name(usb_camera_status, "Camera");
	background_add_name(fsm_status, "FSM");

} /* setup_background_jobs() */

/************************************************************************/
/* labao_top_job()							*/
/* 									*/
/* These jobs must be run as often as possible.				*/
/************************************************************************/

int labao_top_job(void)
{
	return NOERROR;

} /* labao_top_job() */

/************************************************************************/
/* linux_time_status()							*/
/* 									*/
/* Displays the linux system time in the status window.			*/
/************************************************************************/

int linux_time_status(void)
{
	time_t current_time;
	struct tm *now;
	static time_t last_time = 0;

	time(&current_time);
	now = localtime(&current_time);
	wstandout(status_window);
	mvwaddstr(status_window,0,0,"Local Tm: ");
	wstandend(status_window);
	wprintw(status_window," %02d:%02d:%02d",
		now->tm_hour,now->tm_min,now->tm_sec);

	/* Process what we need to process */

	process_server_socket(telescope_server);
	process_server_socket(pico_server);
	process_server_socket(iris_server);

	/* Do we need to save anything? */

	complete_aberrations_record();
	complete_fits_cube();

	/* Periodic things */

	if (current_time > last_time)
	{
		last_time = current_time;

		send_labao_value_all_channels(TRUE);
	}

	return NOERROR;

} /* linux_time_status() */

/************************************************************************/
/* rt_time_status()							*/
/* 									*/
/* Displays the rt system time in the status window.			*/
/************************************************************************/

int rt_time_status(void)
{
	rt_time_struct now;
	ap_time_struct t;
	char	s[20];

	if (get_rt_time(&now) != 0) return ERROR;

	wstandout(status_window);
	mvwaddstr(status_window,1,0,"CHARA Tm: ");
	wstandend(status_window);
	t = rt_to_ap_time(now.time_now);
	if (t.sign == -1)
		strcpy(s,"-");
	else
		strcpy(s," ");
	sprintf(s+1,"%02d:%02d:%02d",t.hrs,t.min,t.sec);
	wprintw(status_window,"%s",s);

	return NOERROR;

} /* rt_time_status() */

/************************************************************************/
/* edac40_status()							*/
/* 									*/
/* Displays the rt system time in the status window.			*/
/************************************************************************/

int edac40_status(void)
{
	wstandout(status_window);
	mvwaddstr(status_window,2,0,"EDAC40  : ");
	wstandend(status_window);
	if (edac40_is_open())
		wprintw(status_window,"%9s","Open");
	else
		wprintw(status_window,"%9s","Closed");

	return NOERROR;

} /* edac40_status() */
