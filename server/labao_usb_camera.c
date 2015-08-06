/************************************************************************/
/* labao_usb_camera.c							*/
/*    									*/
/* Routines for getting data from the USB camera.			*/
/************************************************************************/
/*    									*/
/*  CHARA ARRAY USER INTERFACE						*/
/*     Based on the SUSI User Interface					*/
/*		In turn based on the CHIP User interface		*/
/*    									*/
/*Center for High Angular Resolution Astronomy  			*/
/*  Mount Wilson Observatory, CA 91001, USA				*/
/*    									*/
/* Telephone: 1-626-796-5405  						*/
/* Fax: 1-626-796-6717  						*/
/* email    : theo@chara.gsu.edu    					*/
/* WWW: http://www.chara.gsu.edu					*/
/*    									*/
/* (C) This source code and its associated executable 			*/
/* program(s) are copyright.  						*/
/*    									*/
/************************************************************************/
/*    									*/
/* Author : Theo ten Brummelaar 			    		*/
/* Date   : Oct 2002							*/
/************************************************************************/

#include <ueye.h>
#define LONGLONG_TYPE /* There is a conflict on this fitsio.h ands ueye.h */
#include "labao.h"

/* How many frames do we need in the ring buffer? */

#define NUM_IMAGE_MEM 	50
#define USB_DISP_X	640
#define USB_DISP_Y	512 /* !!! Must be a multiple of 2 */
#define MAX_FILE_NUMBER	999
#define SECS_FOR_PROC  2
#define NUM_MIN 20 /*The number of minimum pixels in each row to average over */

static char fits_filename[2000];
static bool save_fits_file = FALSE;
static float **data_frames[NUM_IMAGE_MEM];
static float **dark;
static float **sum_frame;
static float **this_frame;
static int num_sum_frame = 1;
static int count_sum_frame = 0;
static int current_data_frame_ix=0;
static UEYE_CAMERA_LIST *cam_list;
static HIDS	cam_pointer;
static HWND	cam_window;
static SENSORINFO cam_info;
static IS_RECT 	rectAOI;
static IS_POINT_2D	pos_inc;
static IS_POINT_2D	size_inc;
static IS_POINT_2D	size_min;
static int	pid_image_memory[NUM_IMAGE_MEM];
static char	*image_memory[NUM_IMAGE_MEM];
static float	*image_data_cube = NULL;
static float	*image_data_cube_pointer = NULL;
static int 	image_data_cube_num_frames = 0;
static int 	image_data_cube_count_frames = 0;
static bool	usb_camera_open = FALSE;
static pthread_mutex_t usb_camera_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t usb_camera_thread;
static bool usb_camera_running = FALSE;
static int usb_camera_num_frames = 0;
static int usb_camera_last_num_frames=0;
static void (*usb_camera_callback)(CHARA_TIME time_stamp,
		float **data, int width, int height) = NULL;
static time_t usb_camera_last_proc_sec = 0;
static Window usb_camera_window;
static XImage *usb_camera_ximage = NULL;
static bool usb_camera_display = FALSE;
static void *usb_camera_image;
static time_t usb_camera_display_start = 0;
static int usb_camera_display_frames = 0;
static bool usb_camera_local_display = FALSE;
static int data_record_start = 0;
static int data_record_stop = 0;

/************************************************************************/
/* open_usb_camera()							*/
/*									*/
/* Tries to open a conenction the USB camera and find out what it can	*/
/* do.									*/
/* Returns error level.							*/
/************************************************************************/

int open_usb_camera(void)
{
	int	i, j;
	int	dw;
	int	bits_per_pixel;
	int	num_cams;
	double  ferr;

	/* Close it first if we need to */

	if (usb_camera_open) close_usb_camera();

	/* By default we destripe and show the boxes */

	usb_camera.overlay_boxes = TRUE;
	usb_camera.destripe = TRUE;

	/* How many cameras are there out there? */

	if (is_GetNumberOfCameras(&num_cams))
	{
		return error(ERROR,"Failed to get number of cameras.\n");
	}

	if (num_cams == 1)
		message(system_window, "Found %d camera.\n", num_cams);
	else
		message(system_window, "Found %d cameras.\n", num_cams);

	if (num_cams < 1) return error(ERROR,"Found no cameras.");

	/* Get the list of cameras */

	cam_list = (UEYE_CAMERA_LIST *)malloc(sizeof(UEYE_CAMERA_LIST));
	if (is_GetCameraList(cam_list))
	{
		return error(ERROR,"Failed to get camera list.\n");
	}

	/* How many cameras? */

	dw = cam_list->dwCount;

	/* Now get the correct amount of space for the camera info */

	free(cam_list);
	cam_list = (UEYE_CAMERA_LIST *)malloc(sizeof(DWORD) + 
			dw * sizeof(UEYE_CAMERA_INFO));
	cam_list->dwCount = dw;

	if (is_GetCameraList(cam_list))
	{
		return error(ERROR,"Failed to get camera list.\n");
	}

	/* 
  	 * Open the camera... should be in the same thread as 
  	 * SetDisplayMode() and ExitCamera()
  	 * We may have to do tricky things with is_SetCameraID when we
  	 * have more than one camera.
 	 */

	cam_pointer = (HIDS)0x1; /* With luck, this is the right camera */
	if ((i = is_InitCamera(&cam_pointer, &cam_window)))
	{
		return error(ERROR,
			"Failed to open camera connection (%d).\n",i);
	}

	/* Get the information about the camera sensor */

	if ((i = is_GetSensorInfo(cam_pointer, &cam_info)))
	{
		close_usb_camera();
		return error(ERROR,
			"Failed to get sensor information (%d).\n",i);
	}

	/* Allocate the memory, now that we have this basic info. */

	for (i=0;i<NUM_IMAGE_MEM;i++)
		data_frames[i] = 
			matrix(1, cam_info.nMaxWidth, 1, cam_info.nMaxHeight);
	dark = matrix(1, cam_info.nMaxWidth, 1, cam_info.nMaxHeight);
	sum_frame = matrix(1, cam_info.nMaxWidth, 1, cam_info.nMaxHeight);
	this_frame = matrix(1, cam_info.nMaxWidth, 1, cam_info.nMaxHeight);

	for (i=1;i<=cam_info.nMaxWidth;i++)
	for (j=1;j<=cam_info.nMaxWidth;j++) dark[i][j]=0.0;

	bits_per_pixel = 8;

	/* Allocate Memory for Images. This is done based on a full frame. */

	for(j = 0; j< NUM_IMAGE_MEM; j++)
	{
	    if ((i = is_AllocImageMem(cam_pointer,  
			cam_info.nMaxWidth, cam_info.nMaxHeight,
			bits_per_pixel, 
			&(image_memory[j]), &(pid_image_memory[j]))))
	    {
		close_usb_camera();
		return error(ERROR,"Failed to get image memory (%d).\n",i);
	    }
	
	    /* Make this part of the sequence in the ring buffer */

	    if ((i = is_AddToSequence(cam_pointer, 
			image_memory[j], pid_image_memory[j])))
	    {
		close_usb_camera();
		return error(ERROR,
		"Failed to add image memory to sequence (%d).\n", i);
	    }
	}

	usb_camera_open = TRUE;
	
	/* What are we able to do in terms of AOI? */

	if ((i = is_AOI(cam_pointer,  IS_AOI_IMAGE_GET_POS_INC,
				&pos_inc, sizeof(pos_inc))))
	{
		close_usb_camera();
		return error(ERROR,
			"Failed to get position incriment (%d).\n",i);
	}
	
	if ((i = is_AOI(cam_pointer,  IS_AOI_IMAGE_GET_SIZE_INC,
				&size_inc, sizeof(size_inc))))
	{
		close_usb_camera();
		return error(ERROR,
			"Failed to get size incriment (%d).\n",i);
	}
	
	if ((i = is_AOI(cam_pointer,  IS_AOI_IMAGE_GET_SIZE_MIN,
				&size_min, sizeof(size_min))))
	{
		close_usb_camera();
		return error(ERROR,
			"Failed to get size incriment (%d).\n",i);
	}
	
	/* Default is the whole camera */

	rectAOI.s32X = 0;
	rectAOI.s32Y = 0;
	rectAOI.s32Width = cam_info.nMaxWidth;
	rectAOI.s32Height = cam_info.nMaxHeight;
	
	if ((i = is_AOI(cam_pointer,  IS_AOI_IMAGE_SET_AOI,
				&rectAOI, sizeof(rectAOI))))
	{
		close_usb_camera();
		return error(ERROR,"Failed to set AOI (%d).\n",i);
	} 

	/* With luck that worked */

	if ((i = is_AOI(cam_pointer,  IS_AOI_IMAGE_GET_AOI,
					&rectAOI, sizeof(rectAOI))))
	{
		close_usb_camera();
		return error(ERROR,"Failed to get AOI (%d).\n",i);
	}

	message(system_window,
		"Camera set to %dx%d pixels. Pos inc %dx%d Size in %dx%d",
			rectAOI.s32Width, rectAOI.s32Height,
			pos_inc.s32X, pos_inc.s32Y,
			size_inc.s32X, size_inc.s32Y);

	/* Set Gain Boost off. This increased gain also increases pattern
	   noise in the dark, which can't be destriped. */

	if ((i = is_SetGainBoost(cam_pointer, IS_SET_GAINBOOST_OFF)))
	{
		close_usb_camera();
		return error(ERROR, "Failed to set Gain Boost on.");
	}	

	/* Set Hardware Gain to Maximum. */

	if (usb_camera_set_gain(100))
	{
		close_usb_camera();
		return error(ERROR, "Failed to set camera gain.");
	}

	/* Set Hardware Gain to Maximum. */

	if (set_pixelclock(43))
	{
		close_usb_camera();
		return error(ERROR, "Failed to set camera pixelclock.");
	}

	/* How many bits per pixel? */

	if ((i = is_SetColorMode(cam_pointer,  IS_CM_SENSOR_RAW8)))
	{
		close_usb_camera();
		return error(ERROR,"Failed to set color mode (%d).\n",i);
	}

	/* Setup an event for the frames */

	if ((i = is_EnableEvent(cam_pointer, IS_SET_EVENT_FRAME_RECEIVED)))
	{
		close_usb_camera();
		return error(ERROR,"Failed to enable the event (%d).\n", i);
	}
	
	/* Create  Mutex for this */

	if (pthread_mutex_init(&usb_camera_mutex, NULL) != 0)
		error(FATAL, "Unable to create USB camera mutex.");

	/* Create thread to handle USB data */

	if (pthread_create(&usb_camera_thread, NULL, do_usb_camera, NULL) != 0)
	{
		return error(FATAL, "Error creating TV tracking thread.");
	}

	/* Set Frames per Sec to 200 */

	if ( (ferr=set_frame_rate(200.0)) < 0.0 )
	{
		close_usb_camera();
		return error(ERROR,
			"Failed to set frame rate. Errror %3.2lf", ferr);
	}

	/* That should be it */

	return NOERROR;

} /* open_usb_camera() */
	
