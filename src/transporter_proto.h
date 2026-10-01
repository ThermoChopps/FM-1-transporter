#pragma once

// Mac-facing data channel on the second CDC interface (tools/fm1t.py).
//
// Requests are text lines; replies are text lines, and `read` replies carry
// a raw binary payload:
//
//   ping                  -> PONG
//   status                -> OK uboot=<0|1> loader=<0|1>
//   info                  -> OK key=980F type=3 id=856014 | ERR <why>
//   read <addr> <len>     -> DATA <len>\n <len raw bytes> END <crc32>\n | ERR <why>
//
// Read-only by design: there is no erase or write request.

// core 0: feed bytes received on CDC 1; drain queued output to CDC 1.
void fm1_proto_rx(const char *buf, unsigned len);
void fm1_proto_drain(void);

// core 1: handle a pending request, if any.
void fm1_proto_poll(void);
