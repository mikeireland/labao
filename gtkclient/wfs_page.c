/************************************************************************/
/* wfs_page.c  	                                                	*/
/*                                                                      */
/* Stuff for controlling the WFS.					*/
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
/* Author : Theo ten Brummelaar                                         */
/* Date   : November 2006                                               */
/************************************************************************/

#include "labaogtk.h"
#include <cliserv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <chara.h>
#include <slalib.h>

static GtkWidget *move_entry;
static GtkWidget *tries_entry;
static GtkWidget *num_entry;
static GtkWidget *fps_entry;

static int boxes_up = 0;
static int boxes_down = 1;
static int boxes_left = 2;
static int boxes_right = 3;

/************************************************************************/
/* fill_wfs_page()							*/
/*									*/
/* Put in all the things you need for teh DITHER control page 		*/
/************************************************************************/

void fill_wfs_page(GtkWidget *vbox)
{
	GtkWidget *button;
	GtkWidget *hbox;
	GtkWidget *label;

	/* OK, show the box */

	gtk_widget_show(vbox);

	/* First row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("START WFS");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_START_USB_CAMERA));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("STOP WFS");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_STOP_USB_CAMERA));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("START DISP");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_START_USB_CAMERA_DISPLAY));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("STOP DISP");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_STOP_USB_CAMERA_DISPLAY));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("BOXES ON/OFF");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_TOGGLE_OVERLAY_BOXES));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Second row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("DESTRIPE");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_TOGGLE_DESTRIPE));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("MAKE DARK");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_CREATE_DARK));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("ZERO DARK");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_ZERO_DARK));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("CLOSE SERVO");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_CLOSE_SERVO));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("OPEN SERVO");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_OPEN_SERVO));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Third row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("FLATTEN");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_FLATTEN_WAVEFRONT));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("SET FLAT");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_SET_FLAT_DM));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("IGNORE TILT");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_TOGGLE_IGNORE_TILT));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

	button = gtk_button_new_with_label ("TOGGLE REF");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
		GTK_SIGNAL_FUNC (labao_message_callback),
		(gpointer)(message_array+LABAO_TOGGLE_REFERENCE));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER(button),1);
	gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
	gtk_widget_show(button);

        button = gtk_button_new_with_label ("LOAD DEF");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_LOAD_DEFAULTS));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
	gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);


	/* Fourth row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

	if (engineering_mode)
	{
		button = gtk_button_new_with_label ("LOAD RECON");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_LOAD_RECONSTRUCTOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("APPLY RECON");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_APPLY_RECONSTRUCTOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("MEASURE RECON");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_MEASURE_RECONSTRUCTOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("COMPUTE RECON");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_COMPUTE_RECONSTRUCTOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("SAVE RECON");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_SAVE_RECONSTRUCTOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		/* Fifth row of Buttons */

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox);
		gtk_widget_show(hbox);

		button = gtk_button_new_with_label ("DUMP CENT OFFSET");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_DUMP_CENTROID_OFFSETS));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("DUMP DM OFFSET");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_DUMP_DM_OFFSET));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("ZERO CENT");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_ZERO_CENTROIDS));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("ACT TO SENSOR");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
		    (gpointer)(message_array+LABAO_SAVE_ACTUATOR_TO_SENSOR));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("SAVE DEF");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_message_callback),
			(gpointer)(message_array+LABAO_SAVE_DEFAULTS));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		/* Sixth row of Buttons */

		hbox = gtk_hbox_new(FALSE, 0);
		gtk_container_add (GTK_CONTAINER (vbox), hbox);
		gtk_widget_show(hbox);

		label = gtk_label_new("MOVE:");
		gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
		gtk_widget_set_usize (label, LABAO_WIDTH/15, LABAO_HEIGHT);
		gtk_widget_show(label);

		move_entry = gtk_entry_new ();
		gtk_entry_set_text (GTK_ENTRY (move_entry),"0.1");
		gtk_box_pack_start(GTK_BOX(hbox), move_entry, TRUE, TRUE, 0);
		gtk_widget_set_usize (move_entry, LABAO_WIDTH/10, LABAO_HEIGHT);
		gtk_widget_show(move_entry);

		button = gtk_button_new_with_label ("UP");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_move_callback),
			(gpointer)(&boxes_up));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("DOWN");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_move_callback),
			(gpointer)(&boxes_down));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("LEFT");
			gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_move_callback),
			(gpointer)(&boxes_left));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);

		button = gtk_button_new_with_label ("RIGHT");
		gtk_signal_connect (GTK_OBJECT (button), "clicked",
			GTK_SIGNAL_FUNC (labao_move_callback),
			(gpointer)(&boxes_right));
		gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
		gtk_container_set_border_width (GTK_CONTAINER(button),1);
		gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
		gtk_widget_show(button);
	}

	/* Seventh row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("NUM");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_num_mean_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/15, LABAO_HEIGHT);
        gtk_widget_show(button);

	num_entry = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (num_entry),"200");
        gtk_box_pack_start(GTK_BOX(hbox), num_entry, TRUE, TRUE, 0);
        gtk_widget_set_usize (num_entry, LABAO_WIDTH/10, LABAO_HEIGHT);
        gtk_widget_show(num_entry);

        button = gtk_button_new_with_label ("SAVE ABERR");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_save_aberrations_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("SAVE DATA");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_save_data_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/5, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("PLOT");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_plot_aber_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/10, LABAO_HEIGHT);
        gtk_widget_show(button);

	fps_entry = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (fps_entry),"100");
        gtk_box_pack_start(GTK_BOX(hbox), fps_entry, TRUE, TRUE, 0);
        gtk_widget_set_usize (fps_entry, LABAO_WIDTH/10, LABAO_HEIGHT);
        gtk_widget_show(fps_entry);

        button = gtk_button_new_with_label ("FPS");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_set_fps_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/15, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("THRSH");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_threshold_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/15, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Eigth row of Buttons */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new("ALIGN:");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/13, LABAO_HEIGHT);
        gtk_widget_show(label);

	tries_entry = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (tries_entry),"50");
        gtk_box_pack_start(GTK_BOX(hbox), tries_entry, TRUE, TRUE, 0);
        gtk_widget_set_usize (tries_entry, LABAO_WIDTH/10, LABAO_HEIGHT);
        gtk_widget_show(tries_entry);

        button = gtk_button_new_with_label ("LAB");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_autoalign_lab_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 4*LABAO_WIDTH/25, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("SCOPE");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_autoalign_scope_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 4*LABAO_WIDTH/25, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("BEACON FOC");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_autoalign_beacon_focus_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 4*LABAO_WIDTH/25, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("ZERNIKE");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_autoalign_zernike_callback), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 4*LABAO_WIDTH/25, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("STOP ALIGN");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_STOP_AUTOALIGN));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, 4*LABAO_WIDTH/25, LABAO_HEIGHT);
        gtk_widget_show(button);

} /* fill_wfs_page() */

