/************************************************************************/
/* labao_messages.c							*/
/*                                                                      */
/* Message handling routines for cliserv part of TELESCOPE program.	*/
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

/************************************************************************/
/* setup_standard_labao_messages()					*/
/*									*/
/* Sets up the standard TELESCOPE messages.				*/
/************************************************************************/

void setup_standard_labao_messages(void)
{
        ui_add_message_job(LABAO_OPEN_EDAC40, message_labao_run_function);
        ui_add_message_job(LABAO_CLOSE_EDAC40, message_labao_run_function);
        ui_add_message_job(LABAO_SET_CHANNEL, message_labao_set_channel);
        ui_add_message_job(LABAO_DELTA_CHANNEL, message_labao_delta_channel);
        ui_add_message_job(LABAO_SET_ALL_CHANNELS, 
			message_labao_set_all_channels);
        ui_add_message_job(LABAO_DELTA_ALL_CHANNELS, 
			message_labao_delta_all_channels);
        ui_add_message_job(LABAO_VALUE_ALL_CHANNELS, 
			message_labao_value_all_channels);
        ui_add_message_job(LABAO_LOAD_DEFAULTS, message_labao_load_defaults);
        ui_add_message_job(LABAO_GET_USB_CAMERA, message_labao_get_usb_camera);
        ui_add_message_job(LABAO_SET_ZERNIKE, message_labao_set_zernike);
        ui_add_message_job(LABAO_INC_ZERNIKE, message_labao_inc_zernike);
        ui_add_message_job(LABAO_START_USB_CAMERA, 
		message_labao_run_function);
        ui_add_message_job(LABAO_STOP_USB_CAMERA, 
		message_labao_run_function);
        ui_add_message_job(LABAO_START_USB_CAMERA_DISPLAY, 
		message_labao_run_function);
        ui_add_message_job(LABAO_STOP_USB_CAMERA_DISPLAY, 
		message_labao_run_function);
        ui_add_message_job(LABAO_TOGGLE_OVERLAY_BOXES, 
		message_labao_run_function);
        ui_add_message_job(LABAO_TOGGLE_DESTRIPE, 
		message_labao_run_function);
        ui_add_message_job(LABAO_CREATE_DARK, 
		message_labao_run_function);
	ui_add_message_job(LABAO_SAVE_DEFAULTS, message_labao_save_defaults);
	ui_add_message_job(LABAO_LOAD_RECONSTRUCTOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_SAVE_ACTUATOR_TO_SENSOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_ZERO_CENTROIDS,
		message_labao_run_function);
	ui_add_message_job(LABAO_CLOSE_SERVO,
		message_labao_run_function);
	ui_add_message_job(LABAO_OPEN_SERVO,
		message_labao_run_function);
	ui_add_message_job(LABAO_APPLY_RECONSTRUCTOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_MEASURE_RECONSTRUCTOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_COMPUTE_RECONSTRUCTOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_SAVE_RECONSTRUCTOR,
		message_labao_run_function);
	ui_add_message_job(LABAO_SET_FLAT_DM,
		message_labao_run_function);
	ui_add_message_job(LABAO_FLATTEN_WAVEFRONT,
		message_labao_run_function);
	ui_add_message_job(LABAO_TOGGLE_IGNORE_TILT,
		message_labao_run_function);
	ui_add_message_job(LABAO_DUMP_DM_OFFSET,
		message_labao_run_function);
	ui_add_message_job(LABAO_DUMP_CENTROID_OFFSETS,
		message_labao_run_function);
        ui_add_message_job(LABAO_ZERO_DARK, 
		message_labao_run_function);
        ui_add_message_job(LABAO_SHOW_EDAC40, message_labao_run_function);
        ui_add_message_job(LABAO_START_AUTOALIGN_LAB, 
		message_labao_start_autoalign_lab);
        ui_add_message_job(LABAO_START_AUTOALIGN_SCOPE, 
		message_labao_start_autoalign_scope);
        ui_add_message_job(LABAO_START_AUTOALIGN_ZERNIKE, 
		message_labao_start_autoalign_zernike);
        ui_add_message_job(LABAO_STOP_AUTOALIGN, message_labao_stop_autoalign);
        ui_add_message_job(LABAO_EDAC40_ADD, message_labao_edac40_add);
        ui_add_message_job(LABAO_SAVE_ABERRATIONS, 
		message_labao_save_aberrations);
        ui_add_message_job(LABAO_SET_NUM_MEAN, message_labao_set_num_mean);
        ui_add_message_job(LABAO_SAVE_DATA, message_labao_save_data);
        ui_add_message_job(LABAO_REOPEN_TELESCOPE, 
		message_labao_reopen_telescope);
        ui_add_message_job(LABAO_ADD_WFS_ABERRATION, 
		message_labao_add_wfs_aberration);
        ui_add_message_job(LABAO_ZERO_WFS_ABERRATION, 
		message_labao_zero_wfs_aberration);
        ui_add_message_job(LABAO_SET_FPS, message_labao_set_fps);
        ui_add_message_job(HUT_AOB_CHANGE_DICHROIC,message_aob_change_dichroic);
        ui_add_message_job(LABAO_REOPEN_TIPTILT, message_labao_reopen_tiptilt);
        ui_add_message_job(LABAO_TIPTILT_ON, message_labao_tiptilt_on);
        ui_add_message_job(LABAO_TIPTILT_OFF, message_labao_tiptilt_off);
	ui_add_message_job(LABAO_TOGGLE_REFERENCE, 
                message_labao_toggle_reference);
	ui_add_message_job(LABAO_MOVE_BOXES, message_labao_move_boxes);


} /* setup_standard_labao_messages() */

