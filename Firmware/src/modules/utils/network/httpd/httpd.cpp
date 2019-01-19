
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "sha1.h"
#include "base64.h"

#include "OutputStream.h"
#include "main.h"

#include <string>
#include <map>


#define http_get "GET "
#define http_post "POST "
#define http_options "OPTIONS "
#define http_10 "HTTP/1.0"
#define http_11 "HTTP/1.1"
#define http_content_length "Content-Length: "
#define http_cache_control "Cache-Control: "
#define http_no_cache "no-cache"
#define http_index_html "/index.html"
#define http_404_html "/404.html"
#define http_header_preflight "HTTP/1.0 200 OK\r\nAccess-Control-Allow-Methods: POST\r\nAccess-Control-Allow-Headers:X-Filename, Content-Type\r\nAccess-Control-Max-Age: 86400\r\n"
#define http_header_200 "HTTP/1.0 200 OK\r\n"
#define http_header_304 "HTTP/1.0 304 Not Modified\r\nExpires: Thu, 31 Dec 2037 23:55:55 GMT\r\nCache-Control:max-age=315360000\r\nX-Cache: HIT\r\n"
#define http_header_404 "HTTP/1.0 404 Not found\r\n"
#define http_header_503 "HTTP/1.0 503 Failed\r\n"
#define http_header_all "Server: uIP/1.0\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n"
#define http_content_type_plain "Content-Type: text/plain\r\n\r\n"
#define http_content_type_html "Content-Type: text/html\r\n\r\n"
#define http_content_type_css  "Content-Type: text/css\r\n\r\n"
#define http_content_type_text "Content-Type: text/text\r\n\r\n"
#define http_content_type_png  "Content-Type: image/png\r\n\r\n"
#define http_content_type_gif  "Content-Type: image/gif\r\n\r\n"
#define http_content_type_jpg  "Content-Type: image/jpeg\r\n\r\n"
#define http_html ".html"
#define http_css ".css"
#define http_png ".png"
#define http_gif ".gif"
#define http_jpg ".jpg"
#define http_txt ".txt"

using hdr_map_t = std::map<std::string, std::string>;

/* Send the HTML header
 * NETCONN_NOCOPY: our data is const static, so no need to copy it
 */
static bool write_header(struct netconn *conn, const char *hdr)
{
    err_t err = netconn_write(conn, hdr, strlen(hdr), NETCONN_NOCOPY);
    return err == ERR_OK;
}

static bool write_page(struct netconn *conn, const char *file)
{

    err_t err = netconn_write(conn, file, strlen(file), NETCONN_COPY);
    return err == ERR_OK;
}

static err_t websocket_decode(char *buf, uint16_t& len)
{
    // TODO handle larger frames
    unsigned char *data = (unsigned char*) buf;
    u16_t data_len = len;
    if (data != NULL && data_len > 2) {
        if((data[0] & 0x80) == 0) {
            // must have fin bit
            printf("websocket_decode: FIN bit not set\n");
            return ERR_VAL;
        }
        if((data[1] & 0x80) == 0) {
            // must have mask bit
            printf("websocket_decode: MASK bit not set\n");
            return ERR_VAL;
        }
        uint16_t plen= data[1] & 0x7F;
        if(plen >= 126 || plen != data_len-6) {
            printf("websocket_decode: length not < 126 or does not match packet length: %d - %d\n", plen, data_len-6);
            return ERR_VAL;
        }
        uint8_t opcode = data[0] & 0x0F;
        switch (opcode) {
            case 0x01: // text
            case 0x02: // bin
                if (data_len > 6) {
                    data_len -= 6;
                    /* unmask */
                    for (int i = 0; i < data_len; i++){
                        data[i + 6] ^= data[2 + i % 4];
                    }
                    // decoded data
                    memmove(buf, &data[6], data_len);
                    len = data_len;
                }
                break;
            case 0x08: // close
                return ERR_CLSD;
                break;
        }
        return ERR_OK;
    }
    return ERR_VAL;
}

