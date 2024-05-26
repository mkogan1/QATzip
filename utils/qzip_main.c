/***************************************************************************
 *
 *   BSD LICENSE
 *
 *   Copyright(c) 2007-2024 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/
#include "qzip.h"
#include <sys/capability.h>
#include <linux/capability.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <unistd.h>
#include <sys/resource.h>
#include <grp.h>
#include <stdio.h>
#include <pwd.h>


// MK vvv
static int drop_privileges()
{
  uid_t uid = 167;    // CEPH user
  gid_t gid = 167;    // CEPH group
  gid_t gid_qat = 986;  // QAT group


  struct rlimit rlim;
  int ret = getrlimit(RLIMIT_MEMLOCK, &rlim);
  if (ret == -1) {
    perror("getrlimit");
    //return 1;
  }
  printf(">> Current memory lock limit (soft): %zu bytes\n", rlim.rlim_cur);
  printf(">> Current memory lock limit (hard): %zu bytes\n", rlim.rlim_max);

  // /*
  {
    // Increase memory lock limit (hypothetical value, adjust as needed)
    //rlim.rlim_cur = 1024 * 1024 * 1024; // 1GB
    rlim.rlim_cur = 209715200;
    rlim.rlim_max = 209715200;

    // Set the new limit
    ret = setrlimit(RLIMIT_MEMLOCK, &rlim);
    if (ret == -1) {
      perror("setrlimit");
      //return 1;
    }

    ret = getrlimit(RLIMIT_MEMLOCK, &rlim);
    if (ret == -1) {
      perror("getrlimit");
      //return 1;
    }
    printf("<< Current memory lock limit (soft): %zu bytes\n", rlim.rlim_cur);
    printf("<< Current memory lock limit (hard): %zu bytes\n", rlim.rlim_max);
  }
  // */


  struct passwd *pw;
  pw = getpwuid(uid);
  if (pw == NULL) {
    perror("getpwuid");
    exit(1);
  }
  char *username = pw->pw_name;
  // char *username = "ceph";
  // if (initgroups(username, getgid()) != 0) {
  // if (initgroups(username, 986) != 0) {
  struct group *grp;
  grp = getgrnam("qat");
  if (grp == NULL) {
      perror("getgrnam");
      exit(1);
  }
  // gid_t gid_qat = grp->gr_gid;
  gid_qat = grp->gr_gid;
  if (initgroups(username, gid_qat) != 0) {
          int err = errno;
          printf("unable to initgroups %s , errno=%u, exiting...\n", username, err);
          exit(1);
  }


  if (setgid(gid) != 0) {
    int err = errno;
    printf("unable to setgid %u , errno=%u, exiting...\n", gid, err);
    exit(1);
  }
  if (setuid(uid) != 0) {
    int err = errno;
    printf("unable to setuid %u , errno=%u, exiting...\n", uid, err);
    exit(1);
  }


  gid_t groups[NGROUPS_MAX];
  int num_groups = getgroups(NGROUPS_MAX, groups);
  if (num_groups == -1) {
    perror("getgroups");
    exit(1);
  }
  printf("<< after << Current user groups:\n");
  for (int i = 0; i < num_groups; i++) {
          struct group *grp = getgrgid(groups[i]);
          if (grp != NULL) {
                  printf("  %s = %d \n", grp->gr_name, grp->gr_gid);
          }
  }


  ret = getrlimit(RLIMIT_MEMLOCK, &rlim);
  if (ret == -1) {
    perror("getrlimit");
    //return 1;
  }
  printf("<< Current memory lock limit (soft): %zu bytes\n", rlim.rlim_cur);
  printf("<< Current memory lock limit (hard): %zu bytes\n", rlim.rlim_max);
  return 0;
}
// MK ^^^