/************************************************************************/
/* close_usb_camera()							*/
/*									*/
/* Tries to close the conenction the USB camera.			*/
/* Returns error level.							*/
/************************************************************************/

int close_usb_camera(void)
{
	int	i,j;

	/* Stop the camera if it is running */

	message(system_window,"Stopping the USB Camera.");
	stop_usb_camera(0, NULL); 
	
	/* Stop the thread */

	message(system_window,"Waiting for USB Camera thread to terminate.");
	usb_camera_open = FALSE;

	if (pthread_join(usb_camera_thread,NULL) != 0)
	{
		return error(ERROR,
			"Error waiting for USB camera thread to stop.");
	}

	/* Remove the event */

	message(system_window,"Disabling USB Camera.");
	if ((i = is_DisableEvent(cam_pointer, IS_SET_EVENT_FRAME_RECEIVED)))
	{
		return error(ERROR,"Failed to disable the event (%d).\n", i);
	}
	
	/* Release image memory */

	message(system_window,"Releasing Camera Memory.");
	for (i=0;i<NUM_IMAGE_MEM;i++) free_matrix(data_frames[i], 
			1, cam_info.nMaxWidth, 
			1, cam_info.nMaxHeight);

	free_matrix(dark,1,cam_info.nMaxWidth, 1, cam_info.nMaxHeight);
	free_matrix(sum_frame,1,cam_info.nMaxWidth, 1, cam_info.nMaxHeight);
	free_matrix(this_frame,1,cam_info.nMaxWidth, 1, cam_info.nMaxHeight);

	for(j = 0; j< NUM_IMAGE_MEM; j++)
	{
	    if ((i = is_FreeImageMem(cam_pointer,
			image_memory[j], pid_image_memory[j])))
	    {
		return error(ERROR,"Failed to free image memory (%d).\n",i);
	    }
	}
	free(cam_list);

	/* Close camera conenction */

	message(system_window,"Closing USB Camera.");
	if (is_ExitCamera(cam_pointer))
	{
		return error(ERROR,"Failed to close camera connection.\n");
	}

	werase(system_window);
	wrefresh(system_window);

	return NOERROR;
}

/************************************************************************/
/* do_usb_camera()							*/
/*    									*/
/* Runs as a separrate thread and tries to keep up with the usb camera. */
/************************************************************************/

void *do_usb_camera(void *arg)
{
	unsigned char *p;
	float min_pixels[NUM_MIN];
	float row_min;
	int i,j,k,l;
	static float **data;

	while(usb_camera_open)
	{

		/* Is there anything there? */

		if (!usb_camera_open || !usb_camera_running ||
		     is_WaitEvent(cam_pointer, IS_SET_EVENT_FRAME_RECEIVED, 0))
		{
			usleep(1000);
			continue;
		}

		/* Lock the mutex */

		pthread_mutex_lock(&usb_camera_mutex);

		/* Get the current image */

		if (is_GetImageMem(cam_pointer, &usb_camera_image))
		{
			pthread_mutex_unlock(&usb_camera_mutex);
			usleep(1000);
			continue;
		}

                /*
                 * Transfer the image to the float array, 
                 * computing values for destriping
                 */

                for(j =0, p = usb_camera_image; j< rectAOI.s32Height; j++)
                {
                        for(k=0;k<NUM_MIN;k++) min_pixels[k]=255;
                        for(i=0; i< rectAOI.s32Width; i++)
                        {
                            if(*p < min_pixels[NUM_MIN-1])
                            {

                                /* 
                                 * Find the index to insert the 
                                 * darkpixel value
                                 */

                                l = 0;
                                while(min_pixels[l] < *p) l++;

                                /* Shift the current values along one */

                                for(k=NUM_MIN-1;k>l;k--)
                                        min_pixels[k] = min_pixels[k-1];

                                /* Insert the new min pixel */

                                min_pixels[k] = *p;
                            }
                            this_frame[i+1][j+1] = (float)*p++;
                        }
                        for(;i < cam_info.nMaxWidth; i++) p++;

                        /* If needed, try to destripe */

                        if (usb_camera.destripe)
                        {
                                row_min=0;
                                for (k=0; k<NUM_MIN; k++)
                                        row_min += (float)min_pixels[k];
                                row_min /= NUM_MIN;
                                for (i=1; i<=rectAOI.s32Width;i++)
                                        this_frame[i][j+1] -= row_min;
                        }
                }

		/* Sum this frame in? */

                for(j =1; j <= rectAOI.s32Height; j++)
                {
                    for(i=1; i <= rectAOI.s32Width; i++)
                    {
			sum_frame[i][j] += this_frame[i][j];
		    }
		}

		/* Is that it? */

		if (++count_sum_frame < num_sum_frame)
		{
			pthread_mutex_unlock(&usb_camera_mutex);
			usleep(1000);
			continue;
		}
		
                /* We got a frame! */

                usb_camera_num_frames++;
		count_sum_frame = 0;

                /* 
                 * Now that the data is transferred, increment the data pointer 
                 * NB this is inside the mutex.
                 */

                current_data_frame_ix++;
                current_data_frame_ix = current_data_frame_ix % NUM_IMAGE_MEM;
                data = data_frames[current_data_frame_ix];

                for(j =1; j <= rectAOI.s32Height; j++)
                {
                    for(i=1; i <= rectAOI.s32Width; i++)
                    {
			data[i][j] = sum_frame[i][j]/num_sum_frame - dark[i][j];
			sum_frame[i][j] = 0.0;
		    }
		}

		/* Save this to the data cube if we need to */

		if (image_data_cube_num_frames > 0 &&
	            image_data_cube_count_frames < image_data_cube_num_frames &&
		    image_data_cube_pointer != NULL)
		{
		    if (image_data_cube_count_frames == 0)
			data_record_start = chara_time_now();

		    for(j = 1; j <= rectAOI.s32Height; j++)
		    for(i = 1; i <= rectAOI.s32Width; i++)
			*image_data_cube_pointer++ = data[i][j];

		    if (++image_data_cube_count_frames == 
			image_data_cube_num_frames)
				data_record_stop = chara_time_now();
		}

		/* Is there a callback function? */

		if (usb_camera_callback != NULL)
		{
			(*usb_camera_callback)(
				chara_time_now(), data, 
				rectAOI.s32Width, rectAOI.s32Height);
		}

		/* Unlock this for the next one */

		is_UnlockSeqBuf(cam_pointer, IS_IGNORE_PARAMETER, 
			usb_camera_image);
		
		/* Unlock the mutex */

		pthread_mutex_unlock(&usb_camera_mutex);
	}

	return NULL;

} /* do_usb_camera() */

