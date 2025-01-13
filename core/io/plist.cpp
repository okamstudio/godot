/**************************************************************************/
/*  plist.cpp                                                             */
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

#include "plist.h"

PList::PLNodeType PListNode::get_type() const {
	return data_type;
}

Variant PListNode::get_value() const {
	switch (data_type) {
		case PList::PL_NODE_TYPE_NIL: {
			return Variant();
		} break;
		case PList::PL_NODE_TYPE_STRING: {
			return String::utf8(data_string.get_data());
		} break;
		case PList::PL_NODE_TYPE_ARRAY: {
			Array arr;
			for (const Ref<PListNode> &E : data_array) {
				arr.push_back(E);
			}
			return arr;
		} break;
		case PList::PL_NODE_TYPE_DICT: {
			Dictionary dict;
			for (const KeyValue<String, Ref<PListNode>> &E : data_dict) {
				dict[E.key] = E.value;
			}
			return dict;
		} break;
		case PList::PL_NODE_TYPE_BOOLEAN: {
			return data_bool;
		} break;
		case PList::PL_NODE_TYPE_INTEGER: {
			return data_int;
		} break;
		case PList::PL_NODE_TYPE_REAL: {
			return data_real;
		} break;
		case PList::PL_NODE_TYPE_DATA: {
			int strlen = data_string.length();

			size_t arr_len = 0;
			Vector<uint8_t> buf;
			{
				buf.resize(strlen / 4 * 3 + 1);
				uint8_t *w = buf.ptrw();

				ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &arr_len, (unsigned char *)data_string.get_data(), strlen) != OK, Vector<uint8_t>());
			}
			buf.resize(arr_len);
			return buf;
		} break;
		case PList::PL_NODE_TYPE_DATE: {
			return String(data_string.get_data());
		} break;
	}
	return Variant();
}

Ref<PListNode> PListNode::new_node(const Variant &p_value) {
	Ref<PListNode> node;
	node.instantiate();

	switch (p_value.get_type()) {
		case Variant::NIL: {
			node->data_type = PList::PL_NODE_TYPE_NIL;
		} break;
		case Variant::BOOL: {
			node->data_type = PList::PL_NODE_TYPE_BOOLEAN;
			node->data_bool = p_value;
		} break;
		case Variant::INT: {
			node->data_type = PList::PL_NODE_TYPE_INTEGER;
			node->data_int = p_value;
		} break;
		case Variant::FLOAT: {
			node->data_type = PList::PL_NODE_TYPE_REAL;
			node->data_real = p_value;
		} break;
		case Variant::STRING_NAME:
		case Variant::STRING: {
			node->data_type = PList::PL_NODE_TYPE_STRING;
			node->data_string = p_value.operator String().utf8();
		} break;
		case Variant::DICTIONARY: {
			node->data_type = PList::PL_NODE_TYPE_DICT;
			Dictionary dict = p_value;
			const Variant *next = dict.next(nullptr);
			while (next) {
				Ref<PListNode> sub_node = dict[*next];
				ERR_FAIL_COND_V_MSG(sub_node.is_null(), Ref<PListNode>(), "Invalid dictionary element, should be PListNode.");
				node->data_dict[*next] = sub_node;
				next = dict.next(next);
			}
		} break;
		case Variant::ARRAY: {
			node->data_type = PList::PL_NODE_TYPE_ARRAY;
			Array ar = p_value;
			for (int i = 0; i < ar.size(); i++) {
				Ref<PListNode> sub_node = ar[i];
				ERR_FAIL_COND_V_MSG(sub_node.is_null(), Ref<PListNode>(), "Invalid array element, should be PListNode.");
				node->data_array.push_back(sub_node);
			}
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			node->data_type = PList::PL_NODE_TYPE_DATA;
			PackedByteArray buf = p_value;
			node->data_string = CryptoCore::b64_encode_str(buf.ptr(), buf.size()).utf8();
		} break;
		default: {
			ERR_FAIL_V_MSG(Ref<PListNode>(), "Unsupported data type.");
		} break;
	}
	return node;
}

