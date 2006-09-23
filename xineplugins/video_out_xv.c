/*
 * Copyright (C) 2000-2004 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: video_out_xv.c,v 1.214 2005/09/25 00:44:04 miguelfreitas Exp $
 *
 * video_out_xv.c, X11 video extension interface for xine
 *
 * based on mpeg2dec code from
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * Xv image support by Gerd Knorr <kraxel@goldbach.in-berlin.de>
 *
 * xine-specific code by Guenter Bartsch <bartscgr@studbox.uni-stuttgart.de>
 *
 * overlay support by James Courtier-Dutton <James@superbug.demon.co.uk> - July 2001
 * X11 unscaled overlay support by Miguel Freitas - Nov 2003
 *
 * X11 without threads support by Matthias Kretz <kretz@kde.org> Sep 2006
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <sys/types.h>
#if defined(__FreeBSD__)
#include <machine/param.h>
#endif
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#define LOG_MODULE "video_out_xv_nt"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine.h>
#include <xine/video_out.h>
#include <xine/xine_internal.h>
/* #include "overlay.h" */
#include "deinterlace.h"
#include <xine/xineutils.h>
#include <xine/vo_scale.h>
#include "x11osd.h"

typedef struct xv_driver_s xv_driver_t;

typedef struct {
  int                value;
  int                min;
  int                max;
  Atom               atom;

  cfg_entry_t       *entry;

  xv_driver_t       *this;
} xv_property_t;

typedef struct {
  char              *name;
  int                value;
} xv_portattribute_t;

typedef struct {
  vo_frame_t         vo_frame;

  unsigned int       width, height;
  int                format;
  double             ratio;

  XvImage           *image;
  XShmSegmentInfo    shminfo;

} xv_frame_t;


struct xv_driver_s {

  vo_driver_t        vo_driver;

  config_values_t   *config;

  /* X11 / Xv related stuff */
  Display           *display;
  int                screen;
  Drawable           drawable;
  unsigned int       xv_format_yv12;
  unsigned int       xv_format_yuy2;
  XVisualInfo        vinfo;
  GC                 gc;
  XvPortID           xv_port;
  XColor             black;

  int                use_shm;
  int                use_pitch_alignment;
  xv_property_t      props[VO_NUM_PROPERTIES];
  uint32_t           capabilities;

  xv_frame_t        *recent_frames[VO_NUM_RECENT_FRAMES];
  xv_frame_t        *cur_frame;
  x11osd            *xoverlay;
  int                ovl_changed;

  /* all scaling information goes here */
  vo_scale_t         sc;

  xv_frame_t         deinterlace_frame;
  int                deinterlace_method;
  int                deinterlace_enabled;

  int                use_colorkey;
  uint32_t           colorkey;

  /* hold initial port attributes values to restore on exit */
  xine_list_t       *port_attributes;

  int              (*x11_old_error_handler)  (Display *, XErrorEvent *);

  xine_t            *xine;

  alphablend_t       alphablend_extra_data;

  pthread_mutex_t    display_mutex;
};

typedef struct {
  video_driver_class_t driver_class;

  config_values_t     *config;
  xine_t              *xine;
} xv_class_t;

static int gX11Fail;

static uint32_t xv_get_capabilities (vo_driver_t *this_gen) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  return this->capabilities;
}

static void xv_frame_field (vo_frame_t *vo_img, int which_field) {
  /* not needed for Xv */
}

static void xv_frame_dispose (vo_frame_t *vo_img) {
  xv_frame_t  *frame = (xv_frame_t *) vo_img ;
  xv_driver_t *this  = (xv_driver_t *) vo_img->driver;

  if (frame->image) {

    if (this->use_shm) {
      pthread_mutex_lock (&this->display_mutex);
      XShmDetach (this->display, &frame->shminfo);
      XFree (frame->image);
      pthread_mutex_unlock (&this->display_mutex);

      shmdt (frame->shminfo.shmaddr);
      shmctl (frame->shminfo.shmid, IPC_RMID, NULL);
    }
    else {
      pthread_mutex_lock (&this->display_mutex);
      free (frame->image->data);
      XFree (frame->image);
      pthread_mutex_unlock (&this->display_mutex);
    }
  }

  free (frame);
}

static vo_frame_t *xv_alloc_frame (vo_driver_t *this_gen) {
  /* xv_driver_t  *this = (xv_driver_t *) this_gen; */
  xv_frame_t   *frame ;

  frame = (xv_frame_t *) xine_xmalloc (sizeof (xv_frame_t));
  if (!frame)
    return NULL;
  
  pthread_mutex_init (&frame->vo_frame.mutex, NULL);

  /*
   * supply required functions
   */
  frame->vo_frame.proc_slice = NULL;
  frame->vo_frame.proc_frame = NULL;
  frame->vo_frame.field      = xv_frame_field;
  frame->vo_frame.dispose    = xv_frame_dispose;
  frame->vo_frame.driver     = this_gen;

  return (vo_frame_t *) frame;
}

static int HandleXError (Display *display, XErrorEvent *xevent) {
  char str [1024];
  
  XGetErrorText (display, xevent->error_code, str, 1024);
  printf ("received X error event: %s\n", str);
  gX11Fail = 1;

  return 0;
}

/* called xlocked */
static void x11_InstallXErrorHandler (xv_driver_t *this) {
  this->x11_old_error_handler = XSetErrorHandler (HandleXError);
  XSync(this->display, False);
}

/* called xlocked */
static void x11_DeInstallXErrorHandler (xv_driver_t *this) {
  XSetErrorHandler (this->x11_old_error_handler);
  XSync(this->display, False);
  this->x11_old_error_handler = NULL;
}

