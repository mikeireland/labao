/************************************************************************/
/* dm_page.c   	                                                	*/
/*                                                                      */
/* Stuff for controlling the DM.					*/
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

/* Globals */

static GtkWidget *actuator_labels[LABAO_NUM_ACTUATORS+1];
static GtkWidget *actuator_number_entry;
static GtkWidget *actuator_value_entry;
static float foc_plus = 0.005;
static float foc_minus = -0.005;

/************************************************************************/
/* fill_dm_page()							*/
/*									*/
/* Put in all the things you need for teh DITHER control page 		*/
/************************************************************************/

void fill_dm_page(GtkWidget *vbox)
{
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkObject *adjustment;

	/* OK, show the box */

	gtk_widget_show(vbox);

	/* First row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        /* Button to UPDATE */

        button = gtk_button_new_with_label ("SHOW");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_SHOW_EDAC40));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

	actuator_labels[23] = gtk_label_new("+XXXX23");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[23] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[23], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[23]);

	actuator_labels[24] = gtk_label_new("+XXXX24");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[24] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[24], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[24]);

	actuator_labels[25] = gtk_label_new("+XXXX25");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[25] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[25], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[25]);

	actuator_labels[26] = gtk_label_new("+XXXX26");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[26] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[26], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[26]);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

	/* Create a bunch of spin buttons */

	label = gtk_label_new("Act/Zern#:");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);
	adjustment = gtk_adjustment_new(1, 1, LABAO_NUM_ACTUATORS, 1, 1, 0);
	actuator_number_entry = 
		gtk_spin_button_new((GtkAdjustment *)adjustment,1.0,0);
        gtk_box_pack_start(GTK_BOX(hbox), 
		actuator_number_entry, TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_number_entry, 
		LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(actuator_number_entry);

	label = gtk_label_new("Value:");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);
	actuator_value_entry = gtk_entry_new ();
        gtk_entry_set_text (GTK_ENTRY (actuator_value_entry),"0.0");
        gtk_box_pack_start(GTK_BOX(hbox), 
		actuator_value_entry, TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_value_entry, 
		LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(actuator_value_entry);

	/* Second row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("FOC+");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (edac40_add), (gpointer)(&foc_plus));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	actuator_labels[22] = gtk_label_new("+XXXX22");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[22] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[22], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[22]);

	actuator_labels[10] = gtk_label_new("+XXXX10");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[10] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[10], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[10]);

	actuator_labels[11] = gtk_label_new("+XXXX11");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[11] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[11], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[11]);

	actuator_labels[12] = gtk_label_new("+XXXX12");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[12] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[12], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[12]);

	actuator_labels[27] = gtk_label_new("+XXXX27");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[27] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[27], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[27]);

        /* Button to SET ZERNIKE */

        button = gtk_button_new_with_label ("SET/ZERN");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (set_zernike_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

        /* Button to SET SINGLE */

        button = gtk_button_new_with_label ("SET/ACT");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (set_channel_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Third row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

	actuator_labels[21] = gtk_label_new("+XXXX21");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[21] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[21], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[21]);

	actuator_labels[9] = gtk_label_new("+XXXXX9");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[9] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[9], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[9]);

	actuator_labels[3] = gtk_label_new("+XXXXX3");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[3] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[3], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[3]);

	actuator_labels[4] = gtk_label_new("+XXXXX4");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[4] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[4], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[4]);

	actuator_labels[13] = gtk_label_new("+XXXX13");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[13] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[13], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[13]);

	actuator_labels[28] = gtk_label_new("+XXXX28");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[28] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[28], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[28]);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

        /* Button to DELTA SINGLE */

        button = gtk_button_new_with_label ("INC/ACT");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (delta_channel_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Fourth row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

	actuator_labels[20] = gtk_label_new("+XXXX20");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[20] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[20], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[20]);

	actuator_labels[8] = gtk_label_new("+XXXXX8");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[8] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[8], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[8]);

	actuator_labels[2] = gtk_label_new("+XXXXX2");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[2] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[2], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[2]);

	actuator_labels[1] = gtk_label_new("+XXXXX1");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[1] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[1], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[1]);

	actuator_labels[5] = gtk_label_new("+XXXXX5");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[5] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[5], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[5]);

	actuator_labels[14] = gtk_label_new("+XXXX14");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[14] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[14], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[14]);

	actuator_labels[29] = gtk_label_new("+XXXX29");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[29] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[29], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[29]);

        /* Button to UPDATE */

        button = gtk_button_new_with_label (" UPDATE ");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_VALUE_ALL_CHANNELS));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Fifth row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

	actuator_labels[37] = gtk_label_new("+XXXX37");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[37] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[37], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[37]);

	actuator_labels[19] = gtk_label_new("+XXXX19");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[19] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[19], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[19]);

	actuator_labels[7] = gtk_label_new("+XXXXX7");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[7] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[7], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[7]);

	actuator_labels[6] = gtk_label_new("+XXXXX6");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[6] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[6], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[6]);

	actuator_labels[15] = gtk_label_new("+XXXX15");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[15] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[15], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[15]);

	actuator_labels[30] = gtk_label_new("+XXXX30");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[30] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[30], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[30]);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

        /* Button to SET ALL */

        button = gtk_button_new_with_label ("SET/ALL");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (set_all_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Sixth row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("FOC-");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (edac40_add), (gpointer)(&foc_minus));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	actuator_labels[36] = gtk_label_new("+XXXX36");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[36] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[36], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[36]);

	actuator_labels[18] = gtk_label_new("+XXXX18");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[18] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[18], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[18]);

	actuator_labels[17] = gtk_label_new("+XXXX17");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[17] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[17], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[17]);

	actuator_labels[16] = gtk_label_new("+XXXX16");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[16] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[16], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[16]);

	actuator_labels[31] = gtk_label_new("+XXXX31");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[31] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[31], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[31]);

        /* Button to INC ZERNIKE */

        button = gtk_button_new_with_label ("INC/ZERN");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (inc_zernike_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

        /* Button to DELTA ALL */

        button = gtk_button_new_with_label ("INC/ALL");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (delta_all_callback),NULL);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Seventh and last row of actuator position labels */

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);
	gtk_widget_show(hbox);

        button = gtk_button_new_with_label ("STOP");
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_signal_callback), 
                (gpointer)(signal_array+LABAO_SIGNAL_EXIT));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

	actuator_labels[35] = gtk_label_new("+XXXX35");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[35] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[35], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[35]);

	actuator_labels[34] = gtk_label_new("+XXXX34");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[34] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[34], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[34]);

	actuator_labels[33] = gtk_label_new("+XXXX33");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[33] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[33], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[33]);

	actuator_labels[32] = gtk_label_new("+XXXX32");
        gtk_box_pack_start(GTK_BOX(hbox), actuator_labels[32] , TRUE, TRUE, 0);
        gtk_widget_set_usize (actuator_labels[32], LABAO_WIDTH/9, LABAO_HEIGHT);
	gtk_widget_show(actuator_labels[32]);

	label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label , TRUE, TRUE, 0);
        gtk_widget_set_usize (label, LABAO_WIDTH/18, LABAO_HEIGHT);
	gtk_widget_show(label);

        /* Button to OPEN/CLOSE EDAC */

        button = gtk_button_new_with_label ("OPEN/DM");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_OPEN_EDAC40));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

        button = gtk_button_new_with_label ("CLOSE/DM");
        gtk_signal_connect (GTK_OBJECT (button), "clicked",
                GTK_SIGNAL_FUNC (labao_message_callback),
                (gpointer)(message_array+LABAO_CLOSE_EDAC40));
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER(button),1);
        gtk_widget_set_usize (button, LABAO_WIDTH/9, LABAO_HEIGHT);
        gtk_widget_show(button);

	/* Fifth row of actuator position labels */
} /* fill_dm_page() */

