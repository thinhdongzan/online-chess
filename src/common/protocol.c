#include "common/protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ProtocolMsgType type;
    const char     *name;
} MsgTypeEntry;

static const MsgTypeEntry kMsgTypeTable[] = {
    { MSG_REGISTER,           "REGISTER" },
    { MSG_LOGIN,              "LOGIN" },
    { MSG_LOGOUT,             "LOGOUT" },
    { MSG_LIST_READY,         "LIST_READY" },
    { MSG_CHALLENGE,          "CHALLENGE" },
    { MSG_CHALLENGE_REPLY,    "CHALLENGE_REPLY" },
    { MSG_MOVE,               "MOVE" },
    { MSG_RESIGN,             "RESIGN" },
    { MSG_HISTORY,            "HISTORY" },
    { MSG_GET_LOG,            "GET_LOG" },
    { MSG_OK,                 "OK" },
    { MSG_ERROR,              "ERROR" },
    { MSG_READY_LIST,         "READY_LIST" },
    { MSG_HISTORY_LIST,       "HISTORY_LIST" },
    { MSG_CHALLENGE_RECEIVED, "CHALLENGE_RECEIVED" },
    { MSG_CHALLENGE_UPDATED,  "CHALLENGE_UPDATED" },
    { MSG_MATCH_STARTED,      "MATCH_STARTED" },
    { MSG_MOVE_APPLIED,       "MOVE_APPLIED" },
    { MSG_MATCH_RESULT,       "MATCH_RESULT" },
    { MSG_LOG_BEGIN,          "LOG_BEGIN" },
    { MSG_LOG_CHUNK,          "LOG_CHUNK" },
    { MSG_LOG_END,            "LOG_END" }
};

typedef struct {
    ProtocolErrorCode code;
    const char       *name;
} ErrorCodeEntry;

static const ErrorCodeEntry kErrorCodeTable[] = {
    { ERR_AUTH_REQUIRED,     "AUTH_REQUIRED" },
    { ERR_BAD_CREDENTIALS,   "BAD_CREDENTIALS" },
    { ERR_ALREADY_ONLINE,    "ALREADY_ONLINE" },
    { ERR_INVALID_REQUEST,   "INVALID_REQUEST" },
    { ERR_NOT_FOUND,         "NOT_FOUND" },
    { ERR_FORBIDDEN,         "FORBIDDEN" },
    { ERR_BUSY,              "BUSY" },
    { ERR_RANK_DISTANCE,     "RANK_DISTANCE" },
    { ERR_CHALLENGE_EXPIRED, "CHALLENGE_EXPIRED" },
    { ERR_NOT_YOUR_TURN,     "NOT_YOUR_TURN" },
    { ERR_STALE_STATE,       "STALE_STATE" },
    { ERR_ILLEGAL_MOVE,      "ILLEGAL_MOVE" },
    { ERR_MATCH_FINISHED,    "MATCH_FINISHED" },
    { ERR_STORAGE_ERROR,     "STORAGE_ERROR" }
};

void protocol_buffer_init(FrameBuffer *fb)
{
    if (fb == NULL) {
        return;
    }
    fb->len = 0U;
}

void protocol_buffer_reset(FrameBuffer *fb)
{
    if (fb == NULL) {
        return;
    }
    fb->len = 0U;
}

ProtocolStatus protocol_buffer_append(FrameBuffer *fb,
                                      const uint8_t *bytes,
                                      size_t count)
{
    if (fb == NULL || (bytes == NULL && count > 0U)) {
        return PROTO_ERR_INVALID_ARG;
    }
    if (count == 0U) {
        return PROTO_OK;
    }
    if (count > PROTOCOL_BUFFER_CAPACITY - fb->len) {
        return PROTO_ERR_BUFFER_OVERFLOW;
    }
    memcpy(fb->data + fb->len, bytes, count);
    fb->len += count;
    return PROTO_OK;
}

uint32_t protocol_read_be32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

void protocol_write_be32(uint8_t *buf, uint32_t val)
{
    buf[0] = (uint8_t)((val >> 24) & 0xFFU);
    buf[1] = (uint8_t)((val >> 16) & 0xFFU);
    buf[2] = (uint8_t)((val >> 8)  & 0xFFU);
    buf[3] = (uint8_t)(val & 0xFFU);
}

