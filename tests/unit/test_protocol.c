#include "unity.h"
#include "common/protocol.h"

#include <stdlib.h>
#include <string.h>

static FrameBuffer g_fb;

void setUp(void)
{
    protocol_buffer_init(&g_fb);
}

void tearDown(void)
{
    protocol_buffer_reset(&g_fb);
}

static void test_buffer_lifecycle_and_validation(void)
{
    /* NULL safety checks */
    protocol_buffer_init(NULL);
    protocol_buffer_reset(NULL);

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_buffer_append(NULL, (const uint8_t *)"abc", 3));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_buffer_append(&g_fb, NULL, 3));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, NULL, 0));
    TEST_ASSERT_EQUAL_SIZE_T(0U, g_fb.len);

    const uint8_t sample[] = { 0x01, 0x02, 0x03, 0x04 };
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, sample, sizeof(sample)));
    TEST_ASSERT_EQUAL_SIZE_T(4U, g_fb.len);
    TEST_ASSERT_EQUAL_MEMORY(sample, g_fb.data, sizeof(sample));

    /* Overflow check when exceeding PROTOCOL_BUFFER_CAPACITY */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_buffer_append(&g_fb,
                                                 sample,
                                                 PROTOCOL_BUFFER_CAPACITY));

    protocol_buffer_reset(&g_fb);
    TEST_ASSERT_EQUAL_SIZE_T(0U, g_fb.len);
}

static void test_be32_read_write_roundtrip(void)
{
    uint8_t raw[4];
    protocol_write_be32(raw, 0x12345678U);
    TEST_ASSERT_EQUAL_HEX8(0x12U, raw[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34U, raw[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56U, raw[2]);
    TEST_ASSERT_EQUAL_HEX8(0x78U, raw[3]);
    TEST_ASSERT_EQUAL_UINT32(0x12345678U, protocol_read_be32(raw));
}

static void test_encode_and_decode_single_frame(void)
{
    const char *json = "{\"type\":\"LOGIN\",\"request_id\":1,\"username\":\"an01\",\"password\":\"secret123\"}";
    uint8_t frame[256];
    size_t frame_len = 0U;

    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_encode_frame(json,
                                                strlen(json),
                                                frame,
                                                sizeof(frame),
                                                &frame_len));
    TEST_ASSERT_EQUAL_SIZE_T(PROTOCOL_FRAME_HEADER_SIZE + strlen(json), frame_len);

    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, frame, frame_len));

    char out_json[256];
    size_t out_len = 0U;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              out_json,
                                              sizeof(out_json),
                                              &out_len));
    TEST_ASSERT_EQUAL_SIZE_T(strlen(json), out_len);
    TEST_ASSERT_EQUAL_STRING(json, out_json);
    TEST_ASSERT_EQUAL_SIZE_T(0U, g_fb.len);

    /* Buffer is now empty -> should request more bytes */
    TEST_ASSERT_EQUAL_INT(PROTO_NEED_MORE,
                          protocol_next_frame(&g_fb,
                                              out_json,
                                              sizeof(out_json),
                                              &out_len));
}

static void test_tcp_fragmentation_byte_by_byte(void)
{
    const char *json = "{\"type\":\"MOVE\",\"request_id\":4,\"match_id\":90,\"expected_ply\":0,\"from\":\"e2\",\"to\":\"e4\"}";
    uint8_t frame[256];
    size_t frame_len = 0U;

    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_encode_frame(json,
                                                strlen(json),
                                                frame,
                                                sizeof(frame),
                                                &frame_len));

    char out_json[256];
    size_t out_len = 0U;

    /* Feed every single byte up to frame_len - 1: must always return PROTO_NEED_MORE */
    for (size_t i = 0U; i < frame_len - 1U; i++) {
        TEST_ASSERT_EQUAL_INT(PROTO_OK,
                              protocol_buffer_append(&g_fb, &frame[i], 1U));
        TEST_ASSERT_EQUAL_INT(PROTO_NEED_MORE,
                              protocol_next_frame(&g_fb,
                                                  out_json,
                                                  sizeof(out_json),
                                                  &out_len));
    }

    /* Feed the final byte -> full frame must now be extracted */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, &frame[frame_len - 1U], 1U));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              out_json,
                                              sizeof(out_json),
                                              &out_len));
    TEST_ASSERT_EQUAL_SIZE_T(strlen(json), out_len);
    TEST_ASSERT_EQUAL_STRING(json, out_json);
    TEST_ASSERT_EQUAL_SIZE_T(0U, g_fb.len);
}