static int websocket_write(struct netconn *conn, const char *data, uint16_t len)
{
    // TODO handle larger packets
    // TODO handle fragmentation, fragment large packets
    if (len > 125) {
        printf("websocket_write: packet too large\n");
        return len;
    }
    unsigned char buf[len + 2];
    buf[0] = 0x80 | 0x02; // binary
    buf[1] = len;
    memcpy(&buf[2], data, len);
    len += 2;
    err_t err = netconn_write(conn, buf, len, NETCONN_COPY);
    if(err != ERR_OK) {
        printf("websocket_write: error writing: %d\n", err);
    }
    return len;
}

static err_t handle_websocket(struct netconn *conn, const char *keystr)
{
    unsigned char encoded_key[32];
    char key[64];
    const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    int len = strlen(keystr);

    /* Concatenate key */
    memcpy(key, keystr, len);
    strcpy(&key[len], WS_GUID); // FIXME needs to be length checked
    printf("Resulting key: %s\n", key);

    unsigned char sha1sum[20];
    mbedtls_sha1((unsigned char *) key, sizeof(WS_GUID) + len - 1, sha1sum);
    /* Base64 encode */
    unsigned int olen;
    mbedtls_base64_encode(NULL, 0, &olen, sha1sum, 20); //get length
    int ok = mbedtls_base64_encode(encoded_key, sizeof(encoded_key), &olen, sha1sum, 20);
    if (ok == 0) {
        encoded_key[olen] = '\0';
        printf("Base64 encoded: %s\n", encoded_key);
    } else {
        printf("base64 encode failed\n");
        return ERR_VAL;
    }

    /*
    HTTP/1.1 101 Switching Protocols
    Upgrade: websocket
    Connection: Upgrade
    Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
    */
    write_header(conn, "HTTP/1.1 101 Switching Protocols\r\n");
    write_header(conn, "Upgrade: websocket\r\n");
    write_header(conn, "Connection: Upgrade\r\n");
    std::string swa("Sec-WebSocket-Accept: ");
    swa.append((char *)encoded_key);
    swa.append("\r\n");
    write_header(conn, swa.c_str());
    write_header(conn, "\r\n");

    // now the socket is a websocket
    // ....
    const u16_t bufsize = 256;
    char buf[bufsize];
    char line[132];
    size_t cnt = 0;
    bool discard = false;
    // FIXME this may need to stay around until command thread is done with it
    OutputStream os([conn](const char *ibuf, size_t ilen) { return websocket_write(conn, ibuf, ilen); });
    struct pbuf *p;
    err_t err;
    // read from connection until it closes
    while ((err = netconn_recv_tcp_pbuf(conn, &p)) == ERR_OK) {
        uint16_t n = pbuf_copy_partial(p, buf, bufsize, 0);
        pbuf_free(p);
        if(n > 0) {
            // decode payload, return in buf with new len
            err = websocket_decode(buf, n);
            if(err == ERR_OK) {
                process_command_buffer(n, buf, &os, line, cnt, discard);
            } else if(err == ERR_CLSD) {
                break;
            } else {
                printf("websocket decode error\n");
                break;
            }
        }
    }

    // exit string
    const char ebuf[] = {0x88, 0x02, 0x03, 0xe8};
    u16_t elen = sizeof(ebuf);
    return netconn_write(conn, ebuf, elen, NETCONN_COPY);
}

static err_t readto(struct netconn *conn, struct pbuf* &cp, std::string match, uint16_t& offset, uint16_t from = 0)
{
    size_t match_len = match.size();
    while(true) {
        if(cp != NULL && cp->tot_len >= from + match_len) {
            // find the required character
            u16_t off = pbuf_memfind(cp, match.c_str(), match_len, from);
            if (off != 0xFFFF) {
                offset = off;
                return ERR_OK;
            }
        }

        // Read more data from the port
        struct pbuf *p = NULL;
        err_t err = netconn_recv_tcp_pbuf(conn, &p);
        if(err != ERR_OK) {
            if(p != NULL) pbuf_free(p);
            return err;
        }

        if(cp == NULL) {
            cp = p;
        } else {
            pbuf_cat(cp, p);
        }
    }
}

