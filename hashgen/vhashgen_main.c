#include <getopt.h>
#include "vglobal.h"

static char srvcname[32] = {0};
static char srvcauth[32] = {0};
static char srvcvndr[32] = {0};
static char srvcdesc[32] = {0};

static int  use_cfgfile = 0;
static char cfgfile[1024];

static int show_help = 0;
static int show_ver  = 0;

#define SRVC_MAGIC_LEN 128
static char service_magic[SRVC_MAGIC_LEN] = {0};

static
struct option long_options[] = {
    {"name",         required_argument,  0,        'n'},
    {"auth",         required_argument,  0,        'a'},
    {"vendor",       required_argument,  0,        'o'},
    {"description",  required_argument,  0,        's'},
    {"conf-file",    required_argument,  0,        'f'},
    {"help",         no_argument,        &show_help, 1},
    {"version",      no_argument,        &show_ver,  1},
    {0, 0, 0, 0}
};

void show_usage(void)
{
    printf("Usage: vhashgen [OPTION...]\n");
    printf("hash magic options (each item less than 32 bytes):\n");
    printf("  -n, --name=SERVICE_NAME       service name, by default with '@moon'\n");
    printf("  -a, --auth=SERVICE_AUTH       service author, by default with '@none'\n");
    printf("  -o, --vendor=SERVICE VENDOR   service vendor, by default with '@kortide corp'\n");
    printf("  -s, --descr=SERVICE_INTRO     brief description of service, by default with '@none'\n");
    printf("\n");
    printf("config options:\n");
    printf("  -f, --conf-file=CONFIGFILE    reference config file.\n");
    printf("\n");
    printf("Help options\n");
    printf("  -h  --help                    Show this help message.\n");
    printf("  -v, --version                 Print version.\n");
    printf("\n");
    return ;
}

void show_version(void)
{
    printf("Version 0.0.1\n");
    return ;
}

static
int parse_optarg(char* buf, int len, char* optarg)
{
    if (strlen(optarg) + 1 >= 32) {
        printf("Too long path for config file (less than:%d)\n", len);
        return -1;
    }
    memset(buf, 0, len);
    strcpy(buf, "@");
    strcat(buf, optarg);
    return 0;
}

int main(int argc, char** argv)
{
    int opt_idx = 0;
    int ret = 0;
    int c = 0;

    srand(time(NULL));

    strcpy(srvcname, "@moon");
    strcpy(srvcauth, "@none");
    strcpy(srvcvndr, "@kortide-corp");
    strcpy(srvcdesc, "@none");

    while(c >= 0) {
        c = getopt_long(argc, argv, "n:a:o:s:f:hv", long_options, &opt_idx);
        if (c < 0) {
            break;
        }
        switch(c) {
        case 0: {
            if (long_options[opt_idx].flag != 0)
                break;
            printf ("option %s", long_options[opt_idx].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        }
        case 'n':
            if (parse_optarg(srvcname, 32, optarg) < 0) {
                printf("Too long for service name (should less than 32bytes\n");
                exit(-1);
            }
            break;

        case 'a':
            if (parse_optarg(srvcauth, 32, optarg) < 0) {
                printf("Too long for service author (should less than 32bytes\n");
                exit(-1);
            }
            break;

        case 'o':
            if (parse_optarg(srvcvndr, 32, optarg) < 0) {
                printf("Too long for service vendor (should less than 32bytes\n");
                exit(-1);
            }
            break;

        case 's':
            if (parse_optarg(srvcdesc, 32, optarg) < 0) {
                printf("Too long for service description (should less than 32bytes\n");
                exit(-1);
            }
            break;

        case 'f':
            use_cfgfile = 1;
            if (strlen(optarg) + 1 >= 1024) {
                printf("Too long path for config file (should less than 1K bytes\n");
                exit(-1);
            }
            memset(cfgfile, 0, 1024);
            strcpy(cfgfile, optarg);
            break;

        case 'h':
            show_help = 1;
            break;
        case 'v':
            show_ver = 1;
            break;
        default:
            printf("Invalid option -%c: Unknown option.\n", (char)c);
            show_usage();
            exit(-1);
        }
    }
    if (optind < argc) {
        printf("Too many arguments.\n");
        show_usage();
        exit(-1);
    }

    if (show_help) {
        show_usage();
        exit(0);
    }
    if (show_ver) {
        show_version();
        exit(0);
    }

    ret += sprintf(service_magic + ret, "%s", srvcname);
    ret += sprintf(service_magic + ret, "%s", srvcauth);
    ret += sprintf(service_magic + ret, "%s", srvcvndr);
    ret += sprintf(service_magic + ret, "%s", srvcdesc);
    ret += sprintf(service_magic + ret, "%s", "@");

    if (ret % 2) {
        unsigned char data = (unsigned char)rand();
        ret += sprintf(service_magic + ret, "%x", (data >> 4));
    }

    while(ret < SRVC_MAGIC_LEN) {
        unsigned char data = (unsigned char)rand();
        ret += sprintf(service_magic + ret, "%x%x", (data>>4), (data & 0xf));
    }

    printf("service hash generated:\n%s\n", service_magic);
    return 0;
}

