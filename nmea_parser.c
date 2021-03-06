#include "nmea_parser.h"

#include <stdlib.h>
#include <string.h>

#define NMEA_DEBUG
// #define NMEA_NO_ASSERT // Targeted to platforms, which are not supporting assert.h

#if !defined NMEA_NO_ASSERT && defined NMEA_DEBUG
#include <assert.h>
#else
#ifdef NMEA_DEBUG
#define assert(condition) if(!(condition)) return NMEA_ASSERTION_FAILED
#else 
#define assert(condition)
#define ucassert(condition)
#endif
#endif

#if NMEA_MESSAGE_MAX_LENGTH != 82
#warning NMEA_MESSAGE_MAX_LENGTH set to non - standard value
#endif

// Helper functions

int nmea_nullstr(char* str, size_t length) {
    assert(str != NULL);
    while(length != 0) {
        *str++ = '\0';
        --length;
    }
    return NMEA_SUCCESS;
}

int nmea_strncpy_s(char* dst, const char* src, size_t length) {
    assert(dst != NULL);
    assert(src != NULL);
    while(*src && length != 0) {
        *dst++ = *src++;
        --length;
    }
    *dst = '\0';
    return NMEA_SUCCESS;
}

inline int nmea_checksum_add_c(const int checksum, const int c) {
    return checksum ^ c;
}

#ifndef NMEA_MINIMUM_BUILD

int nmea_checksum_add_str(int checksum, const char* str) {
    assert(str != NULL);
    while(*str) {
        checksum = nmea_checksum_add_c(checksum, *str++);
    }
    return checksum;
}

#endif

int nmea_message_string_checksum(const char* begin, const char* end) { // Without "$", "I", and "*"
    assert(begin != NULL);
    assert(end != NULL);
    int checksum = 0;
    while(*begin && begin != end) {
        checksum = nmea_checksum_add_c(checksum, *begin++);
    }
    return checksum;
}

#ifndef NMEA_MINIMUM_BUILD

int nmea_message_checksum(const nmea_message* message) { // Without "$", "I", and "*"
    assert(message != NULL);

    int checksum = nmea_checksum_add_str(0, message->talker_id);
    checksum = nmea_checksum_add_str(checksum, message->type_code);
    checksum = nmea_checksum_add_c(checksum, ',');

    nmea_value* value_ptr = message->first_value;
    while(value_ptr != NULL) {
        checksum = nmea_checksum_add_str(checksum, value_ptr->value);
        if(value_ptr->next_value != NULL) {
            checksum = nmea_checksum_add_c(checksum, ',');
        }
        value_ptr = value_ptr->next_value;
    }
    return checksum;
}

#endif

// NMEA parser functions

nmea_message* nmea_init_message() {
    nmea_message* message = (nmea_message*)malloc(sizeof(nmea_message));
    if(message == NULL) {
        return NULL;
    }

    nmea_nullstr(message->talker_id, sizeof(message->talker_id));
    nmea_nullstr(message->type_code, sizeof(message->type_code));
    message->value_count = 0;
    message->first_value = NULL;
    message->last_value = NULL;

    return message;
}

int nmea_destroy_message(nmea_message** message) {
    assert(message != NULL);
    assert(*message != NULL);

    nmea_value* value_ptr = (*message)->first_value;
    while(value_ptr) {
        free(value_ptr->value);
        value_ptr->value = NULL;
        value_ptr = value_ptr->next_value;
    }

    (*message)->first_value = NULL;
    (*message)->last_value = NULL;
    (*message)->value_count = 0;

    free(*message);
    *message = NULL;
    return NMEA_SUCCESS;
}

int nmea_message_length(const nmea_message* message, size_t* message_length) {
    assert(message != NULL);
    assert(message_length != NULL);

    *message_length = 7; // $ + talker_id + type_id + ,
    nmea_value* value_ptr = message->first_value;
    while(value_ptr != NULL) {
        *message_length += (int)value_ptr->value_length;
        if(value_ptr->next_value != NULL) {
            *message_length += 1;
        }
        value_ptr = value_ptr->next_value;
    }

    *message_length += 5; // * + 2HEX + \r\n

    return NMEA_SUCCESS;
}

