#include "lisp_app_url.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char storage[LISP_APP_URL_SOURCE_CAPACITY];
static char payload[LISP_APP_URL_SOURCE_CAPACITY + 1];
static struct {
    size_t size, position, chunk;
    unsigned opens, closes, reads;
    uint64_t now;
    int status;
    int64_t declared;
    bool complete, encoded, fail_open, fail_read, cancel;
} mock;
static uint64_t clock_ms(void *user)
{
    (void)user;
    return mock.now++;
}
static bool cancelled(void *user)
{
    (void)user;
    return mock.cancel;
}
static bool open_url(void *user, const char *url, unsigned timeout, lisp_app_url_response_t *response)
{
    (void)user;
    assert(url && timeout > 0 && timeout <= 3000);
    mock.opens++;
    *response = (lisp_app_url_response_t){mock.status, mock.declared, mock.encoded};
    return !mock.fail_open;
}
static int read_url(void *user, char *buffer, size_t capacity, unsigned timeout)
{
    (void)user;
    assert(timeout > 0 && timeout <= 3000);
    mock.reads++;
    if (mock.fail_read)
        return -1;
    size_t remaining = mock.size - mock.position;
    size_t count = remaining < capacity ? remaining : capacity;
    if (count > mock.chunk)
        count = mock.chunk;
    memcpy(buffer, payload + mock.position, count);
    mock.position += count;
    return (int)count;
}
static bool complete_url(void *user)
{
    (void)user;
    return mock.complete;
}
static void close_url(void *user)
{
    (void)user;
    mock.closes++;
}
static const lisp_app_url_io_t io = {NULL, clock_ms, cancelled, open_url, read_url, complete_url, close_url};
static void initialize(void)
{
    memset(&mock, 0, sizeof(mock));
    mock.status = 200;
    mock.declared = -1;
    mock.complete = true;
    mock.chunk = 7;
    strcpy(payload, "(define demo 42)\n");
    mock.size = strlen(payload);
}
static bool download(size_t *length, uint64_t deadline)
{
    return lisp_app_url_download(&io, "https://example.com/app.lisp", storage, sizeof(storage), length,
                                 deadline);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    initialize();
    size_t length = 99;
    if (!strcmp(argv[1], "url")) {
        const char *valid[] = {"https://example.com/a.lisp", "https://raw.githubusercontent.com/u/r/c/a.lisp",
                               "https://example.com:443/a?x=1", "https://example.com/"};
        for (unsigned i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
            assert(lisp_app_url_valid(valid[i], strlen(valid[i])));
        const char *invalid[] = {"",
                                 "http://example.com/a",
                                 "HTTPS://example.com/a",
                                 "https://u:p@example.com/a",
                                 "https://example.com:80/a",
                                 "https://example.com/a#x",
                                 "https://example.com/ a",
                                 "https://example.com/\r\nHeader:x",
                                 "https://example.com",
                                 "https:///a",
                                 "https://-example.com/a",
                                 "https://example..com/a",
                                 "https://example.com./a",
                                 "https://example%2ecom/a",
                                 "https://example.com\\@evil.com/a",
                                 "https://example.com:443x/a",
                                 "https://example.com/\xff"};
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
            assert(!lisp_app_url_valid(invalid[i], strlen(invalid[i])));
        assert(!lisp_app_url_valid(NULL, 1));
        assert(!lisp_app_url_valid("https://example.com/a\0x", 23));
        char long_url[513];
        memset(long_url, 'a', sizeof(long_url));
        memcpy(long_url, "https://example.com/", 20);
        assert(!lisp_app_url_valid(long_url, sizeof(long_url)));
    } else if (!strcmp(argv[1], "body")) {
        assert(download(&length, 30000));
        assert(length == mock.size);
        assert(!strcmp(storage, payload));
        assert(mock.opens == 1 && mock.closes == 1);
        initialize();
        mock.declared = (int64_t)mock.size;
        assert(download(&length, 30000));
    } else if (!strcmp(argv[1], "bounds")) {
        memset(payload, ' ', sizeof(payload));
        mock.size = LISP_APP_URL_SOURCE_LIMIT;
        mock.chunk = 512;
        mock.declared = (int64_t)mock.size;
        assert(download(&length, 30000));
        assert(length == LISP_APP_URL_SOURCE_LIMIT && storage[length] == 0);
        initialize();
        mock.size = LISP_APP_URL_SOURCE_LIMIT + 1;
        mock.declared = -1;
        memset(payload, ' ', sizeof(payload));
        mock.chunk = 512;
        assert(!download(&length, 30000) && length == 0);
        initialize();
        mock.declared = LISP_APP_URL_SOURCE_LIMIT + 1;
        assert(!download(&length, 30000) && mock.reads == 0);
    } else if (!strcmp(argv[1], "invalid")) {
        for (unsigned i = 0; i < 3; i++) {
            initialize();
            payload[3] = i == 0 ? 0 : i == 1 ? '\xff' : 1;
            assert(!download(&length, 30000) && length == 0);
        }
        initialize();
        mock.size = 0;
        assert(!download(&length, 30000));
        initialize();
        mock.encoded = true;
        assert(!download(&length, 30000) && mock.reads == 0);
    } else if (!strcmp(argv[1], "transport")) {
        int codes[] = {301, 302, 307, 401, 404, 500};
        for (unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
            initialize();
            mock.status = codes[i];
            assert(!download(&length, 30000));
            assert(mock.opens == 1 && mock.reads == 0 && mock.closes == 1);
        }
        initialize();
        mock.complete = false;
        assert(!download(&length, 30000));
        initialize();
        mock.declared = (int64_t)mock.size + 1;
        assert(!download(&length, 30000));
        initialize();
        mock.declared = (int64_t)mock.size - 1;
        assert(!download(&length, 30000));
        initialize();
        mock.fail_open = true;
        assert(!download(&length, 30000) && mock.closes == 1);
        initialize();
        mock.fail_read = true;
        assert(!download(&length, 30000) && mock.closes == 1);
    } else if (!strcmp(argv[1], "deadline")) {
        mock.now = 30000;
        assert(!download(&length, 30000) && mock.opens == 0);
        initialize();
        mock.cancel = true;
        assert(!download(&length, 30000) && mock.opens == 0);
        initialize();
        mock.chunk = 1;
        assert(!download(&length, 10) && mock.closes == 1);
    } else if (!strcmp(argv[1], "slot")) {
        lisp_app_url_slot_t slot;
        const char *source;
        size_t count;
        assert(lisp_app_url_slot_init(&slot, storage, sizeof(storage)));
        assert(lisp_app_url_slot_request(&slot, "https://example.com/a", 21));
        assert(!lisp_app_url_slot_request(&slot, "https://example.com/b", 21));
        lisp_app_url_slot_cancel(&slot);
        assert(slot.phase == LISP_APP_URL_FETCHING);
        lisp_app_url_slot_finish(&slot, true, 12);
        assert(slot.phase == LISP_APP_URL_IDLE);
        assert(lisp_app_url_slot_request(&slot, "https://example.com/a", 21));
        strcpy(storage, "(define a 1)");
        lisp_app_url_slot_finish(&slot, true, strlen(storage));
        assert(lisp_app_url_slot_poll(&slot, &source, &count) == LISP_APP_URL_READY);
        assert(source == storage && count == strlen(storage) && slot.phase == LISP_APP_URL_LEASED);
        lisp_app_url_slot_cancel(&slot);
        assert(slot.phase == LISP_APP_URL_LEASED);
        assert(!lisp_app_url_slot_request(&slot, "https://example.com/a", 21));
        lisp_app_url_slot_release(&slot);
        assert(slot.phase == LISP_APP_URL_IDLE);
        assert(lisp_app_url_slot_request(&slot, "https://example.com/a", 21));
        lisp_app_url_slot_finish(&slot, false, 0);
        assert(lisp_app_url_slot_poll(&slot, &source, &count) == LISP_APP_URL_FAILED);
        assert(slot.phase == LISP_APP_URL_IDLE);
    } else
        return 1;
    puts("PASS");
    return 0;
}
