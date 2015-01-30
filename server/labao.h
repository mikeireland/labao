/************************************************************************/
/* labao.h								*/
/*                                                                      */
/* Header file for the labao system.					*/
/************************************************************************/
/*                                                                      */
/*     Center for High Angular Resolution Astronomy                     */
/* Georgia State University, Atlanta GA 30303-3083, U.S.A.              */
/*                                                                      */
/*                                                                      */
/* Telephone: 1-626-796-5405                                            */
/* Fax      : 1-626-796-6717                                            */
/* email    : theo@chara.gsu.edu                                        */
/* WWW      : http://www.chara.gsu.edu/~theo/theo.html                  */
/*                                                                      */
/* (C) This source code and its associated executable                   */
/* program(s) are copyright.                                            */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* Author : Theo ten Brummelaar                                         */
/* Date   : Feb 2014	                                                */
/************************************************************************/

#ifndef __LABAO__
#define __LABAO__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <slalib.h>
#include <zlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <nrc.h>
#include <clock.h>
#include <chara.h>
#include <charaui.h>
#include <cliserv.h>
#include <chara_messages.h>
#include <astromod.h>
#include <astromod_ui.h>
#include <nsimpleX.h>
#include <time.h>
#include <fitsio.h>
#include <uipico.h>

/* This is required for the EDAC40 controller for the OKO-DM */

#define MMDM_37CH	/* This is the mirror we are using */
#include <edac40.h>
#include <mirror_edac40.h>

//#define TEST_ACTUATOR_MAPPING
#ifndef TEST_ACTUATOR_MAPPING
#define NUM_ACTUATORS   37
#else
#define NUM_ACTUATORS   40
#endif

#define NUM_LENSLETS	32
#define DEFAULT_MAXJ	21

#define DEFAULT_FRAME_RATE	(200.0)
#define CENTROID_BOX_WIDTH 11

#define ARCSEC_PER_PIX 0.3

#define SERVO_DAMPING 0.9
#define SERVO_GAIN 0.5

/*
 * Some globals
 */

extern char labao_name[80];
extern char default_display[256];
extern char *dac40_mac_address[NUM_SCOPES];
extern int this_labao;
extern int maxJ;
extern int fsm_state;

/* These centroid offsets in principle need a mutex. In practice, all OK. */

extern float x_centroid_offsets[NUM_LENSLETS];
extern float y_centroid_offsets[NUM_LENSLETS];

/* These globals are strictly read-only from the main thread. */

extern float xc[NUM_LENSLETS];
extern float yc[NUM_LENSLETS];
extern float avg_fluxes[NUM_LENSLETS];
extern struct s_labao_wfs_results wfs_results;

/* How is the camera setup? */

extern struct s_labao_usb_camera usb_camera;

/* What is the default "center" of the pupil? */

extern float	xpos_center;
extern float	ypos_center;

/* For controlling pico motors */

#define NUM_PICO_SERVERS        7
extern int	pico_server;
extern char	*pico_servers[NUM_PICO_SERVERS];
extern int	telescope_server;
extern int	dich_mirror;
extern struct SMIRROR_LIST mirror_list[];
extern float servo_gain;
extern float servo_damping;

/* So we know where the telescope is */

extern struct s_scope_status telescope_status;

/* labao.c */

int main(int argc, char **argv);
void labao_close(void);
void labao_open(void);
void print_usage_message(char *name);
int open_pico_connection(int argc, char **argv);
int open_telescope_connection(int argc, char **argv);

/* labao_background.c */

void setup_background_jobs(void);
int labao_top_job(void);
int linux_time_status(void);
int rt_time_status(void);
int edac40_status(void);

/* labao_messages.c */

void setup_standard_labao_messages(void);
int message_labao_set_channel(struct smessage *message);
int message_labao_delta_channel(struct smessage *message);
int message_labao_set_all_channels(struct smessage *message);
int message_labao_delta_all_channels(struct smessage *message);
int message_labao_value_all_channels(struct smessage *message);
int message_labao_load_defaults(struct smessage *message);
int message_labao_get_usb_camera(struct smessage *message);
int message_labao_set_zernike(struct smessage *message);
int message_labao_inc_zernike(struct smessage *message);
int message_labao_run_function(struct smessage *message);
int message_labao_start_autoalign_lab(struct smessage *message);
int message_labao_start_autoalign_scope(struct smessage *message);
int message_labao_stop_autoalign(struct smessage *message);
int message_labao_edac40_add(struct smessage *message);
int message_labao_set_num_mean(struct smessage *message);
int message_labao_save_data(struct smessage *message);
int message_labao_reopen_telescope(struct smessage *message);
int message_telescope_status(int server, struct smessage *message);
void send_labao_text_message(char *fmt, ...);

/* labao_edac40.c */

