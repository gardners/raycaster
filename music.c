/*
  Routines for controlling music playback in interrupts.

  This is a bit tricky, because we are using C code, and we don't
  really have enough spare RAM for the music routine in the first 
  64KB.  Probably the simplest approach is to use MAP to switch
  to a music player memory context, and then use TAB to set ZP to
  somewhere in there, as well.  We could in theory MAP from $0000-$1FFF,
  and not need to mess with B to move ZP, but that leaves a problem for our
  routine which will probably live in the tape buffer. That we can
  solve, though, by copying the first 1KB of RAM to the memory context
  for the music, so that when we MAP, the same data is there.  Sounds
  like a plan.

  This means our music interrupt will look like:
  <set map>
  lda #$C0
  ldx #$11  ; Map $0000-$1FFF to $1C000
  ldy #0
  ldz #0
  map
  lda #$00   ; (can be changed for tune selection when $1003 below
             ;  is changed to $1000)
  JSR $1003  ; Call music play routine (can be changed for setup)
  LDA #0
  tax
  tay
  taz
  map
  nop
  jmp $ea31
  

*/

#include <stdint.h>
#include <memory.h>
#include "music.h"

uint16_t play_addr;

uint8_t music_irq[27]={
  0xa9,0xc0,      // LDA #$c0
  0xa2,0x11,      // LDX #$11
  0xa0,0x00,      // LDY #$00
  0xa3,0x00,      // LDZ #$00
  0x5c,           // MAP
  0xa9,0x00,      // LDA #$00
  0x20,0x03,0x10, // JSR $1003
  0xa9,0x00,      // LDA #$00
  0xaa,           // TAX
  0xa8,           // TAY
  0x4b,           // TAZ
  0x5c,           // MAP
  0xea,           // NOP
  0xee,0x19,0xd0, // INC $D019 to ack raster interrupt
  0x4c,0x31,0xea // JMP $EA31
};


void music_select_tune(uint8_t a)
{
  music_stop();
  
  asm ("sei");
  // Patch routine to call init routine with A set
  music_irq[10]=a;

  play_addr=lpeek(0x1cf8c)<<8;
  play_addr+=lpeek(0x1cf8d);
  music_irq[12]=play_addr&0xff;
  music_irq[13]=play_addr>>8;
  
  
  music_irq[24]=0x60;
  lcopy(music_irq,0x340,63);
  lcopy(music_irq,0x1c340,63);

  asm ("jsr $0340");

}

void music_stop(void)
{
  asm ("sei");
  // Restore IRQ handler to normal
  POKE(0xD418,0);
  POKE(0x0314,0x31);
  POKE(0x0315,0xea);  
}

void music_start(void)
{

  asm ("sei");

  play_addr=lpeek(0x1cf8e)<<8;
  play_addr+=lpeek(0x1cf8f);
  music_irq[12]=play_addr&0xff;
  music_irq[13]=play_addr>>8;
  
  music_irq[10]=0x00;
  music_irq[24]=0x4c;

  
  lcopy(music_irq,0x340,63);
  lcopy(music_irq,0x1c340,63);
  // Set IRQ handler to music routine
  POKE(0x0314,0x40);
  POKE(0x0315,0x03);

  // Disable CIA interrupt, and enable raster interrupt
  POKE(0xDC0D,0x7f);
  POKE(0xD012,0xFF);
  POKE(0xD011,0x1B);
  POKE(0xD01A,0x81);
  POKE(0xD019,0x81);
  
  asm ("cli");
}