/************************************************************************/
/* set_usb_camera_callback()						*/
/*									*/
/* Sets up the callback. Send NULL if you want to turn off teh callback.*/
/************************************************************************/

void set_usb_camera_callback(void (*new_camera_callback)(CHARA_TIME time_stamp,
					float **data, 
					int aoi_width, int aoi_height))
{
	usb_camera_callback = new_camera_callback;

} /* set_usb_camera_callback() */

/************************************************************************/
/* start_usb_camera()							*/
/*									*/
/* Set the camera running.						*/
/* Returns error level.							*/
/************************************************************************/

int start_usb_camera(int argc, char **argv)
{
	int 	i;

	/* Are we already running? */

	if (usb_camera_running) return NOERROR;

	/* We will use an image queue */

	if ((i = is_InitImageQueue(cam_pointer, 0)))
	{
		close_usb_camera();
	  return error(ERROR, "Failed to enable image queue (%d).\n", i);
	}

	/* Start capturing video */

	if ((i = is_CaptureVideo(cam_pointer, IS_WAIT)))
	{
	     return error(ERROR,"Failed to capture video (%d).\n", i);
	}

	/* Wait for an even second */

	usb_camera_last_proc_sec = time(NULL);
	while (usb_camera_last_proc_sec == time(NULL));
	usb_camera_last_proc_sec = time(NULL);
	usb_camera_num_frames = 0;
	usb_camera_last_num_frames = 0;

	/* OK, we're running */

	usb_camera_running = TRUE;

	/* Tell the clients */

	send_labao_text_message("USB Camera Started");

	return NOERROR;

} /* start_usb_camera() */

/************************************************************************/
/* stop_usb_camera()							*/
/*									*/
/* Stop the camera running.						*/
/* Returns error level.							*/
/************************************************************************/

int stop_usb_camera(int argc, char **argv)
{
	int	i;

	/* Are we even running */

	if (!usb_camera_running) return NOERROR;

	stop_usb_camera_display(0, NULL);

	/* Stop Capturing video */

	if ((i = is_StopLiveVideo(cam_pointer, IS_WAIT)))
	{
	     return error(ERROR,"Failed to free image memory (%d).\n",i);
	}

	usb_camera_running = FALSE;

	/* Tell the clients */

	send_labao_text_message("USB Camera Stopped");

	return NOERROR;

} /* stop_usb_camera() */

/************************************************************************/
/* usb_camera_status()							*/
/*									*/
/* Can be called as a background process.				*/
/************************************************************************/

