/*
 * PN532 NFC reader driver over WS63 UART1 (GPIO_26 TX / GPIO_27 RX).
 * Default baud 115200, 8N1, no flow control.
 *
 * Frame format (NXP UM0701-02):
 *   PREAMBLE  0x00
 *   START     0x00 0xFF
 *   LEN       data length including TFI
 *   LCS       0x100 - LEN  (so LEN + LCS == 0 mod 256)
 *   TFI       0xD4 for host->PN532, 0xD5 for PN532->host
 *   DATA      command bytes...
 *   DCS       checksum of TFI + DATA: 0x100 - (sum mod 256)
 *   POSTAMBLE 0x00
 *
 * After every command host gets an ACK (0x00 0x00 0xFF 0x00 0xFF 0x00) then
 * the actual response frame. We wait for ACK first, then drain the response.
 */

#include <string.h>
#include "pinctrl.h"
#include "uart.h"
#include "soc_osal.h"
#include "osal_debug.h"
#include "errcode.h"

#include "pn532.h"

#define UART_BUS                  1            /* WS63 UART1 */
#define UART_TX_PIN               26           /* GPIO_26, board pin map */
#define UART_RX_PIN               27           /* GPIO_27 */
#define UART_PIN_MODE             1            /* UART_1_MODE per uart_porting.c */
#define UART_BAUD                 115200

#define PN532_HOSTTOPN532         0xD4
#define PN532_PN532TOHOST         0xD5

#define PN532_CMD_GET_FW_VER      0x02
#define PN532_CMD_SAMCONFIG       0x14
#define PN532_CMD_INLISTPASSIVE   0x4A
#define PN532_CMD_INDATAEXCHANGE  0x40

#define MIFARE_AUTH_A             0x60
#define MIFARE_AUTH_B             0x61
#define MIFARE_READ               0x30
#define MIFARE_WRITE              0xA0

/* Generous IO timeouts in ms — PN532 isn't fast. */
#define IO_TIMEOUT_MS             200
#define POLL_PASSIVE_TIMEOUT_MS   80

/* RX double buffer for uapi_uart_init — required by the SDK API even though
 * we use blocking reads via uapi_uart_read(). 256 bytes is plenty for PN532
 * responses (Mifare block read returns ~30 bytes max). */
#define UART_RX_BUF_SIZE          256
static uint8_t g_uart_rx_buf[UART_RX_BUF_SIZE];

static const uart_buffer_config_t g_uart_buf_cfg = {
    .rx_buffer = g_uart_rx_buf,
    .rx_buffer_size = UART_RX_BUF_SIZE,
};

static const uint8_t PN532_ACK_FRAME[6] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};

/* ── low level ─────────────────────────────────────────────────────────── */

static int uart_send(const uint8_t *buf, int len)
{
    int wrote = uapi_uart_write(UART_BUS, buf, len, IO_TIMEOUT_MS);
    return (wrote == len) ? 0 : -1;
}

/* Read up to `len` bytes within `timeout_ms`, return number actually read. */
static int uart_recv(uint8_t *buf, int len, int timeout_ms)
{
    int n = uapi_uart_read(UART_BUS, buf, len, timeout_ms);
    return (n < 0) ? 0 : n;
}

/* Build & send a PN532 host frame around the given command + arguments. */
static int pn532_send_frame(const uint8_t *cmd_data, uint8_t cmd_len)
{
    uint8_t frame[64];
    if (cmd_len > sizeof(frame) - 8) return -1;

    uint8_t i = 0;
    frame[i++] = 0x00;                /* preamble */
    frame[i++] = 0x00;
    frame[i++] = 0xFF;                /* start code */
    uint8_t length = cmd_len + 1;     /* include TFI */
    frame[i++] = length;
    frame[i++] = (uint8_t)(0x100 - length);  /* LCS */
    frame[i++] = PN532_HOSTTOPN532;

    uint8_t sum = PN532_HOSTTOPN532;
    for (uint8_t j = 0; j < cmd_len; j++) {
        frame[i++] = cmd_data[j];
        sum = (uint8_t)(sum + cmd_data[j]);
    }
    frame[i++] = (uint8_t)(0x100 - sum);     /* DCS */
    frame[i++] = 0x00;                       /* postamble */
    return uart_send(frame, i);
}

/* Wait for the 6-byte ACK frame. Returns 0 if seen. */
static int pn532_wait_ack(int timeout_ms)
{
    uint8_t buf[6] = {0};
    int got = uart_recv(buf, 6, timeout_ms);
    if (got < 6) return -1;
    if (memcmp(buf, PN532_ACK_FRAME, 6) != 0) return -2;
    return 0;
}

/* Receive a response frame, validate, and write the payload (everything
 * after PN532_PN532TOHOST byte) into out_data. Returns payload length,
 * or negative on error. */
