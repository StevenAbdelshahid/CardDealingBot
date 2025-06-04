#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "BOARD.h"
#include "CardDealerHSM.h"
#include "GameButton.h"
#include "SensorMotorEventChecker.h"
#include "HCSR04.h"
#include "IO_Ports.h"
#include "RC_Servo.h"
#include "pwm.h"
#include "ES_Framework.h"
#include "ES_Timers.h"

/* ????????? Tunables ????????? */
/* Maximum number of players the HSM will track */
#define MAX_PLAYERS      4
/* Minimum separation (in microseconds of pulse width) between detected players,
   to prevent false duplicates when the sonar sees the same player multiple times */
#define MIN_SEP_US       250u
/* Number of initial steps during calibration in which sonar readings are ignored
   (warming up the sensor and servo) */
#define WARMUP_STEPS     10
/* Milliseconds between each servo step during a sweep */
#define STEP_MS          70u
/* Microseconds increment for each servo step pulse width when sweeping */
#define STEP_US          20u
/* Delay (ms) between stopping the sweep and starting a motor action (deal) */
#define MOTOR_DELAY_MS   800u
/* Duration (ms) for rotating the motor in the ?reverse? (deal-eject) direction */
#define MOTOR_FWD_MS     350u                 /* FastRev duration (deal)  */
/* Duration (ms) for rotating the motor in the opposite (forward) direction,
   used to ?lock? or push cards back after ejecting (usually half of MOTOR_FWD_MS) */
#define MOTOR_LOCK_MS    (MOTOR_FWD_MS/2u)    /* FastFwd tuck             */
/* Duration (ms) for the small ?tuck? movement at the end of a sweep, to push
   any cards back into position before starting the next sweep/deal */
#define NUDGE_MS         100u                 /* sweep-boundary tuck       */
/* PWM duty cycle used for ?fast? motor motion (~1000/1023 ? full speed) */
#define DUTY_FAST        1000u
/* Watchdog timeout in milliseconds; if no sweep event arrives before this timeout,
   the HSM resets to Idle to avoid stalling */
#define WDOG_MS          3000u

/* ????????? Servo range ????????? */
/* Minimum pulse width (in 탎) representing the servo?s zero/0� position */
#define MIN_PULSE_US     1000u
/* Maximum pulse width (in 탎) representing the servo?s 180� position */
#define MAX_PULSE_US     2500u

/* ????????? Pins ????????? */
/* The RC servo output pin identifier */
#define SERVO_PIN        RC_PORTY06
/* The PWM channel used for enabling motor power */
#define ENA_PWM_MACRO    PWM_PORTZ06
/* Motor direction bit masks: IN1 (bit 4) and IN2 (bit 5) on PORTY */
#define IN1_MASK         PIN4
#define IN2_MASK         PIN5
/* Slide-switch pin for ON/OFF control (active-LOW) */
#define MODE_SW_PIN      PIN11
#define PORT_SW          PORTZ
/* Macro to read the slide-switch state; returns 1 if switch is ON, 0 if OFF */
#define IS_SWITCH_ON()   (((IO_PortsReadPort(PORT_SW) & MODE_SW_PIN) == 0))

/* LEDs (active-LOW) ? using RD6 and RD7 to indicate game mode */
#define LED_D6_TRIS      TRISDbits.TRISD6
#define LED_D6_LAT       LATDbits.LATD6
#define LED_D7_TRIS      TRISDbits.TRISD7
#define LED_D7_LAT       LATDbits.LATD7

/* Number of cards per player for each game mode: Blackjack=2, FiveCardDraw=5, GoFish=7 */
static const uint8_t CardsPP[GM_COUNT] = {2, 5, 7};

/* ????????? Helpers ????????? */
/* SetLED: turns on/off the two status LEDs. bit0?LED_D6, bit1?LED_D7 (1 = ON, active-LOW) */
static inline void SetLED(uint8_t p){
    LED_D6_LAT = (p & 1) ? 0 : 1;
    LED_D7_LAT = (p & 2) ? 0 : 1;
}
/* UpdateLEDs: light a pattern depending on the current GameMode_t */
static inline void UpdateLEDs(GameMode_t g){
    SetLED(g == GM_BLACKJACK       ? 1 :
           g == GM_FIVE_CARD_DRAW  ? 2 :
           g == GM_GO_FISH         ? 3 : 0);
}
/* ModeName: return a string name for console logging of the game mode */
static inline const char *ModeName(GameMode_t g){
    return g == GM_BLACKJACK        ? "Blackjack" :
           g == GM_FIVE_CARD_DRAW    ? "FiveCardDraw" :
           "GoFish";
}