int usb_camera_status(void)
{
	time_t now, dt;
	/* Some stupid NRC arrays */
	static float	**values = NULL;
	static float    **data = NULL;
	/* And a fixed, fast array for socket display */
	static unsigned char data4bit[USB_DISP_X*USB_DISP_Y/2];
	unsigned char *cp;
	char	*picture;
	int	i, j, x, y;
	float	x_zero, y_zero;
	static int	last_num_frames = 0;
	fitsfile *fptr;
	int	fits_status;
	long int naxis, naxes[2];
	float	*data_cube, *fp, dm_data[NUM_ACTUATORS];
	int	bitpix;
	long int fpixel, nelements;
	struct smessage usbmess;
	struct smessage cenmess;
	struct smessage imagemess;
	float  *cen_data, *compressed_cen_data;
	float  *image, *compressed_image;
	uLongf	len, clen;

	pthread_mutex_lock(&usb_camera_mutex);

	wstandout(status_window);
	mvwaddstr(status_window,3,0,"Frms/Sec: ");
	wstandend(status_window);

	if (usb_camera_running)
	{
	   if (is_GetFramesPerSecond(cam_pointer, &usb_camera.real_fps))
	 return ERROR;
	    wprintw(status_window,"%9.2f", usb_camera.real_fps);
	}
	else
	{
	   wprintw(status_window,"%9s", "Cam Off");
	}

	wstandout(status_window);
	mvwaddstr(status_window,4,0,"Proc/Sec: ");
	wstandend(status_window);

	if (usb_camera_running)
	{
	    now = time(NULL);
	    if (now - usb_camera_last_proc_sec >= SECS_FOR_PROC)
	    {
		wprintw(status_window,"%9.2f",
			usb_camera.proc_fps = (float)(usb_camera_num_frames - usb_camera_last_num_frames) /
			 (float)(now - usb_camera_last_proc_sec));
		usb_camera_last_num_frames = usb_camera_num_frames;
		usb_camera_last_proc_sec = now;	
	    }
	    else if (usb_camera_last_proc_sec == 0)
	    {
		wprintw(status_window,"%9.2f", 0.0);
	    }
	}
	else
	{
	   wprintw(status_window,"%9s", "Cam Off");
	}

	/* Go get the current image */

	data = matrix(1, rectAOI.s32Width, 1, rectAOI.s32Height);
	usb_camera.max = -1e32;
	usb_camera.min = 1e32;
	for (j=1; j<= rectAOI.s32Height; j++)
	for (i=1; i<= rectAOI.s32Width; i++)
	{
		data[i][j] = data_frames[current_data_frame_ix][i][j];
		if (data[i][j] > usb_camera.max)
			usb_camera.max = data[i][j];
		else if (data[i][j] < usb_camera.min) 
			usb_camera.min = data[i][j];
	}

	/* We are done with the dangerous stuff, unlock everything */

	pthread_mutex_unlock(&usb_camera_mutex);

	/* What size is the image? */

	wstandout(status_window);
	     mvwaddstr(status_window,6,0,"X/dX    : ");
	     wstandend(status_window);
	wprintw(status_window,"%4d/%4d", rectAOI.s32X, rectAOI.s32Width);

	wstandout(status_window);
	     mvwaddstr(status_window,7,0,"Y/dY    : ");
	     wstandend(status_window);
	wprintw(status_window,"%4d/%4d", rectAOI.s32Y, rectAOI.s32Height);

	wstandout(status_window);
        mvwaddstr(status_window,8,0,"Min/Max : ");
        wstandend(status_window);
	if (usb_camera_running)
		wprintw(status_window,"%4.0f/%4.0f", 
			usb_camera.min, usb_camera.max);
	else
		wprintw(status_window,"%4.0f/%4.0f", 0.0, 0.0);

	wstandout(status_window);
        mvwaddstr(status_window,0,20,"Destripe: ");
        wstandend(status_window);
	if (usb_camera.destripe)
		wprintw(status_window,"%9s", "ON");
	else
		wprintw(status_window,"%9s", "OFF");

	wstandout(status_window);
        mvwaddstr(status_window,0,20,"DispBox : ");
        wstandend(status_window);
	if (usb_camera.overlay_boxes)
		wprintw(status_window,"%9s", "ON");
	else
		wprintw(status_window,"%9s", "OFF");

	/* Now we do the display if we need to */

	if (usb_camera_num_frames != last_num_frames &&
		usb_camera_display && usb_camera_running)
	{
		last_num_frames = usb_camera_num_frames;

		/* We need an array to put them in */

		values = matrix(1, USB_DISP_X, 1, USB_DISP_Y);

		/* How do we translate from the image to our array? */

		if (USB_DISP_X == rectAOI.s32Width)
		{
			usb_camera.x_mult = 1.0;
			x_zero = 0.0;
		}
		else
		{
			usb_camera.x_mult = 
				(float)(rectAOI.s32Width)/(float)(USB_DISP_X);
			x_zero = 1.0 - usb_camera.x_mult * 1.0;
		}

		if (USB_DISP_Y == rectAOI.s32Height)
		{
			usb_camera.y_mult = 1.0;
			y_zero = 0.0;
		}
		else
		{
			usb_camera.y_mult = 
				(float)(rectAOI.s32Height)/(float)(USB_DISP_Y);
			y_zero = 1.0 - usb_camera.y_mult * 1.0;
		}

		/* Now make a smaller version of the image */

		for(i = 1; i <= USB_DISP_X; i++)
		for(j = 1; j <= USB_DISP_Y; j++)
		{
			x = (int)(usb_camera.x_mult * i + x_zero + 0.5);
			if (x < 1)
				x = 1;
			else if (x > rectAOI.s32Width)
				x = rectAOI.s32Width;

			y = (int)(usb_camera.y_mult * j + y_zero + 0.5);
			if (y < 1)
				y = 1;
			else if (y > rectAOI.s32Height)
				y = rectAOI.s32Height;

			values[i][j] = data[x][y];
		}

		/* Send this to any clients that want it */

		process_message_socket_all_clients(TRUE);
                if (client_signal_set(LABAO_SIGNAL_READY_FOR_DISPLAY))
                {
		  for (cp=data4bit,i=1;i<USB_DISP_X;i++)
		    for (j=1;j<USB_DISP_Y;j+=2)
		      {
			*cp++ = (unsigned char)((values[i][j] - usb_camera.min)/(usb_camera.max - usb_camera.min)*16) + 
			  16*(unsigned char)((values[i][j+1] - usb_camera.min)/(usb_camera.max - usb_camera.min)*16);
		      }
			usb_camera.num_lenslets = NUM_LENSLETS;
			usb_camera.usb_disp_x = USB_DISP_X;
			usb_camera.usb_disp_y = USB_DISP_Y;
			usbmess.type = LABAO_SET_USB_CAMERA;
			usbmess.length = sizeof(usb_camera);
			usbmess.data = (unsigned char *)&usb_camera;

			len = 5 * NUM_LENSLETS * sizeof(float);
			clen = 4*len;

		        if ((cen_data = (float *)malloc(len)) == NULL)
				error(FATAL, "Out of memory");
		        if ((compressed_cen_data = (float *)malloc(clen))==NULL)
				error(FATAL, "Out of memory");

			for(fp = cen_data, i=0; i<NUM_LENSLETS; i++)
				*fp++ = x_centroid_offsets[i];
			for(i=0; i<NUM_LENSLETS; i++)
				*fp++ = y_centroid_offsets[i];
			for(i=0; i<NUM_LENSLETS; i++)
				*fp++ = xc[i];
			for(i=0; i<NUM_LENSLETS; i++)
				*fp++ = yc[i];
			for(i=0; i<NUM_LENSLETS; i++)
				*fp++ = avg_fluxes[i];

			if ((i = compress((unsigned char *)compressed_cen_data,
                                &clen,
                                (unsigned char *)cen_data, len)) < Z_OK)
                        {
                             return error(ERROR,
				"Failed to compress centroids. %d",i);
                        }

			cenmess.type = LABAO_SEND_CENTROIDS;
			cenmess.length = (int)clen;
			cenmess.data = (unsigned char *)compressed_cen_data;

			/* Theo - this was silly. An image can really only be seen with 
			 a perceived bit depth of 4 or so.*/
			// len = USB_DISP_X * USB_DISP_Y * sizeof(float);
			// clen = 4*len;
			len = USB_DISP_X * USB_DISP_Y /2;
			clen = len;

		        if ((image = (float *)malloc(len)) == NULL)
				error(FATAL, "Out of memory");
		        if ((compressed_image = (float *)malloc(clen))==NULL)
				error(FATAL, "Out of memory");
			/*
			for(fp = image, i = 1; i <= USB_DISP_X; i++)
			for(j = 1; j <= USB_DISP_Y; j++)
				*fp++ = values[i][j];
				*/

			if ((i = compress((unsigned char *)compressed_image,
                                &clen,
					  (unsigned char *)data4bit, len)) < Z_OK)
					  //        (unsigned char *)image, len)) < Z_OK)
                        {
                             return error(ERROR,
				"Failed to compress image. %d",i);
                        }
			/* !!! For bugshooting !!! */
			//fprintf(stderr, "%d\n", clen);
			imagemess.type = LABAO_USB_IMAGE;
			imagemess.length = (int)clen;
			imagemess.data = (unsigned char *)compressed_image;

			for(i=0; i<MAX_CONNECTIONS; i++)
                        {
                                if (client_signal_set_single_client(i,
                                        LABAO_SIGNAL_READY_FOR_DISPLAY))
                                {
                                        ui_send_message_by_index(i, &usbmess);
                                        ui_send_message_by_index(i, &cenmess);
                                        ui_send_message_by_index(i, &imagemess);
				}
			}
			clear_client_signal(LABAO_SIGNAL_READY_FOR_DISPLAY);
			free(cen_data);
			free(compressed_cen_data);
			free(image);
			free(compressed_image);
                }

		if (usb_camera_local_display)
		{
			if (usb_camera_ximage != NULL)
				XDestroyImage(usb_camera_ximage);
			picture = make_picture(USB_DISP_X, USB_DISP_Y, 
				1,values,LIN);

			/*
			 * Overlay the boxes for the centroid targets,
 			 * and the current centroids
 			 */
			if (usb_camera.overlay_boxes)
			{
			    for (i=0;i<NUM_LENSLETS;i++)
			    {
				overlay_rectangle(USB_DISP_X, USB_DISP_Y, 
				    picture,
				    (x_centroid_offsets[i] - 
				    CENTROID_BOX_WIDTH/2-1)/usb_camera.x_mult,
				    USB_DISP_Y-(y_centroid_offsets[i] +
				    CENTROID_BOX_WIDTH/2-1)/usb_camera.y_mult,
				    (x_centroid_offsets[i] + 
				    CENTROID_BOX_WIDTH/2-1)/usb_camera.x_mult,
				    USB_DISP_Y-(y_centroid_offsets[i] - 
				    CENTROID_BOX_WIDTH/2-1)/usb_camera.y_mult,
				    1,0,65535,0);

				overlay_rectangle(USB_DISP_X, USB_DISP_Y,
				    picture,
				    (x_centroid_offsets[i] + 
				    xc[i] - 1-1)/usb_camera.x_mult,
				    USB_DISP_Y-(y_centroid_offsets[i] +
				    yc[i] + 1-1)/usb_camera.y_mult,
				    (x_centroid_offsets[i] + xc[i] + 1-1)/
				    usb_camera.x_mult,
				    USB_DISP_Y-(y_centroid_offsets[i] +
				    yc[i] - 1-1)/usb_camera.y_mult,
				    1,65535,0,0);
			    }	
			}
			usb_camera_ximage = 
				XCreateImage(theDisplay,theVisual,theDepth,
				ZPixmap,0, picture, USB_DISP_X, USB_DISP_Y,
				theBitmapPad,0);
			XPutImage(theDisplay,usb_camera_window,theGC,
				usb_camera_ximage, 0, 0, 0, 0,
				USB_DISP_X, USB_DISP_Y);
			XMapWindow(theDisplay, usb_camera_window);
			XFlush(theDisplay);
		}

		/* Are we saving a fits file? */

		if (save_fits_file)
		{
			/* Build the data image */

			data_cube = (float *)calloc((size_t)rectAOI.s32Height *
				(size_t)rectAOI.s32Width, sizeof(float));
			if (data_cube == NULL) error(FATAL,"Not enough memory");

			for(fp = data_cube, j = 1; j <= rectAOI.s32Height; j++)
			for(i = 1; i <= rectAOI.s32Width; i++)
				*fp++ = data[i][j];

			/* Build the extension image (DM values) */
			for(i=1; i<= NUM_ACTUATORS; i++)
			  dm_data[i-1] = edac40_current_value(i);

			/* Setup for saving the fits file */

			naxis = 2;
			bitpix = FLOAT_IMG;
			naxes[0] = rectAOI.s32Width;
			naxes[1] = rectAOI.s32Height;
			fpixel = 1;
			nelements = naxes[0] * naxes[1];

			/* Create a new FITS file */

			fits_status = 0;
			if (fits_create_file(&fptr,fits_filename,&fits_status))
			{
				error(ERROR,"Failed to create FITS file (%d).",
					fits_status);
			}

			/* Write required keywords into the header */

			if (fits_create_img(fptr, bitpix, naxis, naxes, 
				&fits_status))
			{
				error(ERROR,"Failed to create FITS image (%d).",
				     fits_status);
			}

			/* Write the FITS image */

			if (fits_write_img(fptr, TFLOAT, fpixel, nelements, 
				data_cube, &fits_status))
			{
				error(ERROR,"Failed to write FITS image (%d).",
					fits_status);
			}
			
			/* Write the extension */
			bitpix = FLOAT_IMG;
			naxes[0] = NUM_ACTUATORS;
			naxis=1;
			fpixel = 1;
			nelements = naxes[0];
			if (fits_create_img(fptr, bitpix, naxis, naxes, 
				&fits_status))
			{
				error(ERROR,"Failed to create FITS extension (%d).",
				     fits_status);
			}
			if (fits_write_img(fptr, TFLOAT, fpixel, nelements, 
				dm_data, &fits_status))
			{
				error(ERROR,"Failed to write FITS extension (%d).",
					fits_status);
			}
		       
			

			/* Now some header infromation */

			if(fits_update_key(fptr, TSTRING, "LABAO",
				labao_name, "LABAO Unit name",&fits_status))
			{
				error(ERROR,"Failed to write LABAO (%d).",
					fits_status);
			}

			if(fits_update_key(fptr, TINT, "SENSORID", 
				&(cam_info.SensorID), "Sensor ID",&fits_status))
			{
				error(ERROR,"Failed to write SENSORID (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "MAXWIDTH", 
				&(cam_info.nMaxWidth),
				"Sensor Maximum width",&fits_status))
			{
				error(ERROR,"Failed to write MAXWIDTH (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "MAXHGHT", 
				&(cam_info.nMaxHeight),
				"Sensor Maximum height",&fits_status))
			{
				error(ERROR,"Failed to write MAXHGHT (%d).",
			     	fits_status);
			}

			if(fits_update_key(fptr, TINT, "AOI_X", 
				&(rectAOI.s32X),
				"AOI Positionin X",&fits_status))
			{
				error(ERROR,"Failed to write AOI_X (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "AOI_Y", 
				&(rectAOI.s32Y),
				"AOI Positionin Y",&fits_status))
			{
				error(ERROR,"Failed to write AOI_Y (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "AOI_DX", 
				&(rectAOI.s32Width),
				"AOI Positionin DX",&fits_status))
			{
				error(ERROR,"Failed to write AOI_DX (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "AOI_DY", 
				&(rectAOI.s32Height),
				"AOI Positionin DY",&fits_status))
			{
				error(ERROR,"Failed to write AOI_DY (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TFLOAT, "CAM_FPS",
				&usb_camera.fps,
				"Frames per second.",&fits_status))
			{
				error(ERROR,"Failed to write CAM_FPS (%d).",
					fits_status);
			}

			if(fits_update_key(fptr, TINT, "CAM_GAIN", 
				&usb_camera.gain, "Camera Gain",&fits_status))
			{
				error(ERROR,"Failed to write CAM_GAIN (%d).",
				fits_status);
			}

			if(fits_update_key(fptr, TINT, "CAM_CLOCK", 
				&usb_camera.pixelclock,
				"Camera pixel cloxk",&fits_status))
			{
				error(ERROR,"Failed to write CAM_CLOCK (%d).",
				fits_status);
			}

			/* That should be enough! */

			if (fits_close_file(fptr, &fits_status))
			{
				error(ERROR,"Failed to close fits file (%d).",
					fits_status);
			}

			/* That is all */

			save_fits_file = FALSE;
			free(data_cube);
		}

		free_matrix(values, 1, USB_DISP_X, 1, USB_DISP_Y);
		usb_camera_display_frames++;
	}

	/* Welease Woger */

	free_matrix(data, 1, rectAOI.s32Width, 1, rectAOI.s32Height);

	dt = time(NULL) - usb_camera_display_start;
	wstandout(status_window);
	mvwaddstr(status_window,5,0,"Disp/Sec: ");
	wstandend(status_window);

	if (usb_camera_local_display)
	{

	    if (dt > 0)
	    {
		wprintw(status_window,"%9.2f",
			(float)usb_camera_display_frames/(float)dt);
	    }
	    else
	    {
		wprintw(status_window,"%9.2f", 0.0);
	    }
	}
	else
	{
	   wprintw(status_window,"%9s", "Local Off");
	}

	return NOERROR;
}