static int pn532_recv_frame(uint8_t *out_data, int out_max, int timeout_ms)
{
    uint8_t hdr[5] = {0};
    /* Read until we get past preamble — PN532 may have leading 0x00s. */
    int hdr_got = uart_recv(hdr, 5, timeout_ms);
    if (hdr_got < 5) return -1;

    /* Some implementations skip the leading 0x00. We expect 0x00 0x00 0xFF
     * or 0x00 0xFF here; tolerate either. */
    uint8_t *p = hdr;
    if (p[0] == 0x00 && p[1] == 0x00 && p[2] == 0xFF) {
        p += 3;
    } else if (p[0] == 0x00 && p[1] == 0xFF) {
        p += 2;
    } else {
        return -2;
    }

    /* Pull remaining header bytes (LEN, LCS) — we may have already consumed
     * partial body in the 5-byte read above. Compute how many we still need. */
    uint8_t consumed = (uint8_t)(p - hdr);
    uint8_t tail_have = (uint8_t)(5 - consumed);   /* bytes left in hdr[] */
    uint8_t len_lcs[2];
    if (tail_have >= 2) {
        len_lcs[0] = p[0];
        len_lcs[1] = p[1];
    } else if (tail_have == 1) {
        len_lcs[0] = p[0];
        if (uart_recv(&len_lcs[1], 1, timeout_ms) != 1) return -3;
    } else {
        if (uart_recv(len_lcs, 2, timeout_ms) != 2) return -3;
    }
    uint8_t length = len_lcs[0];
    if ((uint8_t)(length + len_lcs[1]) != 0) return -4;   /* LEN+LCS check */
    if (length < 1 || length > out_max + 1) return -5;

    /* Read TFI + data + DCS + postamble together. */
    uint8_t buf[64];
    if (length + 2 > (int)sizeof(buf)) return -6;
    int need = length + 2;       /* TFI..DATA + DCS + postamble */
    int got = uart_recv(buf, need, timeout_ms);
    if (got < need) return -7;

    if (buf[0] != PN532_PN532TOHOST) return -8;

    /* Verify DCS */
    uint8_t sum = 0;
    for (int i = 0; i < length; i++) sum = (uint8_t)(sum + buf[i]);
    if ((uint8_t)(sum + buf[length]) != 0) return -9;

    int payload_len = length - 1;     /* exclude TFI */
    memcpy(out_data, &buf[1], payload_len);
    return payload_len;
}

/* Send a command, wait ACK, read response. */
static int pn532_exchange(const uint8_t *cmd, uint8_t cmd_len,
                          uint8_t *resp, int resp_max, int resp_timeout_ms)
{
    uapi_uart_flush_rx_data(UART_BUS);
    if (pn532_send_frame(cmd, cmd_len) != 0) return -100;
    if (pn532_wait_ack(IO_TIMEOUT_MS) != 0) return -101;
    return pn532_recv_frame(resp, resp_max, resp_timeout_ms);
}

/* ── public API ────────────────────────────────────────────────────────── */

int pn532_init(void)
{
    /* Pin mux: GPIO_26 → UART1 TX, GPIO_27 → UART1 RX */
    uapi_pin_set_mode(UART_TX_PIN, UART_PIN_MODE);
    uapi_pin_set_mode(UART_RX_PIN, UART_PIN_MODE);

    uart_attr_t attr = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_BIT_8,
        .stop_bits = UART_STOP_BIT_1,
        .parity    = UART_PARITY_NONE,
    };
    uart_pin_config_t pin_config = {
        .tx_pin = UART_TX_PIN,
        .rx_pin = UART_RX_PIN,
        .cts_pin = PIN_NONE,
        .rts_pin = PIN_NONE,
    };

    uapi_uart_deinit(UART_BUS);
    if (uapi_uart_init(UART_BUS, &pin_config, &attr, NULL,
                       (uart_buffer_config_t *)&g_uart_buf_cfg) != ERRCODE_SUCC) {
        osal_printk("[pn532] uapi_uart_init fail\r\n");
        return -1;
    }
    osal_msleep(50);  /* let PN532 settle */

    /* SAMConfiguration: normal mode, timeout 20*50ms=1s, no IRQ */
    uint8_t sam[] = {PN532_CMD_SAMCONFIG, 0x01, 0x14, 0x01};
    uint8_t resp[8];
    int rc = pn532_exchange(sam, sizeof(sam), resp, sizeof(resp), IO_TIMEOUT_MS);
    if (rc < 0) {
        osal_printk("[pn532] SAMConfiguration failed rc=%d\r\n", rc);
        return -2;
    }
    osal_printk("[pn532] SAMConfiguration ok\r\n");
    return 0;
}

