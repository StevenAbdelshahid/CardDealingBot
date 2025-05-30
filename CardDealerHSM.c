/*  CardDealerHSM.c  ? multi-player, fixed-angle card dealer
 *
 *  This HSM (Hierarchical State Machine) controls a servo-based card-dealing robot.
 *  It depends on the following modules:
 *    ? CardDealerHSM.h            ? State and event declarations, GameMode_t, etc.
 *    ? GameButton.{h,c}           ? Reads a push-button and posts GAME_BTN_PRESSED.
 *    ? SensorMotorEventChecker.{h,c}
 *                                 ? Posts DIST_NEAR/FAR events via ultrasonic sensor.
 *    ? HCSR04.{h,c}               ? Low-level ultrasonic trigger/capture.
 *    ? IO_Ports.{h,c}             ? Digital I/O abstraction for the Uno32 board.
 *    ? RC_Servo.{h,c}             ? Generates servo pulses on SERVO_PIN.
 *    ? pwm.{h,c}                  ? Generates PWM for the dispensing motor.
 *    ? ES_Framework.{h,c}         ? Event framework (services, posting, etc.)
 *    ? ES_Timers.{h,c}            ? Software timers used for timeouts.
 *
 *  Hardware roles:
 *    ? The servo (MG996R) rotates the platform to predefined angles.
 *    ? A rubber-banded wheel on a DC motor (controlled via pwm) dispenses one card when spun.
 *    ? Ultrasonic sensor (HC-SR04) detects players? hands at each angle.
 *    ? A slide switch on PORT-Z-11 toggles the entire HSM ON/OFF.
 *
 *  Flow:
 *    1. Idle (power toggle OFF) ? all motion/timers off, sonar disabled.
 *    2. Warm-up (~1 s) after power toggle ON ? servo sweeps but ignores sonar.
 *    3. Calibration sweep ? on each DIST_NEAR:
 *         ? Record pulse width (angle) in playerAngle[]
 *         ? playerRemain[] = CardsPP[CurMode]
 *         ? Immediately deal card #1 at that angle.
 *    4. After one wrap (or 4 players) ? stop sonar, sort angles ascending.
 *    5. Dealing sweeps ? on each servo crossing of a stored angle:
 *         ? If that player still needs cards, deal one.
 *    6. Repeat until every playerRemain[i] == 0, then go Idle.
 *
 *  Game quotas (CardsPP):
 *    GM_BLACKJACK  = 2
 *    GM_DRAW5      = 5
 *    GM_GOFISH     = 7
 *
 *  Group 14 · Jun-2025
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "CardDealerHSM.h"            // HSM declarations & GameMode_t
#include "GameButton.h"               // GAME_BTN_PRESSED events
#include "SensorMotorEventChecker.h"  // DIST_NEAR/FAR events
#include "HCSR04.h"                   // Ultrasonic low-level
#include "IO_Ports.h"                 // Digital I/O
#include "RC_Servo.h"                 // Servo pulse generation
#include "pwm.h"                      // Motor PWM
#include "ES_Framework.h"             // Event framework
#include "ES_Timers.h"                // Software timers

/* ????????? user-adjustable constants ????????? */
#define MAX_PLAYERS        4          /* Maximum simultaneous players     */
#define MIN_SEP_US         120u       /* Minimum separation in µs (~24°)  */
#define WARMUP_STEPS       10         /* Servo steps before sonar active  */
#define STEP_MS            100u       /* Servo step period (ms)           */
#define STEP_US            10u        /* Servo step size (µs)             */
#define MOTOR_DELAY_MS     800u       /* Delay before running motor       */
#define MOTOR_RUN_MS       500u       /* How long motor spins to deal one */
#define DUTY_FAST          800u       /* PWM duty for dispensing motor    */

/* ????????? hardware pin assignments ????????? */
#define SERVO_PIN          RC_PORTY06  /* Connected to servo signal line   */
#define ENA_PWM_MACRO      PWM_PORTZ06 /* Dispensing motor PWM enable pin  */
#define IN1_MASK           PIN4        /* Motor direction pin 1 (PORTY-04) */
#define IN2_MASK           PIN5        /* Motor direction pin 2 (PORTY-05) */

#define MODE_SW_PIN        PIN11       /* PORT-Z-11 ? LOW = system ON      */
#define PORT_SW            PORTZ
#define IS_SWITCH_ON()     (((IO_PortsReadPort(PORT_SW)&MODE_SW_PIN)==0))

/* ????????? cards per game mode ????????? */
static const uint8_t CardsPP[GM_COUNT] = {
    2,    /* Blackjack */
    5,    /* Five-Card Draw */
    7     /* Go Fish */
};