/* called xlocked */
static XvImage *create_ximage (xv_driver_t *this, XShmSegmentInfo *shminfo,
			       int width, int height, int format) {
  unsigned int  xv_format;
  XvImage      *image = NULL;

  if (this->use_pitch_alignment) {
    width = (width + 7) & ~0x7;
  }

  switch (format) {
  case XINE_IMGFMT_YV12:
    xv_format = this->xv_format_yv12;
    break;
  case XINE_IMGFMT_YUY2:
    xv_format = this->xv_format_yuy2;
    break;
  default:
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "create_ximage: unknown format %08x\n",format);
    _x_abort();
  }

  if (this->use_shm) {

    /*
     * try shm
     */

    gX11Fail = 0;
    x11_InstallXErrorHandler (this);

    image = XvShmCreateImage(this->display, this->xv_port, xv_format, 0,
			     width, height, shminfo);

    if (image == NULL )  {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_xv: XvShmCreateImage failed\n"
		"video_out_xv: => not using MIT Shared Memory extension.\n"));
      this->use_shm = 0;
      goto finishShmTesting;
    }

    shminfo->shmid = shmget(IPC_PRIVATE, image->data_size, IPC_CREAT | 0777);

    if (image->data_size==0) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_xv: XvShmCreateImage returned a zero size\n"
		"video_out_xv: => not using MIT Shared Memory extension.\n"));
      this->use_shm = 0;
      goto finishShmTesting;
    }

    if (shminfo->shmid < 0 ) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_xv: shared memory error in shmget: %s\n"
		"video_out_xv: => not using MIT Shared Memory extension.\n"), strerror(errno));
      this->use_shm = 0;
      goto finishShmTesting;
    }

    shminfo->shmaddr  = (char *) shmat(shminfo->shmid, 0, 0);

    if (shminfo->shmaddr == NULL) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_xv: shared memory error (address error NULL)\n");
      this->use_shm = 0;
      goto finishShmTesting;
    }

    if (shminfo->shmaddr == ((char *) -1)) {
      xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	      "video_out_xv: shared memory error (address error)\n");
      this->use_shm = 0;
      goto finishShmTesting;
    }

    shminfo->readOnly = False;
    image->data       = shminfo->shmaddr;

    XShmAttach(this->display, shminfo);

    XSync(this->display, False);
    shmctl(shminfo->shmid, IPC_RMID, 0);

    if (gX11Fail) {
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_xv: x11 error during shared memory XImage creation\n"
		"video_out_xv: => not using MIT Shared Memory extension.\n"));
      shmdt (shminfo->shmaddr);
      shmctl (shminfo->shmid, IPC_RMID, 0);
      shminfo->shmid = -1;
      this->use_shm  = 0;
      goto finishShmTesting;
    }

    /*
     * Now that the Xserver has learned about and attached to the
     * shared memory segment,  delete it.  It's actually deleted by
     * the kernel when all users of that segment have detached from
     * it.  Gives an automatic shared memory cleanup in case we crash.
     */
    shmctl (shminfo->shmid, IPC_RMID, 0);
    shminfo->shmid = -1;

  finishShmTesting:
    x11_DeInstallXErrorHandler(this);
  }


  /*
   * fall back to plain Xv if necessary
   */

  if (!this->use_shm) {
    char *data;

    switch (format) {
    case XINE_IMGFMT_YV12:
      data = malloc (width * height * 3/2);
      break;
    case XINE_IMGFMT_YUY2:
      data = malloc (width * height * 2);
      break;
    default:
      xprintf (this->xine, XINE_VERBOSITY_DEBUG, "create_ximage: unknown format %08x\n",format);
      _x_abort();
    }

    image = XvCreateImage (this->display, this->xv_port,
			   xv_format, data, width, height);
  }
  return image;
}

/* called xlocked */
static void dispose_ximage (xv_driver_t *this,
			    XShmSegmentInfo *shminfo,
			    XvImage *myimage) {

  if (this->use_shm) {

    XShmDetach (this->display, shminfo);
    XFree (myimage);
    shmdt (shminfo->shmaddr);
    if (shminfo->shmid >= 0) {
      shmctl (shminfo->shmid, IPC_RMID, 0);
      shminfo->shmid = -1;
    }

  } 
  else {
    free (myimage->data);
    
    XFree (myimage);
  }
}

static void xv_update_frame_format (vo_driver_t *this_gen,
				    vo_frame_t *frame_gen,
				    uint32_t width, uint32_t height,
				    double ratio, int format, int flags) {
  xv_driver_t  *this  = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;

  if (this->use_pitch_alignment) {
    width = (width + 7) & ~0x7;
  }

  if ((frame->width != width)
      || (frame->height != height)
      || (frame->format != format)) {

    /* printf ("video_out_xv: updating frame to %d x %d (ratio=%d, format=%08x)\n",width,height,ratio_code,format); */

    pthread_mutex_lock (&this->display_mutex);

    /*
     * (re-) allocate xvimage
     */

    if (frame->image) {
      dispose_ximage (this, &frame->shminfo, frame->image);
      frame->image = NULL;
    }

    frame->image = create_ximage (this, &frame->shminfo, width, height, format);

    if(format == XINE_IMGFMT_YUY2) {
      frame->vo_frame.pitches[0] = frame->image->pitches[0];
      frame->vo_frame.base[0] = (uint8_t*)frame->image->data + frame->image->offsets[0];
    } 
    else {
      frame->vo_frame.pitches[0] = frame->image->pitches[0];
      frame->vo_frame.pitches[1] = frame->image->pitches[2];
      frame->vo_frame.pitches[2] = frame->image->pitches[1];
      frame->vo_frame.base[0] = (uint8_t*)frame->image->data + frame->image->offsets[0];
      frame->vo_frame.base[1] = (uint8_t*)frame->image->data + frame->image->offsets[2];
      frame->vo_frame.base[2] = (uint8_t*)frame->image->data + frame->image->offsets[1];
    }

    frame->width  = width;
    frame->height = height;
    frame->format = format;

    pthread_mutex_unlock (&this->display_mutex);
  }

  frame->ratio = ratio;
}

