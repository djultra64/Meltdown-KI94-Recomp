#ifndef KI_BGR555_H
#define KI_BGR555_H

#include <stdint.h>

/*
 * Add two packed BGR555 colors while saturating each five-bit component.
 * This mirrors the branch-free sequence at KI v1.5d addresses
 * 0x8801193c-0x88011958. Bit 15 is retained exactly as the original sequence
 * produces it; the arcade display ignores it when presenting BGR555 pixels.
 */
uint16_t ki_bgr555_saturating_add(uint16_t destination, uint16_t blend);

#endif