ProtocolStatus protocol_encode_frame(const char *json_payload,
                                     size_t payload_len,
                                     uint8_t *out_buf,
                                     size_t out_capacity,
                                     size_t *out_frame_len)
{
    if (json_payload == NULL || out_buf == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }
    if (payload_len < PROTOCOL_MIN_PAYLOAD_SIZE ||
        payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return PROTO_ERR_FRAME_TOO_LARGE;
    }

    size_t total_len = PROTOCOL_FRAME_HEADER_SIZE + payload_len;
    if (out_capacity < total_len) {
        return PROTO_ERR_BUFFER_OVERFLOW;
    }

    protocol_write_be32(out_buf, (uint32_t)payload_len);
    memcpy(out_buf + PROTOCOL_FRAME_HEADER_SIZE, json_payload, payload_len);

    if (out_frame_len != NULL) {
        *out_frame_len = total_len;
    }
    return PROTO_OK;
}

ProtocolStatus protocol_next_frame(FrameBuffer *fb,
                                   char *out_json,
                                   size_t out_capacity,
                                   size_t *out_json_len)
{
    if (fb == NULL || out_json == NULL || out_capacity == 0U) {
        return PROTO_ERR_INVALID_ARG;
    }

    if (fb->len < PROTOCOL_FRAME_HEADER_SIZE) {
        return PROTO_NEED_MORE;
    }

    uint32_t payload_len = protocol_read_be32(fb->data);
    if (payload_len < PROTOCOL_MIN_PAYLOAD_SIZE ||
        payload_len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return PROTO_ERR_FRAME_TOO_LARGE;
    }

    size_t frame_total = PROTOCOL_FRAME_HEADER_SIZE + (size_t)payload_len;
    if (fb->len < frame_total) {
        return PROTO_NEED_MORE;
    }

    if (out_capacity < (size_t)payload_len + 1U) {
        return PROTO_ERR_BUFFER_OVERFLOW;
    }

    memcpy(out_json, fb->data + PROTOCOL_FRAME_HEADER_SIZE, payload_len);
    out_json[payload_len] = '\0';

    size_t remaining = fb->len - frame_total;
    if (remaining > 0U) {
        memmove(fb->data, fb->data + frame_total, remaining);
    }
    fb->len = remaining;

    if (out_json_len != NULL) {
        *out_json_len = (size_t)payload_len;
    }
    return PROTO_OK;
}

const char *protocol_msg_type_to_str(ProtocolMsgType type)
{
    size_t count = sizeof(kMsgTypeTable) / sizeof(kMsgTypeTable[0]);
    for (size_t i = 0U; i < count; i++) {
        if (kMsgTypeTable[i].type == type) {
            return kMsgTypeTable[i].name;
        }
    }
    return "UNKNOWN";
}

ProtocolMsgType protocol_msg_type_from_str(const char *type_str)
{
    if (type_str == NULL) {
        return MSG_UNKNOWN;
    }
    size_t count = sizeof(kMsgTypeTable) / sizeof(kMsgTypeTable[0]);
    for (size_t i = 0U; i < count; i++) {
        if (strcmp(kMsgTypeTable[i].name, type_str) == 0) {
            return kMsgTypeTable[i].type;
        }
    }
    return MSG_UNKNOWN;
}

const char *protocol_error_code_to_str(ProtocolErrorCode code)
{
    if (code == ERR_NONE) {
        return "NONE";
    }
    size_t count = sizeof(kErrorCodeTable) / sizeof(kErrorCodeTable[0]);
    for (size_t i = 0U; i < count; i++) {
        if (kErrorCodeTable[i].code == code) {
            return kErrorCodeTable[i].name;
        }
    }
    return "INVALID_REQUEST";
}

