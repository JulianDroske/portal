#ifndef __LIBPORTAL_H__
#define __LIBPORTAL_H__

// #if !defined(LIBPORTAL_PLATFORM_HAS_X11)
// # if defined(__APPLE__)
// #  define LIBPORTAL_PLATFORM_HAS_APPLE
// # elseif defined(_WIN32)
// #  define LIBPORTAL_PLATFORM_HAS_GDI
// # else
// #  define LIBPORTAL_PLATFORM_HAS_X11
// # endif
// #endif

#if !defined(LIBPORTAL_PLATFORM_HAS_X11) && !defined(LIBPORTAL_PLATFORM_HAS_FBDEV)
# error "enable at least a available platform with LIBPORTAL_PLATFORM_HAS_* to compile libportal"
#endif

#include "stdio.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"


// typedef uint32_t libportal_platform_id_t;

typedef enum {
  LIBPORTAL_PLATFORM_ID_X11 = 1,
  LIBPORTAL_PLATFORM_ID_FBDEV,
} libportal_platform_id_t;

typedef enum {
  LP_OK = 0,

  LPE_INVALID_PLATFORM,
  LPE_MALLOC_FAILURE,
  LPE_MMAP_FAILURE,
  LPE_IOCTL_FAILURE,
  LPE_NO_SUCH_FILE,

  LPE_X11_NO_SUCH_DISPLAY,
  LPE_X11_INVALID_WINDOW,
} libportal_error_t;

typedef uint8_t libportal_byte_t;

// typedef struct {
//   const char *name;
//   const char *value;
// } libportal_option_t;

typedef struct {
  libportal_byte_t *data;
  int w;
  int h;
  int bpl;
} libportal_image_t;

typedef struct {
  void *userdata;
  libportal_platform_id_t platform_id;
} libportal_t;

typedef union {
  struct {
    const char *display_name;
    int window_id;
  } x11;
  struct {
    const char *dev_path;
  } fbdev;
} libportal_platform_options_t;

typedef struct {
  libportal_platform_id_t id;
  const char *name;
} libportal_platform_pair_t;

const libportal_platform_pair_t libportal_available_platform_pairs[];
#define LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(name) for(const libportal_platform_pair_t *name = libportal_available_platform_pairs; name->id; ++name)

libportal_error_t libportal_init(libportal_t *portal, libportal_platform_id_t platform_id, libportal_platform_options_t opts);
void libportal_close(libportal_t *portal);

libportal_image_t libportal_fetchimage(libportal_t *portal);
void libportal_destroyimage(libportal_image_t *image);

#endif /* __LIBPORTAL_H__ */



#ifdef LIBPORTAL_IMPL

#ifdef LIBPORTAL_PLATFORM_HAS_X11
/* ref: https://stackoverflow.com/questions/13479975/fastest-method-for-screen-capturing-on-linux */
# include "X11/Xlib.h"
# include "X11/Xutil.h"
# include "X11/Xmd.h"
# include "X11/Xatom.h"

typedef struct {
  Display *display;
  Window window;
} libportal_userdata_x11_t;
#endif /* LIBPORTAL_PLATFORM_HAS_X11 */


#ifdef LIBPORTAL_PLATFORM_HAS_FBDEV
# include "unistd.h"
# include "fcntl.h"
# include "sys/types.h"
# include "sys/ioctl.h"
# include "sys/mman.h"
# include "linux/fb.h"

typedef struct {
  int fd;
  int width;
  int height;
  int bpl;
  long int map_len;
  libportal_byte_t *memory;
} libportal_userdata_fbdev_t;
#endif /* LIBPORTAL_PLATFORM_HAS_FBDEV */


/* libportal */

const libportal_platform_pair_t libportal_available_platform_pairs[] = {
#ifdef LIBPORTAL_PLATFORM_HAS_X11
  { LIBPORTAL_PLATFORM_ID_X11, "x11" },
#endif
#ifdef LIBPORTAL_PLATFORM_HAS_FBDEV
  { LIBPORTAL_PLATFORM_ID_FBDEV, "fbdev" },
#endif
  { 0, NULL }
};


