/**************************************************************************/
/*  test_tcp_server.h                                                     */
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

#ifndef TEST_TCP_SERVER_H
#define TEST_TCP_SERVER_H

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "tests/test_macros.h"

namespace TestTCPServer {

const int PORT = 12345;
const IPAddress LOCALHOST("127.0.0.1");
const uint32_t SLEEP_DURATION = 1000;
const uint64_t MAX_WAIT_USEC = 100000;

Ref<TCPServer> create_server(const IPAddress &p_address, int p_port) {
	Ref<TCPServer> server;
	server.instantiate();

	REQUIRE_EQ(server->listen(PORT, LOCALHOST), Error::OK);
	REQUIRE(server->is_listening());
	CHECK_FALSE(server->is_connection_available());

	return server;
}

Ref<StreamPeerTCP> create_client(const IPAddress &p_address, int p_port) {
	Ref<StreamPeerTCP> client;
	client.instantiate();

	REQUIRE_EQ(client->connect_to_host(LOCALHOST, PORT), Error::OK);
	CHECK_EQ(client->get_connected_host(), LOCALHOST);
	CHECK_EQ(client->get_connected_port(), PORT);
	CHECK_EQ(client->get_status(), StreamPeerTCP::STATUS_CONNECTING);

	return client;
}

Ref<StreamPeerTCP> accept_connection(Ref<TCPServer> &p_server) {
	// Required to get the connection properly established.
	const uint64_t time = OS::get_singleton()->get_ticks_usec();
	while (!p_server->is_connection_available() && (OS::get_singleton()->get_ticks_usec() - time) < MAX_WAIT_USEC) {
		OS::get_singleton()->delay_usec(SLEEP_DURATION);
	}

	REQUIRE(p_server->is_connection_available());
	Ref<StreamPeerTCP> client_from_server = p_server->take_connection();
	REQUIRE(client_from_server.is_valid());
	CHECK_EQ(client_from_server->get_connected_host(), LOCALHOST);
	CHECK_EQ(client_from_server->get_status(), StreamPeerTCP::STATUS_CONNECTED);

	return client_from_server;
}

Error poll(Ref<StreamPeerTCP> p_client) {
	const uint64_t time = OS::get_singleton()->get_ticks_usec();
	Error err = p_client->poll();
	while (err != Error::OK && (OS::get_singleton()->get_ticks_usec() - time) < MAX_WAIT_USEC) {
		OS::get_singleton()->delay_usec(SLEEP_DURATION);
		err = p_client->poll();
	}
	return err;
}

TEST_CASE("[TCPServer] Instantiation") {
	Ref<TCPServer> server;
	server.instantiate();

	REQUIRE(server.is_valid());
	CHECK_EQ(false, server->is_listening());
}

TEST_CASE("[TCPServer] Accept a connection and receive/send data") {
	Ref<TCPServer> server = create_server(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client = create_client(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client_from_server = accept_connection(server);

	REQUIRE_EQ(poll(client), Error::OK);
	CHECK_EQ(client->get_status(), StreamPeerTCP::STATUS_CONNECTED);

	// Sending data from client to server.
	const String hello_world = "Hello World!";
	client->put_string(hello_world);
	CHECK_EQ(client_from_server->get_string(), hello_world);

	// Sending data from server to client.
	const float pi = 3.1415;
	client_from_server->put_float(pi);
	CHECK_EQ(client->get_float(), pi);

	client->disconnect_from_host();
	server->stop();
	CHECK_FALSE(server->is_listening());
}

TEST_CASE("[TCPServer] Handle multiple clients at the same time") {
	Ref<TCPServer> server = create_server(LOCALHOST, PORT);

	Vector<Ref<StreamPeerTCP>> clients;
	for (int i = 0; i < 5; i++) {
		clients.push_back(create_client(LOCALHOST, PORT));
	}

	Vector<Ref<StreamPeerTCP>> clients_from_server;
	for (int i = 0; i < clients.size(); i++) {
		clients_from_server.push_back(accept_connection(server));
	}

	// Calling poll() to update client status.
	for (Ref<StreamPeerTCP> &c : clients) {
		REQUIRE_EQ(poll(c), Error::OK);
	}

	// Sending data from each client to server.
	for (int i = 0; i < clients.size(); i++) {
		String hello_client = "Hello " + itos(i);
		clients[i]->put_string(hello_client);
		CHECK_EQ(clients_from_server[i]->get_string(), hello_client);
	}

	for (Ref<StreamPeerTCP> &c : clients) {
		c->disconnect_from_host();
	}
	server->stop();
}

TEST_CASE("[TCPServer] When stopped shouldn't accept new connections") {
	Ref<TCPServer> server = create_server(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client = create_client(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client_from_server = accept_connection(server);

	REQUIRE_EQ(poll(client), Error::OK);
	CHECK_EQ(client->get_status(), StreamPeerTCP::STATUS_CONNECTED);

	// Sending data from client to server.
	const String hello_world = "Hello World!";
	client->put_string(hello_world);
	CHECK_EQ(client_from_server->get_string(), hello_world);

	client->disconnect_from_host();
	server->stop();
	CHECK_FALSE(server->is_listening());

	Ref<StreamPeerTCP> new_client = create_client(LOCALHOST, PORT);

	// Required to get the connection properly established.
	uint64_t time = OS::get_singleton()->get_ticks_usec();
	while (!server->is_connection_available() && (OS::get_singleton()->get_ticks_usec() - time) < MAX_WAIT_USEC) {
		OS::get_singleton()->delay_usec(SLEEP_DURATION);
	}

	CHECK_FALSE(server->is_connection_available());

	time = OS::get_singleton()->get_ticks_usec();
	Error err = new_client->poll();
	while (err != Error::OK && err != Error::ERR_CONNECTION_ERROR && (OS::get_singleton()->get_ticks_usec() - time) < MAX_WAIT_USEC) {
		OS::get_singleton()->delay_usec(SLEEP_DURATION);
		err = new_client->poll();
	}
	REQUIRE((err == Error::OK || err == Error::ERR_CONNECTION_ERROR));
	StreamPeerTCP::Status status = new_client->get_status();
	CHECK((status == StreamPeerTCP::STATUS_CONNECTING || status == StreamPeerTCP::STATUS_ERROR));

	new_client->disconnect_from_host();
}

TEST_CASE("[TCPServer] Should disconnect client") {
	Ref<TCPServer> server = create_server(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client = create_client(LOCALHOST, PORT);
	Ref<StreamPeerTCP> client_from_server = accept_connection(server);

	REQUIRE_EQ(poll(client), Error::OK);
	CHECK_EQ(client->get_status(), StreamPeerTCP::STATUS_CONNECTED);

	// Sending data from client to server.
	const String hello_world = "Hello World!";
	client->put_string(hello_world);
	CHECK_EQ(client_from_server->get_string(), hello_world);

	client_from_server->disconnect_from_host();
	server->stop();
	CHECK_FALSE(server->is_listening());

	// Reading for a closed connection will print an error.
	ERR_PRINT_OFF;
	CHECK_EQ(client->get_string(), String());
	ERR_PRINT_ON;
	REQUIRE_EQ(poll(client), Error::OK);
	CHECK_EQ(client->get_status(), StreamPeerTCP::STATUS_NONE);
}

} // namespace TestTCPServer

#endif // TEST_TCP_SERVER_H