/************************************************************************/
/* message_labao_set_channel()                                          */
/*                                                                      */
/************************************************************************/

int message_labao_set_channel(struct smessage *message)
{
	struct s_labao_eda40_set_channel *data;

	if (message->length != sizeof(struct s_labao_eda40_set_channel))
		return error(ERROR,"LABAO_SET_CHANNEL with wrong data");

	data = (struct s_labao_eda40_set_channel *)(message->data);

	if (edac40_set_single_channel(data->channel, data->value) == 0)
		return send_labao_value_all_channels(TRUE);
	else
		return error(ERROR,"Channel or Value is out of range.");

} /* message_labao_set_channel() */

/************************************************************************/
/* message_labao_delta_channel()                                        */
/*                                                                      */
/************************************************************************/

int message_labao_delta_channel(struct smessage *message)
{
	struct s_labao_eda40_set_channel *data;

	if (message->length != sizeof(*data))
		return error(ERROR,"LABAO_DELTA_CHANNEL with wrong data");

	data = (struct s_labao_eda40_set_channel *)(message->data);

	if (edac40_delta_single_channel(data->channel, data->value) == 0)
		return send_labao_value_all_channels(TRUE);
	else
		return error(ERROR,"Channel or Value is out of range.");

} /* message_labao_delta_channel() */

/************************************************************************/
/* message_labao_set_all_channels()                                     */
/*                                                                      */
/************************************************************************/

int message_labao_set_all_channels(struct smessage *message)
{
	float 	*data;
	float	values[LABAO_NUM_ACTUATORS+1];
	int	i;

	if (message->length != sizeof(float))
		return error(ERROR,"LABAO_SET_ALL_CHANNELS with wrong data");

	data = (float *)message->data;

	for(i=1; i<=LABAO_NUM_ACTUATORS; i++) values[i] = *data;

	if (edac40_set_all_channels(values) == 0)
		return send_labao_value_all_channels(TRUE);
	else
		return error(ERROR,"Value is out of range.");

} /* message_labao_set_all_channels() */

/************************************************************************/
/* message_labao_delta_all_channels()                                   */
/*                                                                      */
/************************************************************************/

