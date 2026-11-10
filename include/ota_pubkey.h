#pragma once

// Public key OTA images must be signed with; the private half lives only in the OTA_SIGNING_KEY secret.
static const char OTA_PUBKEY_PEM[] = R"PEM(
-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEg223U6ftn4koTyWUMsRDwteDKdvY
hJ5izDQhK9k5VdKlnuMtdgRF+zA1HcqbRY4DbuPDZGSXGP7IaZgLzxz7vw==
-----END PUBLIC KEY-----
)PEM";
