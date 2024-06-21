#ifndef _PERSIST_H
#define _PERSIST_H

typedef struct {
  uint8_t ch;
  int type;  // 0, ascii; 1, media
  uint8_t data[64];
} KEY_STRING;

KEY_STRING* persist_readBackMyData();
void persist_saveMyData();


#endif