/* ????????? HSM states ????????? */
typedef enum {
    IdleS,
    CalSweepS,   /* Servo sweeping, collecting player angles */
    CalDelayS,   /* Brief pause before dispensing the 1st card */
    CalRunS,     /* Motor running to dispense the 1st card */
    DealSweepS,  /* Servo sweeping, dispensing remaining cards */
    DealDelayS,  /* Pause before dispensing each subsequent card */
    DealRunS     /* Motor running to dispense subsequent cards */
} state_t;
static state_t State;
static uint8_t MyPrio;

/* Timer IDs */
#define TMR_SWEEP   1  /* Used for servo stepping */
#define TMR_MOTOR   2  /* Used for motor delays */

/* ????????? run-time state variables ????????? */
/* playerAngle[i]   ? stored servo pulse (angle) for player i
 * playerRemain[i]  ? number of cards still to deal to player i
 * players          ? count of detected players
 * idx              ? index of current player in dealing rounds
 */
static uint16_t playerAngle[MAX_PLAYERS];
static uint8_t  playerRemain[MAX_PLAYERS];
static uint8_t  players = 0, idx = 0;

/* servo position tracking */
static uint16_t pulse = 1000u, prevP = 1000u;
/* warmCnt > 0 means ?ignore sonar?; decremented each sweep tick */
static int8_t   warmCnt = 0;

/* current game mode; defaults to Blackjack */
static GameMode_t CurMode = GM_BLACKJACK;

/* ????????? low-level control helpers ????????? */
/* Motor_SetDuty(d)  ? set the PWM duty cycle to d (0?1000) */
static inline void Motor_SetDuty(uint16_t d) {
    PWM_SetDutyCycle(ENA_PWM_MACRO, d);
}
/* Motor_SetDirFwd() ? set motor pins to spin wheel forward */
static inline void Motor_SetDirFwd(void) {
    IO_PortsSetPortBits (PORTY, IN1_MASK);
    IO_PortsClearPortBits(PORTY, IN2_MASK);
}
/* Motor_FastFwd()   ? combine direction + speed for fast forward */
static inline void Motor_FastFwd(void) {
    Motor_SetDirFwd();
    Motor_SetDuty(DUTY_FAST);
}
/* Motor_Stop()      ? stop the motor (duty=0) */
static inline void Motor_Stop(void) {
    Motor_SetDuty(0);
}

/**
 * ServoStep()
 *   Advances the servo by STEP_US (?1°) each call.
 *   Returns 1 if the pulse wrapped from P_MAX?P_MIN (one revolution).
 */
static uint8_t ServoStep(void) {
    prevP = pulse;
    pulse = (pulse < 2000u) ? pulse + STEP_US : 1000u;
    RC_SetPulseTime(SERVO_PIN, pulse);
    return (prevP > pulse);
}

/**
 * Cross(a,b,tgt)
 *   Returns true if the servo moved from 'a' to 'b' and crossed 'tgt',
 *   taking wrapping into account.
 */
static inline uint8_t Cross(uint16_t a, uint16_t b, uint16_t tgt) {
    return (a < b && a < tgt && b >= tgt)
        || (a > b && (a < tgt || b >= tgt));
}

/** AllDone()
 *   Returns true if every playerRemain[i] == 0.
 */
static uint8_t AllDone(void) {
    for (uint8_t i = 0; i < players; i++) {
        if (playerRemain[i]) return 0;
    }
    return 1;
}

/** AdvanceIdx()
 *   Round-robins 'idx' to the next player who still needs cards.
 */
static void AdvanceIdx(void) {
    do {
        idx = (idx + 1) % players;
    } while (playerRemain[idx] == 0);
}

/** SortAngles()
 *   Simple insertion sort to order playerAngle[] ascending
 *   (keeps playerRemain[] in sync).
 */
static void SortAngles(void){
    for (uint8_t i = 1; i < players; i++) {
        uint16_t pa = playerAngle[i];
        uint8_t  pr = playerRemain[i];
        int8_t   j  = i - 1;
        while (j >= 0 && playerAngle[j] > pa) {
            playerAngle[j+1] = playerAngle[j];
            playerRemain[j+1] = playerRemain[j];
            j--;
        }
        playerAngle[j+1] = pa;
        playerRemain[j+1] = pr;
    }
}

/* ????????? framework glue ????????? */
/** PostCardDealerHSM(e)
 *   Required by the ES_Framework to post events back to this HSM.
 */
uint8_t PostCardDealerHSM(ES_Event e) {
    return ES_PostToService(MyPrio, e);
}

/** InitCardDealerHSM(prio)
 *   Called once at startup.  Initializes variables, button, PWM, servo.
 */