int open_edac40(int argc, char **argv);
int close_edac40(int argc, char **argv);
int close_edac40_and_zero_dm(int argc, char **argv);
bool edac40_is_open(void);
void edac40_send_current_voltages(void);
int edac40_set_single_channel(int channel, float value);
int edac40_delta_single_channel(int channel, float delta);
int edac40_set_all_channels(float *values);
int edac40_delta_all_channels(float *deltas);
int call_edac40_set_single_channel(int argc, char **argv);
int call_edac40_delta_single_channel(int argc, char **argv);
int call_edac40_set_all_channels(int argc, char **argv);
int call_edac40_delta_all_channels(int argc, char **argv);
float edac40_current_value(int channel);
int show_edac40_values(int argc, char **argv);
int edac40_single_square(int argc, char **argv);
int edac40_cycle_square(int argc, char **argv);
int send_labao_value_all_channels(bool send_to_all_clients);
int exit_and_zero_dm(int argc, char **argv);
int edac40_mult_all_channels(float mult);
int call_edac40_mult_all_channels(int argc, char **argv);
int edac40_add_all_channels(float mult);
int call_edac40_add_all_channels(int argc, char **argv);

/* labao_zernike.c */

void setup_zernike(void);
void cleanup_zernike(void);
int zernike_to_dm(void);
int increment_zernike(int J, float delta);
int set_zernike(int J, float value);
int set_all_zernike(float *values);
int call_increment_zernike(int argc, char **argv);
int call_set_zernike(int argc, char **argv);
int test_set_all_zernike(int argc, char **argv);
int reset_all_zernike(int argc, char **argv);

/* labao_usb_camera.c */

int open_usb_camera(void);
int close_usb_camera(void);
void *do_usb_camera(void *arg);
void set_usb_camera_callback(void (*new_camera_callback)(
					CHARA_TIME time_stamp, float **image,
                                        int width, int height));
int start_usb_camera(int argc, char **argv);
int stop_usb_camera(int argc, char **argv);
int usb_camera_status(void);
int start_usb_camera_display(int argc, char **argv);
int stop_usb_camera_display(int argc, char **argv);
int set_usb_camera_aoi(int x, int y, int width, int height);
void get_usb_camera_aoi(int *x, int *y, int *width, int *height);
int call_set_usb_camera_aoi(int argc, char **argv);
int set_optimal_camera_timing(int argc, char **argv);
int call_set_frame_rate(int argc, char **argv);
double set_frame_rate(double fps);
int set_pixelclock(unsigned int new_pixelclock);
int call_set_pixelclock(int argc, char **argv);
int usb_camera_set_gain(int new_gain);
int call_usb_camera_set_gain(int argc, char **argv);
int toggle_destripe(int argc, char **argv);
int create_dark(void);
int call_create_dark(int argc, char **argv);
int zero_dark(void);
int call_zero_dark(int argc, char **argv);
int save_fits(int argc, char **argv);
int save_fits_cube(int argc, char **argv);
void complete_fits_cube(void);
double usb_camera_set_exptime(double exptime);
int call_usb_camera_set_exptime(int argc, char **argv);
int toggle_overlay_boxes(int argc, char **argv);
int send_labao_set_usb_camera(bool send_to_all_clients);
bool usb_cammera_is_running(void);

/* labso_fsm.c */

void initialize_fsm(void);
int load_defaults(char* filename_in);
int call_load_defaults(int argc, char **argv);
int save_defaults(char *filename_in);
int call_save_defaults(int argc, char **argv);
int save_actuator_to_sensor(char *filename_in);
int call_save_actuator_to_sensor(int argc, char **argv);
int load_reconstructor(char *filename_in);
int call_load_reconstructor(int argc, char **argv);
void zero_centroids(void);
int call_zero_centroids(int argc, char **argv);
void measure_reconstructor(void);
int call_measure_reconstructor(int argc, char **argv);
void close_servo(void);
int call_close_servo(int argc, char **argv);
void open_servo(void);
int call_open_servo(int argc, char **argv);
void run_centroids_and_fsm(CHARA_TIME time_stamp,
		float **data, int aoi_width, int aoi_height);
int fsm_status(void);
int save_reconstructor(int argc, char **argv);
int compute_reconstructor(int argc, char **argv);
int set_flat_dm(void);
int call_set_flat_dm(int argc, char **argv);
int flatten_wavefront(void);
int call_flatten_wavefront(int argc, char **argv);
int dump_dm_offset(int argc, char **argv);
int dump_centroid_offsets(int argc, char **argv);
void apply_reconstructor(void);
int call_apply_reconstructor(int argc, char **argv);
int toggle_ignore_tilt(int argc, char **argv);
int edit_wfs_results_num(int argc, char **argv);
int start_autoalign_lab_dichroic(int argc, char **argv);
int start_autoalign_scope_dichroic(int argc, char **argv);
int stop_autoalign_dichroic(int argc, char **argv);
int edit_servo_parameters(int argc, char **argv);
int move_boxes(int argc, char **argv);
int edit_zero_clamp(int argc, char **argv);
int edit_denom_clamp(int argc, char **argv);
int message_labao_save_aberrations(struct smessage *message);
void complete_aberrations_record(void);
void compute_pupil_center(void);
int call_zero_aberrations(int argc, char **argv);
int call_add_wfs_aberration(int argc, char **argv);
int toggle_use_reference(int argc, char **argv);
int set_reference_now(int argc, char **argv);

#endif
