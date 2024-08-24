#include "stdio.h"

#define BINC_AUTOVARNAME_MAXLENGTH 256

typedef struct {
  const char *var_name;
} bin2c_options_t;

int main_bin2c(FILE* outfp, const char *input_pathname, bin2c_options_t opts) {
  static char auto_var_name[BINC_AUTOVARNAME_MAXLENGTH] = {};
  const char *var_name = opts.var_name;

  FILE *infp = fopen(input_pathname, "rb");
  if (!infp) return 1;

  if (!var_name) {
    int i = 0;
    for (const char *ptr = input_pathname; *ptr; ++ptr) {
      if (i >= BINC_AUTOVARNAME_MAXLENGTH - 2) return 1;
      char ch = *ptr;
      if ( !(
        ch > '0' && ch < '9' ||
        ch > 'a' && ch < 'z' ||
        ch > 'A' && ch < 'Z'
      ) ) {
        ch = '_';
      }
      auto_var_name[i++] = ch;
    }
    auto_var_name[i] = 0;
    var_name = (const char *) auto_var_name;
  }

  fprintf(outfp, "unsigned char %s[] = {\n", var_name);

  size_t input_file_length = 0;

  static char buf[512];
  size_t read_len = 0;
  do {
    read_len = fread(buf, 1, 512, infp);
    for (int i = 0; i < read_len; ++i) {
      fprintf(outfp, "0x%02x,", buf[i]);
      if ((i + 1) % 16 == 0) fprintf(outfp, "\n");
    }
    input_file_length += read_len;
  } while(read_len == 512);

  fprintf(outfp, "\n0};\nlong long int %s_len = %lu;\n", var_name, input_file_length);

  fclose(infp);

  return 0;
}

void print_help() {
  fprintf(stderr, "usage: bin2c [-o <output_file>] ( [[-n <var_name>] <input_file>] ... )\n");
}

int main(int argc, const char **argv) {

  const char *opt_output_pathname = NULL;
  const char *opt_var_name = NULL;

  const char **option_target = NULL;

  FILE *outfp = NULL;

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (arg[0] == '-') {
      if (arg[1] && arg[2] == 0) {
        switch(arg[1]) {
          default:
            fprintf(stderr, "error: invalid argument -%c\n", arg[1]);
            print_help();
            return 1;
          case 'h':
            print_help();
            return 0;
          case 'o':
            if (opt_output_pathname) {
              fprintf(stderr, "error: -o cannot be used twice\n");
              print_help();
              return 1;
            }
            option_target = &opt_output_pathname;
            break;
          case 'n':
            option_target = &opt_var_name;
            break;
        }
      } else {
        fprintf(stderr, "error: invalid argument %s\n", arg);
        print_help();
        return 1;
      }
    } else {
      if (option_target) {
        *option_target = arg;
        option_target = NULL;
      } else {
        if (!outfp) {
          if (!opt_output_pathname || (opt_output_pathname[0] == '-' && opt_output_pathname[1] == 0)) {
            outfp = stdout;
          } else {
            outfp = fopen(opt_output_pathname, "wb");
            if (!outfp) {
              fprintf(stderr, "error: cannot open output file\n");
              return 1;
            }
          }
        }

        bin2c_options_t opts = {
          .var_name = opt_var_name
        };

        if (main_bin2c(outfp, arg, opts)) {
          fclose(outfp);
          fprintf(stderr, "error: failed while processing '%s'\n", arg);
          return 1;
        }
      }
    }
  }

  if (!outfp) {
    fprintf(stdout, "error: too few arguments\n");
    print_help();
    return 1;
  }

  if (outfp != stdout) fclose(outfp);

  return 0;
}

