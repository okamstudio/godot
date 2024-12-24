/**************************************************************************/
/*  enet_godot.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/**
 @file  enet_godot.cpp
 @brief ENet Godot specific functions
*/

#include "core/io/dtls_server.h"
#include "core/io/ip.h"
#include "core/io/net_socket.h"
#include "core/io/packet_peer_dtls.h"
#include "core/io/udp_server.h"
#include "core/os/os.h"

// This must be last for windows to compile (tested with MinGW)
#include "enet/enet.h"

/// Abstract ENet interface for UDP/DTLS.
class ENetGodotSocket {
public:
	virtual Error bind(IPAddress p_ip, uint16_t p_port) = 0;
	virtual Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) = 0;
	virtual Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) = 0;
	virtual Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) = 0;
	virtual int set_option(ENetSocketOption p_option, int p_value) = 0;
	virtual void close() = 0;
	virtual void set_refuse_new_connections(bool p_enable) {} /* Only used by dtls server */
	virtual bool can_upgrade() { return false; } /* Only true in ENetUDP */
	virtual ~ENetGodotSocket() {}
};

class ENetDTLSClient;
class ENetDTLSServer;

/// NetSocket interface
class ENetUDP : public ENetGodotSocket {
	friend class ENetDTLSClient;
	friend class ENetDTLSServer;

private:
	Ref<NetSocket> sock;
	IPAddress local_address;
	bool bound = false;

public:
	ENetUDP() {
		sock = Ref<NetSocket>(NetSocket::create());
		IP::Type ip_type = IP::TYPE_ANY;
		sock->open(NetSocket::TYPE_UDP, ip_type);
	}

	~ENetUDP() {
		sock->close();
	}

	bool can_upgrade() {
		return true;
	}

	Error bind(IPAddress p_ip, uint16_t p_port) {
		local_address = p_ip;
		bound = true;
		return sock->bind(p_ip, p_port);
	}

	Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) {
		Error err = sock->get_socket_address(r_ip, r_port);
		if (bound) {
			*r_ip = local_address;
		}
		return err;
	}

	Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) {
		return sock->sendto(p_buffer, p_len, r_sent, p_ip, p_port);
	}

	Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) {
		Error err = sock->poll(NetSocket::POLL_TYPE_IN, 0);
		if (err != OK) {
			return err;
		}
		return sock->recvfrom(p_buffer, p_len, r_read, r_ip, r_port);
	}

	int set_option(ENetSocketOption p_option, int p_value) {
		switch (p_option) {
			case ENET_SOCKOPT_NONBLOCK: {
				sock->set_blocking_enabled(p_value ? false : true);
				return 0;
			} break;

			case ENET_SOCKOPT_BROADCAST: {
				sock->set_broadcasting_enabled(p_value ? true : false);
				return 0;
			} break;

			case ENET_SOCKOPT_REUSEADDR: {
				sock->set_reuse_address_enabled(p_value ? true : false);
				return 0;
			} break;

			case ENET_SOCKOPT_RCVBUF: {
				return -1;
			} break;

			case ENET_SOCKOPT_SNDBUF: {
				return -1;
			} break;

			case ENET_SOCKOPT_RCVTIMEO: {
				return -1;
			} break;

			case ENET_SOCKOPT_SNDTIMEO: {
				return -1;
			} break;

			case ENET_SOCKOPT_NODELAY: {
				sock->set_tcp_no_delay_enabled(p_value ? true : false);
				return 0;
			} break;
		}

		return -1;
	}

	void close() {
		sock->close();
		local_address.clear();
	}
};

/// DTLS Client ENet interface
class ENetDTLSClient : public ENetGodotSocket {
	bool connected = false;
	Ref<PacketPeerUDP> udp;
	Ref<PacketPeerDTLS> dtls;
	Ref<TLSOptions> tls_options;
	String for_hostname;
	IPAddress local_address;

public:
	ENetDTLSClient(ENetUDP *p_base, String p_for_hostname, Ref<TLSOptions> p_options) {
		for_hostname = p_for_hostname;
		tls_options = p_options;
		udp.instantiate();
		dtls = Ref<PacketPeerDTLS>(PacketPeerDTLS::create());
		if (p_base->bound) {
			uint16_t port;
			p_base->get_socket_address(&local_address, &port);
			p_base->close();
			bind(local_address, port);
		}
	}

	~ENetDTLSClient() {
		close();
	}

