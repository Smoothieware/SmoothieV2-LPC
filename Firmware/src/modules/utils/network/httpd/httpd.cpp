
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "sha1.h"
#include "base64.h"

#include "OutputStream.h"
#include "main.h"

#include <string>
#include <map>


#define http_content_length "Content-Length: "
#define http_cache_control "Cache-Control: "
#define http_no_cache "no-cache"
#define http_404_html "/404.html"
#define http_header_200 "HTTP/1.1 200 OK\r\n"
#define http_header_304 "HTTP/1.1 304 Not Modified\r\nExpires: Thu, 31 Dec 2037 23:55:55 GMT\r\nCache-Control:max-age=315360000\r\nX-Cache: HIT\r\n"
#define http_header_400 "HTTP/1.1 400 Bad Request\r\n"
#define http_header_404 "HTTP/1.1 404 Not found\r\n"
#define http_header_503 "HTTP/1.1 503 Failed\r\n"
#define http_content_type_plain "Content-Type: text/plain\r\n\r\n"
#define http_content_type_html "Content-Type: text/html\r\n\r\n"
#define http_content_type_css  "Content-Type: text/css\r\n\r\n"
#define http_content_type_text "Content-Type: text/text\r\n\r\n"
#define http_content_type_png  "Content-Type: image/png\r\n\r\n"
#define http_content_type_gif  "Content-Type: image/gif\r\n\r\n"
#define http_content_type_jpg  "Content-Type: image/jpeg\r\n\r\n"
#define http_content_type_js  "Content-Type: application/javascript\r\n\r\n"
#define http_html ".html"
#define http_css ".css"
#define http_png ".png"
#define http_gif ".gif"
#define http_jpg ".jpg"
#define http_txt ".txt"
#define http_js ".js"

using hdr_map_t = std::map<std::string, std::string>;

// TODO make this configurabe in config
static const char *webdir = "/sd/www";

/* Send the HTML header
 * NETCONN_NOCOPY: our data is const static, so no need to copy it
 */
static bool write_header(struct netconn *conn, const char *hdr)
{
    err_t err = netconn_write(conn, hdr, strlen(hdr), NETCONN_NOCOPY);
    return err == ERR_OK;
}

// construct path and test if file exists
static std::string make_path(const char *fn)
{
    std::string path(webdir);
    path.append(fn);
    FILE *fd = fopen(path.c_str(), "r");
    if (fd == NULL) {
        return "";
    }
    fclose(fd);
    return path;
}

static bool write_page(struct netconn *conn, const char *file)
{
    FILE *fd = fopen(file, "r");
    if (fd == NULL) {
        printf("write_page: Failed to open: %s\n", file);
        return false;
    }

    size_t bufsize= 2000;
    char *buf= (char *)malloc(bufsize);
    if(buf == NULL) {
        printf("write_page: out of memory\n");
        fclose(fd);
        return false;
    }

    while(!feof(fd)) {
        int len = fread(buf, 1, bufsize, fd);
        if (len <= 0) {
            break;

        } else {
            err_t err = netconn_write(conn, buf, len, NETCONN_COPY);
            if(err != ERR_OK) {
                printf("write_page: got write error: %d\n", err);
                break;
            }
        }
    }
    fclose(fd);
    free(buf);
    return true;
}

#if 0
// for older version of lwip (1.4.1) back port these
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

