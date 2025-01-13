/**************************************************************************/
/*  http_client_tcp.cpp                                                   */
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

#ifndef WEB_ENABLED

#include "http_client_tcp.h"

#include "core/io/stream_peer_tls.h"
#include "core/version.h"

HTTPClient *HTTPClientTCP::_create_func(bool p_notify_postinitialize) {
	return static_cast<HTTPClient *>(ClassDB::creator<HTTPClientTCP>(p_notify_postinitialize));
}

Error HTTPClientTCP::connect_to_host(const String &p_host, int p_port, Ref<TLSOptions> p_options) {
	close();

	conn_port = p_port;
	conn_host = p_host;
	tls_options = p_options;

	ip_candidates.clear();

	String host_lower = conn_host.to_lower();
	if (host_lower.begins_with("http://")) {
		conn_host = conn_host.substr(7, conn_host.length() - 7);
		tls_options.unref();
	} else if (host_lower.begins_with("https://")) {
		if (tls_options.is_null()) {
			tls_options = TLSOptions::client();
		}
		conn_host = conn_host.substr(8, conn_host.length() - 8);
	}

	ERR_FAIL_COND_V(tls_options.is_valid() && tls_options->is_server(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(tls_options.is_valid() && !StreamPeerTLS::is_available(), ERR_UNAVAILABLE, "HTTPS is not available in this build.");
	ERR_FAIL_COND_V(conn_host.length() < HOST_MIN_LEN, ERR_INVALID_PARAMETER);

	if (conn_port < 0) {
		if (tls_options.is_valid()) {
			conn_port = PORT_HTTPS;
		} else {
			conn_port = PORT_HTTP;
		}
	}

	connection = tcp_connection;

	if (tls_options.is_valid() && https_proxy_port != -1) {
		proxy_client.instantiate(); // Needs proxy negotiation.
		server_host = https_proxy_host;
		server_port = https_proxy_port;
	} else if (tls_options.is_null() && http_proxy_port != -1) {
		server_host = http_proxy_host;
		server_port = http_proxy_port;
	} else {
		server_host = conn_host;
		server_port = conn_port;
	}

	if (server_host.is_valid_ip_address()) {
		// Host contains valid IP.
		Error err = tcp_connection->connect_to_host(IPAddress(server_host), server_port);
		if (err) {
			status = STATUS_CANT_CONNECT;
			return err;
		}

		status = STATUS_CONNECTING;
	} else {
		// Host contains hostname and needs to be resolved to IP.
		resolving = IP::get_singleton()->resolve_hostname_queue_item(server_host);
		if (resolving == IP::RESOLVER_INVALID_ID) {
			status = STATUS_CANT_RESOLVE;
			return ERR_CANT_RESOLVE;
		}
		status = STATUS_RESOLVING;
	}

	return OK;
}

void HTTPClientTCP::set_connection(const Ref<StreamPeer> &p_connection) {
	ERR_FAIL_COND_MSG(p_connection.is_null(), "Connection is not a reference to a valid StreamPeer object.");

	if (tls_options.is_valid()) {
		ERR_FAIL_NULL_MSG(Object::cast_to<StreamPeerTLS>(p_connection.ptr()),
				"Connection is not a reference to a valid StreamPeerTLS object.");
	}

	if (connection == p_connection) {
		return;
	}

	close();
	connection = p_connection;
	status = STATUS_CONNECTED;
}

Ref<StreamPeer> HTTPClientTCP::get_connection() const {
	return connection;
}

static bool _check_request_url(HTTPClientTCP::Method p_method, const String &p_url) {
	switch (p_method) {
		case HTTPClientTCP::METHOD_CONNECT: {
			// Authority in host:port format, as in RFC7231.
			int pos = p_url.find_char(':');
			return 0 < pos && pos < p_url.length() - 1;
		}
		case HTTPClientTCP::METHOD_OPTIONS: {
			if (p_url == "*") {
				return true;
			}
			[[fallthrough]];
		}
		default:
			// Absolute path or absolute URL.
			return p_url.begins_with("/") || p_url.begins_with("http://") || p_url.begins_with("https://");
	}
}

Error HTTPClientTCP::request(Method p_method, const String &p_url, const Vector<String> &p_headers, const uint8_t *p_body, int p_body_size) {
	ERR_FAIL_INDEX_V(p_method, METHOD_MAX, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!_check_request_url(p_method, p_url), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(status != STATUS_CONNECTED, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(connection.is_null(), ERR_INVALID_DATA);

	Error err = verify_headers(p_headers);
	if (err) {
		return err;
	}

	String uri = p_url;
	if (tls_options.is_null() && http_proxy_port != -1) {
		uri = vformat("http://%s:%d%s", conn_host, conn_port, p_url);
	}

	String request = String(_methods[p_method]) + " " + uri + " HTTP/1.1\r\n";
	bool add_host = true;
	bool add_clen = p_body_size > 0;
	bool add_uagent = true;
	bool add_accept = true;
	for (int i = 0; i < p_headers.size(); i++) {
		request += p_headers[i] + "\r\n";
		if (add_host && p_headers[i].findn("Host:") == 0) {
			add_host = false;
		}
		if (add_clen && p_headers[i].findn("Content-Length:") == 0) {
			add_clen = false;
		}
		if (add_uagent && p_headers[i].findn("User-Agent:") == 0) {
			add_uagent = false;
		}
		if (add_accept && p_headers[i].findn("Accept:") == 0) {
			add_accept = false;
		}
	}
	if (add_host) {
		if ((tls_options.is_valid() && conn_port == PORT_HTTPS) || (tls_options.is_null() && conn_port == PORT_HTTP)) {
			// Don't append the standard ports.
			request += "Host: " + conn_host + "\r\n";
		} else {
			request += "Host: " + conn_host + ":" + itos(conn_port) + "\r\n";
		}
	}
	if (add_clen) {
		request += "Content-Length: " + itos(p_body_size) + "\r\n";
		// Should it add utf8 encoding?
	}
	if (add_uagent) {
		request += "User-Agent: GodotEngine/" + String(VERSION_FULL_BUILD) + " (" + OS::get_singleton()->get_name() + ")\r\n";
	}
	if (add_accept) {
		request += "Accept: */*\r\n";
	}
	request += "\r\n";
	CharString cs = request.utf8();

	request_buffer->clear();
	request_buffer->put_data((const uint8_t *)cs.get_data(), cs.length());
	if (p_body_size > 0) {
		request_buffer->put_data(p_body, p_body_size);
	}
	request_buffer->seek(0);

	status = STATUS_REQUESTING;
	head_request = p_method == METHOD_HEAD;

	return OK;
}

bool HTTPClientTCP::has_response() const {
	return response_headers.size() != 0;
}

bool HTTPClientTCP::is_response_chunked() const {
	return chunked;
}

int HTTPClientTCP::get_response_code() const {
	return response_num;
}

Error HTTPClientTCP::get_response_headers(List<String> *r_response) {
	if (!response_headers.size()) {
		return ERR_INVALID_PARAMETER;
	}

	for (int i = 0; i < response_headers.size(); i++) {
		r_response->push_back(response_headers[i]);
	}

	response_headers.clear();

	return OK;
}

void HTTPClientTCP::close() {
	if (tcp_connection->get_status() != StreamPeerTCP::STATUS_NONE) {
		tcp_connection->disconnect_from_host();
	}

	connection.unref();
	proxy_client.unref();
	status = STATUS_DISCONNECTED;
	head_request = false;
	if (resolving != IP::RESOLVER_INVALID_ID) {
		IP::get_singleton()->erase_resolve_item(resolving);
		resolving = IP::RESOLVER_INVALID_ID;
	}

	ip_candidates.clear();
	response_headers.clear();
	response_str.clear();
	request_buffer->clear();
	body_size = -1;
	body_left = 0;
	chunk_left = 0;
	chunk_trailer_part = false;
	read_until_eof = false;
	response_num = 0;
	handshaking = false;
}

Error HTTPClientTCP::poll() {
	if (tcp_connection.is_valid()) {
		tcp_connection->poll();
	}
	switch (status) {
		case STATUS_RESOLVING: {
			ERR_FAIL_COND_V(resolving == IP::RESOLVER_INVALID_ID, ERR_BUG);

			IP::ResolverStatus rstatus = IP::get_singleton()->get_resolve_item_status(resolving);
			switch (rstatus) {
				case IP::RESOLVER_STATUS_WAITING:
					return OK; // Still resolving.

				case IP::RESOLVER_STATUS_DONE: {
					ip_candidates = IP::get_singleton()->get_resolve_item_addresses(resolving);
					IP::get_singleton()->erase_resolve_item(resolving);
					resolving = IP::RESOLVER_INVALID_ID;

					Error err = ERR_BUG; // Should be at least one entry.
					while (ip_candidates.size() > 0) {
						err = tcp_connection->connect_to_host(ip_candidates.pop_front(), server_port);
						if (err == OK) {
							break;
						}
					}
					if (err) {
						status = STATUS_CANT_CONNECT;
						return err;
					}

					status = STATUS_CONNECTING;
				} break;
				case IP::RESOLVER_STATUS_NONE:
				case IP::RESOLVER_STATUS_ERROR: {
					IP::get_singleton()->erase_resolve_item(resolving);
					resolving = IP::RESOLVER_INVALID_ID;
					close();
					status = STATUS_CANT_RESOLVE;
					return ERR_CANT_RESOLVE;
				} break;
			}
		} break;
		case STATUS_CONNECTING: {
			StreamPeerTCP::Status s = tcp_connection->get_status();
			switch (s) {
				case StreamPeerTCP::STATUS_CONNECTING: {
					return OK;
				} break;
				case StreamPeerTCP::STATUS_CONNECTED: {
					if (tls_options.is_valid() && proxy_client.is_valid()) {
						Error err = proxy_client->poll();
						if (err == ERR_UNCONFIGURED) {
							proxy_client->set_connection(tcp_connection);
							const Vector<String> headers;
							err = proxy_client->request(METHOD_CONNECT, vformat("%s:%d", conn_host, conn_port), headers, nullptr, 0);
							if (err != OK) {
								status = STATUS_CANT_CONNECT;
								return err;
							}
						} else if (err != OK) {
							status = STATUS_CANT_CONNECT;
							return err;
						}
						switch (proxy_client->get_status()) {
							case STATUS_REQUESTING: {
								return OK;
							} break;
							case STATUS_BODY: {
								proxy_client->read_response_body_chunk();
								return OK;
							} break;
							case STATUS_CONNECTED: {
								if (proxy_client->get_response_code() != RESPONSE_OK) {
									status = STATUS_CANT_CONNECT;
									return ERR_CANT_CONNECT;
								}
								proxy_client.unref();
								return OK;
							}
							case STATUS_DISCONNECTED:
							case STATUS_RESOLVING:
							case STATUS_CONNECTING: {
								status = STATUS_CANT_CONNECT;
								ERR_FAIL_V(ERR_BUG);
							} break;
							default: {
								status = STATUS_CANT_CONNECT;
								return ERR_CANT_CONNECT;
							} break;
						}
					} else if (tls_options.is_valid()) {
						Ref<StreamPeerTLS> tls_conn;
						if (!handshaking) {
							// Connect the StreamPeerTLS and start handshaking.
							tls_conn = Ref<StreamPeerTLS>(StreamPeerTLS::create());
							Error err = tls_conn->connect_to_stream(tcp_connection, conn_host, tls_options);
							if (err != OK) {
								close();
								status = STATUS_TLS_HANDSHAKE_ERROR;
								return ERR_CANT_CONNECT;
							}
							connection = tls_conn;
							handshaking = true;
						} else {
							// We are already handshaking, which means we can use your already active TLS connection.
							tls_conn = static_cast<Ref<StreamPeerTLS>>(connection);
							if (tls_conn.is_null()) {
								close();
								status = STATUS_TLS_HANDSHAKE_ERROR;
								return ERR_CANT_CONNECT;
							}

							tls_conn->poll(); // Try to finish the handshake.
						}

						if (tls_conn->get_status() == StreamPeerTLS::STATUS_CONNECTED) {
							// Handshake has been successful.
							handshaking = false;
							ip_candidates.clear();
							status = STATUS_CONNECTED;
							return OK;
						} else if (tls_conn->get_status() != StreamPeerTLS::STATUS_HANDSHAKING) {
							// Handshake has failed.
							close();
							status = STATUS_TLS_HANDSHAKE_ERROR;
							return ERR_CANT_CONNECT;
						}
						// ... we will need to poll more for handshake to finish.
					} else {
						ip_candidates.clear();
						status = STATUS_CONNECTED;
					}
					return OK;
				} break;
				case StreamPeerTCP::STATUS_ERROR:
				case StreamPeerTCP::STATUS_NONE: {
					Error err = ERR_CANT_CONNECT;
					while (ip_candidates.size() > 0) {
						tcp_connection->disconnect_from_host();
						err = tcp_connection->connect_to_host(ip_candidates.pop_front(), server_port);
						if (err == OK) {
							return OK;
						}
					}
					close();
					status = STATUS_CANT_CONNECT;
					return err;
				} break;
			}
		} break;
		case STATUS_BODY:
		case STATUS_CONNECTED: {
			// Check if we are still connected.
			if (tls_options.is_valid()) {
				Ref<StreamPeerTLS> tmp = connection;
				tmp->poll();
				if (tmp->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
					status = STATUS_CONNECTION_ERROR;
					return ERR_CONNECTION_ERROR;
				}
			} else if (tcp_connection->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
				status = STATUS_CONNECTION_ERROR;
				return ERR_CONNECTION_ERROR;
			}
			// Connection established, requests can now be made.
			return OK;
		} break;
		case STATUS_REQUESTING: {
			if (request_buffer->get_available_bytes()) {
				int avail = request_buffer->get_available_bytes();
				int pos = request_buffer->get_position();
				const Vector<uint8_t> data = request_buffer->get_data_array();
				int wrote = 0;
				Error err;
				if (blocking) {
					err = connection->put_data(data.ptr() + pos, avail);
					wrote += avail;
				} else {
					err = connection->put_partial_data(data.ptr() + pos, avail, wrote);
				}
				if (err != OK) {
					close();
					status = STATUS_CONNECTION_ERROR;
					return ERR_CONNECTION_ERROR;
				}
				pos += wrote;
				request_buffer->seek(pos);
				if (avail - wrote > 0) {
					return OK;
				}
				request_buffer->clear();
			}
			while (true) {
				uint8_t byte;
				int rec = 0;
				Error err = _get_http_data(&byte, 1, rec);
				if (err != OK) {
					close();
					status = STATUS_CONNECTION_ERROR;
					return ERR_CONNECTION_ERROR;
				}

				if (rec == 0) {
					return OK; // Still requesting, keep trying!
				}

				response_str.push_back(byte);
				int rs = response_str.size();
				if (
						(rs >= 2 && response_str[rs - 2] == '\n' && response_str[rs - 1] == '\n') ||
						(rs >= 4 && response_str[rs - 4] == '\r' && response_str[rs - 3] == '\n' && response_str[rs - 2] == '\r' && response_str[rs - 1] == '\n')) {
					// End of response, parse.
					response_str.push_back(0);
					String response;
					response.parse_utf8((const char *)response_str.ptr(), response_str.size());
					Vector<String> responses = response.split("\n");
					body_size = -1;
					chunked = false;
					body_left = 0;
					chunk_left = 0;
					chunk_trailer_part = false;
					read_until_eof = false;
					response_str.clear();
					response_headers.clear();
					response_num = RESPONSE_OK;

					// Per the HTTP 1.1 spec, keep-alive is the default.
					// Not following that specification breaks standard implementations.
					// Broken web servers should be fixed.
					bool keep_alive = true;

					for (int i = 0; i < responses.size(); i++) {
						String header = responses[i].strip_edges();
						String s = header.to_lower();
						if (s.length() == 0) {
							continue;
						}
						if (s.begins_with("content-length:")) {
							body_size = s.substr(s.find_char(':') + 1, s.length()).strip_edges().to_int();
							body_left = body_size;

						} else if (s.begins_with("transfer-encoding:")) {
							String encoding = header.substr(header.find_char(':') + 1, header.length()).strip_edges();
							if (encoding == "chunked") {
								chunked = true;
							}
						} else if (s.begins_with("connection: close")) {
							keep_alive = false;
						}

						if (i == 0 && responses[i].begins_with("HTTP")) {
							String num = responses[i].get_slicec(' ', 1);
							response_num = num.to_int();
						} else {
							response_headers.push_back(header);
						}
					}

					// This is a HEAD request, we won't receive anything.
					if (head_request) {
						body_size = 0;
						body_left = 0;
					}

					if (body_size != -1 || chunked) {
						status = STATUS_BODY;
					} else if (!keep_alive) {
						read_until_eof = true;
						status = STATUS_BODY;
					} else {
						status = STATUS_CONNECTED;
					}
					return OK;
				}
			}
		} break;
		case STATUS_DISCONNECTED: {
			return ERR_UNCONFIGURED;
		} break;
		case STATUS_CONNECTION_ERROR:
		case STATUS_TLS_HANDSHAKE_ERROR: {
			return ERR_CONNECTION_ERROR;
		} break;
		case STATUS_CANT_CONNECT: {
			return ERR_CANT_CONNECT;
		} break;
		case STATUS_CANT_RESOLVE: {
			return ERR_CANT_RESOLVE;
		} break;
	}

	return OK;
}

int64_t HTTPClientTCP::get_response_body_length() const {
	return body_size;
}

PackedByteArray HTTPClientTCP::read_response_body_chunk() {
	ERR_FAIL_COND_V(status != STATUS_BODY, PackedByteArray());

	PackedByteArray ret;
	Error err = OK;

	if (chunked) {
		while (true) {
			if (chunk_trailer_part) {
				// We need to consume the trailer part too or keep-alive will break.
				uint8_t b;
				int rec = 0;
				err = _get_http_data(&b, 1, rec);

				if (rec == 0) {
					break;
				}

				chunk.push_back(b);
				int cs = chunk.size();
				if ((cs >= 2 && chunk[cs - 2] == '\r' && chunk[cs - 1] == '\n')) {
					if (cs == 2) {
						// Finally over.
						chunk_trailer_part = false;
						status = STATUS_CONNECTED;
						chunk.clear();
						break;
					} else {
						// We do not process nor return the trailer data.
						chunk.clear();
					}
				}
			} else if (chunk_left == 0) {
				// Reading length.
				uint8_t b;
				int rec = 0;
				err = _get_http_data(&b, 1, rec);

				if (rec == 0) {
					break;
				}

				chunk.push_back(b);

				if (chunk.size() > 32) {
					ERR_PRINT("HTTP Invalid chunk hex len");
					status = STATUS_CONNECTION_ERROR;
					break;
				}

				if (chunk.size() > 2 && chunk[chunk.size() - 2] == '\r' && chunk[chunk.size() - 1] == '\n') {
					int len = 0;
					for (int i = 0; i < chunk.size() - 2; i++) {
						char c = chunk[i];
						int v = 0;
						if (is_digit(c)) {
							v = c - '0';
						} else if (c >= 'a' && c <= 'f') {
							v = c - 'a' + 10;
						} else if (c >= 'A' && c <= 'F') {
							v = c - 'A' + 10;
						} else {
							ERR_PRINT("HTTP Chunk len not in hex!!");
							status = STATUS_CONNECTION_ERROR;
							break;
						}
						len <<= 4;
						len |= v;
						if (len > (1 << 24)) {
							ERR_PRINT("HTTP Chunk too big!! >16mb");
							status = STATUS_CONNECTION_ERROR;
							break;
						}
					}

					if (len == 0) {
						// End reached!
						chunk_trailer_part = true;
						chunk.clear();
						break;
					}

					chunk_left = len + 2;
					chunk.resize(chunk_left);
				}
			} else {
				int rec = 0;
				err = _get_http_data(&chunk.write[chunk.size() - chunk_left], chunk_left, rec);
				if (rec == 0) {
					break;
				}
				chunk_left -= rec;

				if (chunk_left == 0) {
					const int chunk_size = chunk.size();
					if (chunk[chunk_size - 2] != '\r' || chunk[chunk_size - 1] != '\n') {
						ERR_PRINT("HTTP Invalid chunk terminator (not \\r\\n)");
						status = STATUS_CONNECTION_ERROR;
						break;
					}

					ret.resize(chunk_size - 2);
					uint8_t *w = ret.ptrw();
					memcpy(w, chunk.ptr(), chunk_size - 2);
					chunk.clear();
				}

				break;
			}
		}

	} else {
		int to_read = !read_until_eof ? MIN(body_left, read_chunk_size) : read_chunk_size;
		ret.resize(to_read);
		int _offset = 0;
		while (to_read > 0) {
			int rec = 0;
			{
				uint8_t *w = ret.ptrw();
				err = _get_http_data(w + _offset, to_read, rec);
			}
			if (rec <= 0) { // Ended up reading less.
				ret.resize(_offset);
				break;
			} else {
				_offset += rec;
				to_read -= rec;
				if (!read_until_eof) {
					body_left -= rec;
				}
			}
			if (err != OK) {
				ret.resize(_offset);
				break;
			}
		}
	}

	if (err != OK) {
		close();

		if (err == ERR_FILE_EOF) {
			status = STATUS_DISCONNECTED; // Server disconnected.
		} else {
			status = STATUS_CONNECTION_ERROR;
		}
	} else if (body_left == 0 && !chunked && !read_until_eof) {
		status = STATUS_CONNECTED;
	}

	return ret;
}

HTTPClientTCP::Status HTTPClientTCP::get_status() const {
	return status;
}

void HTTPClientTCP::set_blocking_mode(bool p_enable) {
	blocking = p_enable;
}

bool HTTPClientTCP::is_blocking_mode_enabled() const {
	return blocking;
}

Error HTTPClientTCP::_get_http_data(uint8_t *p_buffer, int p_bytes, int &r_received) {
	if (blocking) {
		// We can't use StreamPeer.get_data, since when reaching EOF we will get an
		// error without knowing how many bytes we received.
		Error err = ERR_FILE_EOF;
		int read = 0;
		int left = p_bytes;
		r_received = 0;
		while (left > 0) {
			err = connection->get_partial_data(p_buffer + r_received, left, read);
			if (err == OK) {
				r_received += read;
			} else if (err == ERR_FILE_EOF) {
				r_received += read;
				return err;
			} else {
				return err;
			}
			left -= read;
		}
		return err;
	} else {
		return connection->get_partial_data(p_buffer, p_bytes, r_received);
	}
}

void HTTPClientTCP::set_read_chunk_size(int p_size) {
	ERR_FAIL_COND(p_size < 256 || p_size > (1 << 24));
	read_chunk_size = p_size;
}

int HTTPClientTCP::get_read_chunk_size() const {
	return read_chunk_size;
}

void HTTPClientTCP::set_http_proxy(const String &p_host, int p_port) {
	if (p_host.is_empty() || p_port == -1) {
		http_proxy_host = "";
		http_proxy_port = -1;
	} else {
		http_proxy_host = p_host;
		http_proxy_port = p_port;
	}
}

void HTTPClientTCP::set_https_proxy(const String &p_host, int p_port) {
	if (p_host.is_empty() || p_port == -1) {
		https_proxy_host = "";
		https_proxy_port = -1;
	} else {
		https_proxy_host = p_host;
		https_proxy_port = p_port;
	}
}

HTTPClientTCP::HTTPClientTCP() {
	tcp_connection.instantiate();
	request_buffer.instantiate();
}

HTTPClient *(*HTTPClient::_create)(bool p_notify_postinitialize) = HTTPClientTCP::_create_func;

#endif // WEB_ENABLED
