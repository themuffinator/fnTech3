/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_NET_QUERY_H
#define FNTECH3_NET_QUERY_H

/* A second IPv4 UDP socket beside the game's, for a profile whose browsers
 * ask a port of their own: Medal of Honor's servers answer GameSpy's status
 * queries there (code/server/server_mohaa_gamespy.hpp). It binds the
 * interface net_ip names, is read wherever the game's socket is (NET_Sleep),
 * so an answer goes out at once rather than at the next server frame, and
 * closes with the network (net_restart, NET_Shutdown).
 *
 * A third, the search socket, is a browser's: Medal of Honor's LAN list
 * broadcasts GameSpy's status query from it (code/client/cl_mohaa.cpp). It
 * binds a port the system picks on the same interface, may broadcast, and
 * closes by itself when its search is over, or with the network. */

#include "qcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called for each datagram that reaches the socket. */
typedef void ( *netQueryHandler_t )( const netadr_t *from, const byte *data, int length );

/* Binds the first free port of `count` from `firstPort`, replacing any
 * socket already open, and returns it; 0 when IPv4 networking is off, a
 * SOCKS proxy carries the game's traffic, or no port is free. */
int		NET_OpenQuerySocket( int firstPort, int count, netQueryHandler_t handler );
void	NET_CloseQuerySocket( void );
/* The bound port, 0 while closed. */
int		NET_QuerySocketPort( void );
/* Sends from the query socket, so that the answer comes from the port asked. */
void	NET_SendQueryPacket( int length, const void *data, const netadr_t *to );

/* Opens the search socket for `lifetime` milliseconds, replacing any search
 * still open, its answers going to `handler`; returns its port, or 0 when
 * NET_OpenQuerySocket would fail. */
int		NET_OpenSearchSocket( int lifetime, netQueryHandler_t handler );
void	NET_CloseSearchSocket( void );
/* Sends from the search socket, to an IPv4 address or as a broadcast. */
void	NET_SendSearchPacket( int length, const void *data, const netadr_t *to );

#ifdef __cplusplus
}
#endif

#endif