#define DEINTERLACE_CROMA
static void xv_deinterlace_frame (xv_driver_t *this) {
  uint8_t    *recent_bitmaps[VO_NUM_RECENT_FRAMES];
  xv_frame_t *frame = this->recent_frames[0];
  int         i;
  int         xvscaling;

  xvscaling = (this->deinterlace_method == DEINTERLACE_ONEFIELDXV) ? 2 : 1;

  if (!this->deinterlace_frame.image
      || (frame->width != this->deinterlace_frame.width)
      || (frame->height != this->deinterlace_frame.height )
      || (frame->format != this->deinterlace_frame.format)
      || (frame->ratio != this->deinterlace_frame.ratio)) {
    pthread_mutex_lock (&this->display_mutex);

    if(this->deinterlace_frame.image)
      dispose_ximage (this, &this->deinterlace_frame.shminfo,
                      this->deinterlace_frame.image);
    
    this->deinterlace_frame.image = create_ximage (this, &this->deinterlace_frame.shminfo,
						   frame->width,frame->height / xvscaling,
						   frame->format);
    this->deinterlace_frame.width  = frame->width;
    this->deinterlace_frame.height = frame->height;
    this->deinterlace_frame.format = frame->format;
    this->deinterlace_frame.ratio  = frame->ratio;

    pthread_mutex_unlock (&this->display_mutex);
  }


  if ( this->deinterlace_method != DEINTERLACE_ONEFIELDXV ) {
#ifdef DEINTERLACE_CROMA

    /* I don't think this is the right way to do it (deinterlacing croma by croma info).
       DScaler deinterlaces croma together with luma, but it's easier for them because
       they have that components 1:1 at the same table.
    */
    for( i = 0; i < VO_NUM_RECENT_FRAMES; i++ )
      if( this->recent_frames[i] && this->recent_frames[i]->width == frame->width &&
          this->recent_frames[i]->height == frame->height )
        recent_bitmaps[i] = (uint8_t*)this->recent_frames[i]->image->data + frame->width*frame->height;
      else
        recent_bitmaps[i] = NULL;

    deinterlace_yuv( (uint8_t*)this->deinterlace_frame.image->data+frame->width*frame->height,
		     recent_bitmaps, frame->width/2, frame->height/2, this->deinterlace_method );
    for( i = 0; i < VO_NUM_RECENT_FRAMES; i++ )
      if( this->recent_frames[i] && this->recent_frames[i]->width == frame->width &&
          this->recent_frames[i]->height == frame->height )
        recent_bitmaps[i] = (uint8_t*)this->recent_frames[i]->image->data + frame->width*frame->height*5/4;
      else
        recent_bitmaps[i] = NULL;

    deinterlace_yuv( (uint8_t*)this->deinterlace_frame.image->data+frame->width*frame->height*5/4,
		     recent_bitmaps, frame->width/2, frame->height/2, this->deinterlace_method );

#else

    /* know bug: we are not deinterlacing Cb and Cr */
    xine_fast_memcpy(this->deinterlace_frame.image->data + frame->width*frame->height,
		     frame->image->data + frame->width*frame->height,
		     frame->width*frame->height*1/2);

#endif

    for( i = 0; i < VO_NUM_RECENT_FRAMES; i++ )
      if( this->recent_frames[i] && this->recent_frames[i]->width == frame->width &&
          this->recent_frames[i]->height == frame->height )
        recent_bitmaps[i] = (uint8_t*)this->recent_frames[i]->image->data;
      else
        recent_bitmaps[i] = NULL;

    deinterlace_yuv( (uint8_t*)this->deinterlace_frame.image->data, recent_bitmaps,
                     frame->width, frame->height, this->deinterlace_method );
  }
  else {
    /*
      dirty and cheap deinterlace method: we give half of the lines to xv
      driver and let it scale for us.
      note that memcpy's below don't seem to impact much on performance,
      specially when fast memcpys are available.
    */
    uint8_t *dst, *src;

    dst = (uint8_t*)this->deinterlace_frame.image->data;
    src = (uint8_t*)this->recent_frames[0]->image->data;
    for( i = 0; i < (int)frame->height; i+=2 ) {
      xine_fast_memcpy(dst,src,frame->width);
      dst += frame->width;
      src += 2 * frame->width;
    }

    dst = (uint8_t*)this->deinterlace_frame.image->data + frame->width * frame->height / 2;
    src = (uint8_t*)this->recent_frames[0]->image->data + frame->width * frame->height;
    for( i = 0; i < (int)frame->height; i+=4 ) {
      xine_fast_memcpy(dst,src,frame->width / 2);
      dst += frame->width / 2;
      src += frame->width;
    }

    dst = (uint8_t*)this->deinterlace_frame.image->data + frame->width * frame->height * 5 / 8;
    src = (uint8_t*)this->recent_frames[0]->image->data + frame->width * frame->height * 5 / 4;
    for( i = 0; i < (int)frame->height; i+=4 ) {
      xine_fast_memcpy(dst,src,frame->width / 2);
      dst += frame->width / 2;
      src += frame->width;
    }
  }

  this->cur_frame = &this->deinterlace_frame;
}

static void xv_clean_output_area (xv_driver_t *this) {
  int i;

  pthread_mutex_lock (&this->display_mutex);

  XSetForeground (this->display, this->gc, this->black.pixel);

  for( i = 0; i < 4; i++ ) {
    if( this->sc.border[i].w && this->sc.border[i].h ) {
      XFillRectangle(this->display, this->drawable, this->gc,
		     this->sc.border[i].x, this->sc.border[i].y,
		     this->sc.border[i].w, this->sc.border[i].h);
    }
  }

  if (this->use_colorkey) {
    XSetForeground (this->display, this->gc, this->colorkey);
    XFillRectangle (this->display, this->drawable, this->gc,
		    this->sc.output_xoffset, this->sc.output_yoffset,
		    this->sc.output_width, this->sc.output_height);
  }
  
  if (this->xoverlay) {
    x11osd_resize (this->xoverlay, this->sc.gui_width, this->sc.gui_height);
    this->ovl_changed = 1;
  }
  
  pthread_mutex_unlock (&this->display_mutex);
}

/*
 * convert delivered height/width to ideal width/height
 * taking into account aspect ratio and zoom factor
 */

static void xv_compute_ideal_size (xv_driver_t *this) {
  _x_vo_scale_compute_ideal_size( &this->sc );
}


/*
 * make ideal width/height "fit" into the gui
 */

static void xv_compute_output_size (xv_driver_t *this) {

  _x_vo_scale_compute_output_size( &this->sc );

  /* onefield_xv divide by 2 the number of lines */
  if (this->deinterlace_enabled
      && (this->deinterlace_method == DEINTERLACE_ONEFIELDXV)
      && this->cur_frame && (this->cur_frame->format == XINE_IMGFMT_YV12)) {
    this->sc.displayed_height  = this->sc.displayed_height / 2 - 1;
    this->sc.displayed_yoffset = this->sc.displayed_yoffset / 2;
  }
}

static void xv_overlay_begin (vo_driver_t *this_gen, 
			      vo_frame_t *frame_gen, int changed) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;

  this->ovl_changed += changed;

  if( this->ovl_changed && this->xoverlay ) {
    pthread_mutex_lock (&this->display_mutex);
    x11osd_clear(this->xoverlay); 
    pthread_mutex_unlock (&this->display_mutex);
  }
  
  this->alphablend_extra_data.offset_x = frame_gen->overlay_offset_x;
  this->alphablend_extra_data.offset_y = frame_gen->overlay_offset_y;
}

static void xv_overlay_end (vo_driver_t *this_gen, vo_frame_t *vo_img) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;

  if( this->ovl_changed && this->xoverlay ) {
    pthread_mutex_lock (&this->display_mutex);
    x11osd_expose(this->xoverlay);
    pthread_mutex_unlock (&this->display_mutex);
  }

  this->ovl_changed = 0;
}

static void xv_overlay_blend (vo_driver_t *this_gen, 
			      vo_frame_t *frame_gen, vo_overlay_t *overlay) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;

  if (overlay->rle) {
    if( overlay->unscaled ) {
      if( this->ovl_changed && this->xoverlay ) {
        pthread_mutex_lock (&this->display_mutex);
        x11osd_blend(this->xoverlay, overlay); 
        pthread_mutex_unlock (&this->display_mutex);
      }
    } else {
      if (frame->format == XINE_IMGFMT_YV12)
        _x_blend_yuv(frame->vo_frame.base, overlay, 
		  frame->width, frame->height, frame->vo_frame.pitches,
                  &this->alphablend_extra_data);
      else
        _x_blend_yuy2(frame->vo_frame.base[0], overlay, 
		   frame->width, frame->height, frame->vo_frame.pitches[0],
                   &this->alphablend_extra_data);
    }
  }
}

