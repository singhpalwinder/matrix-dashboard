#include "ota.h"
#include "ota_pubkey.h"

#include <Arduino.h>
#include <esp_ota_ops.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>

namespace ota {

// DER-encoded ECDSA P-256 signatures are at most 72 bytes.
static constexpr size_t   MAX_SIG_LEN      = 80;
static constexpr uint32_t STALL_TIMEOUT_MS = 10000;
// Flash writes pause the panel's refresh interrupt (it isn't IRAM-safe), which shows
// as a flicker. One 256-byte page per write keeps each pause well under a millisecond.
static constexpr size_t   WRITE_CHUNK      = 256;

static volatile Phase    g_phase   = Phase::IDLE;
static volatile Phase    g_shown   = Phase::IDLE;
static volatile uint32_t g_got     = 0;
static volatile uint32_t g_total   = 0;
static volatile uint32_t g_phaseMs = 0;

Phase    phase()          { return g_phase; }
uint32_t received()       { return g_got; }
uint32_t total()          { return g_total; }
uint32_t phaseStartedMs() { return g_phaseMs; }
void     ackShown(Phase p) { g_shown = p; }
void     clearFailed()    { if (g_phase == Phase::FAILED) g_phase = Phase::IDLE; }

static void setPhase(Phase p) {
  g_phaseMs = millis();
  g_phase = p;
}

static const char* fail(const char* why) {
  setPhase(Phase::FAILED);
  return why;
}

static bool verifySignature(const uint8_t hash[32], const uint8_t* sig,
                            size_t sigLen) {
  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  // PEM length must include the NUL terminator.
  int rc = mbedtls_pk_parse_public_key(
      &pk, (const unsigned char*)OTA_PUBKEY_PEM, sizeof(OTA_PUBKEY_PEM));
  if (rc == 0 && !mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECDSA)) rc = -1;
  if (rc == 0) {
    rc = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, sig, sigLen);
  }
  mbedtls_pk_free(&pk);
  return rc == 0;
}

const char* receive(WiFiClient& c, size_t len, const String& sigB64) {
  if (len == 0) return "missing Content-Length";

  uint8_t sig[MAX_SIG_LEN];
  size_t  sigLen = 0;
  if (sigB64.length() == 0 ||
      mbedtls_base64_decode(sig, sizeof(sig), &sigLen,
                            (const unsigned char*)sigB64.c_str(),
                            sigB64.length()) != 0 ||
      sigLen == 0) {
    return "missing or malformed X-Signature";
  }

  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  if (!part) return "no OTA partition";
  if (len > part->size) return "image is larger than the OTA partition";

  g_total = len;
  g_got   = 0;

  // Erase the whole image range up front while the panel is blank: a stalled
  // refresh on an all-black frame is invisible. Waiting for the render loop to
  // confirm the blank frame is best effort (it may be asleep or busy).
  setPhase(Phase::ERASING);
  for (uint32_t t = millis(); g_shown != Phase::ERASING && millis() - t < 300;) delay(5);

  esp_ota_handle_t h = 0;
  esp_err_t e = esp_ota_begin(part, len, &h);  // erases ceil(len / 4K) sectors
  if (e != ESP_OK) return fail(esp_err_to_name(e));
  setPhase(Phase::WRITING);

  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&md);

  static uint8_t buf[2048];
  size_t   got      = 0;
  uint32_t lastData = millis();
  const char* err   = nullptr;

  while (got < len && !err) {
    size_t want = len - got;
    if (want > sizeof(buf)) want = sizeof(buf);
    int n = c.read(buf, want);
    if (n <= 0) {
      if (!c.connected() || millis() - lastData > STALL_TIMEOUT_MS) {
        err = "connection dropped before full image received";
        break;
      }
      delay(1);
      continue;
    }
    lastData = millis();
    mbedtls_md_update(&md, buf, n);
    for (int off = 0; off < n; off += WRITE_CHUNK) {
      size_t k = (size_t)(n - off) < WRITE_CHUNK ? (size_t)(n - off) : WRITE_CHUNK;
      e = esp_ota_write(h, buf + off, k);
      if (e != ESP_OK) { err = esp_err_to_name(e); break; }
    }
    got += n;
    g_got = got;
  }

  uint8_t hash[32];
  mbedtls_md_finish(&md, hash);
  mbedtls_md_free(&md);

  if (err) {
    // Aborted slot is never marked bootable; running firmware is untouched.
    esp_ota_abort(h);
    return fail(err);
  }

  setPhase(Phase::VERIFYING);
  e = esp_ota_end(h);  // checks the image structure and checksum
  if (e != ESP_OK) return fail(e == ESP_ERR_OTA_VALIDATE_FAILED ? "image failed validation" : esp_err_to_name(e));
  if (!verifySignature(hash, sig, sigLen)) return fail("signature verification failed");
  e = esp_ota_set_boot_partition(part);
  if (e != ESP_OK) return fail(esp_err_to_name(e));

  setPhase(Phase::DONE);
  return nullptr;
}

}  // namespace ota
