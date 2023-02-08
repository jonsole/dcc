/*
 * ltp305.h
 *
 * Created: 22/10/2021 21:49:50
 *  Author: jonso
 */ 


#ifndef LTP305_H_
#define LTP305_H_

#include <stdint.h>

void LTP305_Init(void);
void LTP305_DrawImage(uint8_t Display, const uint8_t *Image);
void LTP305_DrawChar(uint8_t Display, char Char);
void LPT305_SetBrightness(uint8_t Brightness);
void LTP305_Update(void);

#endif /* LTP305_H_ */