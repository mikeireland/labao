This is the original code form Mike for calculating focus and so on.

        /* To compute WFS aberrations, we need the current pupil center. */
        for (i=0;i<NUM_LENSLETS;i++)
        {
                x_mean_offset += x_centroid_offsets[i];
                y_mean_offset += y_centroid_offsets[i];
        }
        x_mean_offset /= NUM_LENSLETS;
        y_mean_offset /= NUM_LENSLETS;
        for (i=0;i<NUM_LENSLETS;i++)
        {
                if (fabs(x_centroid_offsets[i] - x_mean_offset) > max_offset)
                        max_offset = fabs(x_centroid_offsets[i] - x_mean_offset);
                if (fabs(y_centroid_offsets[i] - y_mean_offset) > max_offset)
                        max_offset = fabs(y_centroid_offsets[i] - y_mean_offset);
                xtilt += xc[i];
                ytilt += yc[i];
                focus += xc[i]*(x_centroid_offsets[i]-x_mean_offset) + 
			 yc[i]*(y_centroid_offsets[i]-y_mean_offset);
                a1 += xc[i]*(x_centroid_offsets[i]-x_mean_offset) - 
			 yc[i]*(y_centroid_offsets[i]-y_mean_offset);
                a2 += yc[i]*(x_centroid_offsets[i]-x_mean_offset) + 
			 xc[i]*(y_centroid_offsets[i]-y_mean_offset);
        }
        xtilt /= NUM_LENSLETS/ARCSEC_PER_PIX;
        ytilt /= NUM_LENSLETS/ARCSEC_PER_PIX;
        focus /= NUM_LENSLETS/ARCSEC_PER_PIX;
        a1 /= NUM_LENSLETS/ARCSEC_PER_PIX;
        a2 /= NUM_LENSLETS/ARCSEC_PER_PIX;

        /* So that all terms are at least in units of "pixels" (i.e. max tilt per sub-aperture), we divide by the
           radius of the pupil. */
        focus /= max_offset;
        a1 /= max_offset;
        a2 /= max_offset;


