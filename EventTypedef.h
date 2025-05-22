typedef enum {
    /* ----- framework events ----- */
    ES_NO_EVENT,  ES_ERROR,
    /* ----- HC?SR04 + motor ------- */
    DIST_NEAR,           // <  25?cm
    DIST_FAR,            // > 150?cm
    MOTOR_STALLED,
    MOTOR_MOVING
} ES_EventType_t;
