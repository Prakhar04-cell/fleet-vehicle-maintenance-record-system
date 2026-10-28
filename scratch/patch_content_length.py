with open('src/http_server.c', 'r', encoding='utf-8') as f:
    code = f.read()

target = """static void handle_client_request(SOCKET client) {
    char req_buf[8192];
    int received = recv(client, req_buf, sizeof(req_buf) - 1, 0);
    if (received <= 0) return;
    req_buf[received] = '\\0';

    char method[16] = {0};
    char path[256] = {0};
    sscanf(req_buf, "%15s %255s", method, path);

    /* Handle CORS preflight OPTIONS */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client, 200, "text/plain", "", 0);
        return;
    }

    /* Extract query string if any */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\\0';
        query++;
    }

    /* Locate HTTP body */
    const char *body = strstr(req_buf, "\\r\\n\\r\\n");
    if (body) body += 4;
    else body = "";"""

replacement = """static void handle_client_request(SOCKET client) {
    char req_buf[16384];
    int received = recv(client, req_buf, sizeof(req_buf) - 1, 0);
    if (received <= 0) return;
    req_buf[received] = '\\0';

    char method[16] = {0};
    char path[256] = {0};
    sscanf(req_buf, "%15s %255s", method, path);

    /* Handle CORS preflight OPTIONS */
    if (strcmp(method, "OPTIONS") == 0) {
        send_http_response(client, 200, "text/plain", "", 0);
        return;
    }

    /* Extract query string if any */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\\0';
        query++;
    }

    /* Locate HTTP body and guarantee full read based on Content-Length */
    char *body = strstr(req_buf, "\\r\\n\\r\\n");
    if (body) {
        body += 4;
        const char *cl = strstr(req_buf, "Content-Length:");
        if (!cl) cl = strstr(req_buf, "content-length:");
        if (cl) {
            int content_len = atoi(cl + 15);
            int body_so_far = received - (int)(body - req_buf);
            while (body_so_far < content_len && received < (int)sizeof(req_buf) - 1) {
                int n = recv(client, req_buf + received, (int)sizeof(req_buf) - 1 - received, 0);
                if (n <= 0) break;
                received += n;
                req_buf[received] = '\\0';
                body_so_far += n;
            }
        }
    } else {
        body = req_buf + received;
    }"""

if target in code:
    code = code.replace(target, replacement)
    with open('src/http_server.c', 'w', encoding='utf-8') as f:
        f.write(code)
    print("Patched handle_client_request successfully")
else:
    print("Target not found!")
