/************************************************************************/
/* labao_functs.c							*/
/*                                                                      */
/* Discription                                   	 		*/
/*	Sets up look up table array of user callable function modules.	*/
/*	This is the file to change (along with menus.ini) when		*/
/*	installing new user callable functions.				*/
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
/* Author : Tony Johnson & Theo ten Brummelaar                          */
/* Date   : Original Version 1990 - ported to Linux 1998                */
/************************************************************************/

#define FUNCTS
#include <charaui.h>
#undef FUNCTS
#include <baytech.h>
#include "labao.h"

struct {                       
                char    *name;
                int     (*function)(int argc, char **argv);
        } functions[] = 

	{	
		/* Standard Function Set */

		{NULL,		NULL},

#include <std_ui_functs.h>
#include <std_astro_functs.h>
#include <std_clock_functs.h>

		/* For reopening conenctions */

		{"opico",	open_pico_connection},
		{"oscope",	open_telescope_connection},

		/* Function Set  For Edac40 */

		{"oedac",	open_edac40},
		{"cedac",	close_edac40},
		{"czedac",	close_edac40_and_zero_dm},
		{"ssc",		call_edac40_set_single_channel},
		{"dsc",		call_edac40_delta_single_channel},
		{"sac",		call_edac40_set_all_channels},
		{"dac",		call_edac40_delta_all_channels},
		{"sedac",	show_edac40_values},
		{"sqedac",	edac40_single_square},
		{"sqall",	edac40_cycle_square},
		{"zexit",	exit_and_zero_dm},
		{"mult",	call_edac40_mult_all_channels},
		{"add",		call_edac40_add_all_channels},

		/* Function set for Zernike modes */

		{"inczern",	call_increment_zernike},
		{"setzern",	call_set_zernike},
		{"testzern",	test_set_all_zernike},
		{"reszern",	reset_all_zernike},
		{"zzern",	reset_all_zernike},
		{"szern",	call_save_zernike},
		{"lzern",	call_load_zernike},
		{"nz2dm",	call_new_zernike_to_dm},

		/* Function set for USB camera */

		{"startcam",	start_usb_camera},
		{"stopcam",	stop_usb_camera},
		{"dispon",	start_usb_camera_display},
		{"dispoff",	stop_usb_camera_display},
		{"aoi",		call_set_usb_camera_aoi},
		{"roi",		call_set_usb_camera_aoi},
		{"optimalcam",  set_optimal_camera_timing},
		{"fps",         call_set_frame_rate},
		{"camgain",     call_usb_camera_set_gain},
		{"camclock",    call_set_pixelclock},
		{"destripe",    toggle_destripe},
		{"dark", 	call_create_dark},
		{"zerodark", 	call_zero_dark},
		{"fits",	save_fits},
		{"save",	save_fits},
		{"fitscube",	save_fits_cube},
		{"savecube",	save_fits_cube},
		{"exptime",     call_usb_camera_set_exptime},
		{"tbox",	toggle_overlay_boxes},
		{"nframe",	call_set_num_sum_frame},
		{"sthresh",	set_image_threshold},
		{"thresh",	set_image_threshold},
		{"sci",		set_camera_id},
		{"sid",		set_camera_id},

		/* Function set for finite state machine */

		{"loaddef",	call_load_defaults},
		{"savedef",	call_save_defaults},
		{"loadrecon",   call_load_reconstructor},
		{"saveact",     call_save_actuator_to_sensor},
		{"readact",     read_actuator_to_sensor},
		{"saveint",     call_save_actuator_to_sensor},
		{"saveab",      call_save_actuator_to_aberration},
		{"readab",      read_actuator_to_aberration},
		{"measure",	call_measure_reconstructor},
		{"interaction", call_measure_reconstructor},
		{"recon",	compute_reconstructor},
		{"reconn",	compute_reconstructor_new},
		{"saverecon",	save_reconstructor},
		{"zc", 		call_zero_centroids},
		{"zerocentroids",call_zero_centroids},
		{"flatten",     call_flatten_wavefront},
		{"flat", 	call_flatten_wavefront},
		{"setflat",	call_set_flat_dm},
		{"close", 	call_close_servo},
		{"open",	call_open_servo},
		{"dumpdm",  dump_dm_offset},
		{"dumpco",  dump_centroid_offsets},
		{"applyrecon",	call_apply_reconstructor},
		{"ignoretilt",  toggle_ignore_tilt},
		{"tit", toggle_ignore_tilt},
		{"editn",	edit_wfs_results_num},
		{"num",		edit_wfs_results_num},
		{"alab",	start_autoalign_lab_dichroic},
		{"ascope",	start_autoalign_scope_dichroic},
		{"azern",	start_autoalign_zernike},
		{"afoc",	start_autoalign_beacon_focus},
		{"sdich",	stop_autoalign},
		{"esp",		edit_servo_parameters},
		{"move",	move_boxes},
		{"expand",	expand_boxes},
		{"ezc",		edit_zero_clamp},
		{"zero",	edit_zero_clamp},
		{"edc",		edit_denom_clamp},
		{"denom",	edit_denom_clamp},
		{"zeroab",      call_zero_aberrations},
		{"addab",       call_add_wfs_aberration},
		{"tur",       	toggle_use_reference},
		{"togref",     	toggle_use_reference},
		{"dichmap",     start_scope_dichroic_mapping},
		{"dichfit",     fit_dichroic_map},
		{"dichroic",	select_dichroic},
		{"dich",	select_dichroic},
		{"tcdc",        toggle_coude_dichroic_corrections},
                {"ecdc",        edit_coude_dichroic_corrections},
                {"eimp",        edit_dm_impulse},
                {"imp",        	edit_dm_impulse},

		/* Function set for tiptilt messages */

		{"tton",	send_tiptilt_on},
		{"ttoff",	send_tiptilt_off},
		{"ott",		call_open_labao_tiptilt_data_socket},

		{NULL,		NULL}	};
