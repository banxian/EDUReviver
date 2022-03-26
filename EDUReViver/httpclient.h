#include <stdint.h>

int request_payload_online(const char* sn, const char* uid, const char* signature, const char* payloadname, char** reply, size_t* replylen);