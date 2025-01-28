/**************************************************************************/
/*  remote_debugger_peer.cpp                                              */
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

#include "remote_debugger_peer.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"

bool RemoteDebuggerPeerTCP::is_peer_connected() {
	return connected;
}

bool RemoteDebuggerPeerTCP::has_message() {
	return in_queue.size() > 0;
}

Array RemoteDebuggerPeerTCP::get_message() {
	MutexLock lock(mutex);
	ERR_FAIL_COND_V(!has_message(), Array());
	Array out = in_queue.front()->get();
	in_queue.pop_front();
	return out;
}

Error RemoteDebuggerPeerTCP::put_message(const Array &p_arr) {
	MutexLock lock(mutex);
	if (out_queue.size() >= max_queued_messages) {
		return ERR_OUT_OF_MEMORY;
	}

	out_queue.push_back(p_arr);
	return OK;
}

int RemoteDebuggerPeerTCP::get_max_message_size() const {
	return 8 << 20; // 8 MiB
}

void RemoteDebuggerPeerTCP::close() {
	running = false;
	if (thread.is_started()) {
		thread.wait_to_finish();
	}
	tcp_client->disconnect_from_host();
	out_buf.clear();
	in_buf.clear();
}

RemoteDebuggerPeerTCP::RemoteDebuggerPeerTCP(Ref<StreamPeerTCP> p_tcp) {
	// This means remote debugger takes 16 MiB just because it exists...
	in_buf.resize((8 << 20) + 4); // 8 MiB should be way more than enough (need 4 extra bytes for encoding packet size).
	out_buf.resize(8 << 20); // 8 MiB should be way more than enough
	tcp_client = p_tcp;
	if (tcp_client.is_valid()) { // Attaching to an already connected stream.
		connected = true;
		running = true;
		thread.start(_thread_func, this);
	} else {
		tcp_client.instantiate();
	}
}

RemoteDebuggerPeerTCP::~RemoteDebuggerPeerTCP() {
	close();
}

void RemoteDebuggerPeerTCP::_write_out() {
	while (tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTED && tcp_client->wait(NetSocket::POLL_TYPE_OUT) == OK) {
		uint8_t *buf = out_buf.ptrw();
		if (out_left <= 0) {
			if (out_queue.size() == 0) {
				break; // Nothing left to send
			}
			mutex.lock();
			Variant var = out_queue.front()->get();
			out_queue.pop_front();
			mutex.unlock();
			int size = 0;
			Error err = encode_variant(var, nullptr, size);
			ERR_CONTINUE(err != OK || size > out_buf.size() - 4); // 4 bytes separator.
			encode_uint32(size, buf);
			encode_variant(var, buf + 4, size);
			out_left = size + 4;
			out_pos = 0;
		}
		int sent = 0;
		tcp_client->put_partial_data(buf + out_pos, out_left, sent);
		out_left -= sent;
		out_pos += sent;
	}
}

void RemoteDebuggerPeerTCP::_read_in() {
	while (tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTED && tcp_client->wait(NetSocket::POLL_TYPE_IN) == OK) {
		uint8_t *buf = in_buf.ptrw();
		if (in_left <= 0) {
			if (in_queue.size() > max_queued_messages) {
				break; // Too many messages already in queue.
			}
			if (tcp_client->get_available_bytes() < 4) {
				break; // Need 4 more bytes.
			}
			uint32_t size = 0;
			int read = 0;
			Error err = tcp_client->get_partial_data((uint8_t *)&size, 4, read);
			ERR_CONTINUE(read != 4 || err != OK || size > (uint32_t)in_buf.size());
			in_left = size;
			in_pos = 0;
		}
		int read = 0;
		tcp_client->get_partial_data(buf + in_pos, in_left, read);
		in_left -= read;
		in_pos += read;
		if (in_left == 0) {
			Variant var;
			Error err = decode_variant(var, buf, in_pos, &read);
			ERR_CONTINUE(read != in_pos || err != OK);
			ERR_CONTINUE_MSG(var.get_type() != Variant::ARRAY, "Malformed packet received, not an Array.");
			MutexLock lock(mutex);
			in_queue.push_back(var);
		}
	}
}

Error RemoteDebuggerPeerTCP::connect_to_host(const String &p_host, uint16_t p_port) {
	IPAddress ip;
	if (p_host.is_valid_ip_address()) {
		ip = p_host;
	} else {
		ip = IP::get_singleton()->resolve_hostname(p_host);
	}

	int port = p_port;

	const int tries = 6;
	const int waits[tries] = { 1, 10, 100, 1000, 1000, 1000 };

	tcp_client->connect_to_host(ip, port);

	for (int i = 0; i < tries; i++) {
		tcp_client->poll();
		if (tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTED) {
			print_verbose("Remote Debugger: Connected!");
			break;
		} else {
			const int ms = waits[i];
			OS::get_singleton()->delay_usec(ms * 1000);
			print_verbose("Remote Debugger: Connection failed with status: '" + String::num_int64(tcp_client->get_status()) + "', retrying in " + String::num_int64(ms) + " msec.");
		}
	}

	if (tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		ERR_PRINT(vformat("Remote Debugger: Unable to connect. Status: %s.", String::num_int64(tcp_client->get_status())));
		return FAILED;
	}
	connected = true;
	running = true;
	thread.start(_thread_func, this);
	return OK;
}

void RemoteDebuggerPeerTCP::_thread_func(void *p_ud) {
	// Update in time for 144hz monitors
	const uint64_t min_tick = 6900;
	RemoteDebuggerPeerTCP *peer = static_cast<RemoteDebuggerPeerTCP *>(p_ud);
	while (peer->running && peer->is_peer_connected()) {
		uint64_t ticks_usec = OS::get_singleton()->get_ticks_usec();
		peer->_poll();
		if (!peer->is_peer_connected()) {
			break;
		}
		ticks_usec = OS::get_singleton()->get_ticks_usec() - ticks_usec;
		if (ticks_usec < min_tick) {
			OS::get_singleton()->delay_usec(min_tick - ticks_usec);
		}
	}
}

void RemoteDebuggerPeerTCP::poll() {
	// Nothing to do, polling is done in thread.
}

void RemoteDebuggerPeerTCP::_poll() {
	tcp_client->poll();
	if (connected) {
		_write_out();
		_read_in();
		connected = tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTED;
	}
}

RemoteDebuggerPeer *RemoteDebuggerPeerTCP::create(const String &p_uri) {
	ERR_FAIL_COND_V(!p_uri.begins_with("tcp://"), nullptr);

	String debug_host = p_uri.replace("tcp://", "");
	uint16_t debug_port = 6007;

	if (debug_host.contains_char(':')) {
		int sep_pos = debug_host.rfind_char(':');
		debug_port = debug_host.substr(sep_pos + 1).to_int();
		debug_host = debug_host.substr(0, sep_pos);
	}

	RemoteDebuggerPeerTCP *peer = memnew(RemoteDebuggerPeerTCP);
	Error err = peer->connect_to_host(debug_host, debug_port);
	if (err != OK) {
		memdelete(peer);
		return nullptr;
	}
	return peer;
}

RemoteDebuggerPeer::RemoteDebuggerPeer() {
	max_queued_messages = (int)GLOBAL_GET("network/limits/debugger/max_queued_messages");
}
