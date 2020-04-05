#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/sockets.h"

#include "main.h"
#include "OutputStream.h"
#include "RingBuffer.h"

#include "FreeRTOS.h"
#include "timers.h"

#if !(LWIP_SOCKET && LWIP_SOCKET_SELECT)
#error LWIP_SOCKET_SELECT and  LWIP_SOCKET needed
#endif
#ifndef LWIP_NETCONN_FULLDUPLEX
#error LWIP_NETCONN_FULLDUPLEX is required for this to work
#endif

#define MAX_SERV 3
#define BUFSIZE 256
#define MAGIC 0x6013D852
struct shell_state_t {
    struct shell_state_t *next;
    int socket;
    struct sockaddr_storage cliaddr;
    socklen_t clilen;
    OutputStream *os;
    char line[132];
    size_t cnt = 0;
    bool discard = false;
    uint32_t magic;
};
using shell_t = struct shell_state_t;

static shell_t *shell_list = nullptr;

// Stores OutputStreams that need to be deleted when done
static RingBuffer<OutputStream*, MAX_SERV*2> gc;

// callback from command thread to write data to the socket
static int write_back(shell_t *p_shell, const char *rbuf, size_t len)
{
    if(p_shell->magic != MAGIC) {
        // we probably went away
        printf("shell: write_back: ERROR magic was bad\n");
        return -1;
    }
    // NOTE must write entire buffer
    size_t sent= 0;
    while(sent < len) {
        int n;
        if ( (n = lwip_write(p_shell->socket, rbuf+sent, len-sent)) < 0) {
            //close_chargen(p_shell);
            printf("shell: write_back: error writing\n");
            p_shell->magic= 1;
            return -1;
        }
        sent += n;
        // need to yield here if not all sent
        if(sent < len) {
            // yield some time
            taskYIELD();
        }
    }
    return len;
}

/**************************************************************
 * Close the socket and remove this shell_t from the list.
 **************************************************************/
static void close_shell(shell_t *p_shell)
{
    shell_t *p_search_shell;
    p_shell->magic= 0; // safety
    // if we delete the OutputStream now and command thread is still outputting stuff we will crash
    // it needs to stick around until the command has completed
    if(p_shell->os->is_done()) {
        printf("shell: releasing output stream: %p\n", p_shell->os);
        delete p_shell->os;
    }else{
        printf("shell: delaying releasing output stream: %p\n", p_shell->os);
        p_shell->os->set_closed();
        gc.push_back(p_shell->os);
    }

    printf("shell: closing shell connection: %d\n", p_shell->socket);
    lwip_close(p_shell->socket);

    // Free shell
    if (shell_list == p_shell) {
        shell_list = p_shell->next;
    } else {
        for (p_search_shell = shell_list; p_search_shell; p_search_shell = p_search_shell->next) {
            if (p_search_shell->next == p_shell) {
                p_search_shell->next = p_shell->next;
                break;
            }
        }
    }
    mem_free(p_shell);
}

// This will delete any OutputStreams that are done
// we need to do this so that we don't crash when an OutputStream is deleted before it is done
static void os_garbage_collector( TimerHandle_t xTimer )
{
    while(!gc.empty()) {
        // we only check the oldest, presuming it will be done before newer ones
        OutputStream *os= gc.peek_front();
        if(os->is_done()) {
            os= gc.pop_front();
            delete os;
            printf("shell: releasing output stream: %p\n", os);
        } else {
            // if this is not done then we presume the newer ones aren't either
            break;
        }
    }
}

