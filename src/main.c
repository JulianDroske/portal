#define WBY_STATIC
#define WBY_USE_FIXED_TYPES
#define WBY_USE_ASSERT
#include "web.h"

#ifdef _WIN32
# include "ws2tcpip.h"
#endif

#include "libportal/libportal.c"

#define WBY_IMPLEMENTATION
#include "web.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define JURT_ENABLE_THREAD
#define JURT_IMPLEMENTATION
#include "jurt.h"

#define ARGPARSE_IMPL
#include "miniargparse.h"

#ifdef PORTAL_ENABLE_GUI
# if defined(LIBPORTAL_PLATFORM_HAS_GDI)
#  define SHLNK_USE_WIN32
# elif defined(LIBPORTAL_PLATFORM_HAS_X11)
#  define SHLNK_USE_X11
# endif
# define NUKSHOW_FONT_SSFN
# define NUKSHOW_IMPLEMENTATION
# include "nukshow.h"
#endif

#include "res_indexhtml.h"

// TODO into option
#define MAIN_MAX_FPS 15
#define MAIN_MIN_DURATION (1000 / MAIN_MAX_FPS)

#define MAIN_LOG_PREFIX "[portal] "

typedef struct {
  int w;
  int h;
  int len;
  void *data;
} _main_jpeg_image_t;

typedef struct {
  int is_mode_help_platforms;
  int is_mode_gui;

  libportal_platform_id_t platform_id;
  const char *webserver_address;
  int webserver_port;

  libportal_platform_options_t libportal_options;
} _main_options_t;

typedef struct {
  int is_stillrunning;
  _main_options_t options;
} _main_context_t;

typedef int (_main_command_function_t)(struct wby_con *connection, libportal_t *portal);

typedef struct {
  const char *name;
  _main_command_function_t *f;
} _main_command_t;

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


int main_serve(_main_context_t *main_ctx) {

  _main_options_t main_options = main_ctx->options;

  if (main_options.is_mode_help_platforms) {
    LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
      printf("%s\n", pair->name);
    }
    return 0;
  }

  if (main_options.platform_id == 0) {
    jl_err("platform is not specified");
    return 1;
  }

  if (!main_options.webserver_address) main_options.webserver_address = "0.0.0.0";
  if (!main_options.webserver_port) main_options.webserver_port = 8888;


  int main_libportal_error_code = 0;
  int main_code = 0;

  libportal_t portal = {};
  struct wby_server server = {};
  void *server_memory = NULL;
  unsigned char *image_data = NULL;

  const char *platform_name = NULL;
  switch (main_options.platform_id) {
    case LIBPORTAL_PLATFORM_ID_X11: platform_name = "X11"; break;
    case LIBPORTAL_PLATFORM_ID_FBDEV: platform_name = "fbdev"; break;
    case LIBPORTAL_PLATFORM_ID_GDI: platform_name = "gdi"; break;
  }

  jl_inf(MAIN_LOG_PREFIX "initializating libportal with platform '%s'", platform_name);
  if (main_libportal_error_code = libportal_init(&portal, main_options.platform_id, &main_options.libportal_options)) goto l_main_end;

  /* start webby server */

  struct wby_config webserver_config = {
    .userdata = &portal,
    .address = main_options.webserver_address,
    .port = main_options.webserver_port,
    .connection_max = 4,
    .request_buffer_size = 2048,
    .io_buffer_size = 8192,
    .dispatch = _main_websever_dispatch,
    .ws_connect = _main_webserver_connect,
    .ws_connected = _main_webserver_connected,
    .ws_frame = _main_webserver_frame,
    .ws_closed = _main_webserver_closed
  };
  wby_size server_memory_size = 0;

  jl_inf(MAIN_LOG_PREFIX "starting server");

  /* start server */
  wby_init(&server, &webserver_config, &server_memory_size);
  server_memory = calloc(server_memory_size, 1);
  wby_start(&server, server_memory);

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
  image_data = malloc(1 * 1024 * 1024 /* 1MB */);
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

  main_ctx->is_stillrunning = 1;

  while (main_ctx->is_stillrunning) {

    int64_t starting_timestamp = jl_getmillitimestamp();

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

    int64_t remaining = MAIN_MIN_DURATION - (jl_getmillitimestamp() - starting_timestamp);
    jl_millisleep(remaining > 0? remaining: 1); // release cpu pressure
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
    if (server_memory) free(server_memory);

    libportal_close(&portal);

    if (main_libportal_error_code) main_code = main_libportal_error_code;

    return main_code;
  }
}

void *main_serve_multithread(void *userdata) {
  _main_context_t *main_ctx = (_main_context_t *) userdata;

  main_serve(main_ctx);
  return NULL;
}


