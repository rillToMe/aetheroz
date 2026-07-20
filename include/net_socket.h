#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stdint.h>

// TCP client socket API over lwIP raw callbacks (Fase 6). NO_SYS=1: all lwIP
// entry is serialized by an internal net_lock; blocking calls poll via sti_hlt
// with the lock released, so the BSP timer poll can still deliver packets.

#define KSOCK_MAX      8
#define KSOCK_RXBUF    4096

void ksock_init(void);

// Grab the net_lock around e1000_poll + sys_check_timeouts. Called by cb_network.
void net_lock_acquire(uint64_t* saved);
void net_lock_release(uint64_t saved);

int ksock_socket(void);                                        // -> sockfd or -1
int ksock_connect(int s, uint32_t ip_be, uint16_t port);       // blocking; 0 ok, -1 fail
int ksock_send(int s, const void* buf, uint32_t len);          // bytes sent or -1
int ksock_recv(int s, void* buf, uint32_t len);                // bytes, 0=peer closed, -1=err
int ksock_close(int s);

#endif // NET_SOCKET_H
