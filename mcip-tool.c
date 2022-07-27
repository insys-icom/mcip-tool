#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/utsname.h>

#include "libmcip.h"
#include "m3_cli.h"

void safefree(void **pp);

/* read from MCIP */
static bool read_from_mcip(int sock, bool listen)
{
    int i = 0;
    int x = 0;
    int sel = 0;
    int length = 0;
    fd_set fds_master, fds_tmp;
    struct timeval tv;
    uint8_t p[1500];

    FD_ZERO(&fds_master);
    FD_SET(sock, &fds_master);
    for(; listen == true; ) {
        FD_ZERO(&fds_tmp);
        fds_tmp = fds_master;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        sel = select(sock + 1, &fds_tmp, NULL, NULL, &tv);
        if(sel > 0) {
            /* read everything from socket */
            if(FD_ISSET(sock, &fds_tmp)) {
                x = read(sock, p + length, 1500 - length);

                if(x > 0) {
                    length += x;
                }
                else if(x == -1) {
                    break;
                }
                else {
                    sleep(1);
                }
            }
        }

        /* check if length of received telegram is already here */
        if (length >= 5) {
            /* check if telegram is already complete */
            if (length >= (p[3] | p[4] << 8)) {
                /* print the received telegram */
                for(i = i; i < length; i++) {
                    printf("%c", p[i]);
                }
                printf("\n");

                listen = false;
            }
        }
    }

    return true;
}

/* init CLI session */
static bool init_cli(struct s_m3_cli **cli)
{
    char *M3_CLI_UDS_SOCKET = "/devices/cli_no_auth/cli.socket";

    /* initialise the CLI, opens the socket and retrieves the prompt */
    *cli = m3_cli_initialise(M3_CLI_UDS_SOCKET, 300);
    if (*cli == NULL) {
        printf("Failed to initialise CLI (%d): %s\n", errno, strerror(errno));
        printf("Maybe the container has not been added to the \"Read/Write\" user group for access the CLI without authentication?");
        return false;
    }

    return true;
}

/* send as SMS via CLI */
static bool send_sms(char *number, char *text, char *modem)
{
    struct s_m3_cli *cli = NULL;
    char *cli_answer = NULL;
    char buffer[1000] = { 0 };

    /* initialise the CLI, opens the socket and retrieves the prompt */
    if (init_cli(&cli) == false) {
        return false;
    }

    /* configure the modem that should be used */
    if (modem != NULL) {
        sprintf(buffer, "help.debug.sms.modem=%s", modem);
    }
    else {
        sprintf(buffer, "help.debug.sms.modem=lte2");
    }
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to set modem to use (%d): %s\n", errno, strerror(errno));
        return false;
    }

    /* configure the recipients phone number */
    sprintf(buffer, "help.debug.sms.recipient=%s", number);
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to set the recipient phone number (%d): %s\n", errno, strerror(errno));
        return false;
    }

    /* configure the SMS text */
    sprintf(buffer, "help.debug.sms.text=-----BEGIN ...-----%s-----END ...-----", text);
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to set the SMS text (%d): %s\n", errno, strerror(errno));
        return false;
    }

    /* send the submit command */
    if (m3_cli_query(cli, "help.debug.sms.submit=1", &cli_answer, 60000) == false) {
        printf("Failed to set the SMS (%d): %s\n", errno, strerror(errno));
        return false;
    }

    printf("SMS sending %s\n", cli_answer);
    safefree((void **) & cli_answer);

    m3_cli_shutdown(&cli);

    return true;
}

/* switch_output */
static bool switch_output(char *output, char *state)
{
    char *M3_CLI_UDS_SOCKET = "/devices/cli_no_auth/cli.socket";
    struct s_m3_cli *cli = NULL;
    char *cli_answer = NULL;
    char buffer[1000] = { 0 };

    /* initialise the CLI, opens the socket and retrieves the prompt */
    cli = m3_cli_initialise(M3_CLI_UDS_SOCKET, 100);
    if (cli == NULL) {
        printf("Failed to initialise CLI (%d): %s\n", errno, strerror(errno));
        printf("Maybe the container has not been added to the \"Read/Write\" user group for access the CLI without authentication?");
        return false;
    }

    /* determine output */
    sprintf(buffer, "help.debug.output.output=%s", output);
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to determine output %s (%d): %s\n", output, errno, strerror(errno));
        return false;
    }

    /* determine state  */
    sprintf(buffer, "help.debug.output.change=%s", state);
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to set state of output %s (%d): %s\n", state, errno, strerror(errno));
        return false;
    }

    /* submit */
    sprintf(buffer, "help.debug.output.submit");
    if (m3_cli_send(cli, buffer) == false) {
        printf("Failed to set the the output %s (%d): %s\n", output, errno, strerror(errno));
        return false;
    }

    printf("Output set: %s to %s\n", output, state);
    safefree((void **) & cli_answer);

    m3_cli_shutdown(&cli);

    return true;
}