static void xv_add_recent_frame (xv_driver_t *this, xv_frame_t *frame) {
  int i;

  i = VO_NUM_RECENT_FRAMES-1;
  if( this->recent_frames[i] )
    this->recent_frames[i]->vo_frame.free
       (&this->recent_frames[i]->vo_frame);

  for( ; i ; i-- )
    this->recent_frames[i] = this->recent_frames[i-1];

  this->recent_frames[0] = frame;
}

/* currently not used - we could have a method to call this from video loop */
#if 0
static void xv_flush_recent_frames (xv_driver_t *this) {
  int i;

  for( i=0; i < VO_NUM_RECENT_FRAMES; i++ ) {
    if( this->recent_frames[i] )
      this->recent_frames[i]->vo_frame.free
         (&this->recent_frames[i]->vo_frame);
    this->recent_frames[i] = NULL;
  }
}
#endif

static int xv_redraw_needed (vo_driver_t *this_gen) {
  xv_driver_t  *this = (xv_driver_t *) this_gen;
  int           ret  = 0;

  if( this->cur_frame ) {

    this->sc.delivered_height = this->cur_frame->height;
    this->sc.delivered_width  = this->cur_frame->width;
    this->sc.delivered_ratio  = this->cur_frame->ratio;
    
    this->sc.crop_left        = this->cur_frame->vo_frame.crop_left;
    this->sc.crop_right       = this->cur_frame->vo_frame.crop_right;
    this->sc.crop_top         = this->cur_frame->vo_frame.crop_top;
    this->sc.crop_bottom      = this->cur_frame->vo_frame.crop_bottom;

    xv_compute_ideal_size(this);

    if( _x_vo_scale_redraw_needed( &this->sc ) ) {

      xv_compute_output_size (this);

      xv_clean_output_area (this);

      ret = 1;
    }
  }
  else
    ret = 1;

  return ret;
}

static void xv_display_frame (vo_driver_t *this_gen, vo_frame_t *frame_gen) {
  xv_driver_t  *this  = (xv_driver_t *) this_gen;
  xv_frame_t   *frame = (xv_frame_t *) frame_gen;
  /*
  printf ("video_out_xv: xv_display_frame...\n");
  */
  
  /*
   * queue frames (deinterlacing)
   * free old frames
   */

  xv_add_recent_frame (this, frame); /* deinterlacing */

  this->cur_frame = frame;

  /*
   * let's see if this frame is different in size / aspect
   * ratio from the previous one
   */
  if ( (frame->width != this->sc.delivered_width)
       || (frame->height != this->sc.delivered_height)
       || (frame->ratio != this->sc.delivered_ratio) ) {
    lprintf("frame format changed\n");
    this->sc.force_redraw = 1;    /* trigger re-calc of output size */
  }

  /*
   * deinterlace frame if necessary
   * (currently only working for YUV images)
   */

  if (this->deinterlace_enabled && this->deinterlace_method
      && frame->format == XINE_IMGFMT_YV12
      && (deinterlace_yuv_supported( this->deinterlace_method ) == 1
	  || this->deinterlace_method ==  DEINTERLACE_ONEFIELDXV))
    xv_deinterlace_frame (this);

  /*
   * tell gui that we are about to display a frame,
   * ask for offset and output size
   */
  xv_redraw_needed (this_gen);

  pthread_mutex_lock (&this->display_mutex);

  if (this->use_shm) {
    XvShmPutImage(this->display, this->xv_port,
                  this->drawable, this->gc, this->cur_frame->image,
                  this->sc.displayed_xoffset, this->sc.displayed_yoffset,
                  this->sc.displayed_width, this->sc.displayed_height,
                  this->sc.output_xoffset, this->sc.output_yoffset,
                  this->sc.output_width, this->sc.output_height, True);

  } else {
    XvPutImage(this->display, this->xv_port,
               this->drawable, this->gc, this->cur_frame->image,
               this->sc.displayed_xoffset, this->sc.displayed_yoffset,
               this->sc.displayed_width, this->sc.displayed_height,
               this->sc.output_xoffset, this->sc.output_yoffset,
               this->sc.output_width, this->sc.output_height);
  }

  XSync(this->display, False);

  pthread_mutex_unlock (&this->display_mutex);

  /*
  printf ("video_out_xv: xv_display_frame... done\n");
  */
}

static int xv_get_property (vo_driver_t *this_gen, int property) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  switch (property) {
    case VO_PROP_WINDOW_WIDTH:
      this->props[property].value = this->sc.gui_width;
      break;
    case VO_PROP_WINDOW_HEIGHT:
      this->props[property].value = this->sc.gui_height;
      break;
  }

  lprintf("video_out_xv: property #%d = %d\n", property, this->props[property].value);

  return this->props[property].value;
}

static void xv_property_callback (void *property_gen, xine_cfg_entry_t *entry) {
  xv_property_t *property = (xv_property_t *) property_gen;
  xv_driver_t   *this = property->this;
  
  pthread_mutex_lock (&this->display_mutex);
  XvSetPortAttribute (this->display, this->xv_port,
		      property->atom,
		      entry->num_value);
  pthread_mutex_unlock (&this->display_mutex);
}

static int xv_set_property (vo_driver_t *this_gen,
			    int property, int value) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  if (this->props[property].atom != None) {

    /* value is out of bound */
    if((value < this->props[property].min) || (value > this->props[property].max))
      value = (this->props[property].min + this->props[property].max) >> 1;

    pthread_mutex_lock (&this->display_mutex);
    XvSetPortAttribute (this->display, this->xv_port,
			this->props[property].atom, value);
    XvGetPortAttribute (this->display, this->xv_port,
			this->props[property].atom,
			&this->props[property].value);
    pthread_mutex_unlock (&this->display_mutex);

    if (this->props[property].entry)
      this->props[property].entry->num_value = this->props[property].value;

    return this->props[property].value;
  } 
  else {
    switch (property) {

    case VO_PROP_INTERLACED:
      this->props[property].value = value;
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      "video_out_xv: VO_PROP_INTERLACED(%d)\n", this->props[property].value);
      this->deinterlace_enabled = value;
      if (this->deinterlace_method == DEINTERLACE_ONEFIELDXV) {
         xv_compute_ideal_size (this);
         xv_compute_output_size (this);
      }
      break;
  
    case VO_PROP_ASPECT_RATIO:
      if (value>=XINE_VO_ASPECT_NUM_RATIOS)
	value = XINE_VO_ASPECT_AUTO;

      this->props[property].value = value;
      xprintf(this->xine, XINE_VERBOSITY_LOG, 
	      "video_out_xv: VO_PROP_ASPECT_RATIO(%d)\n", this->props[property].value);
      this->sc.user_ratio = value;

      xv_compute_ideal_size (this);

      this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      break;

    case VO_PROP_ZOOM_X:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	xprintf(this->xine, XINE_VERBOSITY_LOG,
		"video_out_xv: VO_PROP_ZOOM_X = %d\n", this->props[property].value);
	
	this->sc.zoom_factor_x = (double)value / (double)XINE_VO_ZOOM_STEP;

	xv_compute_ideal_size (this);

	this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;

    case VO_PROP_ZOOM_Y:
      if ((value >= XINE_VO_ZOOM_MIN) && (value <= XINE_VO_ZOOM_MAX)) {
        this->props[property].value = value;
	xprintf(this->xine, XINE_VERBOSITY_LOG,
		"video_out_xv: VO_PROP_ZOOM_Y = %d\n", this->props[property].value);

	this->sc.zoom_factor_y = (double)value / (double)XINE_VO_ZOOM_STEP;

	xv_compute_ideal_size (this);

	this->sc.force_redraw = 1;    /* trigger re-calc of output size */
      }
      break;
    }
  }

  return value;
}

