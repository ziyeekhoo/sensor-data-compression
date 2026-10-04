#include <stdio.h>
#include <stdint.h>

#include "codec.h"


int main(void)
{
    EncoderState encoder;
    DecoderState decoder;

    encoder_init(&encoder);
    decoder_init(&decoder);


    // Example sensor values.

    uint8_t no2 = 0;
    uint8_t baro = 128;
    int8_t temp = 50;
    float acceleration = 9.81f;


    // Simulate 2 seconds of data.
    

    for (uint16_t t = 0; t < 2000; t += 100) {

        uint8_t packet[RECORD_SIZE];

        /*
         * Change some values to simulate sensor readings.
         */

        acceleration =
            9.81f + ((float)t / 1000.0f);

        if (t == 500)
            no2 = 23;

        if (t == 1000)
            baro = 140;

        if (t == 1000)
            temp = 55;


        /*
         * Encode.
         */

        encode(
            &encoder,
            t,
            no2,
            baro,
            temp,
            acceleration,
            packet
        );


        // Decode

        uint16_t decoded_time;
        uint8_t decoded_no2;
        uint8_t decoded_baro;
        int8_t decoded_temp;
        float decoded_accel;


        decode(
            &decoder,
            packet,
            &decoded_time,
            &decoded_no2,
            &decoded_baro,
            &decoded_temp,
            &decoded_accel
        );


        printf(
            "t=%4u ms | "
            "packet=%02X %02X %02X %02X | "
            "accel=%.5f | "
            "NO2=%u | "
            "Baro=%u | "
            "Temp=%d\n",

            decoded_time,

            packet[0],
            packet[1],
            packet[2],
            packet[3],

            decoded_accel,
            decoded_no2,
            decoded_baro,
            decoded_temp
        );
    }


    return 0;
}
