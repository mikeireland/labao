/************************************************************************/
/* messages.c 		                                                */
/*                                                                      */
/* Some message handlers and other related routines. Note some message	*/
/* handlers will not be here since they are specific to certain		*/
/* windows.								*/
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
/* Date   : March 2014                                                  */
/************************************************************************/

#include "labaogtk.h"

/************************************************************************/
/* set_labao_messages()							*/
/*									*/
/* Routine to create pointers to all the messages we have.		*/
/************************************************************************/

void set_labao_messages(void)
{
        if (!add_message_job(server, LABAO_VALUE_ALL_CHANNELS, 
		message_labao_value_all_channels))
        {
                fprintf(stderr,"Failed to add LABAO_VALUE_ALL_CHANNELS job.\n");
                exit(-8);
        }
        if (!add_message_job(server, LABAO_SET_USB_CAMERA, 
		message_labao_set_usb_camera))
        {
                fprintf(stderr,"Failed to add LABAO_SET_USB_CAMERA job.\n");
                exit(-8);
	}
        if (!add_message_job(server, LABAO_SEND_CENTROIDS, 
		message_labao_send_centroids))
        {
                fprintf(stderr,"Failed to add LABAO_SEND_CENTROIDS job.\n");
                exit(-8);
        }

        if (!add_message_job(server, LABAO_USB_IMAGE, 
		message_labao_usb_image))
        {
                fprintf(stderr,"Failed to add LABAO_USB_IMAGE job.\n");
                exit(-8);
        }
        if (!add_message_job(server, LABAO_STOP_USB_IMAGE, 
		message_labao_stop_usb_image))
        {
                fprintf(stderr,"Failed to add LABAO_STOP_USB_IMAGE job.\n");
                exit(-8);
        }
        if (!add_message_job(server, LABAO_WFS_RESULTS, 
		message_labao_wfs_results))
        {
                fprintf(stderr,"Failed to add LABAO_WFS_RESULTS job.\n");
                exit(-8);
        }

        if (!add_message_job(server, LABAO_TEXT_MESSAGE, 
		message_labao_text_message))
        {
                fprintf(stderr,"Failed to add LABAO_TEXT_MESSAGE job.\n");
                exit(-8);
        }

} /* set_labao_messages() */

/************************************************************************/
/* message_labao_value_all_channels()                                   */
/*                                                                      */
/************************************************************************/

int message_labao_value_all_channels(int server, struct smessage *mess)
{
        if (mess->length != (LABAO_NUM_ACTUATORS+1)*sizeof(float))
        {
                print_status(ERROR,
                "Got LABAO_VALUE_ALL_CHANNELS MESSAGE with bad data.\n");
                return NOERROR;
        }

	update_dm_page((float *)mess->data);

        return NOERROR;

} /* message_labao_value_all_channels() */

/************************************************************************/
/* message_labao_set_usb_camera()                                       */
/*                                                                      */
/************************************************************************/

int message_labao_set_usb_camera(int server, struct smessage *mess)
{
        if (mess->length != sizeof(usb_camera))
        {
                print_status(ERROR,
                "Got LABAO_SET_USB_CAMERA message with bad data.\n");
                return NOERROR;
        }


	usb_camera = *((struct s_labao_usb_camera *)mess->data);

#warning Place holder
	//update_usb_camera();

        return NOERROR;

} /* message_labao_set_usb_camera() */

/************************************************************************/
/* message_labao_send_centroids()                                       */
/*                                                                      */
/************************************************************************/

int message_labao_send_centroids(int server, struct smessage *mess)
{
	unsigned char *compressed_values;
	float	*fp, *values;
	uLongf	len, clen;
	int	i;
	char	*pc1, *pc2;

        if (mess->length == 0)
        {
                print_status(ERROR,
                "Got LABAO_SEND_CENTROIDS message with bad data.\n");
                return NOERROR;
        }

	/* Do we know how many lenslets there are? */

	if (usb_camera.num_lenslets == 0)
        {
                print_status(ERROR,
                "Got LABAO_SEND_CENTROIDS message without num_lenslets.\n");
                return NOERROR;
        }

	/* Deallocate and reallocate the memory */

	if (x_centroid_offsets != NULL) free(x_centroid_offsets);
	if (y_centroid_offsets != NULL) free(y_centroid_offsets);
	if (xc != NULL) free(xc);
	if (yc != NULL) free(yc);
	if (avg_fluxes != NULL) free(avg_fluxes);
	if ((x_centroid_offsets = malloc(usb_camera.num_lenslets*sizeof(float)))
			== NULL ||
	    (y_centroid_offsets = malloc(usb_camera.num_lenslets*sizeof(float)))
			== NULL ||
	    (xc = malloc(usb_camera.num_lenslets*sizeof(float)))
			== NULL ||
	    (yc = malloc(usb_camera.num_lenslets*sizeof(float)))
			== NULL ||
	    (avg_fluxes = malloc(usb_camera.num_lenslets*sizeof(float)))
			== NULL)
        {
                print_status(ERROR,"Ran out of memory!\n");
                return ERROR;
	}

	clen = mess->length;
	len = 4 * clen;

	if ((values = (float *)malloc(len)) == NULL ||
	     (compressed_values = malloc(clen)) == NULL)
        {
                print_status(ERROR,"Ran out of memory!\n");
                return FALSE;
	}

	pc1 = (char *)mess->data;
	pc2 = (char *)compressed_values;
	for (i=0; i < clen; i++) *pc2++ = *pc1++;

	if (uncompress((unsigned char *)values, &len,
                        compressed_values, clen) != Z_OK)
        {
                print_status(ERROR,"Uncompress failure on image.\n");
                free(compressed_values);
                free(values);
                return FALSE;
        }

	/* Did we get the right amount of data? */

	if (len != 5 * usb_camera.num_lenslets * sizeof(float))
        {
                print_status(ERROR,"Uncompressed size %d!=%d wrong.\n",
			len, 5 * usb_camera.num_lenslets * sizeof(float));
                free(compressed_values);
                free(values);
                return FALSE;
        }

	/* OK, transfer the data */

	for(fp = values, i = 0; i < usb_camera.num_lenslets; i++)
		x_centroid_offsets[i] = *fp++;
	for(i = 0; i < usb_camera.num_lenslets; i++)
		y_centroid_offsets[i] = *fp++;
	for(i = 0; i < usb_camera.num_lenslets; i++)
		xc[i] = *fp++;
	for(i = 0; i < usb_camera.num_lenslets; i++)
		yc[i] = *fp++;
	for(i = 0; i < usb_camera.num_lenslets; i++)
		avg_fluxes[i] = *fp++;

	/* That should be that */

	free(compressed_values);
	free(values);
	
        return NOERROR;

} /* message_labao_send_centroids() */

/************************************************************************/
/* message_labao_wfs_results()                                          */
/*                                                                      */
/************************************************************************/

int message_labao_wfs_results(int server, struct smessage *mess)
{
        if (mess->length != sizeof(wfs_results))
        {
                print_status(ERROR,
                "Got LABAO_WFS_RESULTS message with bad data.\n");
                return NOERROR;
        }

	wfs_results = *((struct s_labao_wfs_results *)mess->data);

	update_wfs_results();

        return NOERROR;

} /* message_labao_wfs_results() */

/************************************************************************/
/* message_labao_text_message()                                          */
/*                                                                      */
/************************************************************************/

int message_labao_text_message(int server, struct smessage *mess)
{
        if (mess->length == 0) return NOERROR;

	gtk_label_set_text((GtkLabel *)message_label, (char *)mess->data);

        return NOERROR;

} /* message_labao_text_message() */