/************************************************************************/
/* labao_autoalign_lab_callback()					*/
/*									*/
/* Start auto alignment.						*/
/************************************************************************/

void labao_autoalign_lab_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	tries;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(tries_entry));
        sscanf(entry,"%d", &tries);

        mess.type = LABAO_START_AUTOALIGN_LAB;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&tries;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_START_AUTOALIGN_LAB message.\n");
        }

} /* labao_autoalign_lab_callback() */


/************************************************************************/
/* labao_autoalign_scope_callback()					*/
/*									*/
/* Start auto alignment.						*/
/************************************************************************/

void labao_autoalign_scope_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	tries;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(tries_entry));
        sscanf(entry,"%d", &tries);

        mess.type = LABAO_START_AUTOALIGN_SCOPE;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&tries;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_START_AUTOALIGN_SCOPE message.\n");
        }

} /* labao_autoalign_scope_callback() */

/************************************************************************/
/* labao_autoalign_beacon_focus_callback()				*/
/*									*/
/* Start auto alignment.						*/
/************************************************************************/

void labao_autoalign_beacon_focus_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	tries;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(tries_entry));
        sscanf(entry,"%d", &tries);

        mess.type = LABAO_START_AUTOALIGN_BEACON_FOCUS;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&tries;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_START_AUTOALIGN_BEACON_FOCUS message.\n");
        }

} /* labao_autoalign_beacon_focus_callback() */

