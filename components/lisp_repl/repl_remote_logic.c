#include "repl_remote_logic.h"
#include <string.h>
static const char clipped[] = "\nWARNING: response truncated\n";
enum { CONTENT_LIMIT = REPL_REMOTE_REPLY_CAPACITY - sizeof(clipped) };
bool repl_remote_source_valid(const char *source, size_t length)
{
    if (source == NULL || length == 0 || length >= REPL_REMOTE_SOURCE_CAPACITY)
        return false;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)source[i];
        if ((c < 32 || c > 126) && c != '\n' && c != '\r' && c != '\t')
            return false;
    }
    return true;
}
void repl_remote_reply_init(repl_remote_reply_t *reply)
{
    if (reply != NULL)
        memset(reply, 0, sizeof(*reply));
}
void repl_remote_reply_append(repl_remote_reply_t *reply, const char *text, size_t length)
{
    if (reply == NULL || text == NULL || reply->used > CONTENT_LIMIT)
        return;
    size_t available = CONTENT_LIMIT - reply->used;
    if (length > available) {
        length = available;
        reply->truncated = true;
    }
    memcpy(reply->text + reply->used, text, length);
    reply->used += length;
    reply->text[reply->used] = 0;
}
void repl_remote_reply_finish(repl_remote_reply_t *reply)
{
    if (reply == NULL || reply->used > CONTENT_LIMIT)
        return;
    if (reply->truncated) {
        memcpy(reply->text + reply->used, clipped, sizeof(clipped));
        reply->used += sizeof(clipped) - 1;
    }
}