#if 1
// for older version of lwip back port these
u8_t pbuf_remove_header(struct pbuf *p, size_t header_size_decrement)
{
    u16_t increment_magnitude;

    if ((p == NULL) || (header_size_decrement > 0xFFFF)) {
        return 1;
    }
    if (header_size_decrement == 0) {
        return 0;
    }

    increment_magnitude = (u16_t)header_size_decrement;
    /* Check that we aren't going to move off the end of the pbuf */
    if(increment_magnitude > p->len) {
        increment_magnitude = p->len;
    }

    /* increase payload pointer (guarded by length check above) */
    p->payload = (u8_t *)p->payload + increment_magnitude;
    /* modify pbuf length fields */
    p->len = (u16_t)(p->len - increment_magnitude);
    p->tot_len = (u16_t)(p->tot_len - increment_magnitude);

    return 0;
}

struct pbuf *pbuf_free_header(struct pbuf *q, u16_t size)
{
    struct pbuf *p = q;
    u16_t free_left = size;
    while (free_left && p) {
        if (free_left >= p->len) {
            struct pbuf *f = p;
            free_left = (u16_t)(free_left - p->len);
            p = p->next;
            f->next = 0;
            pbuf_free(f);
        } else {
            pbuf_remove_header(p, free_left);
            free_left = 0;
        }
    }
    return p;
}
#endif

// returns true if all headers have been read
// sets err if an error is detected
// should be called until all headers have been read, so reading from stream until then
// cp should be left with the last pbuf with residual input data (if any)
static err_t parse_headers(struct netconn *conn, struct pbuf* &cp, std::string& method, std::string& request_target, hdr_map_t& hdrs)
{
    uint16_t offset = 0;

    // find first space
    err_t err = readto(conn, cp, " ", offset);
    if(err != ERR_OK) {
        printf("parse_headers: method: got err: %d\n", err);
        return err;
    }

    // we got the method
    if(offset < 3) {
        printf("parse_headers: method too short\n");
        return ERR_VAL;
    }

    char methbuf[offset + 1];
    uint16_t n = pbuf_copy_partial(cp, methbuf, offset, 0);
    if(n == 0) {
        printf("parse_headers: methbuf pbuf_copy_partial failed\n");
        return ERR_VAL;
    }
    methbuf[n + 1] = '\0';

    printf("parse_headers: got method: %s\n", methbuf);
    method = methbuf;

    // get the request target
    uint16_t last_offset = offset + 1;
    err = readto(conn, cp, " ", offset, last_offset);
    if(err != ERR_OK) {
        printf("parse_headers: request-target: got err: %d\n", err);
        return err;
    }

    uint16_t len = offset - last_offset;
    char reqbuf[len + 1];
    n = pbuf_copy_partial(cp, reqbuf, len, last_offset);
    if(n == 0) {
        printf("parse_headers: reqbuf pbuf_copy_partial failed\n");
        return ERR_VAL;
    }
    reqbuf[n + 1] = '\0';
    request_target = reqbuf;
    printf("parse_headers: reqbuf: %s\n", reqbuf);

    // read the rest of the line (will be the request version HTTP/1.1)
    last_offset = offset + 1;
    err = readto(conn, cp, "\r\n", offset, last_offset);
    if(err != ERR_OK) {
        printf("parse_headers: first line: got err: %d\n", err);
        return err;
    }
    last_offset = offset + 2;

    // now read all the headers upto the crlfcrlf
    // add them to the hdrmap
    while (1) {
        err = readto(conn, cp, "\r\n", offset, last_offset);
        len = offset - last_offset;

        // if zero then it is a blnk line which indicates end of headers
        if(len == 0) {
            // free up pbufs that have been read
            // NOTE real version of pbuf_free_header() will have issues with last_offset being > pbuf->tot_len
            // So:- if(last_offset > cp->tot_len) last_offset= cp->tot_len;
            cp = pbuf_free_header(cp, offset + 1);
            printf("parse_headers: end of headers\n");
            return ERR_OK;
        }

        char hdrbuf[len + 1];
        n = pbuf_copy_partial(cp, hdrbuf, len, last_offset);
        if(n == 0) {
            printf("parse_headers: hdrbuf pbuf_copy_partial failed\n");
            return ERR_VAL;
        }
        hdrbuf[n + 1] = '\0';
        last_offset = offset + 2;

        printf("parse_headers: reading header: %s\n", hdrbuf);
        char *o = strchr(hdrbuf, ':');
        if(o == nullptr || o + 2 >= &hdrbuf[len]) {
            printf("parse_headers: bad header\n");
        } else {
            std::string k(hdrbuf, (size_t)(o - hdrbuf));
            std::string v(o + 2, (size_t)(len - (o - hdrbuf) - 2));
            hdrs[k] = v;
        }
    }
}

