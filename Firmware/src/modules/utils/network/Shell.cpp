    #include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"

#include "main.h"
#include "OutputStream.h"

// this needs to stick around as another thread may try to write to it after the connection is closed
static struct netconn *newconn= NULL;
static int write_back(const char *rbuf, size_t len)
{
    if(newconn != NULL) {
        netconn_write(newconn, rbuf, len, NETCONN_COPY);
    }
    return len;
}

static void shell_thread(void *arg)
{
    struct netconn *conn;
    err_t err;
    LWIP_UNUSED_ARG(arg);

    printf("Network: shell thread started\n");

    /* Create a new connection identifier. */
    conn = netconn_new(NETCONN_TCP);

    /* Bind connection to well known telnet port number 23. */
    netconn_bind(conn, NULL, 23);

    /* Tell connection to go into listening mode. */
    netconn_listen(conn);

    const u16_t bufsize=256;
    char buf[bufsize];
    char line[132];
    size_t cnt = 0;
    bool discard = false;
    OutputStream os(write_back);
    //output_streams.push_back(&os);

    while (1) {
        // wait for new connection.
        err = netconn_accept(conn, &newconn);
        printf("DEBUG: shell: accepted new connection %p, %s\n", newconn, lwip_strerr(err));

        /* Process the new connection. */
        if (err == ERR_OK) {
            netconn_write(newconn, "Welcome to the Smoothie Shell\n", 30, NETCONN_COPY);
            struct pbuf *p;
            // read from connection until it closes
            while ((err = netconn_recv_tcp_pbuf(newconn, &p)) == ERR_OK) {
                int n= pbuf_copy_partial(p, buf, bufsize, 0);
                pbuf_free(p);
                if(n > 0) {
                    if(strncmp(buf, "quit\n", 5) == 0) {
                        netconn_write(newconn, "Goodbye!\n", 9, NETCONN_COPY);
                        break;
                    }
                    process_command_buffer(n, buf, &os, line, cnt, discard);
                }
            }
            struct netconn *c= newconn;
            newconn= nullptr;

            /* Close connection and discard connection identifier. */
            netconn_close(c);
            netconn_delete(c);
            printf("DEBUG: shell: recv got err %s, closing\n", lwip_strerr(err));
        }
    }
}

void shell_init(void)
{
    sys_thread_new("shell_thread", shell_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}
