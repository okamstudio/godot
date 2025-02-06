/**************************************************************************/
/*  ip.h                                                                  */
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

#pragma once

#include "core/io/ip_address.h"
#include "core/os/os.h"

template <typename T>
class TypedArray;

struct _IP_ResolverPrivate;

class IP : public Object {
	GDCLASS(IP, Object);

public:
	enum ResolverStatus {
		RESOLVER_STATUS_NONE,
		RESOLVER_STATUS_WAITING,
		RESOLVER_STATUS_DONE,
		RESOLVER_STATUS_ERROR,
	};

	enum Type {
		TYPE_NONE = 0,
		TYPE_IPV4 = 1,
		TYPE_IPV6 = 2,
		TYPE_ANY = 3,
	};

	enum {
		RESOLVER_MAX_QUERIES = 256,
		RESOLVER_INVALID_ID = -1
	};

	typedef int ResolverID;

private:
	_IP_ResolverPrivate *resolver = nullptr;

protected:
	static IP *singleton;
	static void _bind_methods();

	PackedStringArray _get_local_addresses() const;
	TypedArray<Dictionary> _get_local_interfaces() const;

	static IP *(*_create)();

public:
	struct Interface_Info {
		String name;
		String name_friendly;
		String index;
		List<IPAddress> ip_addresses;
	};

	IPAddress resolve_hostname(const String &p_hostname, Type p_type = TYPE_ANY);
	PackedStringArray resolve_hostname_addresses(const String &p_hostname, Type p_type = TYPE_ANY);
	// async resolver hostname
	ResolverID resolve_hostname_queue_item(const String &p_hostname, Type p_type = TYPE_ANY);
	ResolverStatus get_resolve_item_status(ResolverID p_id) const;
	IPAddress get_resolve_item_address(ResolverID p_id) const;
	virtual void get_local_addresses(List<IPAddress> *r_addresses) const;

	virtual void _resolve_hostname(List<IPAddress> &r_addresses, const String &p_hostname, Type p_type = TYPE_ANY) const = 0;
	Array get_resolve_item_addresses(ResolverID p_id) const;

	virtual void get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const = 0;
	void erase_resolve_item(ResolverID p_id);

	void clear_cache(const String &p_hostname = "");

	static IP *get_singleton();

	static IP *create();

	IP();
	~IP();
};

VARIANT_ENUM_CAST(IP::Type);
VARIANT_ENUM_CAST(IP::ResolverStatus);
