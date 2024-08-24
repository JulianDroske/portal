/* for testing only */
#define LIBPORTAL_PLATFORM_USE_X11
#define LIBPORTAL_PLATFORM_USE_FBDEV

#define WBY_STATIC
#define WBY_USE_FIXED_TYPES
#define WBY_USE_ASSERT
#include "web.h"

#include "jurt.h"

#ifdef _WIN32
# include "ws2tcpip.h"
#endif

#include "libportal/libportal.c"

#define WBY_IMPLEMENTATION
#include "web.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define JURT_IMPLEMENTATION
#include "jurt.h"

#define ARGPARSE_IMPL
#include "miniargparse.h"

#include "res_indexhtml.h"


#define MAIN_LOG_PREFIX "[portal-web] "

typedef int (_main_command_function_t)(struct wby_con *connection, libportal_t *portal);

typedef struct {
  const char *name;
  _main_command_function_t *f;
} _main_command_t;

typedef struct {
  int w;
  int h;
  int len;
  void *data;
} _main_jpeg_image_t;

// int _main_command_fetchimage(struct wby_con *connection, libportal_t *portal) {
//   wby_frame_begin(connection, WBY_WSOP_BINARY_FRAME);

//   _main_image_used = 1;
//   wby_printf(connection, "%llu:%d:%d:", jl_getmillitimestamp(), _main_current_image_width, _main_current_image_height);
//   wby_write(connection, _main_current_image_data, _main_current_image_len);
//   wby_frame_end(connection);
//   return 0;
// }

_main_command_t _main_commands[] = {
  // { "fetchimage:", _main_command_fetchimage },
  { NULL, NULL }
};


int _main_websever_dispatch(struct wby_con *connection, void *userdata) {
  const char *req_path = connection->request.uri;

  if (req_path[0] == '/' && !req_path[1]) req_path = "/index.html";

  if (!strcmp("/index.html", req_path)) {
    struct wby_header headers[] = {
      { "Content-Type", "text/html" },
    };

    wby_response_begin(connection, 200, indexhtml_len, headers, 1);
    wby_write(connection, indexhtml, indexhtml_len);
    wby_response_end(connection);

    return 0;
  }

  return 1;
}

int _main_webserver_connect(struct wby_con *connection, void *userdata) {
  if (!strcmp(connection->request.uri, "/live")) return 0;
  else return 1;
}

void _main_webserver_connected(struct wby_con *connection, void *userdata) { }

int _main_webserver_frame(struct wby_con *connection, const struct wby_frame *frame, void *userdata) {
  libportal_t *portal = (libportal_t *) userdata;

  static unsigned char frame_buf[1024];
  int frame_len = frame->payload_length;
  if (frame_len > 1024) return 1;
  if (wby_read(connection, frame_buf, frame_len)) return 1;

  _main_command_function_t *func = NULL;

  // for (int i = 0; i < _main_commands_count; ++i) {
  //   _main_command_t *command = &_main_commands[i];
  for (_main_command_t *command = _main_commands; command->name; ++command) {
    int name_len = strlen(command->name);
    if (frame_len >= name_len && !strncmp(frame_buf, command->name, name_len)) {
      return command->f(connection, portal);
    }
  }

  return 0;
}

void _main_webserver_closed(struct wby_con *connection, void *userdata) { }

void _main_jpeg_image_writer(void *context, void *data, int size) {
  _main_jpeg_image_t *image = (_main_jpeg_image_t *) context;
  memcpy(image->data + image->len, data, size);
  image->len += size;
}

