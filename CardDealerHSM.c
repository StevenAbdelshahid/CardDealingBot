/* ============================================================================
 *  CardDealerHSM.c  ? Hierarchical?State?Machine for a servo?based card dealer
 *
 *  v2.15 ? Second pass of regression fixes.
 *           ? Always keep a periodic timer running in Idle so the HSM polls the
 *             slide?switch and leaves Idle as soon as it?s flipped ON.
 *           ? Remove the 2?s "sweep?end forward nudge" that was causing the
 *             motor to spin forever. The original (working) design did not
 *             need it ? the short 50?ms nudge after *each* deal is enough to
 *             tidy cards.
 *           ? ServoStep now *immediately* wraps from MAX_PULSE_US back to
 *             MIN_PULSE_US (like the v1.x code) and never touches the motor.
 *           ? No other logic changed ? deals are still triggered strictly at
 *             memorised playerAngle[] crossings; sonar is used only for the
 *             initial calibration hit?test and for console logging.
 * ============================================================================*/

#include <xc.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "BOARD.h"           // for BOARD_GetPBClock()
#include "CardDealerHSM.h"
#include "GameButton.h"
#include "SensorMotorEventChecker.h"
#include "HCSR04.h"          /* exposes HCSR04_Reset() */
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

/* ????????? Tunables ????????? */
#define MAX_PLAYERS      4
#define MIN_SEP_US       150u     // µs between players
#define WARMUP_STEPS     10
#define STEP_MS          70u      // ms between servo steps
#define STEP_US          15u      // µs increment per step
#define MOTOR_DELAY_MS   800u
#define MOTOR_FWD_MS     220u
#define NUDGE_MS         50u
#define DUTY_SLOW        400u
#define DUTY_FAST        1000u

/* ????????? Servo range for MG996R ????????? */
#define MIN_PULSE_US    500u      // ~0°
#define MAX_PULSE_US    2500u     // ~180°
#define CENTER_PULSE_US 1500u     // ~90° (not used for initialization)

/* ????????? Pins / helpers ????????? */
#define SERVO_PIN       RC_PORTY06
#define ENA_PWM_MACRO   PWM_PORTZ06
#define IN1_MASK        PIN4
#define IN2_MASK        PIN5

#define MODE_SW_PIN     PIN11
#define PORT_SW         PORTZ
#define IS_SWITCH_ON()  (((IO_PortsReadPort(PORT_SW) & MODE_SW_PIN) == 0))

#define LED_D6_TRIS     TRISDbits.TRISD6
#define LED_D6_LAT      LATDbits.LATD6
#define LED_D7_TRIS     TRISDbits.TRISD7
#define LED_D7_LAT      LATDbits.LATD7

static const uint8_t CardsPP[GM_COUNT] = {2, 5, 7};

/* helper: bit0?RD6, bit1?RD7 (1 = LED ON, active?LOW) */
static inline void SetLED(uint8_t p)
{
    LED_D6_LAT = (p & 0x01) ? 0 : 1;
    LED_D7_LAT = (p & 0x02) ? 0 : 1;
}
static inline void UpdateGameLEDs(GameMode_t g)
{
    if      (g == GM_BLACKJACK)      SetLED(0x01);
    else if (g == GM_FIVE_CARD_DRAW) SetLED(0x02);
    else if (g == GM_GO_FISH)        SetLED(0x03);
    else                             SetLED(0x00);
}

/* ????????? HSM boilerplate ????????? */
typedef enum {
    IdleS, CalSweepS, CalDelayS, CalRunS,
    DealSweepS, DealDelayS, DealRunS, SweepEndNudgeS, DonePauseS
} state_t;
static state_t State;
static uint8_t MyPrio;

#define TMR_SWEEP      1
#define TMR_MOTOR      2

static uint16_t playerAngle[MAX_PLAYERS];
static uint8_t  playerRemain[MAX_PLAYERS];
static uint8_t  players = 0, idx = 0;

/* Use new servo range and initialize at MIN_PULSE_US */
static uint16_t pulse   = MIN_PULSE_US;
static uint16_t prevP   = MIN_PULSE_US;
static int8_t   warmCnt = 0;
static GameMode_t CurMode = GM_BLACKJACK;
static uint8_t  prevSwitch = 1;   // updated in Init ? see below

/* ????????? Motor helpers ????????? */
static inline void Motor_SetDuty(uint16_t d) { PWM_SetDutyCycle(ENA_PWM_MACRO, d); }
static inline void Motor_SetDirFwd(void)
{
    IO_PortsSetPortBits(PORTY, IN1_MASK);
    IO_PortsClearPortBits(PORTY, IN2_MASK);
}
static inline void Motor_SetDirRev(void)
{
    IO_PortsClearPortBits(PORTY, IN1_MASK);
    IO_PortsSetPortBits(PORTY, IN2_MASK);
}
static inline void Motor_FastFwd(void) { Motor_SetDirFwd();  Motor_SetDuty(DUTY_FAST); }
static inline void Motor_FastRev(void) { Motor_SetDirRev();  Motor_SetDuty(DUTY_FAST); }
static inline void Motor_Stop(void)    { Motor_SetDuty(0); }