Ref<PListNode> PListNode::new_array() {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_ARRAY;
	return node;
}

Ref<PListNode> PListNode::new_dict() {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_DICT;
	return node;
}

Ref<PListNode> PListNode::new_string(const String &p_string) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_STRING;
	node->data_string = p_string.utf8();
	return node;
}

Ref<PListNode> PListNode::new_data(const String &p_string) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_DATA;
	node->data_string = p_string.utf8();
	return node;
}

Ref<PListNode> PListNode::new_date(const String &p_string) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_DATE;
	node->data_string = p_string.utf8();
	node->data_real = (double)Time::get_singleton()->get_unix_time_from_datetime_string(p_string) - 978307200.0;
	return node;
}

Ref<PListNode> PListNode::new_bool(bool p_bool) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_BOOLEAN;
	node->data_bool = p_bool;
	return node;
}

Ref<PListNode> PListNode::new_int(int64_t p_int) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_INTEGER;
	node->data_int = p_int;
	return node;
}

Ref<PListNode> PListNode::new_real(double p_real) {
	Ref<PListNode> node = memnew(PListNode());
	ERR_FAIL_COND_V(node.is_null(), Ref<PListNode>());
	node->data_type = PList::PLNodeType::PL_NODE_TYPE_REAL;
	node->data_real = p_real;
	return node;
}

bool PListNode::push_subnode(const Ref<PListNode> &p_node, const String &p_key) {
	ERR_FAIL_COND_V(p_node.is_null(), false);
	if (data_type == PList::PLNodeType::PL_NODE_TYPE_DICT) {
		ERR_FAIL_COND_V(p_key.is_empty(), false);
		ERR_FAIL_COND_V(data_dict.has(p_key), false);
		data_dict[p_key] = p_node;
		return true;
	} else if (data_type == PList::PLNodeType::PL_NODE_TYPE_ARRAY) {
		data_array.push_back(p_node);
		return true;
	} else {
		ERR_FAIL_V_MSG(false, "PList: Invalid parent node type, should be DICT or ARRAY.");
	}
}