int message_labao_delta_all_channels(struct smessage *message)
{
	float *data;
	float	values[LABAO_NUM_ACTUATORS+1];
	int	i;

	if (message->length != sizeof(float))
		return error(ERROR,"LABAO_DELTA_ALL_CHANNELS with wrong data");

	data = (float *)message->data;

	for(i=1; i<=LABAO_NUM_ACTUATORS; i++) values[i] = *data;

	if (edac40_delta_all_channels(values) == 0)
		return send_labao_value_all_channels(TRUE);
	else
		return error(ERROR,"Value is out of range.");

} /* message_labao_delta_all_channels() */

/************************************************************************/
/* message_labao_value_all_channels()                                   */
/*                                                                      */
/************************************************************************/

int message_labao_value_all_channels(struct smessage *message)
{
	if (message->length != 0)
		return error(ERROR,"LABAO_VALUE_ALL_CHANNELS with wrong data");

	return send_labao_value_all_channels(FALSE);

} /* message_labao_value_all_channels() */

/************************************************************************/
/* message_labao_get_usb_camera()                                       */
/*                                                                      */
/************************************************************************/

int message_labao_get_usb_camera(struct smessage *message)
{
	if (message->length != 0) return error(ERROR,
		"LABAO_VALUE_GET_USB_CAMERA with wrong data");

	return send_labao_set_usb_camera(FALSE);

} /* message_labao_get_usb_camera() */

/************************************************************************/
/* message_labao_set_zernike()                                          */
/*                                                                      */
/************************************************************************/

int message_labao_set_zernike(struct smessage *message)
{
	struct s_labao_eda40_set_channel *data;

	if (message->length != sizeof(struct s_labao_eda40_set_channel))
		return error(ERROR,"LABAO_SET_ZERNIKE with wrong data");

	data = (struct s_labao_eda40_set_channel *)(message->data);

	if (set_zernike(data->channel, data->value) == 0)
	{
		return send_labao_value_all_channels(TRUE);
	}
	else
		return error(ERROR,"J or Value is out of range.");

} /* message_labao_set_zernike() */

/************************************************************************/
/* message_labao_inc_zernike()                                          */
/*                                                                      */
/************************************************************************/

int message_labao_inc_zernike(struct smessage *message)
{
	struct s_labao_eda40_set_channel *data;

	if (message->length != sizeof(struct s_labao_eda40_set_channel))
		return error(ERROR,"LABAO_INC_ZERNIKE with wrong data");

	data = (struct s_labao_eda40_set_channel *)(message->data);

	if (increment_zernike(data->channel, data->value) == 0)
	{
		return send_labao_value_all_channels(TRUE);
	}
	else
		return error(ERROR,"J or Value is out of range.");

} /* message_labao_inc_zernike() */

/************************************************************************/
/* message_labao_run_function()                                         */
/*                                                                      */
/************************************************************************/