	Error bind(IPAddress p_ip, uint16_t p_port) {
		local_address = p_ip;
		return udp->bind(p_port, p_ip);
	}

	Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) {
		if (!udp->is_bound()) {
			return ERR_UNCONFIGURED;
		}
		*r_ip = local_address;
		*r_port = udp->get_local_port();
		return OK;
	}

	Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) {
		if (!connected) {
			udp->connect_to_host(p_ip, p_port);
			if (dtls->connect_to_peer(udp, for_hostname, tls_options)) {
				return FAILED;
			}
			connected = true;
		}
		dtls->poll();
		if (dtls->get_status() == PacketPeerDTLS::STATUS_HANDSHAKING) {
			return ERR_BUSY;
		} else if (dtls->get_status() != PacketPeerDTLS::STATUS_CONNECTED) {
			return FAILED;
		}
		r_sent = p_len;
		return dtls->put_packet(p_buffer, p_len);
	}

	Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) {
		dtls->poll();
		if (dtls->get_status() == PacketPeerDTLS::STATUS_HANDSHAKING) {
			return ERR_BUSY;
		}
		if (dtls->get_status() != PacketPeerDTLS::STATUS_CONNECTED) {
			return FAILED;
		}
		int pc = dtls->get_available_packet_count();
		if (pc == 0) {
			return ERR_BUSY;
		} else if (pc < 0) {
			return FAILED;
		}

		const uint8_t *buffer;
		Error err = dtls->get_packet(&buffer, r_read);
		ERR_FAIL_COND_V(err != OK, err);
		ERR_FAIL_COND_V(p_len < r_read, ERR_OUT_OF_MEMORY);

		memcpy(p_buffer, buffer, r_read);
		r_ip = udp->get_packet_address();
		r_port = udp->get_packet_port();
		return err;
	}

	int set_option(ENetSocketOption p_option, int p_value) {
		return -1;
	}

	void close() {
		dtls->disconnect_from_peer();
		udp->close();
	}
};

/// DTLSServer - ENet interface
class ENetDTLSServer : public ENetGodotSocket {
	Ref<DTLSServer> server;
	Ref<UDPServer> udp_server;
	HashMap<String, Ref<PacketPeerDTLS>> peers;
	int last_service = 0;
	IPAddress local_address;

public:
	ENetDTLSServer(ENetUDP *p_base, Ref<TLSOptions> p_options) {
		udp_server.instantiate();
		if (p_base->bound) {
			uint16_t port;
			p_base->get_socket_address(&local_address, &port);
			p_base->close();
			bind(local_address, port);
		}
		server = Ref<DTLSServer>(DTLSServer::create());
		server->setup(p_options);
	}

	~ENetDTLSServer() {
		close();
	}

	void set_refuse_new_connections(bool p_refuse) {
		udp_server->set_max_pending_connections(p_refuse ? 0 : 16);
	}

