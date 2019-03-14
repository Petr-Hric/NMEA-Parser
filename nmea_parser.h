#ifndef NMEA_H_
#define NMEA_H_

#include <stddef.h>

typedef struct nmea_value nmea_value;

struct nmea_value {
    char* value;
    nmea_value* next_value;
};

typedef struct {
    char talker_id[3]; // 2 + \0
    char type_code[4]; // 3 + \0
    nmea_value* first_value;
    nmea_value* last_value;
} nmea_message;

#define NMEA_MESSAGE_MAX_LENGTH 82

#define NMEA_SUCCESS 0
#define NMEA_ALLOCATION_ERROR -1
#define NMEA_CHECKSUM_ERROR -2
#define NMEA_UNSUPPORTED_MESSAGE -3
#define NMEA_MESSAGE_BEGIN_DELIMITER_NOT_FOUND -4
#define NMEA_MESSAGE_END_DELIMITER_NOT_FOUND -5
#define NMEA_MESSAGE_CHECKSUM_DELIMITER_NOT_FOUND -6
#define NMEA_MESSAGE_LONGER_THAN_MAX_LENGTH -7
#define NMEA_INCORRECT_CHECKSUM_LENGTH -8
#define NMEA_MESSAGE_CHECKSUM_EXPECTED -9
#define NMEA_IMPLEMENTATION_ERROR -10
#define NMEA_MESSAGE_ID_LENGTH_INCORRECT -11
#define NMEA_UNHANDLED_VENDOR_EXT_MESSAGE -12
#define NMEA_UNKNOWN_ERROR -13

/* Do not change! */

#if NMEA_MESSAGE_MAX_LENGTH != 82
#warning NMEA_MESSAGE_MAX_LENGTH set to non - standard value
#endif

void nmea_destroy_message(nmea_message** message);

int nmea_parse_message(const char* str // Input string
    , nmea_message** message // Output
    , size_t* message_end_index // Message end index (Next message begin index)
    , const int strict // Strictly comply the standard
    , int(*vendor_ext_msg_handler)(const char* str, size_t* message_end_index, const int strict)); // Non-standard (vendor ext message) message handler; Can be NULL

#endif