/************************************************************************/
/* update_dm_page()							*/
/*									*/
/* Tries to update the DM page, mostly the actuator positions.		*/
/************************************************************************/

void update_dm_page(float *values)
{
	int	i;
	char	s[10];

#warning We are bypassing engineering mode for now.
	//if (!engineering_mode) return;

	for(i=1; i<= LABAO_NUM_ACTUATORS; i++)
	{
		sprintf(s, "%2d-%7.4f", i, values[i]);
		gtk_label_set_text((GtkLabel *) actuator_labels[i], s); 
	}

	send_ready_for_display = TRUE;

} /* update_dm_page() */

/************************************************************************/
/* set_channel_callback()						*/
/************************************************************************/

void set_channel_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	struct s_labao_eda40_set_channel value;
	char	*entry;

	value.channel = gtk_spin_button_get_value_as_int(
				(GtkSpinButton *)actuator_number_entry);
	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value.value);

	mess.type = LABAO_SET_CHANNEL;
	mess.length = sizeof(struct s_labao_eda40_set_channel);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR, "Failed to send LABAO_SET_CHANNEL message.\n");
        }

} /* set_channel_callback() */

/************************************************************************/
/* delta_channel_callback()						*/
/************************************************************************/

void delta_channel_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	struct s_labao_eda40_set_channel value;
	char	*entry;

	value.channel = gtk_spin_button_get_value_as_int(
				(GtkSpinButton *)actuator_number_entry);
	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value.value);

	mess.type = LABAO_DELTA_CHANNEL;
	mess.length = sizeof(struct s_labao_eda40_set_channel);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR, "Failed to send LABAO_DELTA_CHANNEL message.\n");
        }

} /* delta_channel_callback() */