/************************************************************************/
/* labao_autoalign_zernike_callback()					*/
/************************************************************************/
/* labao_autoalign_zernike_callback()					*/
/*									*/
/* Start auto alignment.						*/
/************************************************************************/

void labao_autoalign_zernike_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	tries;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(tries_entry));
        sscanf(entry,"%d", &tries);

        mess.type = LABAO_START_AUTOALIGN_ZERNIKE;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&tries;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_START_AUTOALIGN_ZERNIKE message.\n");
        }

} /* labao_autoalign_zernike_callback() */

/************************************************************************/
/* labao_num_mean_callback()						*/
/*									*/
/* Set number to use in mean calcualtions.				*/
/************************************************************************/

void labao_num_mean_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	num;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(num_entry));
        sscanf(entry,"%d", &num);

        mess.type = LABAO_SET_NUM_MEAN;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&num;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,"Failed to send LABAO_SET_NUM_MEAN message.\n");
        }

} /* labao_num_mean_callback() */

/************************************************************************/
/* labao_set_fps_callback()						*/
/*									*/
/* Set number to use in mean calcualtions.				*/
/************************************************************************/

void labao_set_fps_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	float	fps;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(fps_entry));
        sscanf(entry,"%f", &fps);

        mess.type = LABAO_SET_FPS;
        mess.length = sizeof(float);
        mess.data = (unsigned char *)&fps;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,"Failed to send LABAO_SET_FPS message.\n");
        }

} /* labao_set_fps_callback() */

/************************************************************************/
/* labao_threshold_callback()						*/
/*									*/
/* Set number to use in mean calcualtions.				*/
/************************************************************************/

void labao_threshold_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	float	threshold;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(fps_entry));
        sscanf(entry,"%f", &threshold);
	gtk_entry_set_text (GTK_ENTRY (fps_entry),"0");

        mess.type = LABAO_SET_THRESHOLD;
        mess.length = sizeof(float);
        mess.data = (unsigned char *)&threshold;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,"Failed to send LABAO_SET_THRESHOLD message.\n");
        }

} /* labao_threshold_callback() */

/************************************************************************/
/* labao_save_aberrations_callback()					*/
/*									*/
/* Ask to save the data.						*/
/************************************************************************/

void labao_save_aberrations_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	num;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(num_entry));
        sscanf(entry,"%d", &num);

        mess.type = LABAO_SAVE_ABERRATIONS;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&num;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_SAVE_ABERRATIONS message.\n");
        }

} /* labao_save_aberrartions_callback() */

/************************************************************************/
/* labao_save_data_callback()						*/
/*									*/
/* Ask to save the data.						*/
/************************************************************************/

void labao_save_data_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	int	num;
        char    *entry;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(num_entry));
        sscanf(entry,"%d", &num);

        mess.type = LABAO_SAVE_DATA;
        mess.length = sizeof(int);
        mess.data = (unsigned char *)&num;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_SAVE_DATA message.\n");
        }

} /* labao_save_data_callback() */

/************************************************************************/
/* labao_move_callback()						*/
/*									*/
/************************************************************************/

void labao_move_callback(GtkButton *button, gpointer data)
{
        struct smessage mess;
	struct s_labao_move_boxes move_data;
        char    *entry;
	float	move;
	int	dir;

        entry = (char *)gtk_entry_get_text(GTK_ENTRY(move_entry));
        sscanf(entry,"%f", &move);
	dir = *((int *) data);

	switch(dir)
	{
		case 0: move_data.dx = 0.0;
			       	move_data.dy = move;
				break;

		case 1: move_data.dx = 0.0;
			       	move_data.dy = -1.0 * move;
				break;

		case 2: move_data.dx = -1.0 * move;
			       	move_data.dy = 0.0;
				break;

		case 3: move_data.dx = move;
			       	move_data.dy = 0.0;
				break;

		default: print_status(ERROR,"Unknown move for Boxes.");
				return;
	}

        mess.type = LABAO_MOVE_BOXES;
        mess.length = sizeof(move_data);
        mess.data = (unsigned char *)&move_data;

        if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_MOVE_BOXES message.\n");
        }

} /* labao_move_callback() */