int main(int argc, const char **argv) {

  const char *webserver_address = "0.0.0.0";
  int webserver_port = 8888;

  int mode_list_platforms = 0;

  /* opt_* needs to be parsed before use */
  const char *opt_platform_name = NULL;

  libportal_platform_options_t libportal_options = {};


  /* construct argparse options */

  jlda_ptr cmd_options_da = jlda_create(sizeof(struct argparse_option));

  struct argparse_option cmd_common_options[] = {
    OPT_HELP(),
    OPT_BOOLEAN('l', "list-platforms", &mode_list_platforms, "list available [platforms] list and exit", NULL, 0, 0),

    OPT_GROUP("portal-web options"),
    OPT_STRING('a', "address", &webserver_address, "listen on [host = 0.0.0.0] for serving frontend", NULL, 0, 0),
    OPT_INTEGER('p', "port", &webserver_port, "listen on [port = 8888]", NULL, 0, 0),
    OPT_STRING(0, "platform", &opt_platform_name, "which [platform = (first available)] to use", NULL, 0, 0),
    OPT_END()
  };
  for (struct argparse_option *opt = cmd_common_options; opt->type != ARGPARSE_OPT_END; ++opt)
    jlda_push(cmd_options_da, opt);

  LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
    switch (pair->id) {
      case LIBPORTAL_PLATFORM_ID_X11:
        jlda_push(cmd_options_da, &(struct argparse_option) OPT_GROUP("x11 platform options"));
        jlda_push(cmd_options_da, &(struct argparse_option) OPT_STRING(0, "xdisplay", &libportal_options.x11.display_name, "connect to [display]"));
        jlda_push(cmd_options_da, &(struct argparse_option) OPT_INTEGER(0, "xwid", &libportal_options.x11.window_id, "target window with [id]"));
        break;
      case LIBPORTAL_PLATFORM_ID_FBDEV:
        jlda_push(cmd_options_da, &(struct argparse_option) OPT_GROUP("fbdev platform options"));
        jlda_push(cmd_options_da, &(struct argparse_option) OPT_STRING(0, "fbdev", &libportal_options.fbdev.dev_path, "read pixels from [filepath]"));
        break;
    }
  }

  jlda_push(cmd_options_da, &(struct argparse_option) OPT_END());

  struct argparse argparse = {};
  const char *const argparse_usage = "portal [options]";
  argparse_init(&argparse, jlda_rawdata(cmd_options_da), &argparse_usage, 0);
  argparse_describe(&argparse, "share your desktop on local network with ease", NULL);
  argparse_parse(&argparse, argc, argv);

  jlda_free(cmd_options_da);

  const char *platform_name;
  int platform_id = 0;
  LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
    if (!opt_platform_name || !strcmp(opt_platform_name, pair->name)) {
      platform_id = pair->id;
      platform_name = pair->name;
      break;
    }
  }

  /* main */

  if (mode_list_platforms) {
    LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
      printf("%s\n", pair->name);
    }
    return 0;
  }

  if (platform_id == 0) {
    jl_err("no such platform");
    return 1;
  }

  int main_libportal_error_code = 0;
  int main_code = 0;

  libportal_t portal = {};
  struct wby_server server = {};
  void *memory = NULL;
  unsigned char *image_data = NULL;

  jl_inf(MAIN_LOG_PREFIX "initializating libportal with platform '%s'", platform_name);
  if (main_libportal_error_code = libportal_init(&portal, platform_id, libportal_options)) goto l_main_end;

  /* start webby server */

  struct wby_config webserver_config = {
    .userdata = &portal,
    .address = webserver_address,
    .port = webserver_port,
    .connection_max = 4,
    .request_buffer_size = 2048,
    .io_buffer_size = 8192,
    .dispatch = _main_websever_dispatch,
    .ws_connect = _main_webserver_connect,
    .ws_connected = _main_webserver_connected,
    .ws_frame = _main_webserver_frame,
    .ws_closed = _main_webserver_closed
  };
  wby_size memory_size = 0;

  jl_inf(MAIN_LOG_PREFIX "starting server");

  /* start server */
  wby_init(&server, &webserver_config, &memory_size);
  memory = calloc(memory_size, 1);
  wby_start(&server, memory);

  /* get actual binded port */
  struct sockaddr_in sockin = {};
  socklen_t sockaddrlen = sizeof(sockin);
  if (getsockname(server.socket, (struct sockaddr *) &sockin, &sockaddrlen) == -1) {
    main_code = 1;
    jl_err("failed to get actual binded port");
    goto l_main_end;
  }
  unsigned short port = ntohs(sockin.sin_port);

  jl_inf(MAIN_LOG_PREFIX "server started on port %u", port);

  /* main process */

  // TODO check result image out of range
  image_data = malloc(1024 * 1024 * 1024 /* 1MB */);
  if (!image_data) {
    main_code = 1;
    jl_err("failed to alloc data for storing result image");
    goto l_main_end;
  }

  _main_jpeg_image_t result_jpeg_image = {
    .w = 0,
    .h = 0,
    .len = 0,
    .data = image_data,
  };

  while (1) {
    if (server.con_count > 0) {
      libportal_image_t image = libportal_fetchimage(&portal);
      result_jpeg_image.w = image.w;
      result_jpeg_image.h = image.h;
      result_jpeg_image.len = 0;
      stbi_write_jpg_to_func(_main_jpeg_image_writer, &result_jpeg_image, image.w, image.h, 4, image.data, 20);
      libportal_destroyimage(&image);
    }

    for(int i = 0; i < server.con_count; ++i) {
      struct wby_connection *con = &server.con[i];
      if (con->state & WBY_CON_STATE_WEBSOCKET) {
        struct wby_con *connection = &con->public_data;

        wby_frame_begin(connection, WBY_WSOP_BINARY_FRAME);
        wby_printf(connection, "%llu:%d:%d:", jl_getmillitimestamp(), result_jpeg_image.w, result_jpeg_image.h);
        wby_write(connection, result_jpeg_image.data, result_jpeg_image.len);
        wby_frame_end(connection);
      }
    }

    wby_update(&server);

    jl_millisleep(1000 / 15); // release cpu pressure
  }

  l_main_end: {
    const char *msg = NULL;
    switch (main_libportal_error_code) {
      default:
        jl_err(MAIN_LOG_PREFIX "crashed with unknown error code %d", main_libportal_error_code);
        break;
      case LP_OK: break;
      case LPE_INVALID_PLATFORM: msg = "invalid platform"; break;
      case LPE_MALLOC_FAILURE: msg = "malloc() failure"; break;
      case LPE_MMAP_FAILURE: msg = "mmap() failure"; break;
      case LPE_IOCTL_FAILURE: msg = "ioctl() failure"; break;
      case LPE_NO_SUCH_FILE: msg = "no such file"; break;
      case LPE_X11_NO_SUCH_DISPLAY: msg = "cannot open display"; break;
      case LPE_X11_INVALID_WINDOW: msg = "invalid window id"; break;
    }
    if (msg) jl_err(MAIN_LOG_PREFIX "%s", msg);

    /* end */

    if (image_data) free(image_data);

    wby_stop(&server);
    if (memory) free(memory);

    libportal_close(&portal);

    if (main_libportal_error_code) main_code = main_libportal_error_code;

    return main_code;
  }
}

