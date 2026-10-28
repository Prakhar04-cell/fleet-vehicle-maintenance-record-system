with open('src/http_server.c', 'r', encoding='utf-8') as f:
    lines = f.readlines()

new_block = """static int send_all(SOCKET sock, const char *data, size_t total_len) {
    size_t sent_bytes = 0;
    while (sent_bytes < total_len) {
        int to_send = (int)(total_len - sent_bytes);
        if (to_send > 16384) to_send = 16384;
        int n = send(sock, data + sent_bytes, to_send, 0);
        if (n <= 0) {
            return -1;
        }
        sent_bytes += (size_t)n;
    }
    return 0;
}

static void send_http_response_ext(SOCKET client, int status_code, const char *content_type, const char *extra_headers, const char *body, size_t body_len) {
    char header[1024];
    const char *status_text = (status_code == 200) ? "OK" :
                              (status_code == 201) ? "Created" :
                              (status_code == 204) ? "No Content" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 401) ? "Unauthorized" :
                              (status_code == 403) ? "Forbidden" :
                              (status_code == 404) ? "Not Found" : "Internal Server Error";

    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\\r\\n"
        "Content-Type: %s; charset=utf-8\\r\\n"
        "Content-Length: %zu\\r\\n"
        "Access-Control-Allow-Origin: *\\r\\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\\r\\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\\r\\n"
        "%s"
        "Connection: close\\r\\n\\r\\n",
        status_code, status_text, content_type, body_len,
        extra_headers ? extra_headers : "");

    send_all(client, header, (size_t)hlen);
    if (body && body_len > 0) {
        send_all(client, body, body_len);
    }
#ifdef _WIN32
    shutdown(client, SD_SEND);
#else
    shutdown(client, SHUT_WR);
#endif
}

static void send_http_response(SOCKET client, int status_code, const char *content_type, const char *body, size_t body_len) {
    send_http_response_ext(client, status_code, content_type, NULL, body, body_len);
}

static void serve_static_file(SOCKET client, const char *filepath, const char *mime_type) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        const char *err = "{\\"error\\": \\"Static file not found\\"}";
        send_http_response(client, 404, "application/json", err, strlen(err));
        return;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (sz < 0) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Failed reading file\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        const char *err = "{\\"error\\": \\"Out of memory\\"}";
        send_http_response(client, 500, "application/json", err, strlen(err));
        return;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);

    send_http_response(client, 200, mime_type, buf, read_bytes);
    free(buf);
}
"""

assert lines[98].startswith('static void send_http_response')
assert lines[154].startswith('}')

lines[98:155] = [new_block]

with open('src/http_server.c', 'w', encoding='utf-8') as f:
    f.writelines(lines)

print('Successfully replaced send_http_response and serve_static_file with send_all')
