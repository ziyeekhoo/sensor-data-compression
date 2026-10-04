#include "codec.h"

#include <string.h>

/*
 * 22-bit floating-point compression
 * IEEE-754 single precision:
 *     1 sign bit
 *     8 exponent bits
 *    23 fraction bits
 *
 * Keep the upper 22 bits:
 *
 *     1 sign + 8 exponent + 13 fraction
 *
 * The 10 least-significant fraction bits are removed.
 *
 * This preserves the full FP32 exponent range while
 * reducing the stored acceleration from 32 -> 22 bits.
 */

static uint32_t float_to_22bit(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));

    /*
     * Keep bits [31:10].
     * The lower 10 fraction bits are discarded.
     */
    return bits >> 10;
}


static float float_from_22bit(uint32_t compressed)
{
    uint32_t bits;

    bits = compressed << 10;

    float value;
    memcpy(&value, &bits, sizeof(value));

    return value;
}


/*
 * ---------------------------------------------------------
 * Find the oldest pending slow-sensor event.
 *
 * Sending the oldest event first prevents starvation and
 * keeps the reconstruction delay small.
 * ---------------------------------------------------------
 */

static EventType get_oldest_event(
    EncoderState *state,
    PendingEvent **event
)
{
    EventType best_type = EVENT_NONE;
    PendingEvent *best = NULL;

    PendingEvent *events[3] = {
        &state->no2,
        &state->baro,
        &state->temp
    };

    EventType types[3] = {
        EVENT_NO2,
        EVENT_BARO,
        EVENT_TEMP
    };

    for (int i = 0; i < 3; i++) {

        if (!events[i]->pending)
            continue;

        if (best == NULL ||
            events[i]->sample_time_ms < best->sample_time_ms) {

            best = events[i];
            best_type = types[i];
        }
    }

    *event = best;

    return best_type;
}


// Initialise encoder

void encoder_init(EncoderState *state)
{
    memset(state, 0, sizeof(*state));
}


// Initialise decoder

void decoder_init(DecoderState *state)
{
    memset(state, 0, sizeof(*state));
}


/* ENCODER
 * 32-bit packet:
 * [31 ........ 10] [9 ...... 8] [7 ........ 0]
 *   22-bit accel     event type      value
 *
 * Event type:
 * 00 = no event
 * 01 = NO2
 * 10 = barometer
 * 11 = temperature
 *
 * The timestamp is implicit:
 * timestamp = frame_number * 100 ms
 */

void encode(
    EncoderState *state,
    uint16_t timestamp_msec,
    uint8_t no2_data,
    uint8_t baro_data,
    int8_t temp_data,
    float acceleration,
    uint8_t output[RECORD_SIZE]
)
{
    uint32_t acceleration_code;
    uint16_t event_code;
    uint32_t packet;

    /* Capture new sensor samples.
     * The sensors are sampled according to their specified sampling periods.
     */

    if ((timestamp_msec % 500U) == 0U) {

        state->no2.pending = 1;
        state->no2.value = no2_data;
        state->no2.sample_time_ms = timestamp_msec;
    }


    if ((timestamp_msec % 250U) == 0U) {

        state->baro.pending = 1;
        state->baro.value = baro_data;
        state->baro.sample_time_ms = timestamp_msec;
    }


    if ((timestamp_msec % 1000U) == 0U) {

        state->temp.pending = 1;
        state->temp.value = (uint8_t)temp_data;
        state->temp.sample_time_ms = timestamp_msec;
    }


    // Encode acceleration.
     

    acceleration_code = float_to_22bit(acceleration);


    // Select one slow-sensor event.

    PendingEvent *event = NULL;

    EventType type = get_oldest_event(state, &event);

    event_code = 0;


    if (type != EVENT_NONE && event != NULL) {

        /*
         * 2-bit event type + 8-bit sensor value
         */
        event_code =
            ((uint16_t)type << 8) |
            event->value;

        /*
         * This event has now been transmitted.
         */
        event->pending = 0;
    }


    /* Combine:
     * 22-bit acceleration
     * 10-bit event
     * = 32 bits
     */

    packet =
        (acceleration_code << 10) |
        (event_code & 0x03FFU);


    // Store as four bytes.

    output[0] = (uint8_t)((packet >> 24) & 0xFF);
    output[1] = (uint8_t)((packet >> 16) & 0xFF);
    output[2] = (uint8_t)((packet >> 8) & 0xFF);
    output[3] = (uint8_t)(packet & 0xFF);


    state->frame_number++;
}


// Decoder

void decode(
    DecoderState *state,
    const uint8_t input[RECORD_SIZE],
    uint16_t *timestamp_msec,
    uint8_t *no2_data,
    uint8_t *baro_data,
    int8_t *temp_data,
    float *acceleration
)
{
    uint32_t packet;
    uint32_t acceleration_code;
    uint16_t event_code;

    EventType type;
    uint8_t value;


    /*
     * Reconstruct 32-bit packet.
     */

    packet =
        ((uint32_t)input[0] << 24) |
        ((uint32_t)input[1] << 16) |
        ((uint32_t)input[2] << 8)  |
        ((uint32_t)input[3]);


    // Extract acceleration.


    acceleration_code = packet >> 10;

    *acceleration = float_from_22bit(acceleration_code);


    // Extract event.

    event_code = (uint16_t)(packet & 0x03FFU);

    type = (EventType)((event_code >> 8) & 0x03U);
    value = (uint8_t)(event_code & 0xFFU);


    // Timestamp is reconstructed from the frame number.
    

    *timestamp_msec =
        (uint16_t)(state->frame_number * 100U);


    // Apply slow-sensor event.
    

    switch (type) {

        case EVENT_NO2:

            state->no2 = value;
            state->next_no2_time_ms += 500U;

            break;


        case EVENT_BARO:

            state->baro = value;
            state->next_baro_time_ms += 250U;

            break;


        case EVENT_TEMP:

            state->temp = value;
            state->next_temp_time_ms += 1000U;

            break;


        case EVENT_NONE:
        default:

            break;
    }


    // Return latest reconstructed sensor values.
     

    *no2_data = state->no2;
    *baro_data = state->baro;
    *temp_data = (int8_t)state->temp;


    state->frame_number++;
}
