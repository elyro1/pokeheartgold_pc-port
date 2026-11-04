#include <hg/native_socket.h>
#include <nitroWiFi/soc.h>

/* The original GameSpy wrapper only parses an IPv4 address through SOC. Use
 * the native SDK's public address type and preserve its network byte order. */
u32 gsSocketInetAddr(const char *address) {
    SOCInAddr result;
    return SOC_InetAtoN(address, &result) ? result.addr : (u32)-1;
}