int pn532_get_firmware_version(uint8_t out[4])
{
    uint8_t cmd = PN532_CMD_GET_FW_VER;
    uint8_t resp[8];
    int rc = pn532_exchange(&cmd, 1, resp, sizeof(resp), IO_TIMEOUT_MS);
    if (rc < 5) return -1;
    /* resp[0] = response code 0x03; resp[1..4] = IC, Ver, Rev, Support */
    out[0] = resp[1];
    out[1] = resp[2];
    out[2] = resp[3];
    out[3] = resp[4];
    return 0;
}

int pn532_read_card_uid(uint8_t uid[PN532_MAX_UID_LEN], uint8_t *uid_len)
{
    /* InListPassiveTarget: MaxTg=1, BrTy=0 (ISO14443-A 106kbps) */
    uint8_t cmd[] = {PN532_CMD_INLISTPASSIVE, 0x01, 0x00};
    uint8_t resp[32];
    int rc = pn532_exchange(cmd, sizeof(cmd), resp, sizeof(resp), POLL_PASSIVE_TIMEOUT_MS);
    if (rc < 0) return rc;
    /* Response: resp[0]=0x4B, resp[1]=NbTg.
     * If NbTg==0, no card. If 1: resp[2]=Tg, resp[3..4]=SENS_RES, resp[5]=SEL_RES,
     * resp[6]=NFCIDLength, resp[7..]=NFCID. */
    if (rc < 7) return -1;
    if (resp[0] != 0x4B) return -2;
    if (resp[1] == 0) return -3;   /* no card found this poll */
    uint8_t n = resp[6];
    if (n == 0 || n > PN532_MAX_UID_LEN) return -4;
    if (rc < 7 + n) return -5;
    memcpy(uid, &resp[7], n);
    *uid_len = n;
    return 0;
}

static int pn532_indataexchange(const uint8_t *apdu, uint8_t apdu_len,
                                uint8_t *resp, int resp_max)
{
    uint8_t cmd[64];
    if (apdu_len > sizeof(cmd) - 2) return -1;
    cmd[0] = PN532_CMD_INDATAEXCHANGE;
    cmd[1] = 0x01;   /* Tg = 1 (first target) */
    memcpy(&cmd[2], apdu, apdu_len);
    int rc = pn532_exchange(cmd, apdu_len + 2, resp, resp_max, IO_TIMEOUT_MS);
    if (rc < 2) return -2;
    /* resp[0] = 0x41 (response code), resp[1] = Status (0 = OK) */
    if (resp[0] != 0x41) return -3;
    if (resp[1] != 0x00) return -4;
    return rc - 2;
}

static int pn532_mifare_auth(uint8_t key_type, const uint8_t key[6],
                             const uint8_t *uid, uint8_t uid_len, uint8_t block)
{
    uint8_t apdu[20];
    uint8_t i = 0;
    apdu[i++] = key_type;        /* 0x60 or 0x61 */
    apdu[i++] = block;
    memcpy(&apdu[i], key, 6); i += 6;
    if (uid_len > 7) return -1;
    memcpy(&apdu[i], uid, uid_len); i += uid_len;
    uint8_t resp[4];
    int rc = pn532_indataexchange(apdu, i, resp, sizeof(resp));
    return rc < 0 ? rc : 0;
}

int pn532_mifare_read_block(uint8_t key_type, const uint8_t key[6],
                            const uint8_t *uid, uint8_t uid_len, uint8_t block,
                            uint8_t out[PN532_MIFARE_BLOCK_SIZE])
{
    if (pn532_mifare_auth(key_type, key, uid, uid_len, block) != 0) return -1;
    uint8_t apdu[2] = {MIFARE_READ, block};
    uint8_t resp[32];
    int rc = pn532_indataexchange(apdu, sizeof(apdu), resp, sizeof(resp));
    if (rc < PN532_MIFARE_BLOCK_SIZE) return -2;
    memcpy(out, resp, PN532_MIFARE_BLOCK_SIZE);
    return 0;
}

int pn532_mifare_write_block(uint8_t key_type, const uint8_t key[6],
                             const uint8_t *uid, uint8_t uid_len, uint8_t block,
                             const uint8_t data[PN532_MIFARE_BLOCK_SIZE])
{
    if (pn532_mifare_auth(key_type, key, uid, uid_len, block) != 0) return -1;
    uint8_t apdu[2 + PN532_MIFARE_BLOCK_SIZE];
    apdu[0] = MIFARE_WRITE;
    apdu[1] = block;
    memcpy(&apdu[2], data, PN532_MIFARE_BLOCK_SIZE);
    uint8_t resp[8];
    return pn532_indataexchange(apdu, sizeof(apdu), resp, sizeof(resp)) < 0 ? -2 : 0;
}
