/*
Copyright (c) 2015, Cisco Systems
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "strings.h"
#include "simd.h"

#define MAX_PARAMS 200

#define ARG_FILENAME        0
#define ARG_INTEGER         1
#define ARG_FLOAT           2
#define ARG_NONE            3
#define ARG_INTLIST         4
#define ARG_FLOATLIST       5

typedef struct
{
  struct param_desc
  {
    char *name;
    char *default_string;
    int type;
    void *value;
  } params[MAX_PARAMS];
  int num;
} param_list;

static void add_param_to_list(param_list *list, char *name, char *default_string, int type, void *value)
{
  list->params[list->num].name = name;
  list->params[list->num].default_string = default_string;
  list->params[list->num].type = type;
  list->params[list->num].value = value;
  list->num++;
}

static char **read_config_file(FILE *f, int *num_of_params)
{
  char a[2000];
  char b[10];
  int ret, ret2;
  char **argv;
  int n;

  argv = malloc((MAX_PARAMS+1)*sizeof(char*));
  if (argv == NULL)
    return NULL;

  for (n = 0; n <= MAX_PARAMS; n++)
    argv[n] = NULL;

  n = 1;
  while (n <= MAX_PARAMS)
  {
    /* Check for double quote */
    ret2 = fscanf(f, " %[\"]", b);

    /* There is no double quote -> read next string */
    if (ret2 == 0)
    {
      ret = fscanf(f, " %1999s", a);

      if (ret < 1 || ret == EOF)
        break;

      if (a[0] == ';')
      {
        ret = fscanf(f, "%[^\n]", a);
        continue;
      }
    }
    /* There is double quote -> read a string enclosed in double quotes */
    else
    {
      ret = fscanf(f, "%[^\",\n]", a);

      if (ret < 1 || ret == EOF)
        break;

      ret = fscanf(f, "\""); /* Read trailing double quote */
    }

    argv[n] = malloc((strlen(a)+1)*sizeof(char));

    if (argv[n] == NULL)
    {
      return NULL;
    }
    strcpy(argv[n], a);
    n++;
  }

  *num_of_params = n;
  return argv;
}

static void delete_config_file_params(char **params)
{
  int n;

  for (n = 0; n <= MAX_PARAMS; n++)
  {
    if (params[n] != NULL)
      free(params[n]);
  }

  free(params);
}

static int parse_params(int argc, char **argv, enc_params *params, param_list *list)
{
    int i, j;
    FILE *config;
    char **file_params;
    int num_file_params;

    char *tmp;
    int cnt;
    
    for (i = 1; i < argc; i++)
    {
      /* Check if parameter is in the list of known parameters */
      for (j = 0; j < list->num; j++)
      {
        if (strcmp(argv[i], list->params[j].name) == 0)
          break;
      }
      if (j == list->num)
      {
        fprintf(stderr, "Unknown parameter: %s\n", argv[i]);
        return -1;
      }

      /* Parse parameter value according to its type */
      switch (list->params[j].type)
      {

      /* Parameter value is a filename */
      case ARG_FILENAME:
        i++;
        if (i == argc)
        {
          fprintf(stderr, "No filename found for parameter: %s\n", argv[i-1]);
          return -1;
        }
        if (strcmp(argv[i-1], "-cf") == 0)
        {
          /* Read the config file and parse it recursively */
          config = fopen(argv[i], "r");
          if (config == NULL)
          {
            fprintf(stderr, "Cannot open config file: %s\n", argv[i]);
            return -1;
          }
          file_params = read_config_file(config, &num_file_params);
          fclose(config);
          if (file_params == NULL)
          {
            return -1;
          }
          if (parse_params(num_file_params, file_params, params, list) < 0)
          {
            return -1;
          }
          delete_config_file_params(file_params);
          }
          else
          {
            if (*((char **)list->params[j].value) != NULL)
            {
              free(*((char **)list->params[j].value));
            }
            *((char **)list->params[j].value) = malloc((strlen(argv[i])+1)*sizeof(char));
            if (*((char **)list->params[j].value) == NULL)
            {
              fprintf(stderr, "Memory allocation failed\n");
              return -1;
            }
            strcpy(*((char **)list->params[j].value), argv[i]);
          }
          break;

      /* Parameter value is a comma-separated list of integers */
      case ARG_INTLIST:
        i++;
        if (i == argc)
        {
          fprintf(stderr, "No integer list found for parameter: %s\n", argv[i-1]);
          return -1;
        }
        cnt = 1;
        if ((tmp = strtok(argv[i], ", ")) == NULL)
        {
          fprintf(stderr,"Error reading integer list for parameter: %s\n", argv[i-1]);
          return -1;
        }
        else
        {
          *(((int *)list->params[j].value)+cnt++) = atoi(tmp);
        }
        while ((tmp = strtok(NULL,", ")) != NULL)
        {
           *(((int *)list->params[j].value)+cnt++) = atoi(tmp);
        }
        *(((int *)list->params[j].value)+0) = cnt-1;
        break;
      /* Parameter value is an integer */
      case ARG_INTEGER:
        i++;
        if (i == argc)
        {
          fprintf(stderr, "No value found for parameter: %s\n", argv[i-1]);
          return -1;
        }
        *((int *)list->params[j].value) = atoi(argv[i]);
        break;

      /* Parameter value is a floating point value */
      case ARG_FLOAT:
        i++;
        if (i == argc)
        {
          fprintf(stderr, "No value found for parameter: %s\n", argv[i-1]);
          return -1;
        }
        *((float *)list->params[j].value) = (float)atof(argv[i]);
        break;

      /* Parameter is a flag and does not have value */
      case ARG_NONE:
        *((int *)list->params[j].value) = 1;
        break;
      default:
        break;
      }
    }
    return 0;
}

