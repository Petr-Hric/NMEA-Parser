#include "nmea_parser.h"

#include <stdio.h>

int main() {
    static const char testString[] =
        "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76\n"
        "$GPGSA,A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38*0A              \n"
        "$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70  \n"
        "$GPGSV,3,2,11,02,39,223,19,13,28,070,17,26,23,252,,04,14,186,14*79    \n"
        "$GPGSV,3,3,11,29,09,301,24,16,09,020,,36,,,*76                        \n"
        "$GPRMC,092750.000,A,5321.6802,N,00630.3372,W,0.02,31.66,280511,,,A*43 \n"
        "$GPGGA,092751.000,5321.6802,N,00630.3371,W,1,8,1.03,61.7,M,55.3,M,,*75\n"
        "$GPGSA,A,3,10,07,05,02,29,04,08,13,,,,,1.72,1.03,1.38*0A              \n"
        "$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70  \n"
        "$GPGSV,3,2,11,02,39,223,16,13,28,070,17,26,23,252,,04,14,186,15*77    \n"
        "$GPGSV,3,3,11,29,09,301,24,16,09,020,,36,,,*76                        \n"
        "$GPRMC,092751.000,A,5321.6802,N,00630.3371,W,0.06,31.66,280511,,,A*45 \n";

    size_t messageEndIndex = 0;
    size_t lastMessageEndIndex = 0;
    nmea_message* message;
    while(nmea_parse_message(testString + lastMessageEndIndex, &message, &messageEndIndex, 0, NULL) == NMEA_SUCCESS) {
        printf("Value count: %zu\n", message->value_count);

        char* test_str;
        nmea_message_to_string(message, &test_str);

        printf("%s\n\n", test_str);

        nmea_destroy_message_string(&test_str);

        nmea_destroy_message(&message);

        lastMessageEndIndex += messageEndIndex;

        fputc('\n', stdout);
    }
    return -1;
}