size_t PListNode::get_asn1_size(uint8_t p_len_octets) const {
	// Get size of all data, excluding type and size information.
	switch (data_type) {
		case PList::PLNodeType::PL_NODE_TYPE_NIL: {
			return 0;
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DATA:
		case PList::PLNodeType::PL_NODE_TYPE_DATE: {
			ERR_FAIL_V_MSG(0, "PList: DATE and DATA nodes are not supported by ASN.1 serialization.");
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_STRING: {
			return data_string.length();
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_BOOLEAN: {
			return 1;
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_INTEGER:
		case PList::PLNodeType::PL_NODE_TYPE_REAL: {
			return 4;
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_ARRAY: {
			size_t size = 0;
			for (int i = 0; i < data_array.size(); i++) {
				size += 1 + _asn1_size_len(p_len_octets) + data_array[i]->get_asn1_size(p_len_octets);
			}
			return size;
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DICT: {
			size_t size = 0;

			for (const KeyValue<String, Ref<PListNode>> &E : data_dict) {
				size += 1 + _asn1_size_len(p_len_octets); // Sequence.
				size += 1 + _asn1_size_len(p_len_octets) + E.key.utf8().length(); //Key.
				size += 1 + _asn1_size_len(p_len_octets) + E.value->get_asn1_size(p_len_octets); // Value.
			}
			return size;
		} break;
		default: {
			return 0;
		} break;
	}
}

int PListNode::_asn1_size_len(uint8_t p_len_octets) {
	if (p_len_octets > 1) {
		return p_len_octets + 1;
	} else {
		return 1;
	}
}

void PListNode::store_asn1_size(PackedByteArray &p_stream, uint8_t p_len_octets) const {
	uint32_t size = get_asn1_size(p_len_octets);
	if (p_len_octets > 1) {
		p_stream.push_back(0x80 + p_len_octets);
	}
	for (int i = p_len_octets - 1; i >= 0; i--) {
		uint8_t x = (size >> i * 8) & 0xFF;
		p_stream.push_back(x);
	}
}

bool PListNode::store_asn1(PackedByteArray &p_stream, uint8_t p_len_octets) const {
	// Convert to binary ASN1 stream.
	bool valid = true;
	switch (data_type) {
		case PList::PLNodeType::PL_NODE_TYPE_NIL: {
			// Nothing to store.
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DATE:
		case PList::PLNodeType::PL_NODE_TYPE_DATA: {
			ERR_FAIL_V_MSG(false, "PList: DATE and DATA nodes are not supported by ASN.1 serialization.");
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_STRING: {
			p_stream.push_back(0x0C);
			store_asn1_size(p_stream, p_len_octets);
			for (int i = 0; i < data_string.size(); i++) {
				p_stream.push_back(data_string[i]);
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_BOOLEAN: {
			p_stream.push_back(0x01);
			store_asn1_size(p_stream, p_len_octets);
			if (data_bool) {
				p_stream.push_back(0x01);
			} else {
				p_stream.push_back(0x00);
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_INTEGER: {
			p_stream.push_back(0x02);
			store_asn1_size(p_stream, p_len_octets);
			for (int i = 4; i >= 0; i--) {
				uint8_t x = (data_int >> i * 8) & 0xFF;
				p_stream.push_back(x);
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_REAL: {
			p_stream.push_back(0x03);
			store_asn1_size(p_stream, p_len_octets);
			for (int i = 4; i >= 0; i--) {
				uint8_t x = (data_int >> i * 8) & 0xFF;
				p_stream.push_back(x);
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_ARRAY: {
			p_stream.push_back(0x30); // Sequence.
			store_asn1_size(p_stream, p_len_octets);
			for (int i = 0; i < data_array.size(); i++) {
				valid = valid && data_array[i]->store_asn1(p_stream, p_len_octets);
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DICT: {
			p_stream.push_back(0x31); // Set.
			store_asn1_size(p_stream, p_len_octets);
			for (const KeyValue<String, Ref<PListNode>> &E : data_dict) {
				CharString cs = E.key.utf8();
				uint32_t size = cs.length();

				// Sequence.
				p_stream.push_back(0x30);
				uint32_t seq_size = 2 * (1 + _asn1_size_len(p_len_octets)) + size + E.value->get_asn1_size(p_len_octets);
				if (p_len_octets > 1) {
					p_stream.push_back(0x80 + p_len_octets);
				}
				for (int i = p_len_octets - 1; i >= 0; i--) {
					uint8_t x = (seq_size >> i * 8) & 0xFF;
					p_stream.push_back(x);
				}
				// Key.
				p_stream.push_back(0x0C);
				if (p_len_octets > 1) {
					p_stream.push_back(0x80 + p_len_octets);
				}
				for (int i = p_len_octets - 1; i >= 0; i--) {
					uint8_t x = (size >> i * 8) & 0xFF;
					p_stream.push_back(x);
				}
				for (uint32_t i = 0; i < size; i++) {
					p_stream.push_back(cs[i]);
				}
				// Value.
				valid = valid && E.value->store_asn1(p_stream, p_len_octets);
			}
		} break;
	}
	return valid;
}

void PListNode::store_text(String &p_stream, uint8_t p_indent) const {
	// Convert to text XML stream.
	switch (data_type) {
		case PList::PLNodeType::PL_NODE_TYPE_NIL: {
			// Nothing to store.
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DATA: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<data>\n";
			p_stream += String("\t").repeat(p_indent);
			p_stream += data_string + "\n";
			p_stream += String("\t").repeat(p_indent);
			p_stream += "</data>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DATE: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<date>";
			p_stream += data_string;
			p_stream += "</date>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_STRING: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<string>";
			p_stream += String::utf8(data_string);
			p_stream += "</string>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_BOOLEAN: {
			p_stream += String("\t").repeat(p_indent);
			if (data_bool) {
				p_stream += "<true/>\n";
			} else {
				p_stream += "<false/>\n";
			}
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_INTEGER: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<integer>";
			p_stream += itos(data_int);
			p_stream += "</integer>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_REAL: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<real>";
			p_stream += rtos(data_real);
			p_stream += "</real>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_ARRAY: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<array>\n";
			for (int i = 0; i < data_array.size(); i++) {
				data_array[i]->store_text(p_stream, p_indent + 1);
			}
			p_stream += String("\t").repeat(p_indent);
			p_stream += "</array>\n";
		} break;
		case PList::PLNodeType::PL_NODE_TYPE_DICT: {
			p_stream += String("\t").repeat(p_indent);
			p_stream += "<dict>\n";
			for (const KeyValue<String, Ref<PListNode>> &E : data_dict) {
				p_stream += String("\t").repeat(p_indent + 1);
				p_stream += "<key>";
				p_stream += E.key;
				p_stream += "</key>\n";
				E.value->store_text(p_stream, p_indent + 1);
			}
			p_stream += String("\t").repeat(p_indent);
			p_stream += "</dict>\n";
		} break;
	}
}

/*************************************************************************/

PList::PList() {
	root = PListNode::new_dict();
}

PList::PList(const String &p_string) {
	String err_str;
	bool ok = load_string(p_string, err_str);
	ERR_FAIL_COND_MSG(!ok, vformat("PList: %s.", err_str));
}

uint64_t PList::read_bplist_var_size_int(Ref<FileAccess> p_file, uint8_t p_size) {
	uint64_t pos = p_file->get_position();
	uint64_t ret = 0;
	switch (p_size) {
		case 1: {
			ret = p_file->get_8();
		} break;
		case 2: {
			ret = BSWAP16(p_file->get_16());
		} break;
		case 3: {
			ret = BSWAP32(p_file->get_32() & 0x00FFFFFF);
		} break;
		case 4: {
			ret = BSWAP32(p_file->get_32());
		} break;
		case 5: {
			ret = BSWAP64(p_file->get_64() & 0x000000FFFFFFFFFF);
		} break;
		case 6: {
			ret = BSWAP64(p_file->get_64() & 0x0000FFFFFFFFFFFF);
		} break;
		case 7: {
			ret = BSWAP64(p_file->get_64() & 0x00FFFFFFFFFFFFFF);
		} break;
		case 8: {
			ret = BSWAP64(p_file->get_64());
		} break;
		default: {
			ret = 0;
		}
	}
	p_file->seek(pos + p_size);

	return ret;
}

Ref<PListNode> PList::read_bplist_obj(Ref<FileAccess> p_file, uint64_t p_offset_idx) {
	Ref<PListNode> node;
	node.instantiate();

	uint64_t ot_off = trailer.offset_table_start + p_offset_idx * trailer.offset_size;
	p_file->seek(ot_off);
	uint64_t marker_off = read_bplist_var_size_int(p_file, trailer.offset_size);
	ERR_FAIL_COND_V_MSG(marker_off == 0, Ref<PListNode>(), "Invalid marker size.");

	p_file->seek(marker_off);
	uint8_t marker = p_file->get_8();
	uint8_t marker_type = marker & 0xF0;
	uint64_t marker_size = marker & 0x0F;

	switch (marker_type) {
		case 0x00: {
			if (marker_size == 0x00) {
				node->data_type = PL_NODE_TYPE_NIL;
			} else if (marker_size == 0x08) {
				node->data_type = PL_NODE_TYPE_BOOLEAN;
				node->data_bool = false;
			} else if (marker_size == 0x09) {
				node->data_type = PL_NODE_TYPE_BOOLEAN;
				node->data_bool = true;
			} else {
				ERR_FAIL_V_MSG(Ref<PListNode>(), "Invalid nil/bool marker value.");
			}
		} break;
		case 0x10: {
			node->data_type = PL_NODE_TYPE_INTEGER;
			node->data_int = static_cast<int64_t>(read_bplist_var_size_int(p_file, pow(2, marker_size)));
		} break;
		case 0x20: {
			node->data_type = PL_NODE_TYPE_REAL;
			node->data_int = static_cast<int64_t>(read_bplist_var_size_int(p_file, pow(2, marker_size)));
		} break;
		case 0x30: {
			node->data_type = PL_NODE_TYPE_DATE;
			node->data_int = BSWAP64(p_file->get_64());
			node->data_string = Time::get_singleton()->get_datetime_string_from_unix_time(node->data_real + 978307200.0).utf8();
		} break;
		case 0x40: {
			if (marker_size == 0x0F) {
				uint8_t ext = p_file->get_8() & 0xF;
				marker_size = read_bplist_var_size_int(p_file, pow(2, ext));
			}
			node->data_type = PL_NODE_TYPE_DATA;
			PackedByteArray buf;
			buf.resize(marker_size + 1);
			p_file->get_buffer(reinterpret_cast<uint8_t *>(buf.ptrw()), marker_size);
			node->data_string = CryptoCore::b64_encode_str(buf.ptr(), buf.size()).utf8();
		} break;
		case 0x50: {
			if (marker_size == 0x0F) {
				uint8_t ext = p_file->get_8() & 0xF;
				marker_size = read_bplist_var_size_int(p_file, pow(2, ext));
			}
			node->data_type = PL_NODE_TYPE_STRING;
			node->data_string.resize(marker_size + 1);
			p_file->get_buffer(reinterpret_cast<uint8_t *>(node->data_string.ptrw()), marker_size);
		} break;
		case 0x60: {
			if (marker_size == 0x0F) {
				uint8_t ext = p_file->get_8() & 0xF;
				marker_size = read_bplist_var_size_int(p_file, pow(2, ext));
			}
			Char16String cs16;
			cs16.resize(marker_size + 1);
			for (uint64_t i = 0; i < marker_size; i++) {
				cs16[i] = BSWAP16(p_file->get_16());
			}
			node->data_type = PL_NODE_TYPE_STRING;
			node->data_string = String::utf16(cs16.ptr(), cs16.length()).utf8();
		} break;
		case 0x80: {
			node->data_type = PL_NODE_TYPE_INTEGER;
			node->data_int = static_cast<int64_t>(read_bplist_var_size_int(p_file, marker_size + 1));
		} break;
		case 0xA0:
		case 0xC0: {
			if (marker_size == 0x0F) {
				uint8_t ext = p_file->get_8() & 0xF;
				marker_size = read_bplist_var_size_int(p_file, pow(2, ext));
			}
			uint64_t pos = p_file->get_position();

			node->data_type = PL_NODE_TYPE_ARRAY;
			for (uint64_t i = 0; i < marker_size; i++) {
				p_file->seek(pos + trailer.ref_size * i);
				uint64_t ref = read_bplist_var_size_int(p_file, trailer.ref_size);

				Ref<PListNode> element = read_bplist_obj(p_file, ref);
				ERR_FAIL_COND_V(element.is_null(), Ref<PListNode>());
				node->data_array.push_back(element);
			}
		} break;
		case 0xD0: {
			if (marker_size == 0x0F) {
				uint8_t ext = p_file->get_8() & 0xF;
				marker_size = read_bplist_var_size_int(p_file, pow(2, ext));
			}
			uint64_t pos = p_file->get_position();

			node->data_type = PL_NODE_TYPE_DICT;
			for (uint64_t i = 0; i < marker_size; i++) {
				p_file->seek(pos + trailer.ref_size * i);
				uint64_t key_ref = read_bplist_var_size_int(p_file, trailer.ref_size);

				p_file->seek(pos + trailer.ref_size * (i + marker_size));
				uint64_t obj_ref = read_bplist_var_size_int(p_file, trailer.ref_size);

				Ref<PListNode> element_key = read_bplist_obj(p_file, key_ref);
				ERR_FAIL_COND_V(element_key.is_null() || element_key->data_type != PL_NODE_TYPE_STRING, Ref<PListNode>());
				Ref<PListNode> element = read_bplist_obj(p_file, obj_ref);
				ERR_FAIL_COND_V(element.is_null(), Ref<PListNode>());
				node->data_dict[String::utf8(element_key->data_string.ptr(), element_key->data_string.length())] = element;
			}
		} break;
		default: {
			ERR_FAIL_V_MSG(Ref<PListNode>(), "Invalid marker type.");
		}
	}
	return node;
}

bool PList::load_file(const String &p_filename) {
	root = Ref<PListNode>();

	Ref<FileAccess> fb = FileAccess::open(p_filename, FileAccess::READ);
	if (fb.is_null()) {
		return false;
	}

	unsigned char magic[8];
	fb->get_buffer(magic, 8);

	if (String((const char *)magic, 8) == "bplist00") {
		fb->seek_end(-26);
		trailer.offset_size = fb->get_8();
		trailer.ref_size = fb->get_8();
		trailer.object_num = BSWAP64(fb->get_64());
		trailer.root_offset_idx = BSWAP64(fb->get_64());
		trailer.offset_table_start = BSWAP64(fb->get_64());
		root = read_bplist_obj(fb, trailer.root_offset_idx);

		return root.is_valid();
	} else {
		// Load text plist.
		Error err;
		Vector<uint8_t> array = FileAccess::get_file_as_bytes(p_filename, &err);
		ERR_FAIL_COND_V(err != OK, false);

		String ret;
		ret.parse_utf8((const char *)array.ptr(), array.size());
		String err_str;
		bool ok = load_string(ret, err_str);
		ERR_FAIL_COND_V_MSG(!ok, false, "PList: " + err_str);

		return true;
	}
}

bool PList::load_string(const String &p_string, String &r_err_out) {
	root = Ref<PListNode>();

	int pos = 0;
	bool in_plist = false;
	bool done_plist = false;
	List<Ref<PListNode>> stack;
	String key;
	while (pos >= 0) {
		int open_token_s = p_string.find_char('<', pos);
		if (open_token_s == -1) {
			r_err_out = "Unexpected end of data. No tags found.";
			return false;
		}
		int open_token_e = p_string.find_char('>', open_token_s);
		pos = open_token_e;

		String token = p_string.substr(open_token_s + 1, open_token_e - open_token_s - 1);
		if (token.is_empty()) {
			r_err_out = "Invalid token name.";
			return false;
		}
		String value;
		if (token[0] == '?' || token[0] == '!') { // Skip <?xml ... ?> and <!DOCTYPE ... >
			int end_token_e = p_string.find_char('>', open_token_s);
			pos = end_token_e;
			continue;
		}

		if (token.find("plist", 0) == 0) {
			in_plist = true;
			continue;
		}

		if (token == "/plist") {
			done_plist = true;
			break;
		}

		if (!in_plist) {
			r_err_out = "Node outside of <plist> tag.";
			return false;
		}

		if (token == "dict") {
			if (!stack.is_empty()) {
				// Add subnode end enter it.
				Ref<PListNode> dict = PListNode::new_dict();
				dict->data_type = PList::PLNodeType::PL_NODE_TYPE_DICT;
				if (!stack.back()->get()->push_subnode(dict, key)) {
					r_err_out = "Can't push subnode, invalid parent type.";
					return false;
				}
				stack.push_back(dict);
			} else {
				// Add root node.
				if (!root.is_null()) {
					r_err_out = "Root node already set.";
					return false;
				}
				Ref<PListNode> dict = PListNode::new_dict();
				stack.push_back(dict);
				root = dict;
			}
			continue;
		}

		if (token == "/dict") {
			// Exit current dict.
			if (stack.is_empty() || stack.back()->get()->data_type != PList::PLNodeType::PL_NODE_TYPE_DICT) {
				r_err_out = "Mismatched </dict> tag.";
				return false;
			}
			stack.pop_back();
			continue;
		}

		if (token == "array") {
			if (!stack.is_empty()) {
				// Add subnode end enter it.
				Ref<PListNode> arr = PListNode::new_array();
				if (!stack.back()->get()->push_subnode(arr, key)) {
					r_err_out = "Can't push subnode, invalid parent type.";
					return false;
				}
				stack.push_back(arr);
			} else {
				// Add root node.
				if (!root.is_null()) {
					r_err_out = "Root node already set.";
					return false;
				}
				Ref<PListNode> arr = PListNode::new_array();
				stack.push_back(arr);
				root = arr;
			}
			continue;
		}

		if (token == "/array") {
			// Exit current array.
			if (stack.is_empty() || stack.back()->get()->data_type != PList::PLNodeType::PL_NODE_TYPE_ARRAY) {
				r_err_out = "Mismatched </array> tag.";
				return false;
			}
			stack.pop_back();
			continue;
		}

		if (token[token.length() - 1] == '/') {
			token = token.substr(0, token.length() - 1);
		} else {
			int end_token_s = p_string.find("</", pos);
			if (end_token_s == -1) {
				r_err_out = vformat("Mismatched <%s> tag.", token);
				return false;
			}
			int end_token_e = p_string.find_char('>', end_token_s);
			pos = end_token_e;
			String end_token = p_string.substr(end_token_s + 2, end_token_e - end_token_s - 2);
			if (end_token != token) {
				r_err_out = vformat("Mismatched <%s> and <%s> token pair.", token, end_token);
				return false;
			}
			value = p_string.substr(open_token_e + 1, end_token_s - open_token_e - 1);
		}
		if (token == "key") {
			key = value;
		} else {
			Ref<PListNode> var = nullptr;
			if (token == "true") {
				var = PListNode::new_bool(true);
			} else if (token == "false") {
				var = PListNode::new_bool(false);
			} else if (token == "integer") {
				var = PListNode::new_int(value.to_int());
			} else if (token == "real") {
				var = PListNode::new_real(value.to_float());
			} else if (token == "string") {
				var = PListNode::new_string(value);
			} else if (token == "data") {
				var = PListNode::new_data(value);
			} else if (token == "date") {
				var = PListNode::new_date(value);
			} else {
				r_err_out = vformat("Invalid value type: %s.", token);
				return false;
			}
			if (stack.is_empty() || !stack.back()->get()->push_subnode(var, key)) {
				r_err_out = "Can't push subnode, invalid parent type.";
				return false;
			}
		}
	}
	if (!stack.is_empty() || !done_plist) {
		r_err_out = "Unexpected end of data. Root node is not closed.";
		return false;
	}
	return true;
}

PackedByteArray PList::save_asn1() const {
	if (root.is_null()) {
		ERR_FAIL_V_MSG(PackedByteArray(), "PList: Invalid PList, no root node.");
	}
	size_t size = root->get_asn1_size(1);
	uint8_t len_octets = 0;
	if (size < 0x80) {
		len_octets = 1;
	} else {
		size = root->get_asn1_size(2);
		if (size < 0xFFFF) {
			len_octets = 2;
		} else {
			size = root->get_asn1_size(3);
			if (size < 0xFFFFFF) {
				len_octets = 3;
			} else {
				size = root->get_asn1_size(4);
				if (size < 0xFFFFFFFF) {
					len_octets = 4;
				} else {
					ERR_FAIL_V_MSG(PackedByteArray(), "PList: Data is too big for ASN.1 serializer, should be < 4 GiB.");
				}
			}
		}
	}

	PackedByteArray ret;
	if (!root->store_asn1(ret, len_octets)) {
		ERR_FAIL_V_MSG(PackedByteArray(), "PList: ASN.1 serializer error.");
	}
	return ret;
}

String PList::save_text() const {
	if (root.is_null()) {
		ERR_FAIL_V_MSG(String(), "PList: Invalid PList, no root node.");
	}

	String ret;
	ret += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	ret += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
	ret += "<plist version=\"1.0\">\n";

	root->store_text(ret, 0);

	ret += "</plist>\n\n";
	return ret;
}

Ref<PListNode> PList::get_root() {
	return root;
}