/* ????????? HSM plumbing ????????? */
/* Define all possible states of the hierarchical state machine */
typedef enum {
    IdleS,             /* Waiting for switch ON */
    CalSweepS,         /* Sweeping servo for player detection */
    CalDelayS,         /* Delay before performing calibration deal */
    CalRevS,           /* Motor running reverse for calibration deal */
    CalLockS,          /* Motor running forward to lock cards after cal deal */
    CalSweepNudgeS,    /* Tuck at end of calibration sweep before starting deals */
    DealSweepS,        /* Sweeping servo to deal cards to all players */
    DealDelayS,        /* Delay before performing a deal */
    DealRevS,          /* Motor running reverse for a deal */
    DealLockS,         /* Motor running forward to lock after a deal */
    SweepNudgeS,       /* Tuck at end-of-sweep between deals */
    DonePauseS         /* All deals done; waiting for switch OFF */
} state_t;

/* Current state of the HSM */
static state_t State;
/* Priority of this service in the ES framework (set at Init) */
static uint8_t MyPrio;

/* Timer identifiers */
#define TMR_SWEEP  1    /* Periodic ?heartbeat? timer for servo steps */
#define TMR_MOTOR  2    /* Timer for scheduling motor actions */
#define TMR_WDOG   3    /* Watchdog timer to detect a stuck sweep */

/* ArmHeartbeat: restarts the TMR_SWEEP timer to fire again after STEP_MS ms */
static void ArmHeartbeat(void){
    ES_Timer_InitTimer(TMR_SWEEP, STEP_MS);
}
/* KickWatchdog: restarts the watchdog timer to fire after WDOG_MS ms */
static void KickWatchdog(void){
    ES_Timer_InitTimer(TMR_WDOG, WDOG_MS);
}

/* ????????? Globals ????????? */
/* Array of servo pulse widths (탎) at which players were detected */
static uint16_t playerAngle[MAX_PLAYERS];
/* Number of cards remaining to deal to each player */
static uint8_t  playerRemain[MAX_PLAYERS];
/* How many players have been detected */
static uint8_t  players = 0;
/* Index of the current player being dealt */
static uint8_t  idx = 0;
/* Current servo pulse width (in 탎) */
static uint16_t pulse = MIN_PULSE_US;
/* Previous servo pulse width, used to detect crossing a player angle */
static uint16_t prevP = MIN_PULSE_US;
/* Warm-up counter: skip first WARMUP_STEPS steps before using sonar */
static int8_t   warmCnt = 0;
/* Current selected game mode (Blackjack/FiveCardDraw/GoFish) */
static GameMode_t CurMode = GM_BLACKJACK;
/* Track last debounced switch state to detect ON/OFF edges */
static uint8_t prevSwitch = 1;

/* ????????? Motor helpers ????????? */
/* Set PWM duty cycle for the motor enable pin */
static inline void Duty(uint16_t d){
    PWM_SetDutyCycle(ENA_PWM_MACRO, d);
}
/* Drive motor in forward direction: IN1=1, IN2=0 */
static inline void Fwd(void){
    IO_PortsSetPortBits(PORTY, IN1_MASK);
    IO_PortsClearPortBits(PORTY, IN2_MASK);
}
/* Drive motor in reverse direction: IN1=0, IN2=1 */
static inline void Rev(void){
    IO_PortsClearPortBits(PORTY, IN1_MASK);
    IO_PortsSetPortBits(PORTY, IN2_MASK);
}
/* FastFwd: run motor forward at full speed (duty=DUTY_FAST) */
static inline void FastFwd(void){
    Fwd();
    Duty(DUTY_FAST);
}
/* FastRev: run motor reverse at full speed (duty=DUTY_FAST) */
static inline void FastRev(void){
    Rev();
    Duty(DUTY_FAST);
}
/* StopM: immediately stop the motor by setting duty=0 */
static inline void StopM(void){
    Duty(0);
}

/* ????????? Servo helper ????????? */
/**
 * ServoStep:
 *   - Increments the servo's pulse width by STEP_US. If it exceeds MAX_PULSE_US,
 *     wraps back to MIN_PULSE_US and returns 1 (indicating a wrap event).
 *   - Sets the new pulse width via RC_SetPulseTime().
 *   - Prints raw and filtered sonar distances (currently identical) to console.
 *   - Returns 1 if the sweep just wrapped; else returns 0.
 */