/* print all applet names of this multi binary */
static void usage_applets(void)
{
    printf("This tool is an applet of the multi binary \"mcip-tool\".\n"   \
           "The names of the other applets are:\n"                         \
           "  sms-tool     send or receive SMS\n"                          \
           "  get-input    get state of inputs\n"                          \
           "  get-pulses   receive pulses of an input\n"                   \
           "  set-output   set the state of an output \n"                  \
           "  cli-cmd      send a command via CLI\n"                       \
           "  container    start/stop/restart a container\n"               \
           "\n");

    return;
}

/* print help for generic mcip-tool and exit */
static void usage_tool(void)
{
    printf("\nUsage: mcip-tool [OPTIONS]\n"                                                       \
            "Send or receive MCIP messages.\n"                                                    \
            "\n"                                                                                  \
            "  -h, --help            Display this help and exit.\n"                               \
            "  -m  --my-oid value    OID (decimal) of this tool. This is mandatory in order to\n" \
            "                        connect to the MCIP server.\n"                               \
            "  -t  --to-oid value    OID (decimal) to whom the message should be sent. If\n"      \
            "                        omitted, the message will be sent to OID 2 (the router).\n"  \
            "  -l, --listen          Listen for a message, print it on the console and exit.\n"   \
            "  -s, --send \"value\"    Send the <value> to the OID given.\n"                      \
            "  -p, --permanently     Do not exit after receiving an MCIP telegram.\n"             \
            "\n");

    usage_applets();

    exit(0);
}

/* print help for sms-tool, input events and input pulses, then exit */
static void usage(char *tool, char *description)
{
    printf("\nUsage: %s [OPTIONS]\n"                                                              \
            "%s\n"                                                                                \
            "\n"                                                                                  \
            "  -h, --help            Display this help and exit.\n"                               \
            "  -m  --my-oid value    OID (decimal) of this tool. \n"                              \
            "  -p, --permanently     Do not exit after receiving an MCIP telegram.\n"             \
            "                        with --to-oid default.\n"                                    \
            "\n", tool, description);

    exit(0);
}

/* print help for generic mcip-tool and exit */
static void usage_sms(void)
{
    printf("\nUsage: sms-tool [OPTIONS]\n"                                                         \
            "Send or receive SMS.\n"                                                               \
            "\n"                                                                                   \
            "  -h, --help                  Display this help and exit.\n"                          \
            "\n"                                                                                   \
            "Receive SMS:\n"                                                                       \
            "  -l, --listen                Listen for a SMS, print it on the console and exit.\n"  \
            "  -p, --permanently           Exit after receiving an SMS\n"                          \
            "  -m  --my-oid value          OID (decimal) of this tool. This is mandatory for\n"    \
            "                              receiving SMS.\n"                                       \
            "\n"                                                                                   \
            "Send SMS:\n"                                                                          \
            "  -s, --send                  Send an SMS.\n"                                         \
            "  -n, --number \"number\"       Phone number to whom the SMS should be sent.\n"       \
            "  -t, --text \"text\"           SMS text to be sent.\n"                               \
            "  -i, --interface \"interface\" Set the interface (modem) to use for sending SMS.\n"  \
            "\n");

    usage_applets();

    exit(0);
}

/* print help for set-output, then exit */
static void usage_output(char *tool, char *description)
{
    printf("\nUsage: %s [OPTIONS]\n"                                                              \
            "%s\n"                                                                                \
            "\n"                                                                                  \
            "  -h, --help            Display this help and exit.\n"                               \
            "  -o, --output          Output to set. Syntax: <slot>.<output> (e.g. -o 4.1).\n"     \
            "  -s, --state           State of output (open, close).\n"                            \
            "\n", tool, description);

    usage_applets();

    exit(0);
}