/************************************************************************/
/* set_all_callback()						*/
/************************************************************************/

void set_all_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	float	value;
	char	*entry;

	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value);

	mess.type = LABAO_SET_ALL_CHANNELS;
	mess.length = sizeof(float);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_SET_ALL_CHANNELS message.\n");
        }

} /* set_all_callback() */

/************************************************************************/
/* delta_all_callback()							*/
/************************************************************************/

void delta_all_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	float value;
	char	*entry;

	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value);

	mess.type = LABAO_DELTA_ALL_CHANNELS;
	mess.length = sizeof(float);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR,
		"Failed to send LABAO_DELTA_ALL_CHANNELS message.\n");
        }

} /* delta_all_callback() */

/************************************************************************/
/* set_zernike_callback()						*/
/************************************************************************/

void set_zernike_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	struct s_labao_eda40_set_channel value;
	char	*entry;

	value.channel = gtk_spin_button_get_value_as_int(
				(GtkSpinButton *)actuator_number_entry);
	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value.value);

	mess.type = LABAO_SET_ZERNIKE;
	mess.length = sizeof(struct s_labao_eda40_set_channel);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR, "Failed to send LABAO_SET_ZERNIKE message.\n");
        }

} /* set_zernike_callback() */

/************************************************************************/
/* inc_zernike_callback()						*/
/************************************************************************/

void inc_zernike_callback(GtkButton *button, gpointer data)
{
	struct smessage mess;
	struct s_labao_eda40_set_channel value;
	char	*entry;

	value.channel = gtk_spin_button_get_value_as_int(
				(GtkSpinButton *)actuator_number_entry);
	entry = (char *)gtk_entry_get_text(GTK_ENTRY(actuator_value_entry));
	sscanf(entry,"%f", &value.value);

	mess.type = LABAO_INC_ZERNIKE;
	mess.length = sizeof(struct s_labao_eda40_set_channel);
	mess.data = (unsigned char *)&value;

	if (!send_message(server, &mess))
        {
          print_status(ERROR, "Failed to send LABAO_INC_ZERNIKE message.\n");
        }

} /* inc_zernike_callback() */

/************************************************************************/
/* edac40_add()								*/
/************************************************************************/

void edac40_add(GtkButton *button, gpointer data)
{
	struct smessage mess;
	float	add;

	add = *((float *)data);

	mess.type = LABAO_EDAC40_ADD;
	mess.length = sizeof(add);
	mess.data = (unsigned char *)&add;

	if (!send_message(server, &mess))
        {
          print_status(ERROR, "Failed to send LABAO_EDAC40_ADD message.\n");
        }

} /* edac40_add() */