static void test_tcp_coalescing_multiple_frames_in_one_recv(void)
{
    const char *msgs[3] = {
        "{\"type\":\"LOGIN\",\"request_id\":1,\"username\":\"p1\",\"password\":\"pass1234\"}",
        "{\"type\":\"CHALLENGE\",\"request_id\":2,\"target_user_id\":12}",
        "{\"type\":\"RESIGN\",\"request_id\":5,\"match_id\":90}"
    };

    uint8_t combined[1024];
    size_t combined_len = 0U;

    for (int i = 0; i < 3; i++) {
        size_t flen = 0U;
        TEST_ASSERT_EQUAL_INT(PROTO_OK,
                              protocol_encode_frame(msgs[i],
                                                    strlen(msgs[i]),
                                                    combined + combined_len,
                                                    sizeof(combined) - combined_len,
                                                    &flen));
        combined_len += flen;
    }

    /* Append all 3 frames at once (simulating TCP packet coalescing) */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, combined, combined_len));

    char out_json[256];
    for (int i = 0; i < 3; i++) {
        size_t out_len = 0U;
        TEST_ASSERT_EQUAL_INT(PROTO_OK,
                              protocol_next_frame(&g_fb,
                                                  out_json,
                                                  sizeof(out_json),
                                                  &out_len));
        TEST_ASSERT_EQUAL_SIZE_T(strlen(msgs[i]), out_len);
        TEST_ASSERT_EQUAL_STRING(msgs[i], out_json);
    }

    TEST_ASSERT_EQUAL_SIZE_T(0U, g_fb.len);
    TEST_ASSERT_EQUAL_INT(PROTO_NEED_MORE,
                          protocol_next_frame(&g_fb,
                                              out_json,
                                              sizeof(out_json),
                                              NULL));
}

static void test_frame_boundary_and_error_cases(void)
{
    uint8_t buf[64];
    char out_json[64];

    /* Invalid arguments for encode / decode */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_encode_frame(NULL, 5U, buf, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_encode_frame("{}", 2U, NULL, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_next_frame(NULL, out_json, sizeof(out_json), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_next_frame(&g_fb, NULL, sizeof(out_json), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_next_frame(&g_fb, out_json, 0U, NULL));

    /* Payload size 0 or > 65536 rejected on encode */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_FRAME_TOO_LARGE,
                          protocol_encode_frame("", 0U, buf, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_FRAME_TOO_LARGE,
                          protocol_encode_frame("{}",
                                                PROTOCOL_MAX_PAYLOAD_SIZE + 1U,
                                                buf,
                                                sizeof(buf),
                                                NULL));

    /* Destination buffer too small on encode */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_encode_frame("{\"type\":\"OK\"}", 13U, buf, 10U, NULL));

    /* Frame header in stream declares 0 length -> PROTO_ERR_FRAME_TOO_LARGE */
    uint8_t zero_hdr[4] = { 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, zero_hdr, sizeof(zero_hdr)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_FRAME_TOO_LARGE,
                          protocol_next_frame(&g_fb, out_json, sizeof(out_json), NULL));

    /* Frame header in stream declares 70000 (> 65536) -> PROTO_ERR_FRAME_TOO_LARGE */
    protocol_buffer_reset(&g_fb);
    uint8_t oversized_hdr[4];
    protocol_write_be32(oversized_hdr, 70000U);
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb,
                                                 oversized_hdr,
                                                 sizeof(oversized_hdr)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_FRAME_TOO_LARGE,
                          protocol_next_frame(&g_fb, out_json, sizeof(out_json), NULL));

    /* Valid frame in buffer, but caller's out_json capacity is too small */
    protocol_buffer_reset(&g_fb);
    const char *small_json = "{\"type\":\"OK\"}";
    size_t flen = 0U;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_encode_frame(small_json,
                                                strlen(small_json),
                                                buf,
                                                sizeof(buf),
                                                &flen));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_buffer_append(&g_fb, buf, flen));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_next_frame(&g_fb, out_json, 5U, NULL));

    /* Maximum allowed 65536-byte payload succeeds */
    protocol_buffer_reset(&g_fb);
    char *max_payload = (char *)malloc(PROTOCOL_MAX_PAYLOAD_SIZE);
    uint8_t *max_frame = (uint8_t *)malloc(PROTOCOL_MAX_FRAME_SIZE);
    char *max_out = (char *)malloc(PROTOCOL_MAX_PAYLOAD_SIZE + 1U);
    TEST_ASSERT_NOT_NULL(max_payload);
    TEST_ASSERT_NOT_NULL(max_frame);
    TEST_ASSERT_NOT_NULL(max_out);

    memset(max_payload, 'a', PROTOCOL_MAX_PAYLOAD_SIZE);
    size_t max_flen = 0U;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_encode_frame(max_payload,
                                                PROTOCOL_MAX_PAYLOAD_SIZE,
                                                max_frame,
                                                PROTOCOL_MAX_FRAME_SIZE,
                                                &max_flen));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_buffer_append(&g_fb, max_frame, max_flen));
    size_t max_out_len = 0U;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              max_out,
                                              PROTOCOL_MAX_PAYLOAD_SIZE + 1U,
                                              &max_out_len));
    TEST_ASSERT_EQUAL_SIZE_T(PROTOCOL_MAX_PAYLOAD_SIZE, max_out_len);

    free(max_payload);
    free(max_frame);
    free(max_out);
}

