/************************************************************************/
/* labaogtk.h                                                           */
/*                                                                      */
/* HEader for LABAO client.		                                */
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
/* Date   : March 2014							*/
/************************************************************************/

#include <chara_messages.h>
#include <cliserv.h>
#include <chara.h>
#include <cliserv_clock.h>
#include <astrogtk.h>
#include <stdlib.h>
#include <nrc.h>
#include <nsimpleX.h>
#include <sys/time.h>
#include <zlib.h>
#include <nsimpleX.h>

#ifdef GTK2
#define LABAO_WIDTH		608
#define LABAO_HEIGHT		0
#else
#define LABAO_WIDTH		150
#define LABAO_HEIGHT		24
#endif

/* Globals */

extern int server;
extern char display[256];      /* Name of display to use */
extern int server_open;
extern int display_delay_ms;
extern char send_ready_for_display;
extern char do_local_display;
#ifdef GTK2
extern int main_page;
extern int status_page;
extern int config_page;
extern int dm_page;
extern int wfs_page;
#endif
extern GtkWidget *notebook;
extern char server_name[56];
extern bool engineering_mode;
extern struct s_labao_usb_camera usb_camera;
extern float *x_centroid_offsets;
extern float *y_centroid_offsets;
extern float *xc;
extern float *yc;
extern float *avg_fluxes;
extern struct s_labao_wfs_results wfs_results;
extern GtkWidget *message_label;

/* Prototypes */

/* labaogtk.c */

int main(int  argc, char *argv[]); 
gint delete_event( GtkWidget *widget, GdkEvent  *event, gpointer   data ); 
gint background_code(gpointer data);
void reopen_socket_callback(GtkButton *button, gpointer user_data);
void labao_message_callback(GtkButton *widget, gpointer type);
void labao_signal_callback(GtkButton *button, gpointer signal);
void clear_display_callback(void);
void status_page_callback(void);
void print_usage_message(char *name);
void update_wfs_results(void);
void labao_plot_aber_callback(GtkButton *button, gpointer user_data);

/* messages.c */

void set_labao_messages(void);
int message_labao_value_all_channels(int server, struct smessage *mess);
int message_labao_set_usb_camera(int server, struct smessage *mess);
int message_labao_send_centroids(int server, struct smessage *mess);
int message_labao_wfs_results(int server, struct smessage *mess);
int message_labao_text_message(int server, struct smessage *mess);

/*  dm_page.c */

void fill_dm_page(GtkWidget *vbox);
void update_dm_page(float *values);
void set_channel_callback(GtkButton *button, gpointer data);
void delta_channel_callback(GtkButton *button, gpointer data);
void set_all_callback(GtkButton *button, gpointer data);
void delta_all_callback(GtkButton *button, gpointer data);
void set_zernike_callback(GtkButton *button, gpointer data);
void inc_zernike_callback(GtkButton *button, gpointer data);
void edac40_add(GtkButton *button, gpointer data);

/* display.c */

int message_labao_usb_image(int server, struct smessage *mess);
int message_labao_stop_usb_image(int server, struct smessage *mess);

/*  wfs_page.c */

void fill_wfs_page(GtkWidget *vbox);
void labao_autoalign_lab_callback(GtkButton *button, gpointer data);
void labao_autoalign_scope_callback(GtkButton *button, gpointer data);
void labao_autoalign_zernike_callback(GtkButton *button, gpointer data);
void labao_num_mean_callback(GtkButton *button, gpointer data);
void labao_set_fps_callback(GtkButton *button, gpointer data);
void labao_save_aberrations_callback(GtkButton *button, gpointer data);
void labao_save_data_callback(GtkButton *button, gpointer data);
