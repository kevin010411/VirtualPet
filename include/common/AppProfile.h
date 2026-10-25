#ifndef APP_PROFILE_H
#define APP_PROFILE_H

#define APP_COMMAND_NONE 0
#define APP_COMMAND_PREDICT 2
#define APP_COMMAND_GUESS_GAME 6
#define APP_COMMAND_CHANGE_OUTFIT 8
#define APP_COMMAND_STATUS 9
#define APP_COMMAND_CHANGE_SPECIES 10
#define APP_COMMAND_USER_ACTION 11
#define APP_COMMAND_COLLECTION_CATALOG 12
#define APP_COMMAND_INSTRUCTION_RESPONSE 13
#ifndef ENABLE_INSTRUCTION_RESPONSE
#define ENABLE_INSTRUCTION_RESPONSE 0
#endif

// Normal daily lifecycle stays on; override to 0 only for diagnostics/size probes.
#ifndef ENABLE_DAILY_POST_PROCESSING
#define ENABLE_DAILY_POST_PROCESSING 1
#endif

// Optional feature defaults stay off. The Profile Resolver emits opt-ins.
#ifndef ENABLE_COLLECTION_CATALOG
#define ENABLE_COLLECTION_CATALOG 0
#endif

#ifndef ENABLE_GUESS_GAME
#define ENABLE_GUESS_GAME 0
#endif

#ifndef ENABLE_GUESS_GAME_SINGLE_ROUND
#define ENABLE_GUESS_GAME_SINGLE_ROUND 0
#endif

// When enabled, GuessLL/GuessRR represent the player's left/right choice.
// GuessLR/GuessRL are not required, and GuessWin/GuessLoss show the outcome.
#ifndef ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT
#define ENABLE_GUESS_GAME_PLAYER_CHOICE_RESULT 0
#endif

#ifndef ENABLE_STARTUP_ANIMATION
#define ENABLE_STARTUP_ANIMATION 0
#endif

#ifndef ENABLE_FIRST_START_ANIMATION
#define ENABLE_FIRST_START_ANIMATION 0
#endif

#if ENABLE_FIRST_START_ANIMATION != 0 && ENABLE_FIRST_START_ANIMATION != 1
#error "ENABLE_FIRST_START_ANIMATION must be 0 or 1."
#endif

#if ENABLE_FIRST_START_ANIMATION && !ENABLE_STARTUP_ANIMATION
#error "FirstStart animation requires startup animation support."
#endif

#ifndef ENABLE_OUTFIT_CHOOSE_ANIMATION
#define ENABLE_OUTFIT_CHOOSE_ANIMATION 0
#endif

#ifndef ENABLE_APPEARANCE_SELECTION
#define ENABLE_APPEARANCE_SELECTION 0
#endif

#ifndef ENABLE_COMMAND_PREDICT
#define ENABLE_COMMAND_PREDICT 0
#endif

#ifndef ENABLE_COMMAND_OUTFIT
#define ENABLE_COMMAND_OUTFIT 0
#endif

#ifndef ENABLE_SEQUENCE_AUDIO
#define ENABLE_SEQUENCE_AUDIO 0
#endif
#if ENABLE_SEQUENCE_AUDIO != 0 && ENABLE_SEQUENCE_AUDIO != 1
#error "ENABLE_SEQUENCE_AUDIO must be 0 or 1."
#endif

#ifndef ENABLE_DEATH
#define ENABLE_DEATH 0
#endif
#if ENABLE_DEATH != 0 && ENABLE_DEATH != 1
#error "ENABLE_DEATH must be 0 or 1."
#endif

#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 0
#endif

// When enabled, Status Sets are selected in exported order and wrap back to
// the first Set. The default preserves equal-probability random selection.
#ifndef ENABLE_SEQUENTIAL_STATUS_SET_SELECTION
#define ENABLE_SEQUENTIAL_STATUS_SET_SELECTION 0
#endif

#if ENABLE_SEQUENTIAL_STATUS_SET_SELECTION != 0 && ENABLE_SEQUENTIAL_STATUS_SET_SELECTION != 1
#error "ENABLE_SEQUENTIAL_STATUS_SET_SELECTION must be 0 or 1."
#endif

#ifndef APP_MAX_PET_STATS
#define APP_MAX_PET_STATS 12
#endif

// Generated per Project; do not reserve the maximum for every firmware build.
#ifndef APP_MAX_SCREEN_RULES
#define APP_MAX_SCREEN_RULES 64
#endif

#if ENABLE_COMMAND_OUTFIT && !ENABLE_APPEARANCE_SELECTION
#error "Appearance selection must be enabled when an appearance command is enabled."
#endif

#endif // APP_PROFILE_H