static void test_message_type_and_error_code_mappings(void)
{
    const ProtocolMsgType all_types[] = {
        MSG_REGISTER, MSG_LOGIN, MSG_LOGOUT, MSG_LIST_READY,
        MSG_CHALLENGE, MSG_CHALLENGE_REPLY, MSG_MOVE, MSG_RESIGN,
        MSG_HISTORY, MSG_GET_LOG, MSG_OK, MSG_ERROR,
        MSG_READY_LIST, MSG_HISTORY_LIST, MSG_CHALLENGE_RECEIVED,
        MSG_CHALLENGE_UPDATED, MSG_MATCH_STARTED, MSG_MOVE_APPLIED,
        MSG_MATCH_RESULT, MSG_LOG_BEGIN, MSG_LOG_CHUNK, MSG_LOG_END
    };

    for (size_t i = 0U; i < sizeof(all_types) / sizeof(all_types[0]); i++) {
        const char *str = protocol_msg_type_to_str(all_types[i]);
        TEST_ASSERT_NOT_NULL(str);
        TEST_ASSERT_EQUAL_INT(all_types[i], protocol_msg_type_from_str(str));
    }

    TEST_ASSERT_EQUAL_STRING("UNKNOWN", protocol_msg_type_to_str(MSG_UNKNOWN));
    TEST_ASSERT_EQUAL_INT(MSG_UNKNOWN, protocol_msg_type_from_str(NULL));
    TEST_ASSERT_EQUAL_INT(MSG_UNKNOWN, protocol_msg_type_from_str("NO_SUCH_TYPE"));

    const ProtocolErrorCode all_codes[] = {
        ERR_AUTH_REQUIRED, ERR_BAD_CREDENTIALS, ERR_ALREADY_ONLINE,
        ERR_INVALID_REQUEST, ERR_NOT_FOUND, ERR_FORBIDDEN,
        ERR_BUSY, ERR_RANK_DISTANCE, ERR_CHALLENGE_EXPIRED,
        ERR_NOT_YOUR_TURN, ERR_STALE_STATE, ERR_ILLEGAL_MOVE,
        ERR_MATCH_FINISHED, ERR_STORAGE_ERROR
    };

    for (size_t i = 0U; i < sizeof(all_codes) / sizeof(all_codes[0]); i++) {
        const char *cstr = protocol_error_code_to_str(all_codes[i]);
        TEST_ASSERT_NOT_NULL(cstr);
        TEST_ASSERT_EQUAL_INT(all_codes[i], protocol_error_code_from_str(cstr));
    }

    TEST_ASSERT_EQUAL_STRING("NONE", protocol_error_code_to_str(ERR_NONE));
    TEST_ASSERT_EQUAL_STRING("INVALID_REQUEST",
                             protocol_error_code_to_str((ProtocolErrorCode)999));
    TEST_ASSERT_EQUAL_INT(ERR_NONE, protocol_error_code_from_str(NULL));
    TEST_ASSERT_EQUAL_INT(ERR_NONE, protocol_error_code_from_str("UNKNOWN_CODE"));
}