/* ????????? Servo step helper ????????? */
// Returns true if we just wrapped from MAX_PULSE_US back to MIN_PULSE_US
static uint8_t ServoStep(void)
{
    prevP = pulse;

    if (pulse >= MAX_PULSE_US) {
        /* Instant wrap ? exactly like the proven v1.x behaviour */
        pulse = MIN_PULSE_US;
        RC_SetPulseTime(SERVO_PIN, pulse);
        return 1;   // wrapped
    } else {
        pulse += STEP_US;
        RC_SetPulseTime(SERVO_PIN, pulse);
    }

    /* sonar only for logging ? NOT used for state decisions */
    uint16_t cm = HCSR04_GetDistanceCm();
    printf("%u,%u,\r\n", cm, cm);

    return 0;
}

static inline uint8_t Cross(uint16_t a, uint16_t b, uint16_t t)
{
    return (a < b && a < t && b >= t) || (a > b && (a < t || b >= t));
}

static uint8_t AllDone(void)
{
    for (uint8_t i = 0; i < players; i++) {
        if (playerRemain[i]) return 0;
    }
    return 1;
}

static void AdvanceIdx(void)
{
    do {
        idx = (idx + 1) % players;
    } while (playerRemain[idx] == 0);
}

static void SortAngles(void)
{
    for (uint8_t i = 1; i < players; i++) {
        uint16_t pa = playerAngle[i];
        uint8_t  pr = playerRemain[i];
        int8_t j = i - 1;
        while (j >= 0 && playerAngle[j] > pa) {
            playerAngle[j + 1]  = playerAngle[j];
            playerRemain[j + 1] = playerRemain[j];
            j--;
        }
        playerAngle[j + 1]  = pa;
        playerRemain[j + 1] = pr;
    }
}

/* ????????? Idle reset (servo centre + card?nudge) ????????? */
static void ResetIdle(void)
{
    /* 1) Stop any ongoing motion & disable player detection */
    Motor_Stop();
    Distance_Enable(0);
    HCSR04_Reset();

    /* 2) Stop timers */
    ES_Timer_StopTimer(TMR_SWEEP);
    ES_Timer_StopTimer(TMR_MOTOR);

    /* 3) Centre servo (500 µs pulse) */
    pulse = prevP = MIN_PULSE_US;
    RC_SetPulseTime(SERVO_PIN, MIN_PULSE_US);

    /* 4) Nudge cards (non?blocking 50 ms) */
    Motor_FastFwd();
    ES_Timer_InitTimer(TMR_MOTOR, NUDGE_MS);

    /* 5) Keep a periodic timer alive so we poll the slide?switch */
    ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

    /* 6) Turn LEDs off and stay in Idle */
    SetLED(0x00);
    State = IdleS;
    puts(",,HSM=IDLE");
}

/* ????????? Framework glue ????????? */
uint8_t PostCardDealerHSM(ES_Event e) { return ES_PostToService(MyPrio, e); }

uint8_t InitCardDealerHSM(uint8_t p)
{
    MyPrio = p;
    GameButton_Init();

    PWM_AddPins(ENA_PWM_MACRO);
    Motor_Stop();
    RC_SetPulseTime(SERVO_PIN, MIN_PULSE_US);

    LED_D6_TRIS = 0;
    LED_D7_TRIS = 0;
    UpdateGameLEDs(GM_BLACKJACK);

    puts("\r\nraw_cm,filt_cm,event");
    puts(",,HSM=IDLE");

    State = IdleS;

    /* Invert prevSwitch so the very first switch transition is seen. */
    prevSwitch = IS_SWITCH_ON() ? 0 /* force edge if already ON */ : 1;

    /* Keep the periodic timer running from the very start */
    ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

    return 1;
}

