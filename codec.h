#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>

#define RECORD_SIZE 4

typedef enum {
    EVENT_NONE = 0,
    EVENT_NO2  = 1,
    EVENT_BARO = 2,
    EVENT_TEMP = 3
} EventType;

// One pending slow-sensor update 
typedef struct {
    uint8_t pending;
    uint8_t value;
    uint32_t sample_time_ms;
} PendingEvent;

// Encoder state 
typedef struct {
    PendingEvent no2;
    PendingEvent baro;
    PendingEvent temp;

    uint32_t frame_number;
} EncoderState;

// Decoder state 
typedef struct {
    uint32_t frame_number;

    uint32_t next_no2_time_ms;
    uint32_t next_baro_time_ms;
    uint32_t next_temp_time_ms;

    uint8_t no2;
    uint8_t baro;
    uint8_t temp;
} DecoderState;

 // Initialise encoder/decoder state
void encoder_init(EncoderState *state);
void decoder_init(DecoderState *state);


// Encode one 100-ms record into exactly 4 bytes.
void encode(
    EncoderState *state,
    uint16_t timestamp_msec,
    uint8_t no2_data,
    uint8_t baro_data,
    int8_t temp_data,
    float acceleration,
    uint8_t output[RECORD_SIZE]
);


/*
 * Decode one 4-byte record.
 * The decoded acceleration is returned for the current frame.
 * Slow-sensor events are reconstructed according to their original sampling schedules.
 */
void decode(
    DecoderState *state,
    const uint8_t input[RECORD_SIZE],
    uint16_t *timestamp_msec,
    uint8_t *no2_data,
    uint8_t *baro_data,
    int8_t *temp_data,
    float *acceleration
);

#endif
