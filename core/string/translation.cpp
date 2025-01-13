/**************************************************************************/
/*  translation.cpp                                                       */
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

#include "translation.h"

#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/string/translation_server.h"

Dictionary Translation::_get_messages() const {
	Dictionary d;
	for (const KeyValue<StringName, StringName> &E : translation_map) {
		d[E.key] = E.value;
	}
	return d;
}

Vector<String> Translation::_get_message_list() const {
	Vector<String> msgs;
	msgs.resize(translation_map.size());
	int idx = 0;
	for (const KeyValue<StringName, StringName> &E : translation_map) {
		msgs.set(idx, E.key);
		idx += 1;
	}

	return msgs;
}

Vector<String> Translation::get_translated_message_list() const {
	Vector<String> msgs;
	msgs.resize(translation_map.size());
	int idx = 0;
	for (const KeyValue<StringName, StringName> &E : translation_map) {
		msgs.set(idx, E.value);
		idx += 1;
	}

	return msgs;
}

void Translation::_set_messages(const Dictionary &p_messages) {
	List<Variant> keys;
	p_messages.get_key_list(&keys);
	for (const Variant &E : keys) {
		translation_map[E] = p_messages[E];
	}
}

void Translation::set_locale(const String &p_locale) {
	locale = TranslationServer::get_singleton()->standardize_locale(p_locale);

	if (Thread::is_main_thread()) {
		_notify_translation_changed_if_applies();
	} else {
		// This has to happen on the main thread (bypassing the ResourceLoader per-thread call queue)
		// because it interacts with the generally non-thread-safe window management, leading to
		// different issues across platforms otherwise.
		MessageQueue::get_main_singleton()->push_callable(callable_mp(this, &Translation::_notify_translation_changed_if_applies));
	}
}

void Translation::_notify_translation_changed_if_applies() {
	if (OS::get_singleton()->get_main_loop() && TranslationServer::get_singleton()->get_loaded_locales().has(get_locale())) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void Translation::add_message(const StringName &p_src_text, const StringName &p_xlated_text, const StringName &p_context) {
	translation_map[p_src_text] = p_xlated_text;
}

void Translation::add_plural_message(const StringName &p_src_text, const Vector<String> &p_plural_xlated_texts, const StringName &p_context) {
	WARN_PRINT("Translation class doesn't handle plural messages. Calling add_plural_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles plurals, such as TranslationPO class");
	ERR_FAIL_COND_MSG(p_plural_xlated_texts.is_empty(), "Parameter vector p_plural_xlated_texts passed in is empty.");
	translation_map[p_src_text] = p_plural_xlated_texts[0];
}

StringName Translation::get_message(const StringName &p_src_text, const StringName &p_context) const {
	StringName ret;
	if (GDVIRTUAL_CALL(_get_message, p_src_text, p_context, ret)) {
		return ret;
	}

	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in get_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	HashMap<StringName, StringName>::ConstIterator E = translation_map.find(p_src_text);
	if (!E) {
		return StringName();
	}

	return E->value;
}

StringName Translation::get_plural_message(const StringName &p_src_text, const StringName &p_plural_text, int p_n, const StringName &p_context) const {
	StringName ret;
	if (GDVIRTUAL_CALL(_get_plural_message, p_src_text, p_plural_text, p_n, p_context, ret)) {
		return ret;
	}

	WARN_PRINT("Translation class doesn't handle plural messages. Calling get_plural_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles plurals, such as TranslationPO class");
	return get_message(p_src_text);
}

void Translation::erase_message(const StringName &p_src_text, const StringName &p_context) {
	if (p_context != StringName()) {
		WARN_PRINT("Translation class doesn't handle context. Using context in erase_message() on a Translation instance is probably a mistake. \nUse a derived Translation class that handles context, such as TranslationPO class");
	}

	translation_map.erase(p_src_text);
}

void Translation::get_message_list(List<StringName> *r_messages) const {
	for (const KeyValue<StringName, StringName> &E : translation_map) {
		r_messages->push_back(E.key);
	}
}

int Translation::get_message_count() const {
	return translation_map.size();
}

void Translation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_locale", "locale"), &Translation::set_locale);
	ClassDB::bind_method(D_METHOD("get_locale"), &Translation::get_locale);
	ClassDB::bind_method(D_METHOD("add_message", "src_message", "xlated_message", "context"), &Translation::add_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("add_plural_message", "src_message", "xlated_messages", "context"), &Translation::add_plural_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_message", "src_message", "context"), &Translation::get_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_plural_message", "src_message", "src_plural_message", "n", "context"), &Translation::get_plural_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("erase_message", "src_message", "context"), &Translation::erase_message, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("get_message_list"), &Translation::_get_message_list);
	ClassDB::bind_method(D_METHOD("get_translated_message_list"), &Translation::get_translated_message_list);
	ClassDB::bind_method(D_METHOD("get_message_count"), &Translation::get_message_count);
	ClassDB::bind_method(D_METHOD("_set_messages", "messages"), &Translation::_set_messages);
	ClassDB::bind_method(D_METHOD("_get_messages"), &Translation::_get_messages);

	GDVIRTUAL_BIND(_get_plural_message, "src_message", "src_plural_message", "n", "context");
	GDVIRTUAL_BIND(_get_message, "src_message", "context");

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "messages", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_messages", "_get_messages");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "locale"), "set_locale", "get_locale");
}