// Read data until we have at least len bytes available.
// conn is the connection
// cp is a pbuf chain containing read bytes so far
// len is the minimum amount of data we need to have read
// NOTE that there may be excess data left in the pbuf chain
// usage: one would call pbuf_copy_partial() after this returns to get the len bytes of data
//        then call pbuf_free_header(cp, len) to free up the pbufs that had that data
static err_t read_len_bytes(struct netconn *conn, struct pbuf* &cp, uint16_t len)
{
    while(true) {
        if(cp != NULL && cp->tot_len >= len) {
            return ERR_OK;
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

class WebsocketState
{

public:
    WebsocketState(struct netconn *c) : conn(c) {}
    ~WebsocketState() { if(p != NULL) pbuf_free(p); }

    struct netconn *conn;
    struct pbuf *p {nullptr};
    bool complete{false};
};

// Read data from a websocket and decode it.
// buf is provided and buflen must contain the size of that buf
// ERR_BUF is returned if it is not big enough and the size it would need to be is assigned to readlen
// readlen is set to the actual number of bytes read
// TODO may need to be able to return partial buffers of payload
static err_t websocket_read(WebsocketState& state, uint8_t *buf, uint16_t buflen, uint16_t& readlen)
{
    err_t err;
    if(state.p == nullptr || state.p->tot_len < 6) {
        // get at least 6 bytes
        if(ERR_OK != (err = read_len_bytes(state.conn, state.p, 6))) {
            return err;
        }
    }

    uint8_t hdr[8];
    // get initial 6 bytes of header
    uint16_t n = pbuf_copy_partial(state.p, hdr, 6, 0);
    if(n != 6) {
        printf("websocket_read: pbuf_copy_partial failed\n");
        return ERR_VAL;
    }
    if((hdr[0] & 0x80) == 0) {
        // must have fin bit
        // TODO need to handle this correctly, as it would notify the end of a large file upload
        printf("websocket_read: WARNING FIN bit not set\n");
        state.complete = false;
        //return ERR_VAL;
    } else {
        state.complete = true;
    }

    if((hdr[1] & 0x80) == 0) {
        // must have mask bit
        printf("websocket_read: MASK bit not set\n");
        return ERR_VAL;
    }
    uint16_t o, m;
    uint16_t plen = hdr[1] & 0x7F;
    if(plen < 126) {
        o = 6;
        m = 2;
    } else if(plen == 126) {
        plen = (hdr[2] << 8) | (hdr[3] & 0xFF);
        o = 8;
        m = 4;
    } else {
        uint32_t s1, s2;
        uint8_t hdr2[10];
        pbuf_copy_partial(state.p, hdr2, 10, 0);
        s1 = ((uint32_t)hdr2[2] << 24) | ((uint32_t)hdr2[3] << 16) | ((uint32_t)hdr2[4] << 8) | ((uint32_t)hdr2[5] & 0xFF);
        s2 = ((uint32_t)hdr2[6] << 24) | ((uint32_t)hdr2[7] << 16) | ((uint32_t)hdr2[8] << 8) | ((uint32_t)hdr2[9] & 0xFF);
        printf("websocket_read: unsupported length: %d - %08lX %08lX\n", plen, s1, s2);
        return ERR_VAL;
    }

    // FIXME we really do not want to have to load the entire packet into memory/pbufs

    // check buffer size for payload before trying to read it all
    if(buflen < plen) {
        // the buffer provided is not big enough, tell caller what size it needs to be
        printf("websocket_read: provided buffer (%d) is not big enough for %d bytes\n", buflen, plen);
        readlen = plen;
        return ERR_BUF;
    }

    // make sure we have all data
    if(ERR_OK == (err = read_len_bytes(state.conn, state.p, plen + o))) {
        // read entire set of header bytes into hdr
        n = pbuf_copy_partial(state.p, hdr, o, 0);

        // read entire payload into provided buffer
        n = pbuf_copy_partial(state.p, buf, plen, o);
        // free up read pbufs
        state.p = pbuf_free_header(state.p, plen + o);

        if(n != plen) {
            printf("websocket_read: pbuf_copy_partial failed: %d\n", plen);
            return ERR_VAL;
        }

        // now unmask the data
        uint8_t opcode = hdr[0] & 0x0F;
        switch (opcode) {
            case 0x00: // continuation
            case 0x01: // text
            case 0x02: // bin
                /* unmask */
                for (int i = 0; i < plen; i++) {
                    buf[i] ^= hdr[m + i % 4];
                }
                readlen = plen;
                break;
            case 0x08: // close
                return -20;
                break;
            default:
                printf("websocket_read: unhandled opcode %d\n", opcode);
                return ERR_VAL;
        }
        return ERR_OK;

    } else {
        return err;
    }

    return ERR_VAL;
}

static int websocket_write(struct netconn *conn, const char *data, uint16_t len, uint8_t mode = 0x01)
{
    unsigned char hdr[4];
    hdr[0] = 0x80 | mode; // binary/text
    uint16_t l;
    if (len < 126) {
        l = 2;
        hdr[1] = len;
    } else if(len < 65535) {
        l = 4;
        hdr[1] = 126;
        hdr[2] = len >> 8;
        hdr[3] = len & 0xFF;
    } else {
        printf("websocket_write: buffer too big\n");
        return -1;
    }
    err_t err = netconn_write(conn, hdr, l, NETCONN_COPY);
    err = netconn_write(conn, data, len, NETCONN_COPY);
    if(err != ERR_OK) {
        printf("websocket_write: error writing: %d\n", err);
    }
    return len;
}

static err_t handle_incoming_websocket(struct netconn *conn, const char *keystr)
{
    unsigned char encoded_key[32];
    int len = strlen(keystr);
    const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char key[len + sizeof(WS_GUID)];

    /* Concatenate key */
    memcpy(key, keystr, len);
    strcpy(&key[len], WS_GUID); // FIXME needs to be length checked
    //printf("Resulting key: %s\n", key);

    unsigned char sha1sum[20];
    mbedtls_sha1((unsigned char *) key, sizeof(WS_GUID) + len - 1, sha1sum);
    /* Base64 encode */
    unsigned int olen;
    mbedtls_base64_encode(NULL, 0, &olen, sha1sum, 20); //get length
    int ok = mbedtls_base64_encode(encoded_key, sizeof(encoded_key), &olen, sha1sum, 20);
    if (ok == 0) {
        encoded_key[olen] = '\0';
        //printf("Base64 encoded: %s\n", encoded_key);
    } else {
        printf("handle_incoming_websocket: base64 encode failed\n");
        return ERR_VAL;
    }

    write_header(conn, "HTTP/1.1 101 Switching Protocols\r\n");
    write_header(conn, "Upgrade: websocket\r\n");
    write_header(conn, "Connection: Upgrade\r\n");
    std::string swa("Sec-WebSocket-Accept: ");
    swa.append((char *)encoded_key);
    swa.append("\r\n");
    write_header(conn, swa.c_str());
    write_header(conn, "\r\n");

    printf("handle_incoming_websocket: websocket now open\n");
    return ERR_OK;
}

static const char endbuf[] = {0x88, 0x02, 0x03, 0xe8};

// this may need to stay around until command thread is done with it so we do not delete it until we need to create another
static OutputStream *os = nullptr;
static err_t handle_command(struct netconn *conn)
{
    const u16_t bufsize = 256;
    char buf[bufsize];
    char line[132];
    size_t cnt = 0;
    bool discard = false;

    // if there is an old Outputstream delete it first
    if(os != nullptr) delete(os);
    // create the OutputStream that commands can write to
    os = new OutputStream([conn](const char *ibuf, size_t ilen) { return websocket_write(conn, ibuf, ilen, 0x01); });

    uint16_t n;
    err_t err;
    WebsocketState state(conn);
    // read packets from connection until it closes
    while ((err = websocket_read(state, (uint8_t *)buf, bufsize, n)) == ERR_OK) {
        // we now have a decoded websocket payload in buf, that is n bytes long
        process_command_buffer(n, buf, os, line, cnt, discard);
    }

    // make sure command thread does not try to write to the soon to be closed (and deleted) conn
    os->set_closed();

    if(err == -20) {
        // send exit string if we got one
        netconn_write(conn, endbuf, sizeof(endbuf), NETCONN_NOCOPY);
    }

    printf("handle_command: websocket closing\n");
    return ERR_OK;
}

static err_t handle_upload(struct netconn *conn)
{
    err_t err;
    WebsocketState state(conn);
    const uint16_t buflen = 2000;
    uint8_t *buf = (uint8_t *)malloc(buflen);
    if(buf == NULL) return ERR_MEM;
    uint16_t n;
    std::string name;
    uint32_t size= 0;
    uint32_t filecnt= 0;
    enum STATE { NAME, SIZE, BODY};
    enum STATE uploadstate= NAME;
    FILE *fp= nullptr;

    // read from connection until it closes
    while ((err = websocket_read(state, buf, buflen, n)) == ERR_OK) {
        //printf("handle_upload: got len %d, complete: %d, state: %d\n", n, state.complete, uploadstate);
        if(uploadstate == NAME) {
            name.assign((char*)buf, n);
            uploadstate= SIZE;

        } else if(uploadstate == SIZE) {
            std::string s((char*)buf, n);
            size= strtoul(s.c_str(), nullptr, 10);
            // open file, if it fails send error message and close connection
            fp= fopen(name.c_str(), "wb");
            if(fp == NULL) {
                printf("handle_upload: failed to open file for write\n");
                websocket_write(conn, "error file open failed", 22);
                break;
            }
            uploadstate= BODY;
            filecnt= 0;

        } else if(uploadstate == BODY) {
            // write to file, if it fails send error message and close connection
            size_t l= fwrite(buf, 1, n, fp);
            if(l != n) {
                printf("handle_upload: failed to write to file\n");
                websocket_write(conn, "error file write failed", 23);
                fclose(fp);
                break;
            }
#if 0
            int cnt = 0;
            for (int i = 0; i < n; ++i) {
                printf("%02X(%c) ", buf[i], buf[i] >= ' ' && buf[i] <= '~' ? buf[i] : '_');
                if(++cnt >= 8) {
                    printf("\n");
                    cnt = 0;
                }
            }
            printf("\n");
#endif
            filecnt += n;
            if(filecnt >= size) {
                // close file
                fclose(fp);
                printf("handle_upload: Done upload of file %s, of size: %lu (%lu)\n", name.c_str(), size, filecnt);
                websocket_write(conn, "ok upload successful", 17);
                uploadstate= NAME;
                break;
            }

        } else {
            printf("handle_upload: state error\n");
        }
    }
    free(buf);

    // send exit string
    netconn_write(conn, endbuf, sizeof(endbuf), NETCONN_NOCOPY);
    printf("handle_upload: websocket closing\n");
    return ERR_OK;
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

    //printf("parse_headers: got method: %s\n", methbuf);
    method.assign(methbuf, n);

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
    request_target.assign(reqbuf, len);
    //printf("parse_headers: reqbuf: %s %d\n", request_target.c_str(), request_target.size());

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
            //printf("parse_headers: end of headers\n");
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

        //printf("parse_headers: reading header: %s\n", hdrbuf);
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
        printf("http_server_netconn_serve: parse_headers failed with err: %d\n", err);
        if(cp != NULL) pbuf_free(cp);

    } else {

        printf("http_server_netconn_serve: method: %s\n", method.c_str());
        printf("http_server_netconn_serve: request_target: <%s>\n", request_target.c_str());
        printf("http_server_netconn_serve: Headers: \n");
        for(auto& x : hdrs) {
            printf(" %s = %s\n", x.first.c_str(), x.second.c_str());
        }

        printf("http_server_netconn_serve: bytes left in pbuf: %d, cp= %p\n", cp == NULL ? 0 : cp->tot_len, cp);

        if(cp != NULL) pbuf_free(cp);

        if(method == "GET" && (request_target == "/command" || request_target == "/upload")) {
            auto i = hdrs.find("Upgrade");
            if(i != hdrs.end() && i->second == "websocket") {
                auto k = hdrs.find("Sec-WebSocket-Key");
                if(k != hdrs.end()) {
                    std::string key = k->second;
                    if(handle_incoming_websocket(conn, key.c_str()) == ERR_OK) {
                        if(request_target == "/command") {
                            handle_command(conn);
                        }else{
                            handle_upload(conn);
                        }
                        return;
                    }
                }
            }
            printf("http_server_netconn_serve: badly formatted websocket request\n");
            write_header(conn, http_header_400);

        } else if(method == "GET") {
            if(request_target == "/") {
                request_target = "/index.html";
            }
            std::string path = make_path(request_target.c_str());
            if(!path.empty()) {
                write_header(conn, http_header_200);
                std::string ext;
                auto o = request_target.find_last_of('.');
                if(o != std::string::npos) {
                    ext = request_target.substr(o);
                }

                //printf("ext: <%s>\n", ext.c_str());

                if(ext == http_html) {
                    write_header(conn, http_content_type_html);
                } else if(ext == http_css) {
                    write_header(conn, http_content_type_css);
                } else if(ext == http_jpg) {
                    write_header(conn, http_content_type_jpg);
                } else if(ext == http_png) {
                    write_header(conn, http_content_type_png);
                } else if(ext == http_gif) {
                    write_header(conn, http_content_type_gif);
                } else if(ext == http_txt) {
                    write_header(conn, http_content_type_text);
                } else if(ext == http_js) {
                    write_header(conn, http_content_type_js);
                } else {
                    write_header(conn, http_content_type_html);
                }
                write_page(conn, path.c_str());

            } else {
                write_header(conn, http_header_404);
                // path = make_path("404.html");
                // if(!path.empty()) {
                //     write_header(conn, http_content_type_html);
                //     write_page(conn, path.c_str());
                // }
            }

        } else {
            printf("http_server_netconn_serve: Unhandled request: %s\n", method.c_str());
            write_header(conn, http_header_400);
        }
    }

    printf("http_server_netconn_serve: request closed\n");
}

/** The main function, never returns! */
static void http_server_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t err;
    LWIP_UNUSED_ARG(arg);

    printf("http_server: thread started\n");

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
    sys_thread_new("http_server_netconn", http_server_thread, NULL, 450, DEFAULT_THREAD_PRIO);
}