static void xv_get_property_min_max (vo_driver_t *this_gen,
				     int property, int *min, int *max) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  *min = this->props[property].min;
  *max = this->props[property].max;
}

static int xv_gui_data_exchange (vo_driver_t *this_gen,
				 int data_type, void *data) {
  xv_driver_t     *this = (xv_driver_t *) this_gen;

  switch (data_type) {
#ifndef XINE_DISABLE_DEPRECATED_FEATURES
  case XINE_GUI_SEND_COMPLETION_EVENT:
    break;
#endif

  case XINE_GUI_SEND_EXPOSE_EVENT: {
    /* XExposeEvent * xev = (XExposeEvent *) data; */

    if (this->cur_frame) {

      pthread_mutex_lock (&this->display_mutex);

      if (this->use_shm) {
	XvShmPutImage(this->display, this->xv_port,
		      this->drawable, this->gc, this->cur_frame->image,
		      this->sc.displayed_xoffset, this->sc.displayed_yoffset,
		      this->sc.displayed_width, this->sc.displayed_height,
		      this->sc.output_xoffset, this->sc.output_yoffset,
		      this->sc.output_width, this->sc.output_height, True);
      } else {
	XvPutImage(this->display, this->xv_port,
		   this->drawable, this->gc, this->cur_frame->image,
		   this->sc.displayed_xoffset, this->sc.displayed_yoffset,
		   this->sc.displayed_width, this->sc.displayed_height,
		   this->sc.output_xoffset, this->sc.output_yoffset,
		   this->sc.output_width, this->sc.output_height);
      }

      if(this->xoverlay)
	x11osd_expose(this->xoverlay);
      
      XSync(this->display, False);
      pthread_mutex_unlock (&this->display_mutex);
    }
  }
  break;

  case XINE_GUI_SEND_DRAWABLE_CHANGED:
    pthread_mutex_lock (&this->display_mutex);
    this->drawable = (Drawable) data;
    XFreeGC(this->display, this->gc);
    this->gc = XCreateGC (this->display, this->drawable, 0, NULL);
    if(this->xoverlay)
      x11osd_drawable_changed(this->xoverlay, this->drawable);
    this->ovl_changed = 1;
    pthread_mutex_unlock (&this->display_mutex);
    this->sc.force_redraw = 1;
    break;

  case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
    {
      int x1, y1, x2, y2;
      x11_rectangle_t *rect = data;

      _x_vo_scale_translate_gui2video(&this->sc, rect->x, rect->y,
				   &x1, &y1);
      _x_vo_scale_translate_gui2video(&this->sc, rect->x + rect->w, rect->y + rect->h,
				   &x2, &y2);
      rect->x = x1;
      rect->y = y1;
      rect->w = x2-x1;
      rect->h = y2-y1;

      /* onefield_xv divide by 2 the number of lines */
      if (this->deinterlace_enabled
          && (this->deinterlace_method == DEINTERLACE_ONEFIELDXV)
          && (this->cur_frame->format == XINE_IMGFMT_YV12)) {
        rect->y = rect->y * 2;
        rect->h = rect->h * 2;
      }

    }
    break;

  default:
    return -1;
  }

  return 0;
}

static void xv_store_port_attribute(xv_driver_t *this, char *name) {
  Atom                 atom;
  xv_portattribute_t  *attr;
  
  attr = (xv_portattribute_t *)malloc( sizeof(xv_portattribute_t) );
  attr->name = strdup(name);
  
  atom = XInternAtom (this->display, attr->name, False);
  XvGetPortAttribute (this->display, this->xv_port, atom, &attr->value);
  
  xine_list_push_back (this->port_attributes, attr);
}

static void xv_restore_port_attributes(xv_driver_t *this) {
  Atom                 atom;
  xine_list_iterator_t ite;
  
  while ((ite = xine_list_front(this->port_attributes)) != NULL) {
    xv_portattribute_t *attr = xine_list_get_value(this->port_attributes, ite);
    xine_list_remove (this->port_attributes, ite);
  
    pthread_mutex_lock(&this->display_mutex);
    atom = XInternAtom (this->display, attr->name, False);
    XvSetPortAttribute (this->display, this->xv_port, atom, attr->value);
    pthread_mutex_unlock(&this->display_mutex);
        
    free( attr->name );
    free( attr );
  }
  
  pthread_mutex_lock(&this->display_mutex);
  XSync(this->display, False);
  pthread_mutex_unlock(&this->display_mutex);
 
  xine_list_delete( this->port_attributes );
}

static void xv_dispose (vo_driver_t *this_gen) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  int          i;

  /* restore port attributes to their initial values */
  xv_restore_port_attributes(this);
  
  if (this->deinterlace_frame.image) {
    pthread_mutex_lock (&this->display_mutex);
    dispose_ximage (this, &this->deinterlace_frame.shminfo,
		    this->deinterlace_frame.image);
    pthread_mutex_unlock (&this->display_mutex);
    this->deinterlace_frame.image = NULL;
  }

  pthread_mutex_lock (&this->display_mutex);
  if(XvUngrabPort (this->display, this->xv_port, CurrentTime) != Success) {
    xprintf (this->xine, XINE_VERBOSITY_DEBUG, "video_out_xv: xv_exit: XvUngrabPort() failed.\n");
  }
  XFreeGC(this->display, this->gc);
  pthread_mutex_unlock (&this->display_mutex);

  for( i=0; i < VO_NUM_RECENT_FRAMES; i++ ) {
    if( this->recent_frames[i] )
      this->recent_frames[i]->vo_frame.dispose
         (&this->recent_frames[i]->vo_frame);
    this->recent_frames[i] = NULL;
  }

  if( this->xoverlay ) {
    pthread_mutex_lock (&this->display_mutex);
    x11osd_destroy (this->xoverlay);
    pthread_mutex_unlock (&this->display_mutex);
  }

  _x_alphablend_free(&this->alphablend_extra_data);
  
  free (this);
}