static uint8_t ServoStep(void){
    prevP = pulse;
    if(pulse >= MAX_PULSE_US){
        pulse = MIN_PULSE_US;
        RC_SetPulseTime(SERVO_PIN, pulse);
        return 1;  /* wrapped back to start */
    }
    pulse += STEP_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    /* telemetry ? output sonar raw & filtered values (same for now) */
    uint16_t cm = HCSR04_GetDistanceCm();
    printf("%u,%u,\r\n", cm, cm);

    return 0;
}

/**
 * Cross:
 *   - Returns true if the sweep has crossed the angle 't' between
 *     the previous pulse width 'a' and current pulse width 'b', even
 *     if wrapping occurred around MAX_PULSE_US to MIN_PULSE_US.
 *   - Used to detect when to deal a card at a memorized angle.
 */
static inline bool Cross(uint16_t a, uint16_t b, uint16_t t){
    return (a < b && a < t && b >= t)
        || (a > b && (a < t || b >= t));
}

/**
 * AllDealt:
 *   - Returns true if every player has zero cards remaining.
 */
static inline bool AllDealt(void){
    for(uint8_t i=0; i<players; i++){
        if(playerRemain[i]) return false;
    }
    return true;
}

/**
 * AdvanceIdx:
 *   - Increment idx mod players, skipping any entries where playerRemain==0.
 *   - Prepares idx for the next player in round-robin dealing.
 */
static void AdvanceIdx(void){
    do {
        idx = (idx + 1) % players;
    } while(playerRemain[idx] == 0);
}

/**
 * SortAngles:
 *   - Sorts the playerAngle[] array in ascending order, carrying along
 *     the corresponding playerRemain[] counts. This ensures the servo
 *     deals in increasing pulse-width order around the circle.
 */
static void SortAngles(void){
    for(uint8_t i=1; i<players; i++){
        uint16_t pa = playerAngle[i];
        uint8_t  pr = playerRemain[i];
        int8_t   j = i - 1;
        while(j >= 0 && playerAngle[j] > pa){
            playerAngle[j+1]  = playerAngle[j];
            playerRemain[j+1] = playerRemain[j];
            j--;
        }
        playerAngle[j+1]  = pa;
        playerRemain[j+1] = pr;
    }
}

/* ????????? Idle reset ????????? */
/**
 * ResetIdle:
 *   - Stops motor motion & disables sonar distance checking.
 *   - Resets servo to MIN_PULSE_US (zero/0�) position.
 *   - Starts a one-time ?nudge? on the motor (FastFwd for NUDGE_MS) to
 *     push cards back into place.
 *   - Arms TMR_SWEEP so the HSM continues polling the slide-switch.
 *   - Turns LEDs off and prints ?,,HSM=IDLE? to console.
 */
static void ResetIdle(void){
    /* 1) Stop any ongoing motion & disable player detection */
    StopM();
    Distance_Enable(0);
    HCSR04_Reset();

    /* 2) Stop motor timer if running */
    ES_Timer_StopTimer(TMR_MOTOR);

    /* 3) Centre servo (MIN_PULSE_US) */
    pulse = prevP = MIN_PULSE_US;
    RC_SetPulseTime(SERVO_PIN, pulse);

    /* 4) Nudge cards (FastFwd for NUDGE_MS) */
    FastFwd();
    ES_Timer_InitTimer(TMR_MOTOR, NUDGE_MS);

    /* 5) Keep a periodic heartbeat timer alive so we poll the slide-switch */
    ArmHeartbeat();

    /* 6) Reset LEDs and state, log to console */
    SetLED(0);
    State = IdleS;
    puts(",,HSM=IDLE");

    /* Restart watchdog as well, in case Idle lingers */
    KickWatchdog();
}

/* ????????? Framework glue ????????? */
uint8_t PostCardDealerHSM(ES_Event e){
    return ES_PostToService(MyPrio, e);
}

uint8_t InitCardDealerHSM(uint8_t p){
    MyPrio = p;
    /* Initialize game-select button service so we can receive GAME_BTN_PRESSED */
    GameButton_Init();
    /* Add motor PWM pin to the PWM module and ensure motor is stopped */
    PWM_AddPins(ENA_PWM_MACRO);
    StopM();
    /* Initialize servo to its minimum pulse */
    RC_SetPulseTime(SERVO_PIN, MIN_PULSE_US);
    /* Configure LED pins as outputs */
    LED_D6_TRIS = LED_D7_TRIS = 0;
    /* Show initial game mode on LEDs */
    UpdateLEDs(GM_BLACKJACK);
    /* Print CSV header for data visualizer: raw_cm, filt_cm, event */
    puts("\r\nraw_cm,filt_cm,event");
    /* Enter Idle state and log it */
    ResetIdle();
    /* Ensure the first physical switch transition is seen as an edge */
    prevSwitch = IS_SWITCH_ON() ? 0 : 1;
    return 1;
}