/* print help for cli-cmd, then exit */
static void usage_cli(char *tool, char *description)
{
    printf("\nUsage: %s [OPTIONS] <CLI command>\n"                                                \
            "%s\n"                                                                                \
            "\n"                                                                                  \
            "  -h, --help            Display this help and exit.\n"                               \
            "\n", tool, description);

    usage_applets();

    exit(0);
}

/* print help for container, then exit */
static void usage_container(char *tool, char *description)
{
    printf("\nUsage: %s [OPTIONS]\n"                                                                \
            "%s\n"                                                                                  \
            "\n"                                                                                    \
            "  -h, --help            Display this help and exit.\n"                                 \
            "  -n, --name            Name of container to stop/restart; Default is the hostname.\n" \
            "  -0, --stop            Stop a container.\n"                                         \
            "  -1, --start           Start a container.\n"                                         \
            "  -r, --restart         Restart the container.\n"                                      \
            "\n", tool, description);

    usage_applets();

    exit(0);
}

/* read the given parameters for generic mcip-tool */
static bool get_options_tool(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, uint16_t *my_oid, uint16_t *to_oid, bool *listen, char **send, bool *perma)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    if (argc < 2) {
        usage_tool();
    }

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
            case 'm': {
                if (pArg != NULL) {
                    *my_oid = atoi(pArg);
                    if (*my_oid < 2 || *my_oid > 65534) {
                        printf("The given value for my-oid must be in range of 2 to 65534)\n");
                        exit(-EINVAL);
                    }
                }
                break;
            }

            case 't': {
                if (pArg != NULL) {
                    *to_oid = atoi(pArg);
                    if (*to_oid < 1 || *to_oid > 65534) {
                        printf("The given value for my-oid must be in range of 1 to 65534)\n");
                        exit(-EINVAL);
                    }
                }
                break;
            }

            case 'l': {
                *listen = true;
                break;
            }

            case 's': {
                *send = pArg;
                break;
            }

            case 'p': {
                *perma = true;
                break;
            }

            default:
            case 'h': {
                usage_tool();
            }
        }
    }

    return true;
}

/* read the given parameters for input events and input pulses */
static bool get_options(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, uint16_t *my_oid, bool *perma, char *description)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
            case 'm': {
                if (pArg != NULL) {
                    *my_oid = atoi(pArg);
                    if (*my_oid < 2 || *my_oid > 65534) {
                        printf("The given value for my-oid must be in range of 2 to 65534)\n");
                        exit(-EINVAL);
                    }
                }
                break;
            }

            case 'p': {
                *perma = true;
                break;
            }

            default:
            case 'h': {
                usage(argv[0], description);
            }
        }
    }

    return true;
}

/* read the given parameters for output */
static bool get_options_output(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, char **output, char **state, char *description)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
             case 's': {
                *state = pArg;
                break;
            }

             case 'o': {
                *output = pArg;
                break;
            }

            default:
            case 'h': {
                usage_output(argv[0], description);
            }
        }
    }

    return true;
}

/* read the given parameters for sms-tool */
static bool get_options_sms(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, uint16_t *my_oid, bool *perma, bool *send, bool *listen, char **number, char **text, char **modem)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
            case 'm': {
                if (pArg != NULL) {
                    *my_oid = atoi(pArg);
                    if (*my_oid < 2 || *my_oid > 65534) {
                        printf("The given value for my-oid must be in range of 2 to 65534)\n");
                        exit(-EINVAL);
                    }
                }
                break;
            }

            case 'p': {
                *perma = true;
                break;
            }

            case 's': {
                *send = true;
                break;
            }

            case 'l': {
                *listen = true;
                break;
            }

            case 'i': {
                if (pArg != NULL) {
                    *modem = pArg;
                }
                break;
            }

            case 'n': {
                if (pArg != NULL) {
                    *number = pArg;
                }
                break;
            }

            case 't': {
                if (pArg != NULL) {
                    *text = pArg;
                }
                break;
            }

            default:
            case 'h': {
                usage_sms();
            }
        }
    }

    return true;
}

/* read the given parameters for cli-cmd */
static bool get_options_cli(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, char **cmd, char *description)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
            default:
            case 'h': {
                usage_cli(argv[0], description);
                break;
            }
        }
    }

    if (optind < argc) {
        *cmd = argv[optind];
    }

    return true;
}

