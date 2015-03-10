/* Automatically generated nanopb header */
/* Generated by nanopb-0.3.1 at Mon Mar  9 19:48:54 2015. */

#ifndef PB_MOVE_PB_H_INCLUDED
#define PB_MOVE_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Enum definitions */
/* Struct definitions */
typedef struct _Ack {
    int32_t id;
} Ack;

typedef struct _Measure {
    int32_t id;
    int32_t channel;
} Measure;

typedef struct _Move {
    int32_t id;
    int32_t x;
    int32_t y;
    int32_t laserPower;
} Move;

typedef struct _Nack {
    int32_t id;
    int32_t reason;
} Nack;

/* Default values for struct fields */

/* Initializer values for message structs */
#define Move_init_default                        {0, 0, 0, 0}
#define Ack_init_default                         {0}
#define Nack_init_default                        {0, 0}
#define Measure_init_default                     {0, 0}
#define Move_init_zero                           {0, 0, 0, 0}
#define Ack_init_zero                            {0}
#define Nack_init_zero                           {0, 0}
#define Measure_init_zero                        {0, 0}

/* Field tags (for use in manual encoding/decoding) */
#define Ack_id_tag                               1
#define Measure_id_tag                           1
#define Measure_channel_tag                      2
#define Move_id_tag                              1
#define Move_x_tag                               2
#define Move_y_tag                               3
#define Move_laserPower_tag                      4
#define Nack_id_tag                              1
#define Nack_reason_tag                          2

/* Struct field encoding specification for nanopb */
extern const pb_field_t Move_fields[5];
extern const pb_field_t Ack_fields[2];
extern const pb_field_t Nack_fields[3];
extern const pb_field_t Measure_fields[3];

/* Maximum encoded size of messages (where known) */
#define Move_size                                44
#define Ack_size                                 11
#define Nack_size                                22
#define Measure_size                             22

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