/* ????????? Main HSM ????????? */
ES_Event RunCardDealerHSM(ES_Event ev){
    /* Debounce the slide-switch: shift in 1 if ON, 0 if OFF into swHist */
    static uint8_t swHist = 0xFF;
    swHist = (swHist << 1) | (IS_SWITCH_ON() ? 1 : 0);
    /* Determine debounced state: 1 if high byte all 1s, 0 if all 0s, else unchanged */
    uint8_t curSwitch = (swHist == 0xFF) ? 1 : (swHist == 0x00 ? 0 : prevSwitch);

    /* ?? Handle switch edges ?? */
    if(curSwitch != prevSwitch){
        if(curSwitch == 0){
            /* OFF edge: immediately ResetIdle (regardless of current state) */
            ResetIdle();
        } else if(State == IdleS){
            /* ON edge, but only if we were Idle: begin calibration sweep */
            players = idx = 0;
            pulse   = prevP = MIN_PULSE_US;
            warmCnt = WARMUP_STEPS;
            memset(playerRemain, 0, sizeof(playerRemain));
            HCSR04_Reset();
            Distance_Enable(1);  /* start sonar for player detection */
            /* Log the starting game mode */
            printf(",,GAME=%s\r\n", ModeName(CurMode));
            State = CalSweepS;
        }
        prevSwitch = curSwitch;
    }

    /* Watchdog: if TMR_WDOG fires, ResetIdle to recover from a stall */
    if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_WDOG){
        ResetIdle();
        return NO_EVENT;
    }

    /* Game-select button allowed only in Idle: change game mode and log it */
    if(State == IdleS && ev.EventType == GAME_BTN_PRESSED){
        CurMode = (GameMode_t)ev.EventParam;
        UpdateLEDs(CurMode);
        printf(",,GAME=%s\r\n", ModeName(CurMode));
        return NO_EVENT;
    }

    /* ????????? State Machine ????????? */
    switch(State){

    /* ????????? IdleS ????????? */
    case IdleS:
        /* In Idle, we only care about maintaining heartbeat so that
           we can detect an ON edge on the slide-switch promptly. */
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP){
            ArmHeartbeat();
            KickWatchdog();
        }
        break;

    /* ????? Calibration sweep ????? */
    case CalSweepS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP){
            /* Perform one servo step; the function logs sonar telemetry */
            uint8_t wrap = ServoStep();
            /* Restart heartbeat & watchdog */
            ArmHeartbeat();
            KickWatchdog();
            /* If still warming up, decrement counter until zero, then print ,,READY */
            if(warmCnt && --warmCnt == 0){
                puts(",,READY");
            }
            /* Once warmed up: if we wrapped the sweep or reached max players, stop */
            if(!warmCnt && (wrap || players == MAX_PLAYERS)){
                /* Disable sonar distance events until after calibration */
                Distance_Enable(0);
                if(players){
                    /* Sort found player angles in ascending order */
                    SortAngles();
                    /* Reset servo to MIN_PULSE_US to start dealing */
                    pulse = prevP = MIN_PULSE_US;
                    /* Each detected player already got one card in calibration,
                       so subtract 1 from their remaining count */
                    for(uint8_t i=0; i<players; i++){
                        playerRemain[i] = CardsPP[CurMode] - 1;
                    }
                    idx = 0;                    /* start dealing with player 0 */
                    State = DealSweepS;
                    puts(",,HSM=SWEEP");
                } else {
                    /* No players detected: bail back to Idle */
                    ResetIdle();
                }
            }
        }
        /* On sonar detection (DIST_NEAR), record a player angle if conditions permit */
        else if(ev.EventType == DIST_NEAR && warmCnt == 0 && players < MAX_PLAYERS){
            uint16_t ang = pulse;
            /* Only record if this is the first player or sufficiently far from last */
            if(!players || (uint16_t)(ang - playerAngle[players-1]) > MIN_SEP_US){
                playerAngle[players]  = ang;
                playerRemain[players] = CardsPP[CurMode];
                printf(",,PLAYER%u=%u\r\n", players+1, ang);
                /* Pause sweep and schedule a calibration deal after MOTOR_DELAY_MS */
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                idx = players++;
                State = CalDelayS;
            }
        }
        break;

    case CalDelayS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Log calibration-deal pulse for console */
            printf(",,CAL_DEAL_PULSE=%u\r\n", pulse);
            /* Spin motor in ?deal? direction (reverse) for MOTOR_FWD_MS */
            FastRev();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_FWD_MS);
            State = CalRevS;
            puts(",,HSM=FDEAL");
        }
        break;

    case CalRevS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Stop reverse and immediately tuck forward to lock cards */
            FastFwd();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_LOCK_MS);
            State = CalLockS;
        }
        break;

    case CalLockS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Stop motor, decrement that player's remaining count (one dealt) */
            StopM();
            --playerRemain[idx];
            /* Restart sweep heartbeat & watchdog */
            ArmHeartbeat();
            KickWatchdog();
            /* Return to calibration sweep state */
            State = CalSweepS;
            puts(",,HSM=CAL");
        }
        break;

    case CalSweepNudgeS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Completed the ?tuck? at end of calibration sweep */
            StopM();
            Distance_Enable(0);
            if(players){
                /* Prepare for dealing: sort angles, reset servo to MIN, set remain counts */
                SortAngles();
                pulse = prevP = MIN_PULSE_US;
                for(uint8_t i=0; i<players; i++){
                    playerRemain[i] = CardsPP[CurMode] - 1;
                }
                idx = 0;
                State = DealSweepS;
                ArmHeartbeat();
                KickWatchdog();
                puts(",,HSM=SWEEP");
            } else {
                /* No players found, return to Idle */
                ResetIdle();
            }
        }
        break;

    /* ????? Dealing sweep ????? */
    case DealSweepS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP){
            /* Step the servo and log sonar telemetry */
            uint8_t wrapped = ServoStep();
            ArmHeartbeat();
            KickWatchdog();
            /* If the sweep wrapped, do a small tuck before continuing (SweepNudgeS) */
            if(wrapped){
                ES_Timer_StopTimer(TMR_SWEEP);
                FastFwd();
                ES_Timer_InitTimer(TMR_MOTOR, NUDGE_MS);
                State = SweepNudgeS;
                break;
            }
            /* If crossing a memorized player angle where cards remain, schedule a deal */
            if(playerRemain[idx] && Cross(prevP, pulse, playerAngle[idx])){
                ES_Timer_StopTimer(TMR_SWEEP);
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                State = DealDelayS;
                puts(",,HSM=DELAY");
            }
        }
        break;

    case DealDelayS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Perform the actual deal for current player */
            printf(",,DEAL_PULSE=%u\r\n", pulse);
            /* 1ms motor off for H-bridge discharge */
            IO_PortsClearPortBits(PORTY, IN1_MASK | IN2_MASK);
            _CP0_SET_COUNT(0);
            while(_CP0_GET_COUNT() < (BOARD_GetPBClock() / 2 / 1000)){}
            /* Spin motor in ?deal? direction (reverse) */
            FastRev();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_FWD_MS);
            State = DealRevS;
            puts(",,HSM=DEAL");
        }
        break;

    case DealRevS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* After reverse fling, perform forward tuck to lock */
            FastFwd();
            ES_Timer_InitTimer(TMR_MOTOR, MOTOR_LOCK_MS);
            State = DealLockS;
        }
        break;

    case DealLockS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Stop motor, decrement that player's remaining count */
            StopM();
            --playerRemain[idx];
            printf(",,P%u_LEFT=%u\r\n", idx+1, playerRemain[idx]);
            /* If all cards dealt to all players, go to DonePauseS */
            if(AllDealt()){
                State = DonePauseS;
                ArmHeartbeat();  /* keep polling switch after Done */
                KickWatchdog();
                puts(",,HSM=DONE");
            } else {
                /* Move to next player and resume dealing sweep */
                AdvanceIdx();
                State = DealSweepS;
                ArmHeartbeat();
                KickWatchdog();
                puts(",,HSM=SWEEP");
            }
        }
        break;

    /* ????? Sweep-boundary tuck ????? */
    case SweepNudgeS:
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_MOTOR){
            /* Finish the tuck, then either deal immediately if on a player angle,
               or resume the sweeping state. */
            StopM();
            if(playerRemain[idx] &&
               (pulse == playerAngle[idx] || Cross(prevP, pulse, playerAngle[idx]))){
                ES_Timer_InitTimer(TMR_MOTOR, MOTOR_DELAY_MS);
                State = DealDelayS;
            } else {
                State = DealSweepS;
                ArmHeartbeat();
                KickWatchdog();
                puts(",,HSM=SWEEP");
            }
        }
        break;

    /* ????? DonePauseS ????? */
    case DonePauseS:
        /* All deals complete: keep the heartbeat alive so the OFF edge is caught */
        if(ev.EventType == ES_TIMEOUT && ev.EventParam == TMR_SWEEP){
            ArmHeartbeat();
            KickWatchdog();
        }
        break;

    default:
        break;
    }

    return NO_EVENT;
}