/* read the given parameters for container */
static bool get_options_container(int argc, char **argv, char *strOpts_tool, struct option *Opts_tool, char **name, int *action, char *description)
{
    int iOpts = 0;
    int c;
    char *pArg = 0;

    while ((c = getopt_long(argc, argv, strOpts_tool, Opts_tool, &iOpts)) != -1) {
        pArg = optarg;
        if (pArg && (pArg[0] == '=')) {
            pArg++;
        }

        switch (c) {
            case 'r': {
                *action = 2; /* restart */
                break;
            }
            case '0': {
                *action = 0; /* stop */
                break;
            }
            case '1': {
                *action = 1; /* start */
                break;
            }
            case 'n': {
                *name = pArg;
                break;
            }
            default:
            case 'h': {
                usage_container(argv[0], description);
                break;
            }
        }
    }

    return true;
}

/* get or send SMS */
static int main_sms_tool(int argc, char **argv)
{
    bool perma = false;
    bool again = true;
    bool send = false;
    bool listen = false;
    char *number;
    char *text;
    char *modem;
    uint16_t my_oid = 3;
    uint8_t p[1500];
    int sock = -1;
    int i = 0;
    int x = 0;
    int sel = 0;
    int length = 0;
    fd_set fds_master, fds_tmp;
    struct timeval tv;
    struct oid_list *my_oids = NULL;
    static char strOpts_sms[] = "hlm:psn:t:i:";
    static struct option Opts_sms[] = {
        { "my-oid",         required_argument,  0, 'm' },
        { "help",           no_argument,        0, 'h' },
        { "permanently",    no_argument,        0, 'p' },
        { "listen",         no_argument,        0, 'l' },
        { "send",           no_argument,        0, 's' },
        { "number",         required_argument,  0, 'n' },
        { "text",           required_argument,  0, 't' },
        { "interface",      required_argument,  0, 'i' },
        { 0,                0,                  0,  0  }
    };

    /* get parameters */
    if (get_options_sms(argc, argv, strOpts_sms, Opts_sms, &my_oid, &perma, &send, &listen, &number, &text, &modem) == false) {
        return -1;
    }

    /* send a SMS */
    if (send == true && number != NULL && text != NULL) {
        return send_sms(number, text, modem);
    }

    /* receive SMS */
    if (listen == true) {
        /* init my OIDs */
        mcip_oid_append(&my_oids, my_oid);

        /* connect to MCIP via UDS (Unix Domain Socket) */
        sock = mcip_uds_register("/devices/mcip.socket", my_oids);
        if(sock == -1) {
            printf("Failed to register to MCIP\n");
            mcip_oids_destroy(my_oids);
            return -1;
        }

        /* read from MCIP */
        FD_ZERO(&fds_master);
        FD_SET(sock, &fds_master);
        do {
            again = true;
            FD_ZERO(&fds_tmp);
            fds_tmp = fds_master;
            tv.tv_sec = 10;
            tv.tv_usec = 0;
            sel = select(sock + 1, &fds_tmp, NULL, NULL, &tv);
            if(sel > 0) {
                /* read everything from socket */
                if(FD_ISSET(sock, &fds_tmp)) {
                    x = read(sock, p + length, 1500 - length);
                    if(x > 0) {
                        length += x;
                    }
                    else {
                        printf("Failed to read from MCIP\n");

                        /* reconnect to MCIP */
                        mcip_uds_deregister(&sock);
                        mcip_oids_destroy(my_oids);

                        /* init my OIDs */
                        mcip_oid_append(&my_oids, my_oid);

                        /* connect to MCIP via UDS (Unix Domain Socket) */
                        sock = mcip_uds_register("/devices/mcip.socket", my_oids);
                        if(sock == -1) {
                            printf("Failed to register to MCIP\n");
                            mcip_oids_destroy(my_oids);
                            return -1;
                        }
                        length = x = sel = 0;
                    }
                }
            }

            /* check if length of received telegram is already here */
            if (length > 7) {
                /* check if telegram is already complete */
                if (length >= (p[3] | p[4] << 8)) {
                    /* print the received telegram */
                    for(i = 7; i < ((p[3] | p[4] << 8) + 5); i++) {
                        printf("%c", p[i]);
                    }
                    printf("\n");
                    again = false;
                }
                length = x = sel = 0;
            }

        }
        while (perma == true || again == true);

        /* deregister */
        mcip_uds_deregister(&sock);

        /* free OID list */
        mcip_oids_destroy(my_oids);
    }

    return 0;
}

