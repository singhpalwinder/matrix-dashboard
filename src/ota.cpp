#include "ota.h"
#include "ota_pubkey.h"

#include <Arduino.h>
#include <Update.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>

namespace ota {

// DER-encoded ECDSA P-256 signatures are at most 72 bytes.
static constexpr size_t   MAX_SIG_LEN     = 80;
static constexpr uint32_t STALL_TIMEOUT_MS = 10000;

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

  if (!Update.begin(len, U_FLASH)) return Update.errorString();

  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&md);

  static uint8_t buf[2048];
  size_t   got      = 0;
  uint32_t lastData = millis();
  const char* err   = nullptr;

  while (got < len) {
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
    if (Update.write(buf, n) != (size_t)n) {
      err = Update.errorString();
      break;
    }
    got += n;
  }

  uint8_t hash[32];
  mbedtls_md_finish(&md, hash);
  mbedtls_md_free(&md);

  if (!err && !verifySignature(hash, sig, sigLen)) {
    err = "signature verification failed";
  }
  if (err) {
    // Aborted slot is never marked bootable; running firmware is untouched.
    Update.abort();
    return err;
  }
  if (!Update.end()) return Update.errorString();
  return nullptr;
}

}  // namespace ota