/* ????????? Main HSM ????????? */
ES_Event RunCardDealerHSM(ES_Event ev)
{
    /* 1) Slide?switch ON/OFF edge detector */
    uint8_t curSwitch = IS_SWITCH_ON() ? 1 : 0;

    if (curSwitch != prevSwitch) {
        if (!curSwitch) {
            /* Switched OFF ? fully reset and stay in Idle */
            ResetIdle();
            prevSwitch = curSwitch;
            return NO_EVENT;
        } else if (State == IdleS) {
            /* Switched ON from Idle ? begin calibration */
            players = 0;
            idx     = 0;
            pulse   = prevP = MIN_PULSE_US;
            warmCnt = WARMUP_STEPS;
            memset(playerRemain, 0, sizeof(playerRemain));

            HCSR04_Reset();
            Distance_Enable(1);
            State = CalSweepS;
            /* TMR_SWEEP already running ? no need to re?init */
            printf(",,HSM=CAL\n,,GAME=%s\r\n", Game_ModeName(CurMode));
        }
        prevSwitch = curSwitch;
    }

    /* 2) Game?select button only in Idle */
    if (State == IdleS && ev.EventType == GAME_BTN_PRESSED) {
        CurMode = (GameMode_t)ev.EventParam;
        UpdateGameLEDs(CurMode);
        printf(",,GAME=%s\r\n", Game_ModeName(CurMode));
        return NO_EVENT;
    }

    switch (State) {
        /* ????????? IdleS ????????? */
        case IdleS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                /* After nudge, stop motor */
                Motor_Stop();
            } else if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
                /* Keep polling timer alive */
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
            }
            break;

        /* ????? Calibration sweep ????? */
        case CalSweepS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
                uint8_t wrapped = ServoStep();
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

                if (warmCnt > 0) {
                    if (--warmCnt == 0) puts(",,READY");
                } else if (wrapped || players == MAX_PLAYERS) {
                    Distance_Enable(0);
                    if (players > 0) {
                        SortAngles();
                        /* Reset sweep to MIN for DealSweep */
                        pulse = prevP = MIN_PULSE_US;
                        for (uint8_t i = 0; i < players; i++) {
                            playerRemain[i] = CardsPP[CurMode] - 1; // already dealt one during calibration
                        }
                        idx = 0;
                        State = DealSweepS;
                        puts(",,HSM=SWEEP");
                    } else {
                        ResetIdle();
                    }
                }
            } else if (ev.EventType == DIST_NEAR && warmCnt == 0 && players < MAX_PLAYERS) {
                uint16_t ang = pulse;
                if (players == 0 || (uint16_t)(ang - playerAngle[players - 1]) > MIN_SEP_US) {
                    playerAngle[players]  = ang;
                    playerRemain[players] = CardsPP[CurMode];
                    printf(",,PLAYER%u=%u\r\n", players + 1, ang);

                    ES_Timer_StopTimer(TMR_SWEEP);
                    ES_Timer_StopTimer(TMR_MOTOR);
                    ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);

                    idx = players++;
                    State = CalDelayS;
                    puts(",,HSM=FDELAY");
                }
            }
            break;

        case CalDelayS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                /* Print & perform calibration deal */
                printf(",,CAL_DEAL_PULSE=%u\r\n", pulse);
                Motor_FastRev();
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_FWD_MS);
                State = CalRunS;
                puts(",,HSM=FDEAL");
            }
            break;

        case CalRunS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                Motor_Stop();
                --playerRemain[idx];
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                State = CalSweepS;
                puts(",,HSM=CAL");
            }
            break;

        /* ????? Dealing sweep ????? */
        case DealSweepS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP) {
                ServoStep();
                ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);

                /* Deal strictly on memorised angle ? sonar not consulted */
                if (playerRemain[idx] && Cross(prevP, pulse, playerAngle[idx])) {
                    ES_Timer_StopTimer(TMR_SWEEP);
                    ES_Timer_StopTimer(TMR_MOTOR);
                    ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                    State = DealDelayS;
                    puts(",,HSM=DELAY");
                }
            }
            break;

        case DealDelayS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                printf(",,DEAL_PULSE=%u\r\n", pulse);
                /* 1?ms driver?off gap to discharge H?bridge */
                IO_PortsClearPortBits(PORTY, IN1_MASK | IN2_MASK);
                _CP0_SET_COUNT(0);
                while (_CP0_GET_COUNT() < (BOARD_GetPBClock() / 2 / 1000)) { }

                Motor_FastRev();
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_FWD_MS);
                State = DealRunS;
                puts(",,HSM=DEAL");
            }
            break;

        case DealRunS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                Motor_Stop();
                --playerRemain[idx];
                printf(",,P%u_LEFT=%u\r\n", idx + 1, playerRemain[idx]);

                /* Short forward nudge (also reused at end?of?sweep) */
                Motor_FastFwd();
                ES_Timer_InitTimer(TMR_MOTOR, NUDGE_MS);
                State = SweepEndNudgeS;
            }
            break;

        /* ????? SweepEndNudgeS ? shared for end?of?deal and end?of?sweep ????? */
        case SweepEndNudgeS:
            if (ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR) {
                Motor_Stop();
                if (AllDone()) {
                    puts(",,HSM=DONE");
                    State = DonePauseS;
                } else {
                    AdvanceIdx();
                    ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
                    State = DealSweepS;
                    puts(",,HSM=SWEEP");
                }
            }
            break;

        /* ????? DonePauseS ????? */
        case DonePauseS:
            /* Remain here until switch OFF (edge handled at top of function) */
            break;

        default:
            break;
    }

    return NO_EVENT;
}