/* get input change events or input pulses */
static int get_input(int argc, char **argv, bool pulses, char *description)
{
    bool perma = false;
    bool print = false;
    bool repeat = false;
    uint16_t my_oid = 4;
    uint8_t p[1500];
    int sock = -1;
    int i = 0;
    int x = 0;
    int sel = 0;
    int length = 0;
    fd_set fds_master, fds_tmp;
    struct timeval tv;
    struct oid_list *my_oids = NULL;
    static char strOpts[] = "hm:p";
    static struct option Opts[] = {
        { "my-oid",         required_argument,  0, 'm' },
        { "help",           no_argument,        0, 'h' },
        { "permanently",    no_argument,        0, 'p' },
        { 0,                0,                  0,  0  }
    };

    /* get parameters */
    if (get_options(argc, argv, strOpts, Opts, &my_oid, &perma, description) == false) {
        return -1;
    }

    /* init my OIDs */
    mcip_oid_append(&my_oids, my_oid);

    /* connect to MCIP via UDS (Unix Domain Socket) */
    sock = mcip_uds_register("/devices/mcip.socket", my_oids);
    if(sock == -1) {
        printf("Failed to register to MCIP\n");
        mcip_oids_destroy(my_oids);
        return -1;
    }

    /* read from MCIP */
    FD_ZERO(&fds_master);
    FD_SET(sock, &fds_master);
    do {
        FD_ZERO(&fds_tmp);
        fds_tmp = fds_master;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        sel = select(sock + 1, &fds_tmp, NULL, NULL, &tv);
        if(sel > 0) {
            /* read everything from socket */
            if(FD_ISSET(sock, &fds_tmp)) {
                x = read(sock, p + length, 1500 - length);
                if(x > 0) {
                    length += x;
                }
                else {
                    printf("Failed to read from MCIP\n");

                    /* reconnect to MCIP */
                    mcip_uds_deregister(&sock);
                    mcip_oids_destroy(my_oids);

                    /* init my OIDs */
                    mcip_oid_append(&my_oids, my_oid);

                    /* connect to MCIP via UDS (Unix Domain Socket) */
                    sock = mcip_uds_register("/devices/mcip.socket", my_oids);
                    if(sock == -1) {
                        printf("Failed to register to MCIP\n");
                        mcip_oids_destroy(my_oids);
                        return -1;
                    }
                    length = x = sel = 0;
                }
            }
        }

        /* check if length of received telegram is already here */
        if (length > 7) {
            /* check if telegram is already complete */
            if (length >= (p[3] | p[4] << 8)) {
                /* only print if we should:
                   it is a input change event e.g. 2.1 is now LOW
                   it is a pulse event e.g. 2.1 pulses detected: 1 */
                print = false;
                if (pulses == false && p[11] == 'i') {
                    print = true;

                }
                else if (pulses == true && p[11] == 'p') {
                    print = true;
                }

                /* print the received telegram */
                if (print == true) {
                    for(i = 7; i < ((p[3] | p[4] << 8) + 5); i++) {
                        printf("%c", p[i]);
                    }
                    printf("\n");
                }
            }
            length = x = sel = 0;
        }

        /* do not abort after a wrong event, when the tool should exit after one event */
        if (perma != true && print == false) {
            repeat = true;
        }
        else {
            repeat = false;
        }
    }
    while (perma == true || repeat == true);

    /* deregister */
    mcip_uds_deregister(&sock);

    /* free OID list */
    mcip_oids_destroy(my_oids);

    return 0;
}

/* get input events */
static int main_get_input(int argc, char **argv)
{
    return get_input(argc, argv, false, "Receive input change events.");
}

/* get input pulses */
static int main_get_pulses(int argc, char **argv)
{
    return get_input(argc, argv, true, "Receive input pulses.");
}