static void test_json_validation_and_escaping(void)
{
    const char *valid = "  { \"type\":\"OK\", \"nested\":{\"arr\":[1,2,3]}, \"msg\":\"a\\\"b\\\\c\\n\" } ";
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_validate_json(valid, strlen(valid)));

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG, protocol_validate_json(NULL, 5U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("", 0U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("[]", 2U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":1", 6U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":1}}{}", 10U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":[1]}]", 10U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":\"unclosed}", 15U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":\"bad\\xesc\"}", 16U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\"a\":\"raw\nline\"}", 16U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON, protocol_validate_json("{\0}", 3U));

    /* Escaping special characters for LOG_CHUNK data */
    char escaped[128];
    const char raw_log[] = "Line1\nLine2\r\t\"quote\"\\slash\b\f\x01";
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_escape_string(raw_log,
                                                      escaped,
                                                      sizeof(escaped)));
    TEST_ASSERT_EQUAL_STRING("Line1\\nLine2\\r\\t\\\"quote\\\"\\\\slash\\b\\f\\u0001",
                             escaped);

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_escape_string(NULL, escaped, sizeof(escaped)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_escape_string("abc", NULL, 10U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_escape_string("abc", escaped, 0U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_json_escape_string("hello\"world", escaped, 6U));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_json_escape_string("\"\"", escaped, 3U));
}

static void test_json_field_extractors_and_header_parser(void)
{
    const char *json =
        "{\"inner\":{\"type\":\"IGNORED\",\"arr\":[\"x\"]},"
        "\"type\":\"MATCH_RESULT\","
        "\"request_id\":42,"
        "\"white_delta\":-3,"
        "\"black_delta\":3,"
        "\"log_available\":true,"
        "\"aborted\":false,"
        "\"note\":\"escaped:\\\"\\\\\\/\\n\\r\\t\\b\\f\"}";

    ProtocolMsgType msg_type = MSG_UNKNOWN;
    int64_t req_id = 0;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_parse_header(json, strlen(json), &msg_type, &req_id));
    TEST_ASSERT_EQUAL_INT(MSG_MATCH_RESULT, msg_type);
    TEST_ASSERT_EQUAL_INT64(42, req_id);

    int64_t w_delta = 0;
    int64_t b_delta = 0;
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_json_get_int(json, "white_delta", &w_delta));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_json_get_int(json, "black_delta", &b_delta));
    TEST_ASSERT_EQUAL_INT64(-3, w_delta);
    TEST_ASSERT_EQUAL_INT64(3, b_delta);

    int log_avail = 0;
    int aborted = 1;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_get_bool(json, "log_available", &log_avail));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_get_bool(json, "aborted", &aborted));
    TEST_ASSERT_EQUAL_INT(1, log_avail);
    TEST_ASSERT_EQUAL_INT(0, aborted);

    char note[64];
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_get_string(json, "note", note, sizeof(note)));
    TEST_ASSERT_EQUAL_STRING("escaped:\"\\/\n\r\t\b\f", note);

    /* Missing fields and type mismatch errors */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_MISSING_FIELD,
                          protocol_json_get_string(json, "missing", note, sizeof(note)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON,
                          protocol_json_get_string(json, "request_id", note, sizeof(note)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_BUFFER_OVERFLOW,
                          protocol_json_get_string(json, "type", note, 5U));

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_MISSING_FIELD,
                          protocol_json_get_int(json, "missing", &w_delta));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON,
                          protocol_json_get_int(json, "type", &w_delta));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON,
                          protocol_json_get_int("{\"bad\":12abc}", "bad", &w_delta));

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_MISSING_FIELD,
                          protocol_json_get_bool(json, "missing", &log_avail));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON,
                          protocol_json_get_bool(json, "type", &log_avail));

    /* NULL argument checks for extractors */
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_get_string(NULL, "k", note, sizeof(note)));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_get_int(NULL, "k", &w_delta));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_json_get_bool(NULL, "k", &log_avail));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_parse_header(NULL, 2U, &msg_type, NULL));

    /* Event without request_id sets out_request_id = -1 */
    const char *evt = "{\"type\":\"CHALLENGE_RECEIVED\",\"challenge_id\":10}";
    req_id = 99;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_parse_header(evt, strlen(evt), &msg_type, &req_id));
    TEST_ASSERT_EQUAL_INT(MSG_CHALLENGE_RECEIVED, msg_type);
    TEST_ASSERT_EQUAL_INT64(-1, req_id);

    /* Unknown message type and invalid request_id */
    const char *unk = "{\"type\":\"PING\"}";
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_UNKNOWN_TYPE,
                          protocol_parse_header(unk, strlen(unk), &msg_type, &req_id));
    const char *bad_req = "{\"type\":\"LOGIN\",\"request_id\":\"not_int\"}";
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_JSON,
                          protocol_parse_header(bad_req,
                                                strlen(bad_req),
                                                &msg_type,
                                                &req_id));
}