int message_labao_run_function(struct smessage *message)
{

	if (message->length != 0)
		return error(ERROR,"Got message with data and I expect none.");

	switch(message->type)
	{
		case LABAO_OPEN_EDAC40:
			return open_edac40(0, NULL);
		case LABAO_CLOSE_EDAC40:
			return close_edac40(0, NULL);
		case LABAO_START_USB_CAMERA:
			return start_usb_camera(0, NULL);
		case LABAO_STOP_USB_CAMERA:
			return stop_usb_camera(0, NULL);
		case LABAO_START_USB_CAMERA_DISPLAY:
			return start_usb_camera_display(0, NULL);
		case LABAO_STOP_USB_CAMERA_DISPLAY:
			return stop_usb_camera_display(0, NULL);
		case LABAO_TOGGLE_OVERLAY_BOXES:
			return toggle_overlay_boxes(0, NULL);
		case LABAO_TOGGLE_DESTRIPE:
			return toggle_destripe(0, NULL);
		case LABAO_CREATE_DARK:
			return call_create_dark(0, NULL);
		case LABAO_LOAD_DEFAULTS: 
			return call_load_defaults(0, NULL);
		case LABAO_SAVE_DEFAULTS: 
			return call_save_defaults(0, NULL);
		case LABAO_LOAD_RECONSTRUCTOR:
			return  call_load_reconstructor(0, NULL);
		case LABAO_SAVE_ACTUATOR_TO_SENSOR:
			return  call_save_actuator_to_sensor(0, NULL);
		case LABAO_ZERO_CENTROIDS:
			return  call_zero_centroids(0, NULL);
		case LABAO_CLOSE_SERVO:
			return 	call_close_servo(0, NULL);
		case LABAO_OPEN_SERVO:
			return  call_open_servo(0, NULL);
		case LABAO_APPLY_RECONSTRUCTOR:
			return  call_apply_reconstructor(0, NULL);
		case LABAO_MEASURE_RECONSTRUCTOR:
			return  call_measure_reconstructor(0, NULL);
		case LABAO_COMPUTE_RECONSTRUCTOR:
			return  compute_reconstructor(0, NULL);
		case LABAO_SAVE_RECONSTRUCTOR:
			return  save_reconstructor(0, NULL);
		case LABAO_SET_FLAT_DM:
			return  call_set_flat_dm(0, NULL);
		case LABAO_FLATTEN_WAVEFRONT:
			return  call_flatten_wavefront(0, NULL);
		case LABAO_TOGGLE_IGNORE_TILT:
			return  toggle_ignore_tilt(0, NULL);
		case LABAO_DUMP_DM_OFFSET:
			return  dump_dm_offset(0, NULL);
		case LABAO_DUMP_CENTROID_OFFSETS:
			return  dump_centroid_offsets(0, NULL);
		case LABAO_ZERO_DARK:
			return  call_zero_dark(0, NULL);
		case LABAO_SHOW_EDAC40:
			return  show_edac40_values(0, NULL);
		default:
			return error(ERROR,"Hmmmm I should never get here.");
	}

} /* message_labao_run_function() */

/************************************************************************/
/* message_labao_start_autoalign_lab()                                  */
/*                                                                      */
/************************************************************************/

int message_labao_start_autoalign_lab(struct smessage *message)
{
	char	*args[2];
	char	argv1[30];
	int	tries;

	if (message->length != sizeof(int))
    		return error(ERROR,
			"Got LABAO_START_AUTOALIGN_LAB with wrong data.");

	tries = *((int *)message->data);

	sprintf(argv1,"%d", tries);

	args[0] = "adich";
	args[1] = argv1;

	return start_autoalign_lab_dichroic(2, args);

} /* message_labao_start_autoalign_lab() */

/************************************************************************/
/* message_labao_start_autoalign_scope()                                */
/*                                                                      */
/************************************************************************/

int message_labao_start_autoalign_scope(struct smessage *message)
{
	char	*args[2];
	char	argv1[30];
	int	tries;

	if (message->length != sizeof(int))
    		return error(ERROR,
			"Got LABAO_START_AUTOALIGN_SCOPE with wrong data.");

	tries = *((int *)message->data);

	sprintf(argv1,"%d", tries);

	args[0] = "adich";
	args[1] = argv1;

	return start_autoalign_scope_dichroic(2, args);

} /* message_labao_start_autoalign_scope() */

/************************************************************************/
/* message_labao_start_autoalign_zernike()                              */
/*                                                                      */
/************************************************************************/

int message_labao_start_autoalign_zernike(struct smessage *message)
{
	char	*args[2];
	char	argv1[30];
	int	tries;

	if (message->length != sizeof(int))
    		return error(ERROR,
			"Got LABAO_START_AUTOALIGN_ZERNIKE with wrong data.");

	tries = *((int *)message->data);

	sprintf(argv1,"%d", tries);

	args[0] = "zdich";
	args[1] = argv1;

	return start_autoalign_zernike(2, args);

} /* message_labao_start_autoalign_zernike() */