ProtocolErrorCode protocol_error_code_from_str(const char *code_str)
{
    if (code_str == NULL) {
        return ERR_NONE;
    }
    size_t count = sizeof(kErrorCodeTable) / sizeof(kErrorCodeTable[0]);
    for (size_t i = 0U; i < count; i++) {
        if (strcmp(kErrorCodeTable[i].name, code_str) == 0) {
            return kErrorCodeTable[i].code;
        }
    }
    return ERR_NONE;
}

ProtocolStatus protocol_validate_json(const char *json, size_t len)
{
    if (json == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }
    if (len == 0U || len > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return PROTO_ERR_INVALID_JSON;
    }

    size_t start = 0U;
    while (start < len && isspace((unsigned char)json[start])) {
        start++;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)json[end - 1U])) {
        end--;
    }

    if (end - start < 2U || json[start] != '{' || json[end - 1U] != '}') {
        return PROTO_ERR_INVALID_JSON;
    }

    int brace_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;

    for (size_t i = start; i < end; i++) {
        unsigned char c = (unsigned char)json[i];
        if (c == '\0') {
            return PROTO_ERR_INVALID_JSON;
        }

        if (in_string) {
            if (c < 0x20U) {
                /* Unescaped control character inside JSON string */
                return PROTO_ERR_INVALID_JSON;
            }
            if (c == '\\') {
                if (i + 1U >= end) {
                    return PROTO_ERR_INVALID_JSON;
                }
                unsigned char esc = (unsigned char)json[++i];
                if (esc != '"' && esc != '\\' && esc != '/' &&
                    esc != 'b' && esc != 'f' && esc != 'n' &&
                    esc != 'r' && esc != 't' && esc != 'u') {
                    return PROTO_ERR_INVALID_JSON;
                }
            } else if (c == '"') {
                in_string = 0;
            }
        } else {
            if (c == '"') {
                in_string = 1;
            } else if (c == '{') {
                brace_depth++;
            } else if (c == '}') {
                brace_depth--;
                if (brace_depth < 0 || (brace_depth == 0 && i + 1U < end)) {
                    return PROTO_ERR_INVALID_JSON;
                }
            } else if (c == '[') {
                bracket_depth++;
            } else if (c == ']') {
                bracket_depth--;
                if (bracket_depth < 0) {
                    return PROTO_ERR_INVALID_JSON;
                }
            }
        }
    }

    if (in_string || brace_depth != 0 || bracket_depth != 0) {
        return PROTO_ERR_INVALID_JSON;
    }

    return PROTO_OK;
}

ProtocolStatus protocol_json_escape_string(const char *input,
                                           char *out_buf,
                                           size_t out_capacity)
{
    if (input == NULL || out_buf == NULL || out_capacity == 0U) {
        return PROTO_ERR_INVALID_ARG;
    }

    size_t pos = 0U;
    for (size_t i = 0U; input[i] != '\0'; i++) {
        unsigned char c = (unsigned char)input[i];
        const char *rep = NULL;
        char hex_buf[7];

        switch (c) {
        case '"':  rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n";  break;
        case '\r': rep = "\\r";  break;
        case '\t': rep = "\\t";  break;
        case '\b': rep = "\\b";  break;
        case '\f': rep = "\\f";  break;
        default:
            if (c < 0x20U) {
                snprintf(hex_buf, sizeof(hex_buf), "\\u%04x", (unsigned int)c);
                rep = hex_buf;
            }
            break;
        }

        if (rep != NULL) {
            size_t rlen = strlen(rep);
            if (pos + rlen >= out_capacity) {
                return PROTO_ERR_BUFFER_OVERFLOW;
            }
            memcpy(out_buf + pos, rep, rlen);
            pos += rlen;
        } else {
            if (pos + 1U >= out_capacity) {
                return PROTO_ERR_BUFFER_OVERFLOW;
            }
            out_buf[pos++] = (char)c;
        }
    }

    out_buf[pos] = '\0';
    return PROTO_OK;
}