enc_params *parse_config_params(int argc, char **argv)
{
  FILE *infile;
  param_list list;
  enc_params *params;
  char *default_argv[MAX_PARAMS*2];
  int default_argc;
  int i;

  params = (enc_params *)malloc(sizeof(enc_params));
  if (params == NULL)
  {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  memset(params, 0, sizeof(enc_params));
  memset(&list, 0, sizeof(param_list));
  params->aspectnum = params->aspectden = 1;

  add_param_to_list(&list, "-cf",                   NULL, ARG_FILENAME, NULL);
  add_param_to_list(&list, "-if",                   NULL, ARG_FILENAME, &params->infilestr);
  add_param_to_list(&list, "-ph",                    "0", ARG_INTEGER,  &params->file_headerlen);
  add_param_to_list(&list, "-fh",                    "0", ARG_INTEGER,  &params->frame_headerlen);
  add_param_to_list(&list, "-of",                   NULL, ARG_FILENAME, &params->outfilestr);
  add_param_to_list(&list, "-rf",                   NULL, ARG_FILENAME, &params->reconfilestr);
  add_param_to_list(&list, "-stat",                 NULL, ARG_FILENAME, &params->statfilestr);
  add_param_to_list(&list, "-n",                   "600", ARG_INTEGER,  &params->num_frames);
  add_param_to_list(&list, "-skip",                  "0", ARG_INTEGER,  &params->skip);
  add_param_to_list(&list, "-width",              "1920", ARG_INTEGER,  &params->width);
  add_param_to_list(&list, "-height",             "1080", ARG_INTEGER,  &params->height);
  add_param_to_list(&list, "-qp",                   "32", ARG_INTEGER,  &params->qp);
  add_param_to_list(&list, "-log2_sb_size",          "7", ARG_INTEGER,  &params->log2_sb_size);
  add_param_to_list(&list, "-f",                    "60", ARG_FLOAT,    &params->frame_rate);
  add_param_to_list(&list, "-lambda_coeffI",       "1.0", ARG_FLOAT,    &params->lambda_coeffI);
  add_param_to_list(&list, "-lambda_coeffP",       "1.0", ARG_FLOAT,    &params->lambda_coeffP);
  add_param_to_list(&list, "-lambda_coeffB",       "1.0", ARG_FLOAT,    &params->lambda_coeffB);
  add_param_to_list(&list, "-lambda_coeffB0",      "1.0", ARG_FLOAT,    &params->lambda_coeffB0);
  add_param_to_list(&list, "-lambda_coeffB1",      "1.0", ARG_FLOAT,    &params->lambda_coeffB1);
  add_param_to_list(&list, "-lambda_coeffB2",      "1.0", ARG_FLOAT,    &params->lambda_coeffB2);
  add_param_to_list(&list, "-lambda_coeffB3",      "1.0", ARG_FLOAT,    &params->lambda_coeffB3);
  add_param_to_list(&list, "-early_skip_thr",      "0.0", ARG_FLOAT,    &params->early_skip_thr);
  add_param_to_list(&list, "-enable_tb_split",       "0", ARG_INTEGER,  &params->enable_tb_split);
  add_param_to_list(&list, "-enable_pb_split",       "0", ARG_INTEGER,  &params->enable_pb_split);
  add_param_to_list(&list, "-max_num_ref",           "1", ARG_INTEGER,  &params->max_num_ref);
  add_param_to_list(&list, "-HQperiod",              "1", ARG_INTEGER,  &params->HQperiod);
  add_param_to_list(&list, "-num_reorder_pics",      "0", ARG_INTEGER,  &params->num_reorder_pics);
  add_param_to_list(&list, "-dyadic_coding",         "1", ARG_INTEGER,  &params->dyadic_coding);
  add_param_to_list(&list, "-interp_ref",            "0", ARG_INTEGER,  &params->interp_ref);
  add_param_to_list(&list, "-dqpP",                  "0", ARG_INTEGER,  &params->dqpP);
  add_param_to_list(&list, "-dqpB",                  "0", ARG_INTEGER,  &params->dqpB);
  add_param_to_list(&list, "-dqpB0",                 "0", ARG_INTEGER,  &params->dqpB0);
  add_param_to_list(&list, "-dqpB1",                 "0", ARG_INTEGER,  &params->dqpB1);
  add_param_to_list(&list, "-dqpB2",                 "0", ARG_INTEGER,  &params->dqpB2);
  add_param_to_list(&list, "-dqpB3",                 "0", ARG_INTEGER,  &params->dqpB3);
  add_param_to_list(&list, "-mqpP",                "1.0", ARG_FLOAT,    &params->mqpP);
  add_param_to_list(&list, "-mqpB",                "1.0", ARG_FLOAT,    &params->mqpB);
  add_param_to_list(&list, "-mqpB0",               "1.0", ARG_FLOAT,    &params->mqpB0);
  add_param_to_list(&list, "-mqpB1",               "1.0", ARG_FLOAT,    &params->mqpB1);
  add_param_to_list(&list, "-mqpB2",               "1.0", ARG_FLOAT,    &params->mqpB2);
  add_param_to_list(&list, "-mqpB3",               "1.0", ARG_FLOAT,    &params->mqpB3);
  add_param_to_list(&list, "-dqpI",                  "0", ARG_INTEGER,  &params->dqpI);
  add_param_to_list(&list, "-intra_period",          "0", ARG_INTEGER,  &params->intra_period);
  add_param_to_list(&list, "-intra_rdo",             "0", ARG_INTEGER,  &params->intra_rdo);
  add_param_to_list(&list, "-max_delta_qp",          "0", ARG_INTEGER,  &params->max_delta_qp);
  add_param_to_list(&list, "-delta_qp_step",         "1", ARG_INTEGER,  &params->delta_qp_step);
  add_param_to_list(&list, "-encoder_speed",         "0", ARG_INTEGER,  &params->encoder_speed);
  add_param_to_list(&list, "-sync",                  "0", ARG_INTEGER,  &params->sync);
  add_param_to_list(&list, "-deblocking",            "1", ARG_INTEGER,  &params->deblocking);
#if CDEF
  add_param_to_list(&list, "-cdef",                  "2", ARG_INTEGER,  &params->cdef); //0: off, 1: slow, 2: medium, 3: fast
#endif
  add_param_to_list(&list, "-clpf",                  "0", ARG_INTEGER,  &params->clpf); //0: off, 1: SB-level, 2: frame-level
  add_param_to_list(&list, "-snrcalc",               "1", ARG_INTEGER,  &params->snrcalc);
  add_param_to_list(&list, "-use_block_contexts",    "0", ARG_INTEGER,  &params->use_block_contexts);
  add_param_to_list(&list, "-enable_bipred",         "0", ARG_INTEGER,  &params->enable_bipred); //TODO: enable_bipred=2 means disable SFP, use separate parameter for SFP instead for clarity
  add_param_to_list(&list, "-bitrate",               "0", ARG_INTEGER,  &params->bitrate);
  add_param_to_list(&list, "-max_qp",               "51", ARG_INTEGER,  &params->max_qp);
  add_param_to_list(&list, "-min_qp",                "1", ARG_INTEGER,  &params->min_qp);
  add_param_to_list(&list, "-max_qpI",              "32", ARG_INTEGER,  &params->max_qpI);
  add_param_to_list(&list, "-min_qpI",              "32", ARG_INTEGER,  &params->min_qpI);
  add_param_to_list(&list, "-qmtx",                  "0", ARG_INTEGER,  &params->qmtx);
  add_param_to_list(&list, "-qmtx_offset",           "0", ARG_INTEGER,  &params->qmtx_offset);// qp offset for qmlevel calculation -32 to 31
  add_param_to_list(&list, "-subsample",           "420", ARG_INTEGER,  &params->subsample);
  add_param_to_list(&list, "-max_clpf_strength",     "4", ARG_INTEGER,  &params->max_clpf_strength);
  add_param_to_list(&list, "-enable_cfl_intra",      "1", ARG_INTEGER,  &params->cfl_intra);
  add_param_to_list(&list, "-enable_cfl_inter",      "0", ARG_INTEGER,  &params->cfl_inter);
  add_param_to_list(&list, "-bitdepth",              "8", ARG_INTEGER,  &params->bitdepth);  // Internal bitdepth (8, 10 or 12)
  add_param_to_list(&list, "-frame_bitdepth",        "8", ARG_INTEGER,  &params->frame_bitdepth);  // Bitdepth of frame buffers (8 or 16)
  add_param_to_list(&list, "-input_bitdepth",        "8", ARG_INTEGER,  &params->input_bitdepth);  // Bitdepth of input source (8, 10 or 12)

  /* Generate "argv" and "argc" for default parameters */
  default_argc = 1;
  for (i = 0; i < list.num; i++)
  {
    if (list.params[i].default_string != NULL)
    {
      default_argv[default_argc++] = list.params[i].name;
      default_argv[default_argc++] = list.params[i].default_string;
    }
  }

  /* Parse default parameters */
  parse_params(default_argc, default_argv, params, &list);

  /* Parse parameters from command line and config files */
  if (parse_params(argc, argv, params, &list) < 0)
    return NULL;

  /* Check if input file is y4m and if so use its geometry */
  if ((infile = fopen(params->infilestr, "rb"))) {
    char buf[256];
    int len = (int)fread(buf, 1, sizeof(buf), infile);
    int pos = 10;
    int num, den;
    buf[255] = 0;
    if (!strncmp(buf, "YUV4MPEG2 ", 10)) {
      char *end;
      while (pos < len && buf[pos] != '\n') {
          switch (buf[pos++]) {
          case 'W':
            params->width = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf);
            while (buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'H':
            params->height = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf);
            while (pos < len && buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'F':
            den = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf+1);
            num = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf+1);
            params->frame_rate = (float)den/num;
            while (buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'I':
            if (buf[pos] != 'p') {
              fprintf(stderr, "Only progressive input supported\n");
              return NULL;
            }
            while (pos < len && buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'C':
            if (!strncmp(buf+pos, "mono", 4)) {
              params->subsample = 400;
              pos += 4;
            } else {
              params->subsample = strtol(buf+pos, &end, 10);
              pos = (int)(end-buf);
            }
            if (buf[pos] == 'p') {
              params->input_bitdepth = strtol(buf + ++pos, &end, 10);
              if (params->input_bitdepth > 8)
                params->frame_bitdepth = 16;
            }
            while (pos < len && buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'A':
            params->aspectnum = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf+1);
            params->aspectden = strtol(buf+pos, &end, 10);
            pos = (int)(end-buf);
            while (pos < len && buf[pos] != '\n' && buf[pos++] != ' ');
            break;
          case 'X':
          default:
            while (buf[pos] != ' ' && buf[pos] != '\n' && pos < len)
              pos++;
            break;
          }
        }
        if (strncmp(buf+pos, "\nFRAME\n", 7)) {
          fprintf(stderr, "Corrupt Y4M file\n");
          return NULL;
        }
        params->file_headerlen = pos + 1;
        params->frame_headerlen = 6;
    }
    fclose(infile);
  }
  return params;
}

void delete_config_params(enc_params *params)
{
  if (params->infilestr != NULL)
    free(params->infilestr);

  if (params->outfilestr != NULL)
    free(params->outfilestr);

  if (params->reconfilestr != NULL)
    free(params->reconfilestr);

  if (params->statfilestr != NULL)
    free(params->statfilestr);

  free(params);
}

void check_parameters(enc_params *params)
{
  if(params->num_frames <= 0)
  {
    fatalerror("Number of frames must be positive");
  }
  if (params->width%8 || params->height%8)
  {
    fatalerror("Width and height must be a multiple of 8\n");
  }

  if (params->max_num_ref < 1 || params->max_num_ref > 4)
  {
    fatalerror("This number of max reference frames is not supported \n");
  }

  if(params->max_delta_qp >= 8)
  {
    fatalerror("max_delta_qp too large\n");
  }

  if(params->HQperiod >= MAX_REF_FRAMES)
  {
    fatalerror("HQperiod too large");
  }

  if (params->num_reorder_pics > 0 && params->HQperiod > 1 && (params->HQperiod % (params->num_reorder_pics+1))!=0) {
    fatalerror("Subgop length (num_reorder_pics+1) must divide HQperiod.\n");
  }

  if (params->dyadic_coding) {
    int nrp1 = params->num_reorder_pics+1;
    if (params->num_reorder_pics == 2) {
      params->dyadic_coding = 0;
      printf("Warning: Dyadic coding disabled with num_reorder_pics=2\n");
    }
    else if (nrp1 != (1<<(log2i(nrp1)))) {
      fatalerror("num_reorder_pics+1 must be a power of 2 with dyadic coding.\n");
    }
  }

  if (params->num_reorder_pics > 0 && params->max_num_ref < 2) {
    fatalerror("More than one reference frame required for reordered pictures.\n");
  }

  if (params->intra_period % (params->num_reorder_pics+1)) {
    fatalerror("Intra period must be a multiple of the subgroup size (num_reorder_pics+1).\n");
  }

  if (params->sync && params->encoder_speed<2) {
    fatalerror("Sync requires encoder_speed=2\n");
  }

  if (params->bitrate > 0 && params->num_reorder_pics > 0){
    fatalerror("Current rate control doesn't work with frame reordering\n");
  }

  if (params->log2_sb_size < 6 || params->log2_sb_size > MAX_SB_SIZE) {
    fatalerror("Illegal value for log2_sb_size\n");
  }

  if (params->qmtx && (params->qmtx_offset < -32 || params->qmtx_offset>31)) {
    fatalerror("qmtrx_offset must be a value from -32 to 31\n");
  }

  if (params->interp_ref == 2 && params->dyadic_coding == 0 && params->num_reorder_pics != 2) {
    fatalerror("interp_ref=2 only supported with dyadic coding\n");
  }

  if (params->subsample != 420 && params->subsample != 444 &&
      params->subsample != 422 && params->subsample != 400) {
    fatalerror("Illegal value for subsample.  Only 444, 422, 420 and 400 supported.\n");
  }

  if (params->bitdepth != 8 && params->bitdepth != 10 && params->bitdepth != 12) {
    fatalerror("Illegal value for bitdepth.  Only 8, 10 and 12 supported.\n");
  }

  if (params->input_bitdepth != 8 && params->input_bitdepth != 10 && params->input_bitdepth != 12) {
    fatalerror("Illegal value for input_bitdepth.  Only 8, 10 and 12 supported.\n");
  }

  if (params->bitdepth > 8) {
    params->frame_bitdepth = 16;
  }
}
