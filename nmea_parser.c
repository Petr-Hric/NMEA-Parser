#include "nmea_parser.h"

#include <stdlib.h>
#include <string.h>

#include <assert.h>

// Helper functions

void nmea_nullstr(char* str, size_t length) {
    assert(str != NULL);
    while(length != 0) {
        *str++ = '\0';
        --length;
    }
}

void nmea_strncpy_s(char* dst, const char* src, size_t length) {
    assert(dst != NULL);
    assert(src != NULL);
    while(*src && length != 0) {
        *dst++ = *src++;
        --length;
    }
    *dst = '\0';
}

// NMEA parser functions

nmea_message* create_nmea_message() {
    nmea_message* message = (nmea_message*)malloc(sizeof(nmea_message));
    if(message == NULL) {
        return NULL;
    }

    message->first_value = NULL;
    message->last_value = NULL;
    nmea_nullstr(message->talker_id, sizeof(message->talker_id));
    nmea_nullstr(message->type_code, sizeof(message->type_code));

    return message;
}

void nmea_destroy_message(nmea_message** message) {
    assert(message != NULL);
    assert(*message != NULL);

    nmea_value* value_ptr = (*message)->first_value;
    while(value_ptr) {
        free(value_ptr->value);
        value_ptr->value = NULL;
        value_ptr = value_ptr->next_value;
    }

    free(*message);
    *message = NULL;
}

int nmea_add_value_internal(nmea_message* message, const char* str_value, const size_t str_value_length) {
    assert(message != NULL);
    assert(str_value != NULL);
    if(message->first_value == NULL) {
        assert(message->last_value == NULL);
        message->first_value = (nmea_value*)malloc(sizeof(nmea_value));
        if((message->first_value->value = (char*)malloc(str_value_length + 1)) == NULL) {
            return NMEA_ALLOCATION_ERROR;
        }
        nmea_strncpy_s(message->first_value->value, str_value, str_value_length);
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
        message->last_value->next_value->next_value = NULL;
        message->last_value = message->last_value->next_value;
    }
    return NMEA_SUCCESS;
}

int nmea_message_checksum(const char* begin, const char* end) { // Without "$", "I", and "*"
    assert(begin != NULL);
    assert(end != NULL);
    int checkSum = 0;
    while(*begin && begin != end) {
        checkSum ^= *begin++;
    }
    return checkSum;
}

int nmea_parse_message(const char* str
    , nmea_message** message
    , size_t* messageEndIndex
    , const int strict
    , int(*vendor_ext_msg_handler)(const char*, size_t*, const int)) {
    assert(str != NULL);
    assert(message != NULL);
    assert(messageEndIndex != NULL);

    *message = NULL;
    *messageEndIndex = 0;

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
        *messageEndIndex = (size_t)((end - str) + 1); // To skip \n

        const char* checksumDelimiter = strstr(begin, "*");
        if(checksumDelimiter != NULL) { // Checksum is optional
            if(checksumDelimiter < end /* For cases that checksum belongs to next message */) {
                if(strict != 0 && (end - checksumDelimiter - 1) != 2) { // Checksum hex value should be two digits long
                    return NMEA_INCORRECT_CHECKSUM_LENGTH;
                }

                if(strtol(checksumDelimiter + 1, NULL, 16) != nmea_message_checksum(begin, checksumDelimiter)) {
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

        *message = (nmea_message*)malloc(sizeof(nmea_message));
        if(message == NULL) {
            return NMEA_ALLOCATION_ERROR;
        }

        (*message)->first_value = NULL;
        (*message)->last_value = NULL;

        nmea_strncpy_s((*message)->talker_id, begin, 2);
        nmea_strncpy_s((*message)->type_code, begin + 2, 3);

        if(*(*message)->talker_id == 'P') { // Vendor extension message (not standardized)
            if(vendor_ext_msg_handler != NULL) {
                return vendor_ext_msg_handler(begin, messageEndIndex, strict);
            }
            return NMEA_UNHANDLED_VENDOR_EXT_MESSAGE;
        }

        const char* fieldDelimiter = NULL;
        while((((fieldDelimiter = strstr(previousFieldDelimiter + 1, ",")) != NULL && fieldDelimiter <= end)
            || ((fieldDelimiter = strstr(previousFieldDelimiter + 1, "*")) != NULL) && fieldDelimiter <= end)) {
            nmea_add_value_internal(*message, previousFieldDelimiter + 1, fieldDelimiter - previousFieldDelimiter - 1);
            previousFieldDelimiter = fieldDelimiter;
        }

        return NMEA_SUCCESS;
    }
    assert(0);
}