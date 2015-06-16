/************************************************************************/
/* display.c 		                                                */
/*                                                                      */
/* Hanldes messages for displaying usb_images.				*/
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
/* Date   : May 2014                                                    */
/************************************************************************/

#include "labaogtk.h"
#include <zlib.h>
#include <math.h>

static XImage *usb_image_image = NULL;
static char usb_image_window_up = FALSE;
static Window usb_image_window;
static float **values = NULL;
static int last_usb_disp_x = -1;
static int last_usb_disp_y = -1;

/************************************************************************/
/* message_labao_usb_image() 	                                        */
/*                                                                      */
/************************************************************************/

int message_labao_usb_image(int server, struct smessage *mess)
{
        unsigned char *compressed_values;
	unsigned char *uncompressed_values;
	char *usb_image;
	uLongf  len, clen;
	int	i,j;
	unsigned char *pu1, achar;
	char	*pc1, *pc2;

	/* Do we do anything at all? */

        if (!do_local_display) return NOERROR;

	/* Is there any data? */

        if (mess->length == 0)
        {
                print_status(ERROR,
			"Got LABAO_USB_IMAGE with no data.\n");
		send_ready_for_display = TRUE;
                return TRUE;
        }

	/* Do we know how big the image is suppose to be? */

	if (usb_camera.usb_disp_x <= 0 || usb_camera.usb_disp_y <= 0)
	{
		print_status(ERROR,"Got image without status (%d,%d)!\n",
			usb_camera.usb_disp_x, usb_camera.usb_disp_y);
		send_ready_for_display = TRUE;
		return FALSE;
	}

	clen = mess->length;
	len = usb_camera.usb_disp_x * usb_camera.usb_disp_y / 2;
		
	/* Allocate memory */

	if ((uncompressed_values = malloc(len)) == NULL)
	{
		print_status(ERROR,"Ran out of memory!\n");
		send_ready_for_display = TRUE;
		return FALSE;
	}
	
	if ((compressed_values = malloc(clen)) == NULL)
	{
		print_status(ERROR,"Ran out of memory!\n");
		free(uncompressed_values);
		send_ready_for_display = TRUE;
		return FALSE;
	}
	
	/* Copy the compressed stuff */

	pc1 = (char *)mess->data;
	pc2 = (char *)compressed_values;

	for(i=0; i<clen; i++) *pc2++ = *pc1++;

	/* Uncompress the data */

	if (uncompress((unsigned char *)uncompressed_values, &len, 
			compressed_values, clen) != Z_OK)
	{
		print_status(ERROR,"Uncompress failure on image.\n");
		free(compressed_values);
		free(uncompressed_values);
		send_ready_for_display = TRUE;
		return FALSE;
	}
	
	/* Does it come out right? */

	if (len != usb_camera.usb_disp_x*usb_camera.usb_disp_y/2)
	{
		print_status(ERROR,
			"Uncompressed size does not match frame size.\n");
		free(compressed_values);
		free(uncompressed_values);
		send_ready_for_display = TRUE;
		return FALSE;
	}

	/* Have the dimmensions changed? */

	if (usb_camera.usb_disp_x != last_usb_disp_x || 
		usb_camera.usb_disp_y != last_usb_disp_y)
	{
		if (values != NULL) 
			free_matrix(values,1,last_usb_disp_x,1,last_usb_disp_y);
		values =matrix(1,usb_camera.usb_disp_x,1,usb_camera.usb_disp_y);

		if (usb_image_window_up)
		{
			XDestroyWindow(theDisplay, usb_image_window);
			usb_image_window_up = FALSE;
		}
		last_usb_disp_x = usb_camera.usb_disp_x;
		last_usb_disp_y = usb_camera.usb_disp_y;
	}

	/* Copy the data */

	for(pu1 = (unsigned char *) uncompressed_values, i=1; i<=usb_camera.usb_disp_x; i++)
	for(j= 1; j<=usb_camera.usb_disp_y; j+=2)
	  {
	    achar = *pu1++;
	    values[i][j] = (achar % 16)*(usb_camera.max - usb_camera.min) + usb_camera.min;
	    values[i][j+1] = (achar / 16)*(usb_camera.max - usb_camera.min) + usb_camera.min;
	  }

	/* OK, finished with the memory */

	free(compressed_values);
	free(uncompressed_values);

	/* Put up the window if we have to */

	if (!usb_image_window_up)
	{
		usb_image_window = openWindow(server_name,
			theWidth-usb_camera.usb_disp_x-30,
			theHeight-usb_camera.usb_disp_y-30,
			usb_camera.usb_disp_x,
			usb_camera.usb_disp_y);
		usb_image_window_up = TRUE;
	}

	/* Display it */

	usb_image = make_picture(usb_camera.usb_disp_x,
				 usb_camera.usb_disp_y,1,
                                 values,LIN);
	if (usb_camera.overlay_boxes)
	{
	    for (i=0;i<usb_camera.num_lenslets;i++)
	    {
		overlay_rectangle(usb_camera.usb_disp_x, usb_camera.usb_disp_y,
		    usb_image,
		    (x_centroid_offsets[i] -
		    usb_camera.centroid_box_width/2-1)/usb_camera.x_mult,
		    usb_camera.usb_disp_y-(y_centroid_offsets[i] +
		    usb_camera.centroid_box_width/2-1)/usb_camera.y_mult,
		    (x_centroid_offsets[i] +
		    usb_camera.centroid_box_width/2-1)/usb_camera.x_mult,
		    usb_camera.usb_disp_y-(y_centroid_offsets[i] -
		    usb_camera.centroid_box_width/2-1)/usb_camera.y_mult,
		    1,0,65535,0);

		overlay_rectangle(usb_camera.usb_disp_x, usb_camera.usb_disp_y,
		    usb_image,
		    (x_centroid_offsets[i] +
		    xc[i] - 1-1)/usb_camera.x_mult,
		    usb_camera.usb_disp_y-(y_centroid_offsets[i] +
		    yc[i] + 1-1)/usb_camera.y_mult,
		    (x_centroid_offsets[i] + xc[i] + 1-1)/
		    usb_camera.x_mult,
		    usb_camera.usb_disp_y-(y_centroid_offsets[i] +
		    yc[i] - 1-1)/usb_camera.y_mult,
		    1,65535,0,0);
	    }
	}

	usb_image_image = XCreateImage(theDisplay,theVisual,
				    theDepth,ZPixmap,0, usb_image,
				    usb_camera.usb_disp_x,
				    usb_camera.usb_disp_y,
                                    theBitmapPad, 0);
	XPutImage(theDisplay,usb_image_window,theGC, usb_image_image,0,0,0,0,
				    usb_camera.usb_disp_x,
				    usb_camera.usb_disp_y);
	XMapWindow(theDisplay, usb_image_window);
	XFlush(theDisplay);

	if (usb_image_image != NULL) XDestroyImage(usb_image_image);

	/* We must be ready for another image */

	send_ready_for_display = TRUE;

        return TRUE;

} /* message_labao_usb_image() */

/************************************************************************/
/* stop_usb_image()                                                     */
/*                                                                      */
/************************************************************************/

int message_labao_stop_usb_image(int server, struct smessage *mess)
{
	if (usb_image_window_up)
        {
                usb_image_window_up = FALSE;
		usleep(200000);
                XDestroyWindow(theDisplay, usb_image_window);
		XFlush(theDisplay);
        }       

	return TRUE;

} /* stop_usb_image() */
