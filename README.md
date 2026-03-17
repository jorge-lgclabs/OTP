# OTP
A suite of OTP (One-time pad) tools for encoding and decoding cipher messages using websocket APIs

+ Keygen
  - Generates a key of specified length from 27 valid characters
  - ```keygen keylength```

+ ENC server
  - Listens on a specified port and spawns child processes to handle client connections
  - Verifies connecting clients are legitimate before accepting data
  - Receives plaintext and key, returns encrypted ciphertext
  - Requires key length to be at least as long as the plaintext
  - Supports up to five concurrent connections
  - ```enc_server listening_port```

+ ENC client
  - Connects to enc_server, sending plaintext file and key file for encryption
  - Outputs received ciphertext to stdout for piping or redirection
  - Validates input files for bad characters and sufficient key length before sending
  - Exits with specific error codes for invalid input (1), connection failures (2), or success (0)
  - ```enc_client plaintext key port```

+ DEC server
  - Listens on a specified port and spawns child processes to handle client connections
  - Verifies connecting clients are legitimate before accepting data
  - Receives ciphertext and key, returns decrypted plaintext
  - Requires key length to be at least as long as the ciphertext
  - Supports up to five concurrent connections
  - ```dec_server listening_port```

+ DEC client
  - Connects to dec_server, sending ciphertext file and key file for decryption
  - Outputs received plaintext to stdout for piping or redirection
  - Validates input files for bad characters and sufficient key length before sending
  - Exits with specific error codes for invalid input (1), connection failures (2), or success (0)
  - ```dec_client ciphertext key port```