libportal_error_t libportal_init(libportal_t *portal, libportal_platform_id_t platform_id, libportal_platform_options_t opts) {

  portal->platform_id = platform_id;

  // TODO convert opts to map and parse

  switch (platform_id) {

    default: break;

#ifdef LIBPORTAL_PLATFORM_HAS_X11
    case LIBPORTAL_PLATFORM_ID_X11: {
      libportal_error_t error = LP_OK;

      libportal_userdata_x11_t *x11 = malloc(sizeof(libportal_userdata_x11_t));
      if (!x11) return LPE_MALLOC_FAILURE;

      /* nullable */
      const char *display_name = opts.x11.display_name;
      int window_id = opts.x11.window_id;

      Display *display = XOpenDisplay(display_name);
      if (!display) { error = LPE_X11_NO_SUCH_DISPLAY; goto x11_error_l; }
      Window window = window_id? (Window) window_id: XDefaultRootWindow(display);
      if (window <= 0) { error = LPE_X11_INVALID_WINDOW; goto x11_error_l; }

      *x11 = (libportal_userdata_x11_t) {
        .display = display,
        .window = window
      };
      portal->userdata = x11;

      return LP_OK;

      x11_error_l: {
        if (display) XCloseDisplay(display);
        if (x11) free(x11);
        return error;
      }
    } break;
#endif

#ifdef LIBPORTAL_PLATFORM_HAS_FBDEV
    case LIBPORTAL_PLATFORM_ID_FBDEV: {
      libportal_error_t error = LP_OK;

      const char *dev_path = opts.fbdev.dev_path;
      if (!dev_path) dev_path = "/dev/fb0";

      libportal_userdata_fbdev_t *fbdev = NULL;
      int fd = 0;

      fbdev = malloc(sizeof(libportal_userdata_fbdev_t));
      if (!fbdev) { error = LPE_MALLOC_FAILURE; goto fbdev_error_l; }

      fd = open(dev_path, O_RDONLY | O_NONBLOCK);
      if (fd < 0) { error = LPE_NO_SUCH_FILE; goto fbdev_error_l; }

      // struct fb_fix_screeninfo finfo;
      struct fb_var_screeninfo vinfo;

      if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) { error = LPE_IOCTL_FAILURE; goto fbdev_error_l; }

      int width = vinfo.xres;
      int height = vinfo.yres;
      long int fb_len = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

      libportal_byte_t *memory = (libportal_byte_t *) mmap(0, fb_len, PROT_READ, MAP_SHARED, fd, 0);
      if (memory == MAP_FAILED) { error = LPE_MMAP_FAILURE; goto fbdev_error_l; }

      *fbdev = (libportal_userdata_fbdev_t) {
        .fd = fd,
        .width = width,
        .height = height,
        .bpl = vinfo.bits_per_pixel / 8 * width,
        .map_len = fb_len,
        .memory = memory
      };
      portal->userdata = fbdev;

      return LP_OK;

      fbdev_error_l: {
        if (fd) close(fd);
        if (fbdev) free(fbdev);
        return 1;
      }
    } break;
#endif

  }

  return 1;
}


void libportal_close(libportal_t *portal) {
  if (!portal->userdata) return;

  switch (portal->platform_id) {

    default: break;

#ifdef LIBPORTAL_PLATFORM_HAS_X11
    case LIBPORTAL_PLATFORM_ID_X11: {
      libportal_userdata_x11_t *x11 = (libportal_userdata_x11_t *) portal->userdata;
      XCloseDisplay(x11->display);
      free(x11);
    } break;
#endif

#ifdef LIBPORTAL_PLATFORM_HAS_FBDEV
    case LIBPORTAL_PLATFORM_ID_FBDEV: {
      libportal_userdata_fbdev_t *fbdev = (libportal_userdata_fbdev_t *) portal->userdata;
      munmap(fbdev->memory, fbdev->map_len);
      close(fbdev->fd);
    } break;
#endif

  }

  portal->userdata = NULL;
}


libportal_image_t libportal_fetchimage(libportal_t *portal) {

  libportal_image_t image = {};

  switch (portal->platform_id) {

    default: break;

#ifdef LIBPORTAL_PLATFORM_HAS_X11
    case LIBPORTAL_PLATFORM_ID_X11: {
      libportal_userdata_x11_t *x11 = (libportal_userdata_x11_t *) portal->userdata;

      XWindowAttributes attrs = {};
      XGetWindowAttributes(x11->display, x11->window, &attrs);
      XImage *ximage = XGetImage(x11->display, x11->window, 0, 0, attrs.width, attrs.height, AllPlanes, ZPixmap);

      image.w = ximage->width;
      image.h = ximage->height;
      image.bpl = ximage->bytes_per_line;

      size_t image_size = image.h * image.bpl;
      libportal_byte_t *data = (libportal_byte_t *) malloc(image_size);
      if (data) {
        // TODO copy based on bpl
        // memcpy(data, ximage->data, image_size);
        /* converts bgr to rgb, intended with compiler optimization */
        for (int i = 0, n = image_size; i < n; i += 4) {
          libportal_byte_t *dst = data + i;
          libportal_byte_t *src = ximage->data + i;
          dst[0] = src[2];
          dst[1] = src[1];
          dst[2] = src[0];
          dst[3] = src[3];
        }
        image.data = data;
      }

      XDestroyImage(ximage);
    } break;
#endif

#ifdef LIBPORTAL_PLATFORM_HAS_FBDEV
    case LIBPORTAL_PLATFORM_ID_FBDEV: {
      libportal_userdata_fbdev_t *fbdev = (libportal_userdata_fbdev_t *) portal->userdata;

      image.w = fbdev->width;
      image.h = fbdev->height;
      libportal_byte_t *data = (libportal_byte_t *) malloc(fbdev->map_len);

      if (data) {
        // memcpy(data, fbdev->memory, fbdev->map_len);
        /* BGRX to RGBX */
        for (int i = 0, n = fbdev->map_len; i < n; i += 4) {
          libportal_byte_t *dst = data + i;
          libportal_byte_t *src = fbdev->memory + i;
          dst[0] = src[2];
          dst[1] = src[1];
          dst[2] = src[0];
          dst[3] = src[3];
        }
        image.data = data;
      }
    } break;
#endif

  }

  return image;
}


void libportal_destroyimage(libportal_image_t *image) {
  if (image && image->data) {
    free(image->data);
    image->data = NULL;
  }
}

#endif /* LIBPORTAL_IMPL */