int nmea_add_value(nmea_message* message, const char* str_value, const size_t str_value_length) {
    assert(message != NULL);
    assert(str_value != NULL);
    if(message->first_value == NULL) {
        assert(message->last_value == NULL);
        message->first_value = (nmea_value*)malloc(sizeof(nmea_value));
        if((message->first_value->value = (char*)malloc(str_value_length + 1)) == NULL) {
            return NMEA_ALLOCATION_ERROR;
        }
        nmea_strncpy_s(message->first_value->value, str_value, str_value_length);
        message->first_value->value_length = str_value_length;
        message->first_value->next_value = NULL;
        message->last_value = message->first_value;
    } else {
        assert(message->last_value != NULL);
        assert(message->last_value->next_value == NULL);
        message->last_value->next_value = (nmea_value*)malloc(sizeof(nmea_value));
        if((message->last_value->next_value->value = (char*)malloc(str_value_length + 1)) == NULL) {
            return NMEA_ALLOCATION_ERROR;
        }
        nmea_strncpy_s(message->last_value->next_value->value, str_value, str_value_length);
        message->last_value->next_value->value_length = str_value_length;
        message->last_value->next_value->next_value = NULL;
        message->last_value = message->last_value->next_value;
    }

    ++message->value_count;

    return NMEA_SUCCESS;
}

int nmea_parse_message(const char* str
    , nmea_message** message
    , size_t* message_end_index
    , const int strict
    , int(*vendor_ext_msg_handler)(const char* str, size_t* message_end_index, const int strict)) {
    assert(str != NULL);
    assert(message != NULL);
    assert(message_end_index != NULL);

    *message = NULL;
    *message_end_index = 0;

    const char* begin = strstr(str, "$");
    if(begin == NULL) {
        if((begin = strstr(str, "!")) != NULL) {
            ++begin;
            return NMEA_UNSUPPORTED_MESSAGE; // ! message not supported yet
        }
        return NMEA_MESSAGE_BEGIN_DELIMITER_NOT_FOUND;
    } else {
        ++begin;

        const char* end = strstr(begin, "\r");
        if(end == NULL && (end = strstr(begin, "\n")) == NULL) {
            return NMEA_MESSAGE_END_DELIMITER_NOT_FOUND;
        }

        if(strict != 0 && (end - begin) > NMEA_MESSAGE_MAX_LENGTH) { // Message length (including $ and \n) should be 82 characters (for NMEA 0183)
            return NMEA_MESSAGE_LONGER_THAN_MAX_LENGTH;
        }

        assert(str <= end);
        *message_end_index = (size_t)((end - str) + 1); // To skip \n

        const char* checksumDelimiter = strstr(begin, "*");
        if(checksumDelimiter != NULL) { // Checksum is optional
            if(checksumDelimiter < end /* For cases that checksum belongs to next message */) {
                if(strict != 0 && (end - checksumDelimiter - 1) != 2) { // Checksum hex value should be two digits long
                    return NMEA_INCORRECT_CHECKSUM_LENGTH;
                }

                if(strtol(checksumDelimiter + 1, NULL, 16) != nmea_message_string_checksum(begin, checksumDelimiter)) {
                    return NMEA_CHECKSUM_ERROR;
                }
                end = checksumDelimiter;
            }
            checksumDelimiter = NULL;
        }

        const char* previousFieldDelimiter = strstr(begin, ",");
        if(previousFieldDelimiter == NULL) { // This should not happen..
            return NMEA_IMPLEMENTATION_ERROR;
        }

        if((previousFieldDelimiter - begin) != 5) { // Tag + type code should be 5 characters long
            return NMEA_MESSAGE_ID_LENGTH_INCORRECT;
        }

        if(strict != 0 && (*(begin + 2) == 'R' && *(begin + 3) == 'M' && checksumDelimiter == NULL)) { // Checksum is compulsory for XXRMX messages (Or just form XXRMA, XXRMB and XXRMC???)
            return NMEA_MESSAGE_CHECKSUM_EXPECTED;
        }

        if((*message = nmea_init_message()) == NULL) {
            return NMEA_ALLOCATION_ERROR;
        }

        (*message)->first_value = NULL;
        (*message)->last_value = NULL;

        nmea_strncpy_s((*message)->talker_id, begin, 2);
        nmea_strncpy_s((*message)->type_code, begin + 2, 3);

        if(*(*message)->talker_id == 'P') { // Vendor extension message (not standardized)
            if(vendor_ext_msg_handler != NULL) {
                return vendor_ext_msg_handler(begin, message_end_index, strict);
            }
            return NMEA_UNHANDLED_VENDOR_EXT_MESSAGE;
        }

        const char* fieldDelimiter = NULL;
        while((((fieldDelimiter = strstr(previousFieldDelimiter + 1, ",")) != NULL && fieldDelimiter <= end)
            || ((fieldDelimiter = strstr(previousFieldDelimiter + 1, "*")) != NULL) && fieldDelimiter <= end)) {
            nmea_add_value(*message, previousFieldDelimiter + 1, fieldDelimiter - previousFieldDelimiter - 1);
            previousFieldDelimiter = fieldDelimiter;
        }

        return NMEA_SUCCESS;
    }
}

