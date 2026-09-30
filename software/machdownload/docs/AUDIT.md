# Engineering Validation Notes

## Failure Modes Reviewed

1. **Remote object changes during resume**  
   Compare available size, ETag, and Last-Modified metadata before reusing partial data.

2. **Range request unexpectedly returns HTTP 200**  
   Segmented workers require HTTP 206 responses.

3. **Compressed transfer changes byte offsets**  
   Requests use `Accept-Encoding: identity`.

4. **Partial worker failure**  
   Final assembly begins only after every segment reports success.

5. **Premature EOF or wrong segment length**  
   Each segment is checked against its expected byte count.

6. **Completed-file overwrite**  
   Duplicate names are resolved with a numbered filename rather than silently replacing an existing file.

7. **Network instability**  
   Retries are bounded and use exponential backoff.

8. **Excessive connection count**  
   Worker count is capped and can be reduced for small files.

9. **TLS handling**  
   Standard Python HTTPS certificate validation is retained; the project does not implement custom certificate bypass logic.

## Local Test Result

The bundled range server and self-test were executed against an 8 MiB test payload. The segmented download completed and the assembled file matched the expected SHA-256 digest.

## Validation Still Required

A release candidate should additionally be tested against multiple real-world CDNs, redirects, authenticated endpoints, proxies, interrupted networks, and Windows packaging environments.