	Error bind(IPAddress p_ip, uint16_t p_port) {
		local_address = p_ip;
		return udp_server->listen(p_port, p_ip);
	}

	Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) {
		if (!udp_server->is_listening()) {
			return ERR_UNCONFIGURED;
		}
		*r_ip = local_address;
		*r_port = udp_server->get_local_port();
		return OK;
	}

	Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) {
		String key = String(p_ip) + ":" + itos(p_port);
		if (!peers.has(key)) [[unlikely]] {
			// The peer might have been disconnected due to a DTLS error.
			// We need to wait for it to time out, just mark the packet as sent.
			r_sent = p_len;
			return OK;
		}
		Ref<PacketPeerDTLS> peer = peers[key];
		Error err = peer->put_packet(p_buffer, p_len);
		if (err == OK) {
			r_sent = p_len;
		} else if (err == ERR_BUSY) {
			r_sent = 0;
		} else {
			// The peer might have been disconnected due to a DTLS error.
			// We need to wait for it to time out, just mark the packet as sent.
			r_sent = p_len;
			return OK;
		}
		return err;
	}

	Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) {
		udp_server->poll();
		// TODO limits? Maybe we can better enforce allowed connections!
		if (udp_server->is_connection_available()) {
			Ref<PacketPeerUDP> udp = udp_server->take_connection();
			IPAddress peer_ip = udp->get_packet_address();
			int peer_port = udp->get_packet_port();
			Ref<PacketPeerDTLS> peer = server->take_connection(udp);
			PacketPeerDTLS::Status status = peer->get_status();
			if (status == PacketPeerDTLS::STATUS_HANDSHAKING || status == PacketPeerDTLS::STATUS_CONNECTED) {
				String key = String(peer_ip) + ":" + itos(peer_port);
				peers[key] = peer;
			}
		}

		List<String> remove;
		Error err = ERR_BUSY;
		// TODO this needs to be fair!

		for (KeyValue<String, Ref<PacketPeerDTLS>> &E : peers) {
			Ref<PacketPeerDTLS> peer = E.value;
			peer->poll();

			if (peer->get_status() == PacketPeerDTLS::STATUS_HANDSHAKING) {
				continue;
			} else if (peer->get_status() != PacketPeerDTLS::STATUS_CONNECTED) {
				// Peer disconnected, removing it.
				remove.push_back(E.key);
				continue;
			}

			if (peer->get_available_packet_count() > 0) {
				const uint8_t *buffer;
				err = peer->get_packet(&buffer, r_read);
				if (err != OK || p_len < r_read) {
					// Something wrong with this peer, removing it.
					remove.push_back(E.key);
					err = ERR_BUSY;
					r_read = 0;
					continue;
				}

				Vector<String> s = E.key.rsplit(":", false, 1);
				ERR_CONTINUE(s.size() != 2); // BUG!

				memcpy(p_buffer, buffer, r_read);
				r_ip = s[0];
				r_port = s[1].to_int();
				break; // err = OK
			}
		}

		// Remove disconnected peers from map.
		for (String &E : remove) {
			peers.erase(E);
		}

		return err; // OK, ERR_BUSY, or possibly an error.
	}

	int set_option(ENetSocketOption p_option, int p_value) {
		return -1;
	}

	void close() {
		for (KeyValue<String, Ref<PacketPeerDTLS>> &E : peers) {
			E.value->disconnect_from_peer();
		}
		peers.clear();
		udp_server->stop();
		server->stop();
		local_address.clear();
	}
};

static enet_uint32 timeBase = 0;

int enet_initialize(void) {
	return 0;
}

void enet_deinitialize(void) {
}

enet_uint32 enet_host_random_seed(void) {
	return (enet_uint32)OS::get_singleton()->get_unix_time();
}

enet_uint32 enet_time_get(void) {
	return OS::get_singleton()->get_ticks_msec() - timeBase;
}

void enet_time_set(enet_uint32 newTimeBase) {
	timeBase = OS::get_singleton()->get_ticks_msec() - newTimeBase;
}

int enet_address_set_host(ENetAddress *address, const char *name) {
	IPAddress ip = IP::get_singleton()->resolve_hostname(name);
	ERR_FAIL_COND_V(!ip.is_valid(), -1);

	enet_address_set_ip(address, ip.get_ipv6(), 16);
	return 0;
}

void enet_address_set_ip(ENetAddress *address, const uint8_t *ip, size_t size) {
	int len = size > 16 ? 16 : size;
	memset(address->host, 0, 16);
	memcpy(address->host, ip, len);
}

int enet_address_get_host_ip(const ENetAddress *address, char *name, size_t nameLength) {
	return -1;
}

int enet_address_get_host(const ENetAddress *address, char *name, size_t nameLength) {
	return -1;
}

ENetSocket enet_socket_create(ENetSocketType type) {
	ENetUDP *socket = memnew(ENetUDP);

	return socket;
}

int enet_host_dtls_server_setup(ENetHost *host, void *p_options) {
	ERR_FAIL_COND_V_MSG(!DTLSServer::is_available(), -1, "DTLS server is not available in this build.");
	ENetGodotSocket *sock = (ENetGodotSocket *)host->socket;
	if (!sock->can_upgrade()) {
		return -1;
	}
	host->socket = memnew(ENetDTLSServer(static_cast<ENetUDP *>(sock), Ref<TLSOptions>(static_cast<TLSOptions *>(p_options))));
	memdelete(sock);
	return 0;
}

int enet_host_dtls_client_setup(ENetHost *host, const char *p_for_hostname, void *p_options) {
	ERR_FAIL_COND_V_MSG(!PacketPeerDTLS::is_available(), -1, "DTLS is not available in this build.");
	ENetGodotSocket *sock = (ENetGodotSocket *)host->socket;
	if (!sock->can_upgrade()) {
		return -1;
	}
	host->socket = memnew(ENetDTLSClient(static_cast<ENetUDP *>(sock), String::utf8(p_for_hostname), Ref<TLSOptions>(static_cast<TLSOptions *>(p_options))));
	memdelete(sock);
	return 0;
}