// NMEA message tools

int nmea_checksum_digit_to_string(const int digit, char* output) {
    assert(output != NULL);
    *output = (char)(digit > 9 ? (0x41 + digit - 10) : (0x30 + digit));
    return NMEA_SUCCESS;
}

int nmea_checksum_to_string(const int number, char* output) {
    assert(output != NULL);
    int return_value = nmea_checksum_digit_to_string(number >> 4, output);
    if(return_value != NMEA_SUCCESS) {
        return return_value;
    }

    return_value = nmea_checksum_digit_to_string(number & 0x0F, output + 1);
    if(return_value != NMEA_SUCCESS) {
        return return_value;
    }

    return NMEA_SUCCESS;
}

int nmea_message_to_string(const nmea_message* message, char** message_str) {
    assert(message != NULL);
    assert(message_str != NULL);

    size_t message_length = 0;
    int return_value = nmea_message_length(message, &message_length);
    if(return_value != NMEA_SUCCESS) {
        return return_value;
    }

    if((*message_str = (char*)malloc(message_length + 1)) == NULL) {
        return NMEA_ALLOCATION_ERROR;
    }

    *(*message_str + message_length) = '\0';

    char* current_ptr = *message_str;
    if((return_value = nmea_strncpy_s(current_ptr, "$", 1)) != NMEA_SUCCESS) {
        return return_value;
    }

    if((return_value = nmea_strncpy_s(current_ptr += 1, message->talker_id, 2)) != NMEA_SUCCESS) {
        return return_value;
    }

    if((return_value = nmea_strncpy_s(current_ptr += 2, message->type_code, 3)) != NMEA_SUCCESS) {
        return return_value;
    }

    if((return_value = nmea_strncpy_s(current_ptr += 3, ",", 1)) != NMEA_SUCCESS) {
        return return_value;
    }

    nmea_value* value_ptr = message->first_value;
    while(value_ptr != NULL) {
        if((return_value = nmea_strncpy_s(current_ptr += 1, value_ptr->value, value_ptr->value_length)) != NMEA_SUCCESS) {
            return return_value;
        }

        if(value_ptr->next_value != NULL) {
            if((return_value = nmea_strncpy_s(current_ptr += value_ptr->value_length, ",", 1)) != NMEA_SUCCESS) {
                return return_value;
            }
        } else {
            if((return_value = nmea_strncpy_s(current_ptr += value_ptr->value_length, "*", 1)) != NMEA_SUCCESS) {
                return return_value;
            }
        }
        value_ptr = value_ptr->next_value;
    }

    if((return_value = nmea_checksum_to_string(nmea_message_checksum(message), current_ptr += 1)) != NMEA_SUCCESS) {
        return return_value;
    }

    if((return_value = nmea_strncpy_s(current_ptr += 2, "\r\n", 2)) != NMEA_SUCCESS) {
        return return_value;
    }

    return NMEA_SUCCESS;
}

int nmea_destroy_message_string(char** message_str) {
    assert(message_str != NULL);
    free(*message_str);
    *message_str = NULL;
    return NMEA_SUCCESS;
}