/** Serve one HTTP connection accepted in the http thread */
static void http_server_netconn_serve(struct netconn *conn)
{
    err_t err;
    struct pbuf *cp = NULL;
    std::string method;
    std::string request_target;
    hdr_map_t hdrs;

    err = parse_headers(conn, cp, method, request_target, hdrs);
    if(err != ERR_OK) {
        printf("parse_headers failed with err: %d\n", err);
        if(cp != NULL) pbuf_free(cp);

    } else {

        printf("method: %s\n", method.c_str());
        printf("request_target: %s\n", request_target.c_str());
        printf("Headers: \n");
        for(auto& x : hdrs) {
            printf(" %s = %s\n", x.first.c_str(), x.second.c_str());
        }

        printf("bytes left in pbuf: %d, cp= %p\n", cp == NULL ? 0 : cp->tot_len, cp);

        if(cp != NULL) pbuf_free(cp);

        /* look for web connect
            GET / HTTP/1.1
            Host: server.example.com
            Upgrade: websocket
            Connection: Upgrade
            Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
        */

        if(method == "GET" && request_target == "/command") {
            auto i = hdrs.find("Upgrade");
            if(i != hdrs.end() && i->second == "websocket") {
                auto k = hdrs.find("Sec-WebSocket-Key");
                if(k != hdrs.end()) {
                    std::string key = k->second;
                    handle_websocket(conn, key.c_str());
                    return;
                }
            }
            printf("badly formatted websocket request\n");

        } else if(method == "GET" && request_target == "/") {
            write_header(conn, http_header_200);
            write_header(conn, http_content_type_html);
            write_page(conn, "index.html");

        } else if(method == "GET") {
            write_header(conn, http_header_200);
            write_header(conn, http_content_type_html);
            write_page(conn, request_target.c_str());

        } else {
            printf("Unhandled request\n");
        }
    }

}

/** The main function, never returns! */
static void http_server_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t err;
    LWIP_UNUSED_ARG(arg);

    printf("http_server: thread started");

    /* Create a new TCP connection handle */
    /* Bind to port 80 (HTTP) with default IP address */
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, 80);

    /* Put the connection into LISTEN state */
    netconn_listen(conn);

    do {
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK) {
            http_server_netconn_serve(newconn);
            // Close the connection (server closes in HTTP)
            netconn_close(newconn);
            netconn_delete(newconn);
        }
    } while(err == ERR_OK);
    printf("http_server_thread: netconn_accept received error %d, shutting down", err);
    netconn_close(conn);
    netconn_delete(conn);
}

/** Initialize the HTTP server (start its thread) */
void http_server_init(void)
{
    sys_thread_new("http_server_netconn", http_server_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}
