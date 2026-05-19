#ifndef PN532_H
#define PN532_H

#include <stdint.h>

/* Max ISO14443-A UID length (most cards are 4 or 7 bytes). */
#define PN532_MAX_UID_LEN  10

/* Mifare classic block size. */
#define PN532_MIFARE_BLOCK_SIZE  16

/* Default Mifare key A (factory default — change for production cards). */
#define PN532_MIFARE_DEFAULT_KEY_A  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/* Bring up UART1 (GPIO_26/27 @ 115200 8N1), wake PN532, run SAMConfiguration.
 * Returns 0 on success. After this, calls below are safe to use. */
int pn532_init(void);

/* PN532 GetFirmwareVersion. Output format example: 0x32010607 = PN532 v1.6.
 * Returns 0 + writes 4 bytes into out[0..3] on success. */
int pn532_get_firmware_version(uint8_t out[4]);

/* Poll once for an ISO14443-A target (Mifare/NTAG).
 * On success returns 0, writes UID length into *uid_len, UID bytes into uid[].
 * Returns negative if no card or protocol error. Typical poll period 300 ms. */
int pn532_read_card_uid(uint8_t uid[PN532_MAX_UID_LEN], uint8_t *uid_len);

/* Mifare Classic: authenticate with key A or key B then read a 16-byte block.
 * key_type = 0x60 for key A, 0x61 for key B.
 * Returns 0 + writes block content on success. */
int pn532_mifare_read_block(uint8_t key_type,
                            const uint8_t key[6],
                            const uint8_t *uid, uint8_t uid_len,
                            uint8_t block,
                            uint8_t out[PN532_MIFARE_BLOCK_SIZE]);

/* Mifare Classic: authenticate then write a 16-byte block.
 * WARNING: writing to sector trailer (block n*4+3) with wrong key bytes
 * permanently bricks that sector. */
int pn532_mifare_write_block(uint8_t key_type,
                             const uint8_t key[6],
                             const uint8_t *uid, uint8_t uid_len,
                             uint8_t block,
                             const uint8_t data[PN532_MIFARE_BLOCK_SIZE]);

#endif