void enet_host_refuse_new_connections(ENetHost *host, int p_refuse) {
	ERR_FAIL_NULL(host->socket);
	((ENetGodotSocket *)host->socket)->set_refuse_new_connections(p_refuse);
}

int enet_socket_bind(ENetSocket socket, const ENetAddress *address) {
	IPAddress ip;
	if (address->wildcard) {
		ip = IPAddress("*");
	} else {
		ip.set_ipv6(address->host);
	}

	ENetGodotSocket *sock = (ENetGodotSocket *)socket;
	if (sock->bind(ip, address->port) != OK) {
		return -1;
	}
	return 0;
}

void enet_socket_destroy(ENetSocket socket) {
	ENetGodotSocket *sock = (ENetGodotSocket *)socket;
	sock->close();
	memdelete(sock);
}

int enet_socket_send(ENetSocket socket, const ENetAddress *address, const ENetBuffer *buffers, size_t bufferCount) {
	ERR_FAIL_NULL_V(address, -1);

	ENetGodotSocket *sock = (ENetGodotSocket *)socket;
	IPAddress dest;
	Error err;
	size_t i = 0;

	dest.set_ipv6(address->host);

	// Create a single packet.
	Vector<uint8_t> out;
	uint8_t *w;
	int size = 0;
	int pos = 0;
	for (i = 0; i < bufferCount; i++) {
		size += buffers[i].dataLength;
	}

	out.resize(size);
	w = out.ptrw();
	for (i = 0; i < bufferCount; i++) {
		memcpy(&w[pos], buffers[i].data, buffers[i].dataLength);
		pos += buffers[i].dataLength;
	}

	int sent = 0;
	err = sock->sendto((const uint8_t *)&w[0], size, sent, dest, address->port);
	if (err != OK) {
		if (err == ERR_BUSY) { // Blocking call
			return 0;
		}

		WARN_PRINT("Sending failed!");
		return -1;
	}

	return sent;
}

int enet_socket_receive(ENetSocket socket, ENetAddress *address, ENetBuffer *buffers, size_t bufferCount) {
	ERR_FAIL_COND_V(bufferCount != 1, -1);

	ENetGodotSocket *sock = (ENetGodotSocket *)socket;

	int read;
	IPAddress ip;

	Error err = sock->recvfrom((uint8_t *)buffers[0].data, buffers[0].dataLength, read, ip, address->port);
	if (err == ERR_BUSY) {
		return 0;
	}
	if (err == ERR_OUT_OF_MEMORY) {
		// A packet above the ENET_PROTOCOL_MAXIMUM_MTU was received.
		return -2;
	}

	if (err != OK) {
		return -1;
	}

	enet_address_set_ip(address, ip.get_ipv6(), 16);

	return read;
}

int enet_socket_get_address(ENetSocket socket, ENetAddress *address) {
	IPAddress ip;
	uint16_t port;
	ENetGodotSocket *sock = (ENetGodotSocket *)socket;

	if (sock->get_socket_address(&ip, &port) != OK) {
		return -1;
	}

	enet_address_set_ip(address, ip.get_ipv6(), 16);
	address->port = port;

	return 0;
}

// Not implemented
int enet_socket_wait(ENetSocket socket, enet_uint32 *condition, enet_uint32 timeout) {
	return 0; // do we need this function?
}

int enet_socketset_select(ENetSocket maxSocket, ENetSocketSet *readSet, ENetSocketSet *writeSet, enet_uint32 timeout) {
	return -1;
}

int enet_socket_listen(ENetSocket socket, int backlog) {
	return -1;
}

int enet_socket_set_option(ENetSocket socket, ENetSocketOption option, int value) {
	ENetGodotSocket *sock = (ENetGodotSocket *)socket;
	return sock->set_option(option, value);
}

int enet_socket_get_option(ENetSocket socket, ENetSocketOption option, int *value) {
	return -1;
}

int enet_socket_connect(ENetSocket socket, const ENetAddress *address) {
	return -1;
}

ENetSocket enet_socket_accept(ENetSocket socket, ENetAddress *address) {
	return nullptr;
}

int enet_socket_shutdown(ENetSocket socket, ENetSocketShutdown how) {
	return -1;
}