uint8_t InitCardDealerHSM(uint8_t prio) {
    MyPrio = prio;
    GameButton_Init();                   // Enables GAME_BTN_PRESSED events

    PWM_AddPins(ENA_PWM_MACRO);
    Motor_Stop();                        // Ensure motor is off

    RC_SetPulseTime(SERVO_PIN, 1000u);   // Center servo at 0°

    puts("\r\nraw_cm,filt_cm,event");
    puts(",,HSM=IDLE");                  // Initial debug log
    State = IdleS;
    return 1;
}

/* ????????? RunCardDealerHSM(e) ? called each ES_Run() tick ????????? */
ES_Event RunCardDealerHSM(ES_Event ev) {
    /* ===== Emergency abort if switch flipped OFF ===== */
    if (!IS_SWITCH_ON() && State != IdleS) {
        Motor_Stop();
        ES_Timer_StopTimer(TMR_SWEEP);
        ES_Timer_StopTimer(TMR_MOTOR);
        Distance_Enable(0);            // Disable sonar
        State = IdleS;
        puts(",,HSM=IDLE");
        return NO_EVENT;
    }

    /* ===== In Idle, allow the game-mode button ===== */
    if (State == IdleS && ev.EventType == GAME_BTN_PRESSED) {
        CurMode = (GameMode_t)ev.EventParam;
        printf(",,GAME=%s\r\n", Game_ModeName(CurMode));
        return NO_EVENT;
    }

    switch (State) {

    /* --------------------- Idle --------------------- */
    case IdleS:
        if (IS_SWITCH_ON()) {
            // Prepare for calibration
            players  = 0;
            idx      = 0;
            pulse    = prevP = 1000u;
            warmCnt  = WARMUP_STEPS;
            memset(playerRemain, 0, sizeof(playerRemain));

            Distance_Enable(1);               // Enable sonar
            ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
            State = CalSweepS;
            printf(",,HSM=CAL\n,,GAME=%s\r\n", Game_ModeName(CurMode));
        }
        break;

    /* ------------- Calibration sweep ------------- */
    case CalSweepS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
            uint8_t wrapped = ServoStep();
            ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

            if (warmCnt > 0) {
                // Still in warm-up: ignore sonar
                if (--warmCnt == 0) puts(",,READY");
            }
            else if (wrapped || players == MAX_PLAYERS) {
                // Completed one revolution ? disable sonar
                Distance_Enable(0);
                if (players) {
                    SortAngles();
                    idx = 0;
                    State = DealSweepS;
                    puts(",,HSM=SWEEP");
                } else {
                    // No players found ? back to Idle
                    State = IdleS;
                    puts(",,HSM=IDLE");
                }
            }
        }
        else if (ev.EventType == DIST_NEAR && warmCnt == 0 && players < MAX_PLAYERS) {
            // New player detected at angle 'pulse'
            uint16_t ang = pulse;
            if (players == 0 || (uint16_t)(ang - playerAngle[players-1]) > MIN_SEP_US) {
                playerAngle[players]  = ang;
                playerRemain[players] = CardsPP[CurMode];
                printf(",,PLAYER%u=%u\r\n", players+1, ang);

                // Deal first card immediately
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                idx = players;
                players++;
                State = CalDelayS;
                puts(",,HSM=FDELAY");
            }
        }
        break;

    case CalDelayS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
            Motor_FastFwd();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_RUN_MS);
            State = CalRunS;
            puts(",,HSM=FDEAL");
        }
        break;

    case CalRunS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
            Motor_Stop();
            --playerRemain[idx];              // Account for first card
            ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
            State = CalSweepS;
            puts(",,HSM=CAL");
        }
        break;

    /* --------------- Dealing sweeps --------------- */
    case DealSweepS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
            ServoStep();
            ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

            if (playerRemain[idx] && Cross(prevP, pulse, playerAngle[idx])) {
                // Arrived at a stored angle for a player who still needs cards
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                State = DealDelayS;
                puts(",,HSM=DELAY");
            }
        }
        break;

    case DealDelayS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
            Motor_FastFwd();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_RUN_MS);
            State = DealRunS;
            puts(",,HSM=DEAL");
        }
        break;

    case DealRunS:
        if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
            Motor_Stop();
            --playerRemain[idx];
            printf(",,P%u_LEFT=%u\r\n", idx+1, playerRemain[idx]);

            if (AllDone()) {
                State = IdleS;
                puts(",,HSM=DONE");
            } else {
                AdvanceIdx();
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                State = DealSweepS;
                puts(",,HSM=SWEEP");
            }
        }
        break;

    default:
        break;
    }

    return NO_EVENT;
}