/* set output state */
static int set_output(int argc, char **argv, char *description)
{
    char *output;
    char *state;
    static char strOpts[] = "ho:s:";
    static struct option Opts[] = {
        { "help",           no_argument,        0, 'h' },
        { "output",         required_argument,  0, 'o' },
        { "state",          required_argument,  0, 's' },
        { 0,                0,                  0,  0  }
    };

    /* get parameters */
    if (get_options_output(argc, argv, strOpts, Opts, &output, &state, description) == false) {
        return -1;
    }

    switch_output(output, state);

    return 0;
}

/* set output state */
static int main_set_output(int argc, char **argv)
{
    return set_output(argc, argv, "Set output state.");
}

/* the normal mcip-tool operation */
static int main_mcip_tool(int argc, char **argv)
{
    bool listen = 0;
    bool perma = false;
    uint16_t my_oid = 0;
    uint16_t to_oid = 2;
    char *send = NULL;
    int sock = -1;
    struct oid_list *my_oids = NULL;
    char *s = NULL;
    static char strOpts_tool[] = "hm:t:ls:p";
    static struct option Opts_tool[] = {
        { "my-oid",         required_argument,  0, 'm' },
        { "to-oid",         required_argument,  0, 't' },
        { "help",           no_argument,        0, 'h' },
        { "listen",         no_argument,        0, 'l' },
        { "send",           required_argument,  0, 's' },
        { "permanently",    no_argument,        0, 'p' },
        { 0,                0,                  0,  0  }
    };

    /* get parameters */
    if (get_options_tool(argc, argv, strOpts_tool, Opts_tool, &my_oid, &to_oid, &listen, &send, &perma) == false) {
        return -1;
    }

    /* init my OIDs */
    mcip_oid_append(&my_oids, my_oid);

    /* connect to MCIP via UDS (Unix Domain Socket) */
    sock = mcip_uds_register("/devices/mcip.socket", my_oids);
    if(sock == -1) {
        printf("Failed to register to MCIP\n");
        mcip_oids_destroy(my_oids);
        return -1;
    }

    /* send the given string */
    if (send != NULL) {
        s = calloc(1, 2 + 2 + strlen(send));
        s[0] = to_oid & 0x00FF;
        s[1] = (to_oid & 0xFF00) >> 8;
        if (my_oid == 0) {
            strcat(s + 2, send);
            if (mcip_send(sock, MCIP_CMD_WRITE, strlen(send) + 2, s) != 0) {
                printf("Failed to send string to MCIP\n");
            }
        }
        else {
            s[2] = my_oid & 0x00FF;
            s[3] = (my_oid & 0xFF00) >> 8;
            strcat(s + 4, send);
            if (mcip_send(sock, MCIP_CMD_WRITE, strlen(send) + 4, s) != 0) {
                printf("Failed to send string to MCIP\n");
            }
        }

        safefree((void **) &s);
    }

    /* read from MCIP */
    do {
        if (read_from_mcip(sock, listen) == false) {
            printf("Failed to read from MCIP\n");

            /* reconnect to MCIP */
            mcip_uds_deregister(&sock);
            mcip_oids_destroy(my_oids);

            /* init my OIDs */
            mcip_oid_append(&my_oids, my_oid);

            /* connect to MCIP via UDS (Unix Domain Socket) */
            sock = mcip_uds_register("/devices/mcip.socket", my_oids);
            if(sock == -1) {
                printf("Failed to register to MCIP\n");
                mcip_oids_destroy(my_oids);
                return -1;
            }
        }
    }
    while (perma == true);

    /* deregister */
    mcip_uds_deregister(&sock);

    /* free OID list */
    mcip_oids_destroy(my_oids);

    return 0;
}

/* send a cli command and return the answer */
static int main_cli_cmd(int argc, char **argv)
{
    struct s_m3_cli *cli = NULL;
    char *cli_answer = NULL;
    char *cmd;
    static char strOpts[] = "h";
    static struct option Opts[] = {
        { "help",           no_argument,        0, 'h' },
        { 0,                0,                  0,  0  }
    };

    /* get parameters */
    if (get_options_cli(argc, argv, strOpts, Opts, &cmd,
                        "Send a command to the cli and print the answer") == false) {
        return -1;
    }

    if (init_cli(&cli) == false) {
        return -1;
    }

    /* send the command */
    if (cmd != NULL) {
        if (m3_cli_query(cli, cmd, &cli_answer, 6000) == false) {
            printf("Failed to send the command (%d): %s\n", errno, strerror(errno));
            return -1;
        }
    }
    else {
        printf("No command has been given\n");
        return 0;
    }

    if (cli_answer != NULL) {
        printf("%s\n", cli_answer);
    }
    safefree((void **) & cli_answer);

    m3_cli_shutdown(&cli);

    return 0;
}