/* called xlocked */
static int xv_check_yv12 (Display *display, XvPortID port) {
  XvImageFormatValues *formatValues;
  int                  formats;
  int                  i;

  formatValues = XvListImageFormats (display, port, &formats);
  
  for (i = 0; i < formats; i++)
    if ((formatValues[i].id == XINE_IMGFMT_YV12) &&
	(! (strcmp (formatValues[i].guid, "YV12")))) {
      XFree (formatValues);
      return 0;
    }

  XFree (formatValues);
  return 1;
}

/* called xlocked */
static void xv_check_capability (xv_driver_t *this,
				 int property, XvAttribute attr,
				 int base_id,
				 char *config_name,
				 char *config_desc,
				 char *config_help) {
  int          int_default;
  cfg_entry_t *entry;
  char        *str_prop = attr.name;
   
  /*
   * some Xv drivers (Gatos ATI) report some ~0 as max values, this is confusing.
   */
  if (VO_PROP_COLORKEY && (attr.max_value == ~0))
    attr.max_value = 2147483615;

  this->props[property].min  = attr.min_value;
  this->props[property].max  = attr.max_value;
  this->props[property].atom = XInternAtom (this->display, str_prop, False);

  XvGetPortAttribute (this->display, this->xv_port,
		      this->props[property].atom, &int_default);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  "video_out_xv: port attribute %s (%d) value is %d\n", str_prop, property, int_default);

  /* disable autopaint colorkey by default */
  /* might be overridden using config entry */
  if(strcmp(str_prop, "XV_AUTOPAINT_COLORKEY") == 0)
    int_default = 0;
    
  if (config_name) {
    /* is this a boolean property ? */
    if ((attr.min_value == 0) && (attr.max_value == 1)) {
      this->config->register_bool (this->config, config_name, int_default,
				   config_desc,
				   config_help, 20, xv_property_callback, &this->props[property]);

    } else {
      this->config->register_range (this->config, config_name, int_default,
				    this->props[property].min, this->props[property].max,
				    config_desc,
				    config_help, 20, xv_property_callback, &this->props[property]);
    }

    entry = this->config->lookup_entry (this->config, config_name);

    if((entry->num_value < this->props[property].min) || 
       (entry->num_value > this->props[property].max)) {

      this->config->update_num(this->config, config_name, 
			       ((this->props[property].min + this->props[property].max) >> 1));
      
      entry = this->config->lookup_entry (this->config, config_name);
    }

    this->props[property].entry = entry;

    xv_set_property (&this->vo_driver, property, entry->num_value);

    if (strcmp(str_prop, "XV_COLORKEY") == 0) {
      this->use_colorkey |= 1;
      this->colorkey = entry->num_value;
    } else if(strcmp(str_prop, "XV_AUTOPAINT_COLORKEY") == 0) {
      if(entry->num_value==1)
        this->use_colorkey |= 2;	/* colorkey is autopainted */
    }
  } else
    this->props[property].value  = int_default;

}

static void xv_update_deinterlace(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  this->deinterlace_method = entry->num_value;
}

static void xv_update_XV_FILTER(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  Atom atom;
  int xv_filter;

  xv_filter = entry->num_value;

  pthread_mutex_lock(&this->display_mutex);
  atom = XInternAtom (this->display, "XV_FILTER", False);
  XvSetPortAttribute (this->display, this->xv_port, atom, xv_filter);
  pthread_mutex_unlock(&this->display_mutex);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  "video_out_xv: bilinear scaling mode (XV_FILTER) = %d\n",xv_filter);
}

static void xv_update_XV_DOUBLE_BUFFER(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;
  Atom         atom;
  int          xv_double_buffer;

  xv_double_buffer = entry->num_value;

  pthread_mutex_lock(&this->display_mutex);
  atom = XInternAtom (this->display, "XV_DOUBLE_BUFFER", False);
  XvSetPortAttribute (this->display, this->xv_port, atom, xv_double_buffer);
  pthread_mutex_unlock(&this->display_mutex);

  xprintf(this->xine, XINE_VERBOSITY_DEBUG,
	  "video_out_xv: double buffering mode = %d\n", xv_double_buffer);
}

static void xv_update_xv_pitch_alignment(void *this_gen, xine_cfg_entry_t *entry) {
  xv_driver_t *this = (xv_driver_t *) this_gen;

  this->use_pitch_alignment = entry->num_value;
}

