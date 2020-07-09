void music_select_tune(uint8_t a);
void music_stop(void);
void music_start(void);

// Temporary tune selection list for using commando music
#define TUNE_INTRO 0x00
#define TUNE_GETTING_READY 0x03
#define TUNE_INGAME 0x01
#define TUNE_VICTORY 0x02
#define TUNE_VICTORY_BACKWARDS 0x04