/* container stop/restart */
static int main_container(int argc, char **argv)
{
    struct s_m3_cli *cli = NULL;
    char *cli_answer = NULL;
    char *name = NULL;
    char *cmd = NULL;
    int action = 2; /* default: restart */
    bool ret = false;
    struct utsname buf;
    static char strOpts[] = "hn:r01";
    static struct option Opts[] = {
        { "help",           no_argument,        0, 'h' },
        { "name",           required_argument,  0, 'n' },
        { "restart",        no_argument,        0, 'r' },
        { "stop",           no_argument,        0, '0' },
        { "start",          no_argument,        0, '1' },
        { 0,                0,                  0,  0  }
    };
    char cmd_stop[] = "help.debug.container_state.state_change=stop";
    char cmd_start[] = "help.debug.container_state.state_change=start";
    char cmd_restart[] = "help.debug.container_state.state_change=restart";
    char cmd_submit[] = "help.debug.container_state.submit";

    /* get parameters */
    if (get_options_container(argc, argv, strOpts, Opts, &name, &action,
                        "Send the command to restart or to stop a container") == false) {
        return -1;
    }

    if (init_cli(&cli) == false) {
        return -1;
    }

    /* the container name has not been given, use the host name of this container */
    if (name == NULL) {
        if (uname(&buf) != 0) {
            printf("Could not get the host name of this container\n");
            return -1;
        }
        cmd = calloc(1, strlen(buf.nodename) + strlen("help.debug.container_state.name=") + 1);
        sprintf(cmd, "help.debug.container_state.name=%s", buf.nodename);
    }
    /* send the name of the container to be stopped/restarted */
    else {
        cmd = calloc(1, strlen(name) + strlen("help.debug.container_state.name=") + 1);
        sprintf(cmd, "help.debug.container_state.name=%s", name);
    }

    if (m3_cli_query(cli, cmd, &cli_answer, 6000) == false) {
        printf("Failed to send the command (%d): %s\n", errno, strerror(errno));
        safefree((void **) &cmd);
        return -1;
    }
    safefree((void **) &cmd);
    safefree((void **) & cli_answer);

    /* send the command restart */
    if (action == 2) {
        ret = m3_cli_query(cli, cmd_restart, &cli_answer, 6000);
    }
    /* send the command restart */
    else if (action == 1) {
        ret = m3_cli_query(cli, cmd_start, &cli_answer, 6000);
    }
    else {
        ret = m3_cli_query(cli, cmd_stop, &cli_answer, 6000);
    }
    if (ret == false) {
        printf("Failed to send the command (%d): %s\n", errno, strerror(errno));
        return -1;
    }
    safefree((void **) & cli_answer);

    /* send the command to execute the restart */
    if (m3_cli_query(cli, cmd_submit, &cli_answer, 6000) == false) {
        printf("Failed to send the command (%d): %s\n", errno, strerror(errno));
        return -1;
    }

    m3_cli_shutdown(&cli);

    return 0;
}


/* tool to send and receive simple MCIP messages */
int main(int argc, char **argv)
{
    char *p;
    char *cmdname = *argv;

    /* remove absolute path, we want only the filename itself */
    if ((p = strrchr(cmdname, '/')) != NULL) {
        cmdname = p + 1;
    }

    /* check what part of this multi binary should run */
    if (strcmp(cmdname, "mcip-tool") == 0) {
        return main_mcip_tool(argc, argv);
    }
    else if (strcmp(cmdname, "get-input") == 0) {
        return main_get_input(argc, argv);
    }
    else if (strcmp(cmdname, "get-pulses") == 0) {
        return main_get_pulses(argc, argv);
    }
    else if (strcmp(cmdname, "sms-tool") == 0) {
        return main_sms_tool(argc, argv);
    }
    else if (strcmp(cmdname, "set-output") == 0) {
        return main_set_output(argc, argv);
    }
    else if (strcmp(cmdname, "cli-cmd") == 0) {
        return main_cli_cmd(argc, argv);
    }
    else if (strcmp(cmdname, "container") == 0) {
        return main_container(argc, argv);
    }

    return 0;
}
