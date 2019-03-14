#ifndef NMEA_H_
#define NMEA_H_

#include <stddef.h>

typedef struct nmea_value nmea_value;

struct nmea_value {
    char* value; // All values are represented as string
    size_t value_length; // Value string length
    nmea_value* next_value; // Next value
};

typedef struct {
    char talker_id[3]; // 2 + NUL
    char type_code[4]; // 3 + NUL
    size_t value_count; // Number of values in message
    nmea_value* first_value; // Pointer to first value in message
    nmea_value* last_value; // Pointer to last value in message for value add optimization
} nmea_message;

/// Function to parse NMEA string

int nmea_parse_message(const char* str // Input string
    , nmea_message** message // Output
    , size_t* message_end_index // Message end index (Next message begin index)
    , const int strict // Strictly comply the standard
    , int(*vendor_ext_msg_handler)(const char* str, size_t* message_end_index, const int strict)); // Non-standard (vendor ext message) message handler; Can be NULL

/// Function to create user message

nmea_message* nmea_init_message();

/// Function to destroy user message

int nmea_destroy_message(nmea_message** message);

/// Function to add value to user message

int nmea_add_value(nmea_message* message, const char* str_value, const size_t str_value_length);

/// Function to convert nmea_message to string

int nmea_message_to_string(const nmea_message* message, char** message_str);

/// Function to destroy user message string

int nmea_destroy_message_string(char** message_str);

/// Possible return values

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
#define NMEA_ASSERTION_FAILED -14

/// Maximum length of NMEA message string (including <CRLF> 

#define NMEA_MESSAGE_MAX_LENGTH 82

#endif