static void test_build_ok_and_error_frames(void)
{
    uint8_t frame[512];
    size_t frame_len = 0U;
    char json_out[512];
    size_t json_len = 0U;

    /* 1. Build ERROR frame with request_id */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_error_frame(7,
                                                     ERR_RANK_DISTANCE,
                                                     "Rank difference > 10",
                                                     frame,
                                                     sizeof(frame),
                                                     &frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_buffer_append(&g_fb, frame, frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              json_out,
                                              sizeof(json_out),
                                              &json_len));

    ProtocolMsgType mtype = MSG_UNKNOWN;
    int64_t req_id = -1;
    char code_buf[32];
    char msg_buf[64];
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_parse_header(json_out, json_len, &mtype, &req_id));
    TEST_ASSERT_EQUAL_INT(MSG_ERROR, mtype);
    TEST_ASSERT_EQUAL_INT64(7, req_id);
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_get_string(json_out,
                                                   "code",
                                                   code_buf,
                                                   sizeof(code_buf)));
    TEST_ASSERT_EQUAL_STRING("RANK_DISTANCE", code_buf);
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_json_get_string(json_out,
                                                   "message",
                                                   msg_buf,
                                                   sizeof(msg_buf)));
    TEST_ASSERT_EQUAL_STRING("Rank difference > 10", msg_buf);

    /* 2. Build ERROR frame without request_id and NULL message (defaults to code) */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_error_frame(-1,
                                                     ERR_INVALID_REQUEST,
                                                     NULL,
                                                     frame,
                                                     sizeof(frame),
                                                     &frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_buffer_append(&g_fb, frame, frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              json_out,
                                              sizeof(json_out),
                                              &json_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_parse_header(json_out, json_len, &mtype, &req_id));
    TEST_ASSERT_EQUAL_INT(MSG_ERROR, mtype);
    TEST_ASSERT_EQUAL_INT64(-1, req_id);

    /* 3. Build OK frame with request_id and extra fields */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_ok_frame(1,
                                                  "\"user_id\":5,\"score\":12",
                                                  frame,
                                                  sizeof(frame),
                                                  &frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_buffer_append(&g_fb, frame, frame_len));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_next_frame(&g_fb,
                                              json_out,
                                              sizeof(json_out),
                                              &json_len));
    int64_t uid = 0;
    int64_t score = 0;
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_parse_header(json_out, json_len, &mtype, &req_id));
    TEST_ASSERT_EQUAL_INT(MSG_OK, mtype);
    TEST_ASSERT_EQUAL_INT64(1, req_id);
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_json_get_int(json_out, "user_id", &uid));
    TEST_ASSERT_EQUAL_INT(PROTO_OK, protocol_json_get_int(json_out, "score", &score));
    TEST_ASSERT_EQUAL_INT64(5, uid);
    TEST_ASSERT_EQUAL_INT64(12, score);

    /* 4. Build OK frame variants (no extra fields, no request_id) */
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_ok_frame(2, NULL, frame, sizeof(frame), NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_ok_frame(-1,
                                                  "\"status\":\"ready\"",
                                                  frame,
                                                  sizeof(frame),
                                                  NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_OK,
                          protocol_build_ok_frame(-1, NULL, frame, sizeof(frame), NULL));

    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_build_error_frame(1, ERR_BUSY, NULL, NULL, 0U, NULL));
    TEST_ASSERT_EQUAL_INT(PROTO_ERR_INVALID_ARG,
                          protocol_build_ok_frame(1, NULL, NULL, 0U, NULL));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_buffer_lifecycle_and_validation);
    RUN_TEST(test_be32_read_write_roundtrip);
    RUN_TEST(test_encode_and_decode_single_frame);
    RUN_TEST(test_tcp_fragmentation_byte_by_byte);
    RUN_TEST(test_tcp_coalescing_multiple_frames_in_one_recv);
    RUN_TEST(test_frame_boundary_and_error_cases);
    RUN_TEST(test_message_type_and_error_code_mappings);
    RUN_TEST(test_json_validation_and_escaping);
    RUN_TEST(test_json_field_extractors_and_header_parser);
    RUN_TEST(test_build_ok_and_error_frames);
    return UNITY_END();
}
