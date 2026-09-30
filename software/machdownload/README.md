# MachDownload

**Type:** Python HTTP/HTTPS download manager  
**Stage:** Working alpha

A dependency-light segmented downloader for direct HTTP/HTTPS files.

## Implemented

- 1–32 HTTP Range workers;
- interruption and resume state;
- remote-object identity checks using size, ETag, and Last-Modified when available;
- TLS certificate verification;
- bounded retry with exponential backoff;
- exact segment-length and final-size validation;
- duplicate-filename protection;
- CLI and desktop GUI;
- local build scripts for Windows packaging.

## Validation

The source passes Python syntax checks. The bundled local range-server test successfully reconstructed an 8 MiB segmented download to the expected SHA-256 value.

## Remaining Release Work

- Windows packaging verification;
- real-world CDN regression testing;
- proxy/auth/cookie workflows;
- signed/expiring URL resume behavior;
- queue management;
- optional trusted-hash verification in the interface.

The project handles ordinary direct downloads only and does not include DRM bypass, credential harvesting, CAPTCHA bypass, or streaming-site extraction.