/************************************************************************/
/* message_labao_stop_autoalign()                                       */
/*                                                                      */
/************************************************************************/

int message_labao_stop_autoalign(struct smessage *message)
{
	if (message->length != 0)
		return error(ERROR,"Got LABAO_STOP_AUTOALIGN with wrong data.");

	return stop_autoalign(0, NULL);

} /* message_labao_stop_autoalign() */

/************************************************************************/
/* message_labao_edac40_add()	                                        */
/*                                                                      */
/************************************************************************/

int message_labao_edac40_add(struct smessage *message)
{
	float	add;

	if (message->length != sizeof(add))
		return error(ERROR,"Got LABAO_EDAC40_ADD with wrong data.");

	add = *((float *)message->data);

	/* We either lie to the servo or move the mirror itself. */

	if (fsm_state == FSM_SERVO_LOOP)
	{
		return add_wfs_aberration(4, add);
	}
	else
	{
		return edac40_add_all_channels(add);
	}

	return NOERROR;

} /* message_labao_edac40_add() */

/************************************************************************/
/* send_labao_text_message()						*/
/*									*/
/* Printf like command to send text to show on client.			*/
/************************************************************************/

void send_labao_text_message(char *fmt, ...)
{
	char	s[300];
	va_list args;
	struct smessage message;

	va_start(args, fmt);
        vsprintf(s, fmt, args);

	message.type = LABAO_TEXT_MESSAGE;
	message.data = (unsigned char *)s;
	message.length = strlen(s)+1;

	ui_send_message_all(&message);

} /* send_labao_text_message() */

/************************************************************************/
/* message_labao_set_num_mean()                                         */
/*                                                                      */
/************************************************************************/

int message_labao_set_num_mean(struct smessage *message)
{
	char	*args[2];
	char	argv1[30];
	int	n;

	if (message->length != sizeof(int))
	    return error(ERROR,"Got LABAO_SET_NUM_MEAN with wrong data.");

	n = *((int *)message->data);

	sprintf(argv1,"%d", n);

	args[0] = "num";
	args[1] = argv1;

	return edit_wfs_results_num(2, args);

} /* message_labao_set_num_mean() */

/************************************************************************/
/* message_labao_reopen_telescope()                                     */
/*                                                                      */
/************************************************************************/

int message_labao_reopen_telescope(struct smessage *message)
{
	if (message->length != 0) 
	    return error(ERROR,"Got LABAO_REOPEN_TELESCOPE with wrong data.");

	return open_telescope_connection(0, NULL);

} /* message_labao_reopen_telescope() */

/************************************************************************/
/* message_labao_save_data()                                            */
/*                                                                      */
/************************************************************************/

int message_labao_save_data(struct smessage *message)
{
	char	*args[2];
	char	argv1[30];
	int	n;

	if (message->length != sizeof(int))
	    return error(ERROR,"Got LABAO_SAVE_DATA with wrong data.");

	n = *((int *)message->data);

	sprintf(argv1,"%d", n);

	args[0] = "save_cube";
	args[1] = argv1;

	return save_fits_cube(2, args);

} /* message_labao_save_data() */

/************************************************************************/
/* message_labao_add_wfs_aberration()                                   */
/*                                                                      */
/************************************************************************/

int message_labao_add_wfs_aberration(struct smessage *message)
{
	struct s_labao_eda40_set_channel *data;

	if (message->length != sizeof(*data))
		return error(ERROR,"LABAO_ADD_WFS_ABERRATION with wrong data");

	data = (struct s_labao_eda40_set_channel *)(message->data);

	if (add_wfs_aberration(data->channel, data->value) != NOERROR)
		return error(ERROR,"Zernike number or value is out of range.");
	else
		return NOERROR;

} /* message_labao_add_wfs_aberration() */

/************************************************************************/
/* message_labao_zero_wfs_aberration()                                   */
/*                                                                      */
/************************************************************************/