/************************************************************************/
/* start_usb_camera_display()						*/
/*									*/
/* Start showing the images from teh USB camera.			*/
/************************************************************************/

int start_usb_camera_display(int argc, char **argv)
{
	if (!usb_camera_open || !usb_camera_running)
		return error(ERROR,"USB camera is not running.");

	if (usb_camera_display) return NOERROR;

	/* 
	 * We should only do a local display if we are being run directly
	 * from the server itself. If we are being run from a client, the
	 * client will handle the display.
	 */

	usb_camera_local_display = (client_socket == -1);

	if (usb_camera_local_display)
	{
		/* Create the window */

		usb_camera_window = openWindow(labao_name,10,10, 
			USB_DISP_X, USB_DISP_Y);

		/* Wait for an even second */

		usb_camera_display_start = time(NULL);
		while (usb_camera_display_start == time(NULL));
		usb_camera_display_start = time(NULL);
		usb_camera_display_frames = 0;
	}

	/* Not much else to do */

	usb_camera_display = TRUE;

	/* Tell the clients */

	send_labao_text_message("USB Camera Display Started");

	return NOERROR;

} /* start_usb_camera_display() */

/************************************************************************/
/* stop_usb_camera_display()						*/
/*									*/
/* Start showing the images from teh USB camera.			*/
/************************************************************************/

int stop_usb_camera_display(int argc, char **argv)
{
	struct smessage mess;

	if (!usb_camera_open || !usb_camera_running || !usb_camera_display)			return NOERROR;

	/* Remove the window */

	if (usb_camera_local_display)
	{
		XDestroyWindow(theDisplay, usb_camera_window);
		XFlush(theDisplay);
	}

	/* Not much else to do */

	usb_camera_display = FALSE;

	/* Tell the clients */

	mess.type = LABAO_STOP_USB_IMAGE;
	mess.length = 0;
	mess.data = (unsigned char *)NULL;
	ui_send_message_all(&mess);

	/* Tell the clients */

	send_labao_text_message("USB Camera Display Stopped");

	return NOERROR;

} /* stop_usb_camera_display() */

/************************************************************************/
/* set_usb_camera_aoi()							*/
/*									*/
/* Set the USB camera area of interest.					*/
/* Returns								*/
/* 0 - All good								*/
/* -1 - Camera is running or no open.					*/
/* -2 - AOI out of bounds.						*/
/* -3 - X Position is not a multiple of what we need.			*/
/* -4 - Y Position is not a multiple of what we need.			*/
/* -5 - X Size is not a multiple of what we need.			*/
/* -6 - Y Size is not a multiple of what we need.			*/
/* -7 - Failed to set AOI.						*/
/************************************************************************/

int set_usb_camera_aoi(bool move, int x, int y, int width, int height)
{
	IS_RECT 	newAOI;
	int		dx, dy;

	/* This seems unecessary now. */

	/* if (!usb_camera_open || usb_camera_running) return -1; */

	if (x >= cam_info.nMaxWidth || x < 0 || 
		 y >= cam_info.nMaxHeight || y < 0 ||
		 x + width > cam_info.nMaxWidth ||
		 y + height > cam_info.nMaxHeight) return -2;

	if (x % pos_inc.s32X) return -3;
	if (y % pos_inc.s32Y) return -4;
	if (width % size_inc.s32X) return -5;
	if (height % size_inc.s32Y) return -6;
	if (width < size_min.s32X || height < size_min.s32Y) return -7;

	usb_camera.x = newAOI.s32X = x;
	usb_camera.dx = newAOI.s32Y = y;
	usb_camera.y = newAOI.s32Width = width;
	usb_camera.dy = newAOI.s32Height = height;

	if (is_AOI(cam_pointer, IS_AOI_IMAGE_SET_AOI, &newAOI, sizeof(newAOI)))
	{
		return -8;
	}
	
	/* Move the centroids if we need to - not the first time */

        if (move&& (newAOI.s32X != rectAOI.s32X || newAOI.s32Y != rectAOI.s32Y))
        {       
                dx = rectAOI.s32X - newAOI.s32X;
                dy = rectAOI.s32Y - newAOI.s32Y;
                
                move_centroids(dx, dy);
        }       

	rectAOI = newAOI;

	return 0;

} /* set_usb_camera_aoi() */

/************************************************************************/
/* get_usb_camera_aoi()							*/
/*									*/
/* Get the USB camera area of interest 					*/
/************************************************************************/

void get_usb_camera_aoi(int *x, int *y, int *width, int *height)
{
	*x = rectAOI.s32X;
	*y = rectAOI.s32Y;
	*width = rectAOI.s32Width;
	*height = rectAOI.s32Height;

} /* get_usb_camera_aoi() */