/* Locate the value token right after `"key" :` at top-level object depth 1 */
static const char *find_json_key_value(const char *json, const char *key)
{
    size_t key_len = strlen(key);
    int brace_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;

    for (const char *p = json; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;

        if (in_string) {
            if (c == '\\' && *(p + 1) != '\0') {
                p++;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }

        if (c == '{') {
            brace_depth++;
            continue;
        }
        if (c == '}') {
            brace_depth--;
            continue;
        }
        if (c == '[') {
            bracket_depth++;
            continue;
        }
        if (c == ']') {
            bracket_depth--;
            continue;
        }

        if (c == '"') {
            if (brace_depth == 1 && bracket_depth == 0) {
                const char *kstart = p + 1;
                const char *kend = kstart;
                while (*kend != '\0' && *kend != '"') {
                    if (*kend == '\\' && *(kend + 1) != '\0') {
                        kend += 2;
                    } else {
                        kend++;
                    }
                }
                if (*kend == '"') {
                    size_t token_len = (size_t)(kend - kstart);
                    const char *after = kend + 1;
                    while (*after != '\0' && isspace((unsigned char)*after)) {
                        after++;
                    }
                    if (*after == ':' && token_len == key_len &&
                        strncmp(kstart, key, key_len) == 0) {
                        after++;
                        while (*after != '\0' && isspace((unsigned char)*after)) {
                            after++;
                        }
                        return after;
                    }
                    p = kend;
                    continue;
                }
            }
            in_string = 1;
        }
    }

    return NULL;
}

ProtocolStatus protocol_json_get_string(const char *json,
                                        const char *key,
                                        char *out_buf,
                                        size_t out_capacity)
{
    if (json == NULL || key == NULL || out_buf == NULL || out_capacity == 0U) {
        return PROTO_ERR_INVALID_ARG;
    }

    const char *val = find_json_key_value(json, key);
    if (val == NULL) {
        return PROTO_ERR_MISSING_FIELD;
    }
    if (*val != '"') {
        return PROTO_ERR_INVALID_JSON;
    }

    val++;
    size_t pos = 0U;
    while (*val != '\0' && *val != '"') {
        char out_c = *val;
        if (*val == '\\') {
            val++;
            if (*val == '\0') {
                return PROTO_ERR_INVALID_JSON;
            }
            switch (*val) {
            case '"':  out_c = '"';  break;
            case '\\': out_c = '\\'; break;
            case '/':  out_c = '/';  break;
            case 'n':  out_c = '\n'; break;
            case 'r':  out_c = '\r'; break;
            case 't':  out_c = '\t'; break;
            case 'b':  out_c = '\b'; break;
            case 'f':  out_c = '\f'; break;
            default:
                return PROTO_ERR_INVALID_JSON;
            }
        }
        if (pos + 1U >= out_capacity) {
            return PROTO_ERR_BUFFER_OVERFLOW;
        }
        out_buf[pos++] = out_c;
        val++;
    }

    if (*val != '"') {
        return PROTO_ERR_INVALID_JSON;
    }

    out_buf[pos] = '\0';
    return PROTO_OK;
}

ProtocolStatus protocol_json_get_int(const char *json,
                                     const char *key,
                                     int64_t *out_val)
{
    if (json == NULL || key == NULL || out_val == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }

    const char *val = find_json_key_value(json, key);
    if (val == NULL) {
        return PROTO_ERR_MISSING_FIELD;
    }

    if (*val != '-' && !isdigit((unsigned char)*val)) {
        return PROTO_ERR_INVALID_JSON;
    }

    char *endptr = NULL;
    long long parsed = strtoll(val, &endptr, 10);
    if (endptr == val) {
        return PROTO_ERR_INVALID_JSON;
    }
    if (*endptr != '\0' && *endptr != ',' && *endptr != '}' &&
        !isspace((unsigned char)*endptr)) {
        return PROTO_ERR_INVALID_JSON;
    }

    *out_val = (int64_t)parsed;
    return PROTO_OK;
}

ProtocolStatus protocol_json_get_bool(const char *json,
                                      const char *key,
                                      int *out_bool)
{
    if (json == NULL || key == NULL || out_bool == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }

    const char *val = find_json_key_value(json, key);
    if (val == NULL) {
        return PROTO_ERR_MISSING_FIELD;
    }

    if (strncmp(val, "true", 4) == 0 &&
        (val[4] == ',' || val[4] == '}' || isspace((unsigned char)val[4]))) {
        *out_bool = 1;
        return PROTO_OK;
    }
    if (strncmp(val, "false", 5) == 0 &&
        (val[5] == ',' || val[5] == '}' || isspace((unsigned char)val[5]))) {
        *out_bool = 0;
        return PROTO_OK;
    }

    return PROTO_ERR_INVALID_JSON;
}

ProtocolStatus protocol_parse_header(const char *json,
                                     size_t len,
                                     ProtocolMsgType *out_type,
                                     int64_t *out_request_id)
{
    if (json == NULL || out_type == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }

    ProtocolStatus st = protocol_validate_json(json, len);
    if (st != PROTO_OK) {
        return st;
    }

    char type_buf[32];
    st = protocol_json_get_string(json, "type", type_buf, sizeof(type_buf));
    if (st != PROTO_OK) {
        return st;
    }

    ProtocolMsgType msg_type = protocol_msg_type_from_str(type_buf);
    if (msg_type == MSG_UNKNOWN) {
        *out_type = MSG_UNKNOWN;
        return PROTO_ERR_UNKNOWN_TYPE;
    }

    *out_type = msg_type;

    if (out_request_id != NULL) {
        int64_t req_id = -1;
        ProtocolStatus req_st = protocol_json_get_int(json, "request_id", &req_id);
        if (req_st == PROTO_OK) {
            *out_request_id = req_id;
        } else if (req_st == PROTO_ERR_MISSING_FIELD) {
            *out_request_id = -1;
        } else {
            return req_st;
        }
    }

    return PROTO_OK;
}

ProtocolStatus protocol_build_error_frame(int64_t request_id,
                                          ProtocolErrorCode err_code,
                                          const char *message,
                                          uint8_t *out_frame,
                                          size_t out_capacity,
                                          size_t *out_frame_len)
{
    if (out_frame == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }

    const char *code_str = protocol_error_code_to_str(err_code);
    char escaped_msg[512];
    const char *raw_msg = (message != NULL) ? message : code_str;
    ProtocolStatus st = protocol_json_escape_string(raw_msg,
                                                    escaped_msg,
                                                    sizeof(escaped_msg));
    if (st != PROTO_OK) {
        return st;
    }

    char json_buf[768];
    int written = 0;
    if (request_id >= 0) {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"ERROR\",\"request_id\":%lld,\"code\":\"%s\",\"message\":\"%s\"}",
                           (long long)request_id,
                           code_str,
                           escaped_msg);
    } else {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"ERROR\",\"code\":\"%s\",\"message\":\"%s\"}",
                           code_str,
                           escaped_msg);
    }

    if (written <= 0 || (size_t)written >= sizeof(json_buf)) {
        return PROTO_ERR_BUFFER_OVERFLOW;
    }

    return protocol_encode_frame(json_buf,
                                 (size_t)written,
                                 out_frame,
                                 out_capacity,
                                 out_frame_len);
}

ProtocolStatus protocol_build_ok_frame(int64_t request_id,
                                       const char *extra_fields_json,
                                       uint8_t *out_frame,
                                       size_t out_capacity,
                                       size_t *out_frame_len)
{
    if (out_frame == NULL) {
        return PROTO_ERR_INVALID_ARG;
    }

    char json_buf[2048];
    int written = 0;
    int has_extra = (extra_fields_json != NULL && extra_fields_json[0] != '\0');

    if (request_id >= 0 && has_extra) {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"OK\",\"request_id\":%lld,%s}",
                           (long long)request_id,
                           extra_fields_json);
    } else if (request_id >= 0) {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"OK\",\"request_id\":%lld}",
                           (long long)request_id);
    } else if (has_extra) {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"OK\",%s}",
                           extra_fields_json);
    } else {
        written = snprintf(json_buf,
                           sizeof(json_buf),
                           "{\"type\":\"OK\"}");
    }

    if (written <= 0 || (size_t)written >= sizeof(json_buf)) {
        return PROTO_ERR_BUFFER_OVERFLOW;
    }

    return protocol_encode_frame(json_buf,
                                 (size_t)written,
                                 out_frame,
                                 out_capacity,
                                 out_frame_len);
}