int message_labao_zero_wfs_aberration(struct smessage *message)
{
	if (message->length != 0)
		return error(ERROR,"LABAO_ZERO_WFS_ABERRATION with wrong data");

	return call_zero_aberrations(0, NULL);

} /* message_labao_zero_wfs_aberration() */

/************************************************************************/
/* message_labao_set_fps() 		                                */
/*                                                                      */
/************************************************************************/

int message_labao_set_fps(struct smessage *message)
{
	float *fps;

	if (message->length != sizeof(float))
		return error(ERROR,"LABAO_SET_FPS with wrong data");

	fps = (float *)(message->data);

	if (set_frame_rate(*fps) < 0)
		return ERROR;
	else
		return NOERROR;

} /* message_labao_set_fps() */

/************************************************************************/
/* message_labao_load_defaults()	                                */
/*                                                                      */
/************************************************************************/

int message_labao_load_defaults(struct smessage *message)
{
	char	*args[2];

	if (message->length == 0) return call_load_defaults(0, NULL);

	args[0] = "loaddef";
	args[1] = (char *)(message->data);
	return call_load_defaults(2, args);

} /* message_labao_load_defaults() */

/************************************************************************/
/* message_labao_save_defaults()	                                */
/*                                                                      */
/************************************************************************/

int message_labao_save_defaults(struct smessage *message)
{
	char	*args[2];

	if (message->length == 0) return call_save_defaults(0, NULL);

	args[0] = "savedef";
	args[1] = (char *)(message->data);
	return call_save_defaults(2, args);

} /* message_labao_save_defaults() */

/************************************************************************/
/* message_labao_reopen_tiptilt()	                                */
/*                                                                      */
/************************************************************************/

int message_labao_reopen_tiptilt(struct smessage *message)
{
	if (message->length != 0) 
		return error(ERROR,"LABAO_REOPEN_TIPTILT with wrong data");

	return call_open_labao_tiptilt_data_socket(0, NULL);

} /* message_labao_reopen_tiptilt() */

/************************************************************************/
/* message_labao_tiptilt_on()		                                */
/*                                                                      */
/************************************************************************/

int message_labao_tiptilt_on(struct smessage *message)
{
	if (message->length != 0) 
		return error(ERROR,"LABAO_TIPTILT_ON with wrong data");

	return send_tiptilt_on(0, NULL);

} /* message_labao_tiptilt_on() */

/************************************************************************/
/* message_labao_tiptilt_off()		                                */
/*                                                                      */
/************************************************************************/

int message_labao_tiptilt_off(struct smessage *message)
{
	if (message->length != 0) 
		return error(ERROR,"LABAO_TIPTILT_OFF with wrong data");

	return send_tiptilt_off(0, NULL);

} /* message_labao_tiptilt_off() */

/************************************************************************/
/* message_labao_toggle_reference()                                     */
/*                                                                      */
/************************************************************************/

int message_labao_toggle_reference(struct smessage *message)
{
        if (message->length != 0) 
                return error(ERROR,"Wrong data for LABAO_TOGGLE_REFERENCE");

        return toggle_use_reference(0, NULL);

} /* message_labao_toggle_reference() */

/************************************************************************/
/* message_labao_move_boxes()                                  */
/*                                                                      */
/************************************************************************/

int message_labao_move_boxes(struct smessage *message)
{
	char	*args[3];
	char	argv1[30];
	char	argv2[30];
	struct  s_labao_move_boxes *data;

	if (message->length != sizeof(struct s_labao_move_boxes))
    		return error(ERROR,
			"Got LABAO_MOVE_BOXES with wrong data.");

	data = (struct s_labao_move_boxes *)message->data;

	sprintf(argv1,"%.4f", data->dx);
	sprintf(argv2,"%.4f", data->dy);

	args[0] = "move";
	args[1] = argv1;
	args[2] = argv2;

	return move_boxes(3, args);

} /* message_labao_move_boxes() */