int main(int argc, char **argv)
{
    int ret = QZ_OK;
    int arg_count; /* number of files or directories to process */
    g_program_name = qzipBaseName(argv[0]);
    char *out_name = NULL;
    FILE *stream_out = NULL;
    int option_f = 0;
    Qz7zItemList_T *the_list;
    int is_good_7z = 0;
    int is_dir = 0;
    int recursive_mode = 0;
    errno = 0;
    int is_format_set = 0;
    char resolved_path[PATH_MAX];

    while (true) {
        int optc;
        int long_idx = -1;
        char *stop = NULL;

        optc = getopt_long(argc, argv, g_short_opts, g_long_opts, &long_idx);
        if (optc < 0) {
            break;
        }
        switch (optc) {
        case 'd':
            g_decompress = 1;
            break;
        case 'h':
            help();
            exit(OK);
            break;
        case 'k':
            g_keep = 1;
            break;
        case 'V':
            version();
            exit(OK);
            break;
        case 'R':
            recursive_mode = 1;
            break;
        case 'A':
            if (strcmp(optarg, "deflate") == 0) {
                g_params_th.comp_algorithm = QZ_DEFLATE;
            } else if (strcmp(optarg, "lz4") == 0) {
                g_params_th.comp_algorithm = QZ_LZ4;
            } else if (strcmp(optarg, "lz4s") == 0) {
                g_params_th.comp_algorithm = QZ_LZ4s;
            } else if (strcmp(optarg, "zstd") == 0) {
                g_params_th.comp_algorithm = QZ_ZSTD;
            } else {
                QZ_ERROR("Error service arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'H':
            if (strcmp(optarg, "static") == 0) {
                g_params_th.huffman_hdr = QZ_STATIC_HDR;
            } else if (strcmp(optarg, "dynamic") == 0) {
                g_params_th.huffman_hdr = QZ_DYNAMIC_HDR;
            } else {
                QZ_ERROR("Error huffman arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'O':
            if (strcmp(optarg, "gzip") == 0) {
                g_params_th.data_fmt = QZIP_DEFLATE_GZIP;
            } else if (strcmp(optarg, "gzipext") == 0) {
                g_params_th.data_fmt = QZIP_DEFLATE_GZIP_EXT;
            } else if (strcmp(optarg, "7z") == 0) {
                g_params_th.data_fmt = QZIP_DEFLATE_RAW;
            } else if (strcmp(optarg, "deflate_4B") == 0) {
                g_params_th.data_fmt = QZIP_DEFLATE_4B;
            } else if (strcmp(optarg, "lz4") == 0) {
                g_params_th.data_fmt = QZIP_LZ4_FH;
            } else if (strcmp(optarg, "lz4s") == 0) {
                g_params_th.data_fmt = QZIP_LZ4S_BK;
            } else {
                QZ_ERROR("Error gzip header format arg: %s\n", optarg);
                return -1;
            }
            is_format_set = 1;
            break;
        case 'o':
            out_name = optarg;
            break;
        case 'L':
            g_params_th.comp_lvl = QZIP_GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || ERANGE == errno ||
                g_params_th.comp_lvl > QZ_DEFLATE_COMP_LVL_MAXIMUM ||
                g_params_th.comp_lvl <= 0) {
                QZ_ERROR("Error compLevel arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'C':
            g_params_th.hw_buff_sz =
                QZIP_GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || ERANGE == errno ||
                g_params_th.hw_buff_sz > USDM_ALLOC_MAX_SZ / 2) {
                printf("Error chunk size arg: %s\n", optarg);
                return -1;
            }
            break;
        case 'r':
            g_params_th.req_cnt_thrshold =
                QZIP_GET_LOWER_32BITS(strtoul(optarg, &stop, 0));
            if (*stop != '\0' || errno) {
                printf("Error request count threshold: %s\n", optarg);
                return -1;
            }
            break;
        case 'f':
            option_f = 1;
            break;
        case 'P':
            if (strcmp(optarg, "busy") == 0) {
                g_params_th.polling_mode = QZ_BUSY_POLLING;
            } else {
                printf("Error set polling mode: %s\n", optarg);
                return -1;
            }
            break;
        default:
            tryHelp();
        }
    }

    // MK
    printf("drop_privileges() <<< qzip: running as uid=%u, gid=%u\n", getuid(), getgid());
    drop_privileges();
    printf("drop_privileges() >>> qzip: running as uid=%u, gid=%u\n", getuid(), getgid());

    arg_count = argc - optind;
    if (0 == arg_count && isatty(fileno((FILE *)stdin))) {
        help();
        exit(OK);
    }

    if (g_decompress) {
        if (g_params_th.data_fmt == QZIP_LZ4S_BK) {
            QZ_ERROR("Don't support lz4s decompression.\n");
            exit(ERROR);
        }
        g_params_th.direction = QZ_DIR_DECOMPRESS;
    } else {
        g_params_th.direction = QZ_DIR_COMPRESS;
    }

    if (0 == arg_count) {
        if (isatty(fileno((FILE *)stdout)) && 0 == option_f &&
            0 == g_decompress) {
            printf("qzip: compressed data not written to a terminal. "
                   "Use -f to force compression.\n");
            printf("For help, type: qzip -h\n");
        } else {
            if (qatzipSetup(&g_sess, &g_params_th)) {
                fprintf(stderr, "qatzipSetup session failed\n");
                exit(ERROR);
            }
            stream_out = stdout;
            stdout = freopen(NULL, "w", stdout);
            processStream(&g_sess, stdin, stream_out, g_decompress == 0);
        }
    } else if (g_params_th.data_fmt == QZIP_DEFLATE_RAW &&
               !g_decompress) { //compress into 7z
        QZ_DEBUG("going to compress files into 7z archive ...\n");

        if (!out_name) {
            QZ_ERROR("Should use '-o' to specify an output name\n");
            help();
            exit(ERROR);
        }

        if (qatzipSetup(&g_sess, &g_params_th)) {
            fprintf(stderr, "qatzipSetup session failed\n");
            exit(ERROR);
        }

        for (int i = 0, j = optind; i < arg_count; ++i, ++j) {
            if (access(argv[j], F_OK)) {
                QZ_ERROR("%s: No such file or directory\n", argv[j]);
                exit(ERROR);
            }
        }

        the_list = itemListCreate(arg_count, argv);
        if (!the_list) {
            exit(ERROR);
        }
        ret = qz7zCompress(&g_sess, the_list, out_name);
        itemListDestroy(the_list);
    } else {  // decompress from 7z; compress into gz; decompress from gz
        while (optind < argc) {

            /* To avoid CWE-22: Improper Limitation of a Pathname to a Restricted Directory
             * ('Path Traversal') attacks.
             * http://cwe.mitre.org/data/definitions/22.html
             */
            if (!realpath(argv[optind], resolved_path)) {
                QZ_ERROR("%s: No such file or directory\n", argv[optind]);
                exit(ERROR);
            }

            QzSuffix_T  suffix = getSuffix(argv[optind]);

            is_dir = checkDirectory(argv[optind]);
            if (g_decompress && !is_dir) {
                QzSuffixCheckStatus_T check_res = checkSuffix(suffix, is_format_set);
                if (E_CHECK_SUFFIX_UNSUPPORT == check_res) {
                    QZ_ERROR("Error: %s: Wrong suffix. Supported suffix: 7z/gz/lz4.\n",
                             argv[optind]);
                    exit(ERROR);
                }
                if (E_CHECK_SUFFIX_FORMAT_UNMATCH == check_res) {
                    QZ_ERROR("Error: %s: Suffix is not matched with format\n",
                             argv[optind]);
                    exit(ERROR);
                }
            }

            if (qatzipSetup(&g_sess, &g_params_th)) {
                fprintf(stderr, "qatzipSetup session failed\n");
                exit(ERROR);
            }

            if (g_decompress) {
                if (!recursive_mode)  {
                    if (suffix == E_SUFFIX_7Z) {
                        is_good_7z = check7zArchive(argv[optind]);
                        if (is_good_7z < 0) {
                            exit(ERROR);
                        }

                        if (arg_count > 1) {
                            fprintf(stderr, "only support decompressing ONE 7z "
                                    "archive for ONE command!\n");
                            exit(ERROR);
                        }

                        QZ_DEBUG(" this is a 7z archive, "
                                 "going to decompress ... \n");
                        g_params_th.data_fmt = QZIP_DEFLATE_RAW;
                        if (qatzipSetup(&g_sess, &g_params_th)) {
                            fprintf(stderr, "qatzipSetup session  failed\n");
                            exit(ERROR);
                        }
                        ret = qz7zDecompress(&g_sess, argv[optind++]);
                    } else if (suffix == E_SUFFIX_GZ) {
                        processFile(&g_sess, argv[optind++], out_name,
                                    g_decompress == 0);
                    } else if (suffix == E_SUFFIX_LZ4) {
                        if (g_params_th.data_fmt != QZIP_LZ4_FH) {
                            QZ_ERROR("Error: Suffix(.lz4) doesn't match the data format,"
                                     "please confirm the data format\n");
                            exit(ERROR);
                        }
                        processFile(&g_sess, argv[optind++], out_name,
                                    g_decompress == 0);
                    } else {
                        QZ_ERROR("Error: %s: Wrong suffix. Supported suffix: "
                                 "7z/gz/lz4.\n", argv[optind]);
                        exit(ERROR);
                    }
                }  else {
                    processFile(&g_sess, argv[optind++], out_name,
                                g_decompress == 0);
                }

            } else { // compress
                processFile(&g_sess, argv[optind++], out_name,
                            g_decompress == 0);
            }
        }
    }

    if (qatzipClose(&g_sess)) {
        exit(ERROR);
    }

    return ret;
}
