/*-
 * Copyright 2012 Matthew Endsley
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "http.h"

#include "OutputStream.h"

static bool splitURL(const char *furl, std::string& host, std::string& rurl)
{

    if(furl != NULL && strlen(furl) > 0) {
        std::string loc(furl);
        size_t n1 = loc.find("http://");
        if(n1 != std::string::npos) {
            n1 += 7;
            size_t n2 = loc.find("/", n1);
            if(n2 != std::string::npos) {
                host = loc.substr(n1, n2 - n1);
                rurl = loc.substr(n2);
                return true;
            }
        }
    }
    return false;
}

// return a socket connected to a hostname, or -1
static int connectsocket(const char* host, int port)
{

    struct addrinfo* result= NULL;
    sockaddr_in addr= {0};
    int s= -1;
    int err=lwip_getaddrinfo(host, NULL, NULL, &result);
    if(err != 0) {
        printf("failed to getaddrinfo: %d - did you set a dns server?\n", err);
        goto error;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    for (struct addrinfo* ai = result; ai != NULL; ai = ai->ai_next) {
        if (ai->ai_family != AF_INET)
            continue;

        const sockaddr_in *ai_in = (const sockaddr_in*)ai->ai_addr;
        addr.sin_addr = ai_in->sin_addr;
        break;
    }

    lwip_freeaddrinfo(result);

    if (addr.sin_addr.s_addr == INADDR_ANY){
        printf("s_addr is bad\n");
        goto error;
    }

    s = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == -1){
        printf("failed to open socket: %d\n", errno);
        goto error;
    }

    if (lwip_connect(s, (const sockaddr*)&addr, sizeof(addr))){
        printf("failed to connect: %d\n", errno);
        goto error;
    }

    return s;

error:
    if (s != -1)
        lwip_close(s);
    if (result)
        lwip_freeaddrinfo(result);
    return -1;
}

// Response data/funcs
struct HttpResponse {
    std::vector<char> body;
    int code;
};

static void* response_realloc(void* opaque, void* ptr, int size)
{
    return realloc(ptr, size);
}

static void response_body(void* opaque, const char* data, int size)
{
    printf("Body: size %d\n", size);

    HttpResponse* response = (HttpResponse*)opaque;
    response->body.insert(response->body.end(), data, data + size);
}

static void response_header(void* opaque, const char* ckey, int nkey, const char* cvalue, int nvalue)
{
    std::string k(ckey, nkey);
    std::string v(cvalue, nvalue);
    printf("%s: %s\n", k.c_str(), v.c_str());
}

static void response_code(void* opaque, int code)
{
    HttpResponse* response = (HttpResponse*)opaque;
    response->code = code;
}

static const http_funcs responseFuncs = {
    response_realloc,
    response_body,
    response_header,
    response_code,
};

bool wget(const char *url, const char *fn, OutputStream& os)
{
    std::string host, req;

    if(!splitURL(url, host, req)) {
        printf("bad url: %s\n", url);
        return false;
    }

    std::string request("GET ");
    request.append(req);
    request.append(" HTTP/1.0\r\nHost: ");
    request.append(host);
    request.append("\r\n\r\n\r\n");

    printf("request: %s to host: %s\n", request.c_str(), host.c_str());

    int conn = connectsocket(host.c_str(), 80);
    if (conn < 0) {
        printf("Failed to connect socket\n");
        return false;
    }

    size_t len = lwip_send(conn, request.c_str(), request.size(), 0);
    if (len != request.size()) {
        printf("Failed to send request\n");
        lwip_close(conn);
        return false;
    }

    HttpResponse response;
    response.code = 0;

    http_roundtripper rt;
    http_init(&rt, responseFuncs, &response);

    bool needmore = true;
    char buffer[1024];
    while (needmore) {
        const char* data = buffer;
        int ndata = lwip_recv(conn, buffer, sizeof(buffer), 0);
        if (ndata <= 0) {
            printf("Error receiving data\n");
            http_free(&rt);
            lwip_close(conn);
            return false;
        }

        while (needmore && ndata) {
            int read;
            needmore = http_data(&rt, data, ndata, &read);
            ndata -= read;
            data += read;
        }
    }

    if (http_iserror(&rt)) {
        printf("Error parsing data\n");
        http_free(&rt);
        lwip_close(conn);
        return false;
    }

    http_free(&rt);
    lwip_close(conn);

    printf("Response: %d\n", response.code);

    if(fn != nullptr) {
        if (!response.body.empty()) {
            FILE *fp = fopen(fn, "w");
            if(fp == NULL) {
                printf("failed to open file: %s\n", fn);
                return false;
            }
            for(char c : response.body) {
                fputc(c, fp);
            }
            fclose(fp);
        }
    }else{
        for(char c : response.body) {
            os.printf("%c", c);
        }
    }

    return true;
}