static vo_driver_t *open_plugin (video_driver_class_t *class_gen, const void *visual_gen) {
  xv_class_t           *class = (xv_class_t *) class_gen;
  config_values_t      *config = class->config;
  xv_driver_t          *this;
  int                   i, formats;
  XvAttribute          *attr;
  XvImageFormatValues  *fo;
  int                   nattr;
  x11_visual_t         *visual = (x11_visual_t *) visual_gen;
  XColor                dummy;
  XvImage              *myimage;
  unsigned int          adaptors, j;
  unsigned int          ver,rel,req,ev,err;
  XShmSegmentInfo       myshminfo;
  XvPortID              xv_port;
  XvAdaptorInfo        *adaptor_info;
  unsigned int          adaptor_num;
  pthread_mutexattr_t   mutexattr;

  this = (xv_driver_t *) xine_xmalloc (sizeof (xv_driver_t));
  if (!this)
    return NULL;

  _x_alphablend_init(&this->alphablend_extra_data, class->xine);
  
  this->display           = visual->display;
  this->screen            = visual->screen;
  this->config            = config;
  pthread_mutexattr_init(&mutexattr);
  pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_ERRORCHECK_NP);
  pthread_mutex_init(&this->display_mutex, &mutexattr);

  /*
   * check for Xvideo support
   */

  pthread_mutex_lock(&this->display_mutex);
  if (Success != XvQueryExtension(this->display, &ver,&rel, &req, &ev,&err)) {
    xprintf (class->xine, XINE_VERBOSITY_LOG, _("video_out_xv: Xv extension not present.\n"));
    pthread_mutex_unlock(&this->display_mutex);
    return NULL;
  }

  /*
   * check adaptors, search for one that supports (at least) yuv12
   */

  if (Success != XvQueryAdaptors(this->display,DefaultRootWindow(this->display), &adaptors, &adaptor_info))  {
    xprintf(class->xine, XINE_VERBOSITY_DEBUG, "video_out_xv: XvQueryAdaptors failed.\n");
    pthread_mutex_unlock(&this->display_mutex);
    return NULL;
  }

  xv_port = 0;

  for ( adaptor_num = 0; (adaptor_num < adaptors) && !xv_port; adaptor_num++ ) {

    if (adaptor_info[adaptor_num].type & XvImageMask) {

      for (j = 0; j < adaptor_info[adaptor_num].num_ports && !xv_port; j++)
        if (( !(xv_check_yv12 (this->display,
			       adaptor_info[adaptor_num].base_id + j)))
            && (XvGrabPort (this->display,
			    adaptor_info[adaptor_num].base_id + j,
			    0) == Success)) {
          xv_port = adaptor_info[adaptor_num].base_id + j;
        }
      
      if( xv_port )
        break;
    }
  }

  if (!xv_port) {
    xprintf(class->xine, XINE_VERBOSITY_LOG,
	    _("video_out_xv: Xv extension is present but I couldn't find a usable yuv12 port.\n"
	      "              Looks like your graphics hardware driver doesn't support Xv?!\n"));
    
    /* XvFreeAdaptorInfo (adaptor_info); this crashed on me (gb)*/
    pthread_mutex_unlock(&this->display_mutex);
    return NULL;
  } 
  else
    xprintf(class->xine, XINE_VERBOSITY_LOG,
	    _("video_out_xv: using Xv port %ld from adaptor %s for hardware "
	      "colorspace conversion and scaling.\n"), xv_port,
            adaptor_info[adaptor_num].name);
  
  pthread_mutex_unlock(&this->display_mutex);
  
  this->xv_port           = xv_port;

  _x_vo_scale_init (&this->sc, 1, 0, config );
  this->sc.frame_output_cb   = visual->frame_output_cb;
  this->sc.user_data         = visual->user_data;

  this->drawable                = visual->d;
  pthread_mutex_lock (&this->display_mutex);
  this->gc                      = XCreateGC (this->display, this->drawable, 0, NULL);
  pthread_mutex_unlock (&this->display_mutex);
  this->capabilities            = VO_CAP_CROP;
  this->use_shm                 = 1;
  this->deinterlace_method      = 0;
  this->deinterlace_frame.image = NULL;
  this->use_colorkey            = 0;
  this->colorkey                = 0;
  this->xoverlay                = NULL;
  this->ovl_changed             = 0;
  this->x11_old_error_handler   = NULL;
  this->xine                    = class->xine;

  pthread_mutex_lock (&this->display_mutex);
  XAllocNamedColor (this->display,
		    DefaultColormap(this->display, this->screen),
		    "black", &this->black, &dummy);
  pthread_mutex_unlock (&this->display_mutex);

  this->vo_driver.get_capabilities     = xv_get_capabilities;
  this->vo_driver.alloc_frame          = xv_alloc_frame;
  this->vo_driver.update_frame_format  = xv_update_frame_format;
  this->vo_driver.overlay_begin        = xv_overlay_begin;
  this->vo_driver.overlay_blend        = xv_overlay_blend;
  this->vo_driver.overlay_end          = xv_overlay_end;
  this->vo_driver.display_frame        = xv_display_frame;
  this->vo_driver.get_property         = xv_get_property;
  this->vo_driver.set_property         = xv_set_property;
  this->vo_driver.get_property_min_max = xv_get_property_min_max;
  this->vo_driver.gui_data_exchange    = xv_gui_data_exchange;
  this->vo_driver.dispose              = xv_dispose;
  this->vo_driver.redraw_needed        = xv_redraw_needed;

  /*
   * init properties
   */

  for (i = 0; i < VO_NUM_PROPERTIES; i++) {
    this->props[i].value = 0;
    this->props[i].min   = 0;
    this->props[i].max   = 0;
    this->props[i].atom  = None;
    this->props[i].entry = NULL;
    this->props[i].this  = this;
  }

  this->props[VO_PROP_INTERLACED].value      = 0;
  this->sc.user_ratio                        =
    this->props[VO_PROP_ASPECT_RATIO].value  = XINE_VO_ASPECT_AUTO;
  this->props[VO_PROP_ZOOM_X].value          = 100;
  this->props[VO_PROP_ZOOM_Y].value          = 100;

  /*
   * check this adaptor's capabilities
   */
  this->port_attributes = xine_list_new();

  pthread_mutex_lock (&this->display_mutex);
  attr = XvQueryPortAttributes(this->display, xv_port, &nattr);
  if(attr && nattr) {
    int k;

    for(k = 0; k < nattr; k++) {
      if((attr[k].flags & XvSettable) && (attr[k].flags & XvGettable)) {
	/* store initial port attribute value */
	xv_store_port_attribute(this, attr[k].name);
	
	if(!strcmp(attr[k].name, "XV_HUE")) {
	  if (!strncmp(adaptor_info[adaptor_num].name, "NV", 2)) {
            xprintf (this->xine, XINE_VERBOSITY_NONE, "video_out_xv: ignoring broken XV_HUE settings on NVidia cards");
	  } else {
	    xv_check_capability (this, VO_PROP_HUE, attr[k],
			         adaptor_info[adaptor_num].base_id,
			         NULL, NULL, NULL);
	  }
	} else if(!strcmp(attr[k].name, "XV_SATURATION")) {
	  xv_check_capability (this, VO_PROP_SATURATION, attr[k],
			       adaptor_info[adaptor_num].base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(attr[k].name, "XV_BRIGHTNESS")) {
	  xv_check_capability (this, VO_PROP_BRIGHTNESS, attr[k],
			       adaptor_info[adaptor_num].base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(attr[k].name, "XV_CONTRAST")) {
	  xv_check_capability (this, VO_PROP_CONTRAST, attr[k],
			       adaptor_info[adaptor_num].base_id,
			       NULL, NULL, NULL);

	} else if(!strcmp(attr[k].name, "XV_COLORKEY")) {
	  xv_check_capability (this, VO_PROP_COLORKEY, attr[k],
			       adaptor_info[adaptor_num].base_id,
			       "video.device.xv_colorkey",
			       _("video overlay colour key"),
			       _("The colour key is used to tell the graphics card where to "
				 "overlay the video image. Try different values, if you experience "
				 "windows becoming transparent."));

	} else if(!strcmp(attr[k].name, "XV_AUTOPAINT_COLORKEY")) {
	  xv_check_capability (this, VO_PROP_AUTOPAINT_COLORKEY, attr[k],
			       adaptor_info[adaptor_num].base_id,
			       "video.device.xv_autopaint_colorkey",
			       _("autopaint colour key"),
			       _("Make Xv autopaint its colorkey."));

	} else if(!strcmp(attr[k].name, "XV_FILTER")) {
	  int xv_filter;
	  /* This setting is specific to Permedia 2/3 cards. */
	  xv_filter = config->register_range (config, "video.device.xv_filter", 0,
					      attr[k].min_value, attr[k].max_value,
					      _("bilinear scaling mode"),
					      _("Selects the bilinear scaling mode for Permedia cards. "
						"The individual values are:\n\n"
						"Permedia 2\n"
						"0 - disable bilinear filtering\n"
						"1 - enable bilinear filtering\n\n"
						"Permedia 3\n"
						"0 - disable bilinear filtering\n"
						"1 - horizontal linear filtering\n"
						"2 - enable full bilinear filtering"),
					      20, xv_update_XV_FILTER, this);
	  config->update_num(config,"video.device.xv_filter",xv_filter);
	} else if(!strcmp(attr[k].name, "XV_DOUBLE_BUFFER")) {
	  int xv_double_buffer;
	  xv_double_buffer = 
	    config->register_bool (config, "video.device.xv_double_buffer", 1,
	      _("enable double buffering"),
	      _("Double buffering will synchronize the update of the video image to the "
		"repainting of the entire screen (\"vertical retrace\"). This eliminates "
		"flickering and tearing artifacts, but will use more graphics memory."),
	      20, xv_update_XV_DOUBLE_BUFFER, this);
	  config->update_num(config,"video.device.xv_double_buffer",xv_double_buffer);
	}
      }
    }
    XFree(attr);
  }
  else
    xprintf(this->xine, XINE_VERBOSITY_DEBUG, "video_out_xv: no port attributes defined.\n");
  XvFreeAdaptorInfo(adaptor_info);

  /*
   * check supported image formats
   */

  fo = XvListImageFormats(this->display, this->xv_port, (int*)&formats);
  pthread_mutex_unlock (&this->display_mutex);
  
  this->xv_format_yv12 = 0;
  this->xv_format_yuy2 = 0;
  
  for(i = 0; i < formats; i++) {
    lprintf ("Xv image format: 0x%x (%4.4s) %s\n",
	     fo[i].id, (char*)&fo[i].id,
	     (fo[i].format == XvPacked) ? "packed" : "planar");

    if (fo[i].id == XINE_IMGFMT_YV12)  {
      this->xv_format_yv12 = fo[i].id;
      this->capabilities |= VO_CAP_YV12;
      xprintf(this->xine, XINE_VERBOSITY_LOG,
	      _("video_out_xv: this adaptor supports the yv12 format.\n"));
    } else if (fo[i].id == XINE_IMGFMT_YUY2) {
      this->xv_format_yuy2 = fo[i].id;
      this->capabilities |= VO_CAP_YUY2;
      xprintf(this->xine, XINE_VERBOSITY_LOG, 
	      _("video_out_xv: this adaptor supports the yuy2 format.\n"));
    }
  }

  if(fo) {
    pthread_mutex_lock(&this->display_mutex);
    XFree(fo);
    pthread_mutex_unlock(&this->display_mutex);
  }

  /*
   * try to create a shared image
   * to find out if MIT shm really works, using supported format
   */
  pthread_mutex_lock (&this->display_mutex);
  myimage = create_ximage (this, &myshminfo, 100, 100,
			   (this->xv_format_yv12 != 0) ? XINE_IMGFMT_YV12 : XINE_IMGFMT_YUY2);
  dispose_ximage (this, &myshminfo, myimage);
  pthread_mutex_unlock (&this->display_mutex);

  this->use_pitch_alignment = 
    config->register_bool (config, "video.device.xv_pitch_alignment", 0,
			   _("pitch alignment workaround"),
			   _("Some buggy video drivers need a workaround to function properly."),
			   10, xv_update_xv_pitch_alignment, this);

  this->deinterlace_method = 
    config->register_enum (config, "video.output.xv_deinterlace_method", 4,
			   deinterlace_methods,
			   _("deinterlace method (deprecated)"),
			   _("This config setting is deprecated. You should use the new deinterlacing "
			     "post processing settings instead.\n\n"
			     "From the old days of analog television, where the even and odd numbered "
			     "lines of a video frame would be displayed at different times comes the "
			     "idea to increase motion smoothness by also recording the lines at "
			     "different times. This is called \"interlacing\". But unfortunately, "
			     "todays displays show the even and odd numbered lines as one complete frame "
			     "all at the same time (called \"progressive display\"), which results in "
			     "ugly frame errors known as comb artifacts. Software deinterlacing is an "
			     "approach to reduce these artifacts. The individual values are:\n\n"
			     "none\n"
			     "Disables software deinterlacing.\n\n"
			     "bob\n"
			     "Interpolates between the lines for moving parts of the image.\n\n"
			     "weave\n"
			     "Similar to bob, but with a tendency to preserve the full resolution, "
			     "better for high detail in low movement scenes.\n\n"
			     "greedy\n"
			     "Very good adaptive deinterlacer, but needs a lot of CPU power.\n\n"
			     "onefield\n"
			     "Always interpolates and reduces vertical resolution.\n\n"
			     "onefieldxv\n"
			     "Same as onefield, but does the interpolation in hardware.\n\n"
			     "linearblend\n"
			     "Applies a slight vertical blur to remove the comb artifacts. Good results "
			     "with medium CPU usage."),
			   10, xv_update_deinterlace, this);
  this->deinterlace_enabled = 0;

  pthread_mutex_lock (&this->display_mutex);
  if(this->use_colorkey==1) {
    this->xoverlay = x11osd_create (this->xine, this->display, this->screen,
                                    this->drawable, X11OSD_COLORKEY);
    if(this->xoverlay)
      x11osd_colorkey(this->xoverlay, this->colorkey, &this->sc);
  } else {
    this->xoverlay = x11osd_create (this->xine, this->display, this->screen,
                                    this->drawable, X11OSD_SHAPED);
  }
  pthread_mutex_unlock (&this->display_mutex);

  if( this->xoverlay )
    this->capabilities |= VO_CAP_UNSCALED_OVERLAY;

  return &this->vo_driver;
}

/*
 * class functions
 */

static char* get_identifier (video_driver_class_t *this_gen) {
  return "XvNT";
}

static char* get_description (video_driver_class_t *this_gen) {
  return _("xine video output plugin using the MIT X video extension but no X threads");
}

static void dispose_class (video_driver_class_t *this_gen) {
  xv_class_t        *this = (xv_class_t *) this_gen;
  
  free (this);
}

static void *init_class (xine_t *xine, void *visual_gen) {
  xv_class_t        *this = (xv_class_t *) xine_xmalloc (sizeof (xv_class_t));

  this->driver_class.open_plugin     = open_plugin;
  this->driver_class.get_identifier  = get_identifier;
  this->driver_class.get_description = get_description;
  this->driver_class.dispose         = dispose_class;

  this->config                       = xine->config;
  this->xine                         = xine;

  return this;
}

static const vo_info_t vo_info_xv = {
  0,                    /* priority    */
  XINE_VISUAL_TYPE_X11  /* visual type */
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 21, "xvnt", XINE_VERSION_CODE, &vo_info_xv, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
