#ifndef ONLINE_CHESS_PROTOCOL_H
#define ONLINE_CHESS_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Framing constants: [4-byte big-endian length N][N-byte UTF-8 JSON] */
#define PROTOCOL_FRAME_HEADER_SIZE 4U
#define PROTOCOL_MIN_PAYLOAD_SIZE  1U
#define PROTOCOL_MAX_PAYLOAD_SIZE  65536U
#define PROTOCOL_MAX_FRAME_SIZE    (PROTOCOL_FRAME_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD_SIZE)
#define PROTOCOL_BUFFER_CAPACITY   (PROTOCOL_MAX_FRAME_SIZE * 2U)

/* Return status codes for protocol operations */
typedef enum {
    PROTO_OK                    =  0,
    PROTO_NEED_MORE             =  1,
    PROTO_ERR_INVALID_ARG       = -1,
    PROTO_ERR_FRAME_TOO_LARGE   = -2,
    PROTO_ERR_BUFFER_OVERFLOW   = -3,
    PROTO_ERR_INVALID_JSON      = -4,
    PROTO_ERR_MISSING_FIELD     = -5,
    PROTO_ERR_UNKNOWN_TYPE      = -6
} ProtocolStatus;

/* Message types defined in technical-design.md section 5 */
typedef enum {
    MSG_UNKNOWN = 0,

    /* Client -> Server requests */
    MSG_REGISTER,
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_LIST_READY,
    MSG_CHALLENGE,
    MSG_CHALLENGE_REPLY,
    MSG_MOVE,
    MSG_RESIGN,
    MSG_HISTORY,
    MSG_GET_LOG,

    /* Server -> Client responses and events */
    MSG_OK,
    MSG_ERROR,
    MSG_READY_LIST,
    MSG_HISTORY_LIST,
    MSG_CHALLENGE_RECEIVED,
    MSG_CHALLENGE_UPDATED,
    MSG_MATCH_STARTED,
    MSG_MOVE_APPLIED,
    MSG_MATCH_RESULT,
    MSG_LOG_BEGIN,
    MSG_LOG_CHUNK,
    MSG_LOG_END
} ProtocolMsgType;

/* Standard application error codes defined in technical-design.md section 5 */
typedef enum {
    ERR_NONE = 0,
    ERR_AUTH_REQUIRED,
    ERR_BAD_CREDENTIALS,
    ERR_ALREADY_ONLINE,
    ERR_INVALID_REQUEST,
    ERR_NOT_FOUND,
    ERR_FORBIDDEN,
    ERR_BUSY,
    ERR_RANK_DISTANCE,
    ERR_CHALLENGE_EXPIRED,
    ERR_NOT_YOUR_TURN,
    ERR_STALE_STATE,
    ERR_ILLEGAL_MOVE,
    ERR_MATCH_FINISHED,
    ERR_STORAGE_ERROR
} ProtocolErrorCode;

/* Stream accumulation buffer for a TCP connection */
typedef struct {
    uint8_t data[PROTOCOL_BUFFER_CAPACITY];
    size_t  len;
} FrameBuffer;

/* Frame buffer lifecycle and byte stream feeding */
void protocol_buffer_init(FrameBuffer *fb);
void protocol_buffer_reset(FrameBuffer *fb);
ProtocolStatus protocol_buffer_append(FrameBuffer *fb,
                                      const uint8_t *bytes,
                                      size_t count);

/* Encode and decode length-prefixed frames */
uint32_t protocol_read_be32(const uint8_t *buf);
void protocol_write_be32(uint8_t *buf, uint32_t val);

ProtocolStatus protocol_encode_frame(const char *json_payload,
                                     size_t payload_len,
                                     uint8_t *out_buf,
                                     size_t out_capacity,
                                     size_t *out_frame_len);

ProtocolStatus protocol_next_frame(FrameBuffer *fb,
                                   char *out_json,
                                   size_t out_capacity,
                                   size_t *out_json_len);

/* Message type and error code string conversions */
const char *protocol_msg_type_to_str(ProtocolMsgType type);
ProtocolMsgType protocol_msg_type_from_str(const char *type_str);

const char *protocol_error_code_to_str(ProtocolErrorCode code);
ProtocolErrorCode protocol_error_code_from_str(const char *code_str);

/* Lightweight JSON validation, escaping, and field extraction */
ProtocolStatus protocol_validate_json(const char *json, size_t len);

ProtocolStatus protocol_json_escape_string(const char *input,
                                           char *out_buf,
                                           size_t out_capacity);

ProtocolStatus protocol_json_get_string(const char *json,
                                        const char *key,
                                        char *out_buf,
                                        size_t out_capacity);

ProtocolStatus protocol_json_get_int(const char *json,
                                     const char *key,
                                     int64_t *out_val);

ProtocolStatus protocol_json_get_bool(const char *json,
                                      const char *key,
                                      int *out_bool);

/* Parse common header fields ("type" and optional "request_id") */
ProtocolStatus protocol_parse_header(const char *json,
                                     size_t len,
                                     ProtocolMsgType *out_type,
                                     int64_t *out_request_id);

/* Convenience builders for framed OK and ERROR responses */
ProtocolStatus protocol_build_error_frame(int64_t request_id,
                                          ProtocolErrorCode err_code,
                                          const char *message,
                                          uint8_t *out_frame,
                                          size_t out_capacity,
                                          size_t *out_frame_len);

ProtocolStatus protocol_build_ok_frame(int64_t request_id,
                                       const char *extra_fields_json,
                                       uint8_t *out_frame,
                                       size_t out_capacity,
                                       size_t *out_frame_len);

#ifdef __cplusplus
}
#endif

#endif /* ONLINE_CHESS_PROTOCOL_H */