static void shell_thread(void *arg)
{
    LWIP_UNUSED_ARG(arg);
    int listenfd;
    struct sockaddr_in shell_saddr;
    fd_set readset;
    fd_set writeset;
    int i, maxfdp1;
    shell_t *p_shell;

    printf("Network: Shell thread started\n");

    // OutputStream garbage collector timer
    TimerHandle_t timer_handle= xTimerCreate("osgarbage", pdMS_TO_TICKS(1000), pdTRUE, nullptr, os_garbage_collector);
    if( xTimerStart( timer_handle, 1000 ) != pdPASS ) {
        printf("shell_thread: ERROR: Failed to start the timer\n");
    }

    lwip_socket_thread_init();

    memset(&shell_saddr, 0, sizeof (shell_saddr));

    /* First acquire our socket for listening for connections */
    listenfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    shell_saddr.sin_family = AF_INET;
    shell_saddr.sin_addr.s_addr = PP_HTONL(INADDR_ANY);
    shell_saddr.sin_port = lwip_htons(23); /* telnet server port */

    LWIP_ASSERT("shell_thread: Socket create failed.", listenfd >= 0);

    if (lwip_bind(listenfd, (struct sockaddr *) &shell_saddr, sizeof (shell_saddr)) == -1) {
        LWIP_ASSERT("shell_thread: Socket bind failed.", 0);
    }

    /* Put socket into listening mode */
    if (lwip_listen(listenfd, MAX_SERV) == -1) {
        LWIP_ASSERT("shell_thread: Listen failed.", 0);
    }

    /* Wait forever for network input: This could be connections or data */
    for (;;) {
        maxfdp1 = listenfd + 1;

        /* Determine what sockets need to be in readset */
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_SET(listenfd, &readset);
        for (p_shell = shell_list; p_shell; p_shell = p_shell->next) {
            if (maxfdp1 < p_shell->socket + 1) {
                maxfdp1 = p_shell->socket + 1;
            }
            FD_SET(p_shell->socket, &readset);
            //FD_SET(p_shell->socket, &writeset); // this will just always be ready
        }

        /* Wait for data or a new connection */
        i = lwip_select(maxfdp1, &readset, &writeset, 0, 0);

        if (i == 0) {
            continue;
        }

        /* At least one descriptor is ready */
        if (FD_ISSET(listenfd, &readset)) {
            /* We have a new connection request */
            /* create a new control block */
            p_shell = (shell_t *) mem_malloc(sizeof(shell_t));
            if(p_shell != nullptr) {
                p_shell->socket = lwip_accept(listenfd, (struct sockaddr *) &p_shell->cliaddr, &p_shell->clilen);
                if (p_shell->socket < 0) {
                    mem_free(p_shell);
                    printf("shell: accept socket error: %d\n", errno);
                } else {
                    /* Keep this shell state in our list */
                    p_shell->next = shell_list;
                    shell_list = p_shell;
                    printf("shell: accepted shell connection: %d\n", p_shell->socket);
                }
                // initialise command buffer state
                p_shell->cnt = 0;
                p_shell->discard = false;
                p_shell->os = new OutputStream([p_shell](const char *ibuf, size_t ilen) { return write_back(p_shell, ibuf, ilen); });
                //output_streams.push_back(p_shell->os);
                p_shell->magic= MAGIC;
                lwip_write(p_shell->socket, "Welcome to the Smoothie Shell\n", 30);

            } else {
                /* No memory to accept connection. Just accept and then close */
                int sock;
                struct sockaddr cliaddr;
                socklen_t clilen;

                sock = lwip_accept(listenfd, &cliaddr, &clilen);
                if (sock >= 0) {
                    lwip_close(sock);
                }
                printf("shell: out of memory on listen\n");
            }
        }

        /* Go through list of connected clients and process data */
        for (p_shell = shell_list; p_shell; p_shell = p_shell->next) {
            if (FD_ISSET(p_shell->socket, &readset)) {
                // This socket is ready for reading.
                char buf[BUFSIZE];
                size_t n = lwip_read(p_shell->socket, buf, BUFSIZE);
                if (n > 0) {
                    if(strncmp(buf, "quit\n", 5) == 0 || strncmp(buf, "quit\r\n", 6) == 0) {
                        lwip_write(p_shell->socket, "Goodbye!\n", 9);
                        close_shell(p_shell);
                        break;
                    }
                    process_command_buffer(n, buf, p_shell->os, p_shell->line, p_shell->cnt, p_shell->discard);
                } else {
                    close_shell(p_shell);
                    break;
                }
            }
        }
    }
    lwip_socket_thread_cleanup();
}

void shell_init(void)
{
    sys_thread_new("shell_thread", shell_thread, NULL, 350, DEFAULT_THREAD_PRIO);
}
