#ifndef _COMMON_H_
#define _COMMON_H_

#define RSS_ON
#define MAX_TBK_LEN (12)

typedef struct TLVBlOCK {
  // uint8_t type;
  uint8_t offset;
  uint32_t length;
  uint64_t hash;
}__attribute__((packed)) TLVBLK;

typedef struct TLVBLKLIST {
  uint8_t comps;
  uint8_t cur_comps;
  TLVBLK tlv_block[MAX_TBK_LEN];
}__attribute__((packed)) TBKLIST;

#endif
