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

uint8_t music_irq[24]={
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
  0x4c,0x31,0xea // JMP $EA31
};


void music_jsr_1000(uint8_t a)
{
  asm ("sei");
  // Patch routine to call $1000 with A set
  music_irq[10]=a;
  music_irq[12]=0x1000&0xff;
  music_irq[21]=0x60;
  lcopy(music_irq,0x340,63);
  lcopy(music_irq,0x1c340,63);
  asm ("jsr $0340");  
}

void music_stop(void)
{
  asm ("sei");
  // Restore IRQ handler to normal
  POKE(0x0314,0x31);
  POKE(0x0315,0xea);  
}

void music_start(void)
{

  asm ("sei");
  music_irq[10]=0x00;
  music_irq[12]=0x1003&0xff;
  music_irq[21]=0x4c;
  lcopy(music_irq,0x340,63);
  lcopy(music_irq,0x1c340,63);
  // Set IRQ handler to music routine
  POKE(0x0314,0x40);
  POKE(0x0315,0x03);

  // Copy the bottom 4KB of memory
  lcopy(0x0000L,0x001c000L,4*1024);  
  
  asm ("cli");
}