_main_options_t main_parseargs(int argc, const char **argv) {

  /* opt_* needs to be parsed before use */
  const char *opt_platform_name = NULL;

  _main_options_t main_options = {};

  /* construct argparse options */

  jl_dynamicarray_ptr cmd_options_da = jl_dynamicarray_create(sizeof(struct argparse_option));

  struct argparse_option cmd_common_options[] = {
    OPT_HELP(),
    OPT_BOOLEAN('l', "help-platforms", &main_options.is_mode_help_platforms, "list available [platforms] list and exit", NULL, 0, 0),

    OPT_GROUP("global options"),
    OPT_STRING('a', "address", &main_options.webserver_address, "listen on [host = 0.0.0.0] for serving frontend", NULL, 0, 0),
    OPT_INTEGER('p', "port", &main_options.webserver_port, "listen on [port = 8888]", NULL, 0, 0),
    OPT_STRING(0, "platform", &opt_platform_name, "which [platform = (first available)] to use", NULL, 0, 0),

    OPT_END()
  };
  for (struct argparse_option *opt = cmd_common_options; opt->type != ARGPARSE_OPT_END; ++opt)
    jl_dynamicarray_push(cmd_options_da, opt);

  LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
    switch (pair->id) {
      case LIBPORTAL_PLATFORM_ID_X11:
        jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_GROUP("x11 platform options"));
        jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_STRING(0, "xdisplay", &main_options.libportal_options.x11.display_name, "connect to [display]"));
        jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_INTEGER(0, "xwid", &main_options.libportal_options.x11.window_id, "target window with [id]"));
        break;
      case LIBPORTAL_PLATFORM_ID_FBDEV:
        jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_GROUP("fbdev platform options"));
        jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_STRING(0, "fbdev", &main_options.libportal_options.fbdev.dev_path, "read pixels from [filepath]"));
        break;
    }
  }

  jl_dynamicarray_push(cmd_options_da, &(struct argparse_option) OPT_END());

  struct argparse argparse = {};
  const char *const argparse_usage = "portal [options]";
  argparse_init(&argparse, jl_dynamicarray_rawdata(cmd_options_da), &argparse_usage, 0);
  argparse_describe(&argparse, "share your desktop on local network with ease", NULL);
  argparse_parse(&argparse, argc, argv);

  jl_dynamicarray_free(cmd_options_da);

  LIBPORTAL_AVAILABLE_PLATFORMS_FOREACH(pair) {
    if (!opt_platform_name || !strcmp(opt_platform_name, pair->name)) {
      main_options.platform_id = pair->id;
      break;
    }
  }

  return main_options;
}



_main_context_t main_ctx = { };

int main(int argc, const char **argv) {

  _main_options_t main_options = main_parseargs(argc, argv);
  main_ctx.options = main_options;

#ifdef PORTAL_ENABLE_GUI

  shlnk_backend_parameters_t shlnk_params = {
    .title = "portal"
  };
  shlnk_context_t* shlnk_ctx = shlnk_init(shlnk_params);
  while (shlnk_stillrunning(shlnk_ctx)) {
    if (shlnk_begin_mainwindow(shlnk_ctx)) {
      shlnkproject_on_paint(shlnk_ctx, shlnk_ctx->ctx);
    }
    shlnk_end_mainwindow(shlnk_ctx);

    shlnk_draw(shlnk_ctx);
    shlnk_waitfps(shlnk_ctx, 30);
  }
  shlnk_end(shlnk_ctx);
  shlnkproject_on_deinit(shlnk_ctx, shlnk_ctx->ctx);
  return 0;

#else

  int ret = main_serve(&main_ctx);
  return ret;

#endif

}


#ifdef PORTAL_ENABLE_GUI

SHLNKPROJECT_DEFINE_FUNC(main, shlnk_ctx, ctx) {

  const int is_server_running = main_ctx.is_stillrunning;

  const char *string_mainbutton = is_server_running?
    "Stop":
    "Start";
  const char *string_serverstatus = is_server_running?
    "Server is running":
    "Server is not running";

  float font_size = shlnk_font_get_size(shlnk_ctx);
  static char input_ipaddr_buffer[16] = {};
  static int input_ipaddr_len = 0;
  static char input_port_buffer[8] = {};
  static int input_port_len = 0;

  const int edit_flags = is_server_running? NK_EDIT_READ_ONLY: NK_EDIT_FIELD;

  shlnk_font_set_size(shlnk_ctx, font_size * 4 / 5);

  nk_layout_row(ctx, NK_DYNAMIC, 0, 2, (float[]) { 0.75f, 0.25f });
  nk_label(ctx, "Address (defaults: 0.0.0.0)", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
  nk_label(ctx, "Port (defaults: 8888)", NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_BOTTOM);
  shlnk_font_set_size(shlnk_ctx, font_size);
  nk_edit_string(ctx, edit_flags, input_ipaddr_buffer, &input_ipaddr_len, 16, NULL);
  nk_edit_string(ctx, edit_flags, input_port_buffer, &input_port_len, 8, NULL);

  nk_layout_row_static(ctx, font_size, 0, 1);
  nk_spacer(ctx);

  nk_layout_row(ctx, NK_DYNAMIC, 0, 2, (float[]) { 0.1f, 0.8f, 0.1f });
  nk_spacer(ctx);
  int ev_server_toggle = nk_button_label(ctx, string_mainbutton);
  nk_spacer(ctx);

  nk_layout_row_static(ctx, font_size, 0, 1);
  nk_spacer(ctx);

  nk_layout_row_dynamic(ctx, 0, 1);
  nk_label(ctx, string_serverstatus, NK_TEXT_CENTERED);

  if (ev_server_toggle) {
    if (is_server_running) {
      main_ctx.is_stillrunning = 0;
    } else {
      jl_createthread(main_serve_multithread, &main_ctx, 0);
    }
  }
}

SHLNKPROJECT_SET_ENTRY_PAGE(main);

#endif

