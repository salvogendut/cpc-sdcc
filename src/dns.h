#ifndef DNS_H
#define DNS_H

/*
 * DNS A-record resolver.
 *
 * dns_resolve() sends a DNS query to the given server, waits up to 3000 ms
 * for a TYPE A answer, and writes the 4-byte IPv4 result into result_ip.
 *
 * Parameters:
 *   dns_server_ip  — 4-byte big-endian DNS server address
 *   hostname       — NUL-terminated ASCII hostname
 *   result_ip      — output: 4-byte buffer to receive the resolved address
 *
 * Returns 0 on success, negative on error:
 *   -1  chip or socket error
 *   -2  send failed
 *   -3  timeout (no response within 3000 ms)
 *   -4  DNS returned error / no A record in response
 */
int dns_resolve(const unsigned char *dns_server_ip, const char *hostname,
                unsigned char *result_ip);

#endif /* DNS_H */