/************************************************************************/
/* call_set_usb_camera_aoi()						*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_set_usb_camera_aoi(int argc, char **argv)
{
	char	s[100];
	int x, y, width, height;

	/* We ignore these checks to make life easier 
	if (!usb_camera_open) return error(ERROR,
		"Can not change AOI unless camera is open.");
		
	if (usb_camera_running) return error(ERROR,
		"Can not change AOI while camera is running.");
	*/

	/* Check out the command line */

	if (argc > 1)
	{
		sscanf(argv[1],"%d",&x);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", rectAOI.s32X);
		if (quick_edit("X position",s,s,NULL,INTEGER)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%d",&x);
	}

	if (argc > 2)
	{
		sscanf(argv[2],"%d",&y);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", rectAOI.s32Y);
		if (quick_edit("Y position",s,s,NULL,INTEGER)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%d",&y);
	}

	if (argc > 3)
	{
		sscanf(argv[3],"%d",&width);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", rectAOI.s32Width);
		if (quick_edit("Width",s,s,NULL,INTEGER)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%d",&width);
	}

	if (argc > 4)
	{
		sscanf(argv[4],"%d",&height);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", rectAOI.s32Height);
		if (quick_edit("Height",s,s,NULL,INTEGER)
		     == KEY_ESC) return NOERROR;
		sscanf(s,"%d",&height);
	}

	switch(set_usb_camera_aoi(TRUE, x, y, width, height))
	{
		case 0: return NOERROR;

		case -2: return error(ERROR, "AOI is out of bounds.");

		case -3: return error(ERROR, 
			"X position must be a multiple of %d.", pos_inc.s32X);

		case -4: return error(ERROR, 
			"Y position must be a multiple of %d.", pos_inc.s32Y);

		case -5: return error(ERROR, 
			"Width must be a multiple of %d.", size_inc.s32X);

		case -6: return error(ERROR, 
			"Height must be a multiple of %d.", size_inc.s32Y);

		case -7: return error(ERROR, 
			"Size must be at least %dx%d.", 
			size_min.s32X, size_min.s32Y);

		case -8: return error(ERROR, "Failed to set AOI.");

		case -1:
		default: return error(ERROR,"Odd... we should never get here.");

	}

} /* call_set_usb_cammera_aoi() */

/************************************************************************/
/* set_optimal_camera_timing()		    				*/
/*									*/
/* User callable function to get the max pixel clock rate and the 	*/
/* maximum frame rate.  						*/
/************************************************************************/

int set_optimal_camera_timing(int argc, char **argv)
{
	int pMaxPxlClk, errcode;
	double pMaxFrameRate;

	if (!usb_camera_open) return error(ERROR,
		"Can not set camera timing unless camera is open.");
		
	if (!usb_camera_running) return error(ERROR,
		"Camera must be running to find optimal timing.");

	/* Run the function */

	if ((errcode=is_SetOptimalCameraTiming(cam_pointer, 
		IS_BEST_PCLK_RUN_ONCE, 10000, &pMaxPxlClk, &pMaxFrameRate)))
	{
		switch(errcode)
		{
			case IS_AUTO_EXPOSURE_RUNNING:
				return error(ERROR,
					"Someone turned on AUTO exposure!");

			case IS_NOT_SUPPORTED:
				return error(ERROR,
		"No fun - this camera doesn't support finding optimal timing.");

			default:
				return error(ERROR, "Unknown Error.");
		}
	}

	/* Output the results */

	message(system_window, "Max Clock: %d. Max Frame Rate: %6.1lf.",
		pMaxPxlClk, pMaxFrameRate);

	return NOERROR;

} /* set_optimal_camera_timing() */

/************************************************************************/
/* set_frame_rate()	    		    				*/
/*									*/
/* Positive Number - Actual frame rate set				*/
/* -1 : Invalid Frame Rate.   						*/
/* -2 : Another error setting frame rate   				*/
/* -3 : An error setting exposure time    				*/
/************************************************************************/

double set_frame_rate(double fps)
{
	double newFPS, dummy_exposure=0.0;
	int errcode;

	if ((errcode = is_SetFrameRate(cam_pointer, fps, &newFPS)) )
	{
		if (errcode == IS_INVALID_PARAMETER)
			return -1.0; 
		else 
			return -2.0;
	}

	/* As recommended in the manual, now set the exposure. */

	if ( (errcode = is_Exposure(cam_pointer, IS_EXPOSURE_CMD_SET_EXPOSURE, 
		&dummy_exposure, sizeof(dummy_exposure))) )
		return -3.0;

	usb_camera.fps = newFPS;
	send_labao_set_usb_camera(TRUE);

	return (newFPS);

} /* set_frame_rate() */

/************************************************************************/
/* call_set_frame_rate()				    		*/
/*									*/
/* User Callable version of set_frame_rate  				*/
/************************************************************************/

int call_set_frame_rate(int argc, char **argv)
{
	char s[100];
	double input_fps, newFPS;
	if (argc > 1)
	{
		sscanf(argv[1],"%lf",&input_fps);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %6.2lf", input_fps);
		if (quick_edit("Frames Per Sec",s,s,NULL,FLOAT)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%lf",&input_fps);
	}

	if ((newFPS = set_frame_rate(input_fps)) < 0.0)
	{
		if (newFPS == -1.0)
			return error(ERROR, "Invalid Frame Rate");
		else return error(ERROR, "Unknown error setting Frame Rate");
	}

	message(system_window, "Frames per sec now: %6.1lf", newFPS);

	return NOERROR;
}

/************************************************************************/
/* set_pixelclock()	   		 		    		*/
/*									*/
/* Set the pixel clock rate in MHz 					*/
/* Error -1: Invalid value. 						*/
/* Error -2: Another error.						*/
/************************************************************************/

int set_pixelclock(unsigned int new_pixelclock)
{
	int errcode;
	double dummy_exposure=0.0;
	
	if ( (errcode = is_SetPixelClock(cam_pointer, new_pixelclock)) ){
		if (IS_INVALID_PARAMETER) return -1;
		else return -2;
	}

	/* As recommended in the manual, now set the exposure. */

  	 if ( (errcode = is_Exposure(cam_pointer, IS_EXPOSURE_CMD_SET_EXPOSURE, 
		&dummy_exposure, sizeof(dummy_exposure))) )
			return -3;

	usb_camera.pixelclock = new_pixelclock;
	send_labao_set_usb_camera(TRUE);

	return 0;

} /* set_pixelclock() */

/************************************************************************/
/* call_set_pixelclock()	 		    	 		*/
/*									*/
/* User callable version of set_pixelclock.				*/
/************************************************************************/

int call_set_pixelclock(int argc, char **argv)
{
	int errcode;
	int input_pixelclock, min_clock, max_clock;
	char instructions[80], s[80];

	/* Fist, get the range of supported pixel clocks */

	if ( (errcode = is_GetPixelClockRange(cam_pointer, 
		&min_clock, &max_clock)) )
		return error(ERROR, "Could not get Pixelclock range");

	if (argc > 1)
	{
		sscanf(argv[1],"%u",&input_pixelclock);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", usb_camera.pixelclock);
		sprintf(instructions, "Pixel Clock (Mhz, %u to %u)", 
			min_clock, max_clock);
		if (quick_edit(instructions,s,s,NULL,INTEGER)
			== KEY_ESC) return NOERROR;
		sscanf(s,"%u",&input_pixelclock);
	}

	if (input_pixelclock < min_clock || input_pixelclock > max_clock)
		return error(ERROR, "Pixelclock out of range!");

	if (set_pixelclock(input_pixelclock))
		return error(ERROR, "Error setting pixel clock");

	return NOERROR;

} /* call_set_pixelclock() */

/************************************************************************/
/* set_gain()	  			  		     		*/
/*									*/
/* Set the CMOS chip gain, between 0 and 100.				*/
/************************************************************************/

int usb_camera_set_gain(int new_gain)
{
	int errcode;
	
	if ( (errcode = is_SetHardwareGain(cam_pointer, new_gain, 
		IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER,IS_IGNORE_PARAMETER)) )
		return -1;

	usb_camera.gain = new_gain;
	send_labao_set_usb_camera(TRUE);

	return 0;	

} /* usb_camera_set_gain() */

/************************************************************************/
/* call_usb_camera_set_gain() 					     	*/
/*									*/
/* User Callable: Set the CMOS chip gain.				*/
/************************************************************************/

int call_usb_camera_set_gain(int argc, char **argv)
{
	char s[80];
	unsigned int input_gain;

	if (argc > 1)
	{
		sscanf(argv[1],"%u",&input_gain);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %d", usb_camera.gain);
		if (quick_edit("Gain (0 to 100)",s,s,NULL,INTEGER)
			== KEY_ESC) return NOERROR;
		sscanf(s,"%d",&input_gain);
	}
	if (usb_camera_set_gain(input_gain))
		return error(ERROR, "Failed to set gain!");

  	return NOERROR;

} /* call_usb_camera_set_gain() */

/************************************************************************/
/* toggle_destripe() 		 		    			*/
/*									*/
/* User Callable: toggle destriping the camera.				*/
/************************************************************************/

int toggle_destripe(int argc, char **argv)
{
	if (usb_camera.destripe)
	{
		usb_camera.destripe=FALSE;
		message(system_window, "NOT destriping the raw images.");
		send_labao_text_message("NOT destriping the raw images.");

	}
	else
	{
		usb_camera.destripe=TRUE;
		message(system_window, "Destriping the raw images.");
		send_labao_text_message("Destriping the raw images.");
	}

	send_labao_set_usb_camera(TRUE);
	return NOERROR;

} /* toggle_destripe() */

/************************************************************************/
/* create_dark() 		  		    			*/
/*									*/
/* User Callable: toggle destriping the camera.				*/
/************************************************************************/

int create_dark(void)
{
	float **dark_increment;
	int i,j,k;

	dark_increment = matrix(1,cam_info.nMaxWidth, 1, cam_info.nMaxHeight);

	for (i=1;i<=cam_info.nMaxWidth;i++)
	for (j=1;j<=cam_info.nMaxHeight;j++) dark_increment[i][j]=0.0;

	/* Lock the mutex - we don't want half a frame here. */

	pthread_mutex_lock(&usb_camera_mutex);

	/* Average the last frames */

	for (k=0;k<NUM_IMAGE_MEM;k++)
	{
		for (i=1;i<=cam_info.nMaxWidth;i++)
		for (j=1;j<=cam_info.nMaxHeight;j++)
			dark_increment[i][j] += data_frames[k][i][j];
	}

	/* Now add this to the dark, which was previously subtracted. */

	for (i=1;i<=cam_info.nMaxWidth;i++)
	for (j=1;j<=cam_info.nMaxHeight;j++)
		dark[i][j] += dark_increment[i][j]/NUM_IMAGE_MEM;

	/* All done! */

	pthread_mutex_unlock(&usb_camera_mutex);
	free_matrix(dark_increment,1,cam_info.nMaxWidth,1,cam_info.nMaxHeight);

	send_labao_text_message("Dark Created.");

	return NOERROR;

} /* create_dark() */

/************************************************************************/
/* call_create_dark() 		 		    			*/
/*									*/
/* User Callable: create a dark frame.    				*/
/************************************************************************/

int call_create_dark(int argc, char **argv)
{
	create_dark();

	return NOERROR;

} /* call_create_dark() */

/************************************************************************/
/* zero_dark() 			  		    			*/
/*									*/
/************************************************************************/

int zero_dark(void)
{
	int	i,j;

	pthread_mutex_lock(&usb_camera_mutex);

	for (i=1;i<=cam_info.nMaxWidth;i++)
	for (j=1;j<=cam_info.nMaxHeight;j++) dark[i][j] = 0.0;

	pthread_mutex_unlock(&usb_camera_mutex);

	send_labao_text_message("Dark Zeroed.");

	return NOERROR;

} /* zero_dark() */

/************************************************************************/
/* call_zero_dark() 		 		    			*/
/*									*/
/* User Callable: zero a dark frame.    				*/
/************************************************************************/

int call_zero_dark(int argc, char **argv)
{
	zero_dark();

	return NOERROR;

} /* call_zero_dark() */

/************************************************************************/
/* save_fits()								*/
/*									*/
/* Save an image in a fits file.					*/
/************************************************************************/

int save_fits(int argc, char **argv)
{
	char	filename[256], filename_base[256], s[256];
	     int     year, month, day, doy;
	int	file_number = 0;
	FILE	*output;

	/* Is there an argument for the filename? */

	if (argc > 1)
	{
		if (strlen(argv[1]) == 0)
			return error(ERROR,"No valid filename.");

		strcpy(fits_filename, argv[1]);

	}
	else
	{
		/* We need to build the filename */

		/* Get current GMT */

		get_ut_date(&year, &month, &day, &doy);

		/* Now we try and create a new filename */

		sprintf(filename_base,"%s%4d_%02d_%02d_%s_",
			get_data_directory(s), year,month,day,labao_name);

		for (file_number=1;file_number<=MAX_FILE_NUMBER;file_number++)
		{
			/* Create the filename */

			sprintf(filename,"%s%03d.fit",
				filename_base,file_number); 

			/* Does it already exist ? */

			if ((output = fopen(filename,"r")) == NULL) break;

			fclose(output);
		}

		if (file_number >= MAX_FILE_NUMBER)
		{
			return error(ERROR,
			"Exceeded maximum number of files with this name.");
		}

		strcpy(fits_filename, filename);
	}

	message(system_window,"Saving image data in %s.", fits_filename);

	save_fits_file = TRUE;

	return NOERROR;

} /* save_fits() */

/************************************************************************/
/* save_fits_cube()							*/
/*									*/
/* Save multiple images in a fits file.					*/
/************************************************************************/

int save_fits_cube(int argc, char **argv)
{
	int 	n;
	char	s[567];

	if (!usb_camera_running) return error(ERROR,"Camera is not running.");

	/* Deal with the command lines */

	if (argc > 1)
	{
		sscanf(argv[1], "%d", &n);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", 100);
		if (quick_edit("Number of frames",s,s,NULL,INTEGER)
			== KEY_ESC) return NOERROR;
		sscanf(s,"%d",&n);
	}

	if (n < 1) return error(ERROR,"Must have at least 1 frame.");

	/* We need to lock the mutex */

	pthread_mutex_lock(&usb_camera_mutex);

	/* Are we already doing this? */

	if (image_data_cube_num_frames > 0 &&
		image_data_cube_count_frames < image_data_cube_num_frames)
	{
		pthread_mutex_unlock(&usb_camera_mutex);
		return error(ERROR,"We seem to already be saving data.");
	}

	/* Allocate the memory we will need */

	image_data_cube = (float *)calloc((size_t)rectAOI.s32Height *
				(size_t)rectAOI.s32Width *(size_t)n,
				sizeof(float));

	if (image_data_cube == NULL)
	{
		pthread_mutex_unlock(&usb_camera_mutex);
		return error(ERROR,"Not enough memory - try fewer frames.");
	}

	/* OK, set things up so the frames will be saved */

	image_data_cube_pointer = image_data_cube;
	image_data_cube_num_frames = n;
	image_data_cube_count_frames = 0;

	/* OK, unlock things */

	pthread_mutex_unlock(&usb_camera_mutex);

	/* That should be all */

	message(system_window,  "Trying to save %d frames of data.", n);
	send_labao_text_message("Trying to save %d frames of data.", n);
	return NOERROR;

} /* save_fits_cube() */

/************************************************************************/
/* complete_fits_cube()							*/
/*									*/
/* Wait and see if it is time to save a fits cube.			*/
/************************************************************************/

void complete_fits_cube(void)
{
	char	filename[256], filename_base[256], s[256];
	int     year, month, day, doy;
	int	file_number = 0;
	FILE	*output;
	fitsfile *fptr;
	int	fits_status;
	long int naxis, naxes[3];
	int	bitpix;
	long int fpixel, nelements;
	int	i;
	char	s1[240], s2[240];

	/* Are we done??? */

	pthread_mutex_lock(&usb_camera_mutex);
	if (image_data_cube_num_frames == 0 ||
	    image_data_cube_count_frames < image_data_cube_num_frames)
	{
		pthread_mutex_unlock(&usb_camera_mutex);
		return;
	}
	pthread_mutex_unlock(&usb_camera_mutex);

	/* Get current GMT */

	get_ut_date(&year, &month, &day, &doy);

	/* Now we try and create a new filename */

	sprintf(filename_base,"%s%4d_%02d_%02d_%s_wfs_",
		get_data_directory(s), year,month,day,labao_name);

	for (file_number=1;file_number<=MAX_FILE_NUMBER;file_number++)
	{
		 /* Create the filename */

		 sprintf(filename,"%s%03d.fit",
			filename_base,file_number);

		 /* Does it already exist ? */

		 if ((output = fopen(filename,"r")) == NULL) break;
			 fclose(output);
	}

	if (file_number >= MAX_FILE_NUMBER)
	{
		 error(ERROR,
			"Exceeded maximum number of files with this name.");
		 return;
	}

	/* Setup for saving the fits file */

	naxis = 3;
	bitpix = FLOAT_IMG;
	naxes[0] = rectAOI.s32Width;
	naxes[1] = rectAOI.s32Height;
	naxes[2] = image_data_cube_num_frames;
	fpixel = 1;
	nelements = naxes[0] * naxes[1] * naxes[2];

	/* Create a new FITS file */

	fits_status = 0;
	if (fits_create_file(&fptr, filename, &fits_status))
	{
		error(ERROR,"Failed to create FITS file (%d).",
			fits_status);
	}

	/* Write required keywords into the header */

	if (fits_create_img(fptr, bitpix, naxis, naxes, &fits_status))
	{
		error(ERROR,"Failed to create FITS image (%d).",
		fits_status);
	}

	/* Write the FITS image */

	if (fits_write_img(fptr, TFLOAT, fpixel, nelements, 
		image_data_cube, &fits_status))
	{
		error(ERROR,"Failed to write FITS image (%d).",
			fits_status);
	}

	/* Now some header infromation */

	if(fits_update_key(fptr, TSTRING, "LABAO",
		labao_name, "LABAO Unit name",&fits_status))
	{
		error(ERROR,"Failed to write LABAO (%d).",
			fits_status);
	}

	if(fits_update_key(fptr, TINT, "NUMFRAME", &image_data_cube_num_frames,
		"Number of frames", &fits_status))
	{
		error(ERROR,"Failed to write NUMFRAME (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "SENSORID", 
		&(cam_info.SensorID),
		"Sensor ID",&fits_status))
	{
		error(ERROR,"Failed to write SENSORID (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "MAXWIDTH", 
		&(cam_info.nMaxWidth),
		"Sensor Maximum width",&fits_status))
	{
		error(ERROR,"Failed to write MAXWIDTH (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "MAXHGHT", 
		&(cam_info.nMaxHeight),
		"Sensor Maximum height",&fits_status))
	{
		error(ERROR,"Failed to write MAXHGHT (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "AOI_X", 
		&(rectAOI.s32X),
		"AOI Positionin X",&fits_status))
	{
		error(ERROR,"Failed to write AOI_X (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "AOI_Y", 
		&(rectAOI.s32Y),
		"AOI Positionin Y",&fits_status))
	{
		error(ERROR,"Failed to write AOI_Y (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "AOI_DX", 
		&(rectAOI.s32Width),
		"AOI Positionin DX",&fits_status))
	{
		error(ERROR,"Failed to write AOI_DX (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "AOI_DY", 
		&(rectAOI.s32Height),
		"AOI Positionin DY",&fits_status))
	{
		error(ERROR,"Failed to write AOI_DY (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TFLOAT, "CAM_FPS",
		&usb_camera.fps,
		"Frames per second.",&fits_status))
	{
		error(ERROR,"Failed to write CAM_FPS (%d).",
			fits_status);
	}

	if(fits_update_key(fptr, TINT, "CAM_GAIN", 
		&usb_camera.gain,
		"Camera Gain",&fits_status))
	{
		error(ERROR,"Failed to write CAM_GAIN (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "CAM_CLOCK", 
		&usb_camera.pixelclock,
		"Camera pixel cloxk",&fits_status))
	{
		error(ERROR,"Failed to write CAM_CLOCK (%d).",
		fits_status);
	}

	if(fits_update_key(fptr, TINT, "LABSTART", &data_record_start,
                        "Time of first datum (mS)",&fits_status))
        {
                error(ERROR,"Failed to write LABSTART (%d).", fits_status);
        }

        if(fits_update_key(fptr, TINT, "LABSTOP", &data_record_stop,
                        "Time of last datum (mS)",&fits_status))
        {
                error(ERROR,"Failed to write LABSTOP (%d).", fits_status);
        }

	for(i=0; i<NUM_LENSLETS; i++)
	{
		sprintf(s1,"CEN_X_%02d", i+1);
                sprintf(s2,"X position reference centroid %02d", i+1);

                if(fits_update_key(fptr, TFLOAT, s1,
                        &(x_centroid_offsets[i]), s2, &fits_status))
                {
                        error(ERROR,"Failed to write %s (%d).", s1,fits_status);
                }

		sprintf(s1,"CEN_Y_%02d", i+1);
                sprintf(s2,"Y position reference centroid %02d", i+1);

                if(fits_update_key(fptr, TFLOAT, s1,
                        &(y_centroid_offsets[i]),
                        s2,&fits_status))
                {
                        error(ERROR,"Failed to write %s (%d).", s1,fits_status);
                }
        }

	/* That should be enough! */

	if (fits_close_file(fptr, &fits_status))
	{
		error(ERROR,"Failed to close fits file (%d).",
			fits_status);
	}

	/* Clean up memory go */

	message(system_window,  "Saved file %s", filename);
	send_labao_text_message("Saved file %s", filename);

	pthread_mutex_lock(&usb_camera_mutex);
	image_data_cube_num_frames = 0;
        image_data_cube_count_frames = 0;
	free(image_data_cube);
	pthread_mutex_unlock(&usb_camera_mutex);

} /* complete_fits_cube() */

/************************************************************************/
/* usb_camera_set_exptime()		    				*/
/*									*/
/* Positive Number - Actual exposure time set				*/
/* -1 : Failed call.	   						*/
/************************************************************************/

double usb_camera_set_exptime(double exposure_in)
{
	double exposure;
	int errcode;

	exposure = exposure_in;
	if ((errcode = is_Exposure(cam_pointer, IS_EXPOSURE_CMD_SET_EXPOSURE,
		&exposure, sizeof(exposure))) )
	{
		if (errcode == IS_INVALID_PARAMETER)
			return -1.0; 
		else 
			return -2.0;
	}

	usb_camera.exptime = exposure_in;
	send_labao_set_usb_camera(TRUE);

	return exposure;

} /* set_frame_rate() */

/************************************************************************/
/* call_set_frame_rate()				    		*/
/*									*/
/* User Callable version of set_frame_rate  				*/
/************************************************************************/

int call_usb_camera_set_exptime(int argc, char **argv)
{
	char s[100];
	double input_exptime, new_exptime;

	if (argc > 1)
	{
		sscanf(argv[1],"%lf",&input_exptime);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %6.2lf", usb_camera.exptime);
		if (quick_edit("Exposure time (sec)",s,s,NULL,FLOAT)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%lf",&input_exptime);
	}

	if ((new_exptime = usb_camera_set_exptime(input_exptime)) < 0.0)
	{
		if (new_exptime == -1.0)
			return error(ERROR, "Invalid exposure time");
		else return error(ERROR, "Unknown error setting Frame Rate");
	}

	message(system_window, "Exposure time now: %6.1lf", new_exptime);

	return NOERROR;
}

/************************************************************************/
/* toggle_overlay_boxes()	 		    			*/
/*									*/
/* Turns on or off the over lay boxes on teh display			*/
/************************************************************************/

int toggle_overlay_boxes(int argc, char **argv)
{
	if (usb_camera.overlay_boxes)
	{
		usb_camera.overlay_boxes=FALSE;
		message(system_window, "NOT showing overlay boxes.");
		send_labao_text_message("NOT showing overlay boxe.");
	}
	else
	{
		usb_camera.overlay_boxes=TRUE;
		message(system_window, "Showing overlay boxes.");
		send_labao_text_message("Showing overlay boxe.");
	}

	send_labao_set_usb_camera(TRUE);
	return NOERROR;

} /* toggle_overlay_boxes() */

/************************************************************************/
/* send_labao_set_usb_camera()						*/
/*									*/
/* Send current usb_camera one client, or all if argument is		*/
/* TRUE;								*/
/************************************************************************/

int send_labao_set_usb_camera(bool send_to_all_clients)
{
	struct smessage mess;

	usb_camera.num_lenslets = NUM_LENSLETS;
	usb_camera.usb_disp_x = USB_DISP_X;
	usb_camera.usb_disp_y = USB_DISP_Y;
	usb_camera.centroid_box_width = CENTROID_BOX_WIDTH;
	mess.type = LABAO_SET_USB_CAMERA;
	mess.length = sizeof(usb_camera);
	mess.data   = (unsigned char *)&usb_camera;

	if (send_to_all_clients)
	{
		ui_send_message_all(&mess);
	}
	else
	{
		if (client_socket != -1) ui_send_message(client_socket, &mess);
	}

	return NOERROR;

} /* send_labao_set_usb_camera() */

/************************************************************************/
/* usb_camera_is_running()						*/
/*									*/
/* So outside world can tell if teh camera is running or not.		*/
/************************************************************************/

bool usb_cammera_is_running(void) {return usb_camera_running; }

/************************************************************************/
/* set_num_sum_frame()							*/
/*									*/
/* Get the USB camera area of interest 					*/
/************************************************************************/

int set_num_sum_frame(int num)
{
	if (num <= 0) return -1;

	num_sum_frame = num;
	count_sum_frame = 0;

	return num;

} /* set_num_sum_frame() */

/************************************************************************/
/* call_set_usb_camera_aoi()						*/
/*									*/
/* User callable version.						*/
/************************************************************************/

int call_set_num_sum_frame(int argc, char **argv)
{
	char	s[100];
	int	num;

	if (argc > 1)
	{
		sscanf(argv[1],"%d",&num);
	}
	else
	{
		clean_command_line();
		sprintf(s,"    %5d", num);
		if (quick_edit("Number of frames to sum",s,s,NULL,INTEGER)
		   == KEY_ESC) return NOERROR;
		sscanf(s,"%d",&num);
	}

	if (set_num_sum_frame(num) < 0)
		return error(ERROR,"Invalid number of frames to sum");

	return NOERROR;

} /* call_set_num_sum_frame() */
