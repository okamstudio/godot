@export_enum("Red", "Green", "Blue") var test_untyped
@export_enum("Red:10", "Green:20", "Blue:30") var test_with_values

var temp_array_int: Array[int]
var temp_array_string: Array[String]
var temp_packed_byte_array: PackedByteArray
var temp_packed_int32_array: PackedInt32Array
var temp_packed_int64_array: PackedInt64Array
var temp_packed_string_array: PackedStringArray

@export_enum("Red", "Green", "Blue") var test_weak_variant
@export_enum("Red", "Green", "Blue") var test_weak_int = 0
@export_enum("Red", "Green", "Blue") var test_weak_string = ""
@export_enum("Red", "Green", "Blue") var test_weak_array_int = temp_array_int
@export_enum("Red", "Green", "Blue") var test_weak_array_string = temp_array_string
@export_enum("Red", "Green", "Blue") var test_weak_packed_byte_array = temp_packed_byte_array
@export_enum("Red", "Green", "Blue") var test_weak_packed_int32_array = temp_packed_int32_array
@export_enum("Red", "Green", "Blue") var test_weak_packed_int64_array = temp_packed_int64_array
@export_enum("Red", "Green", "Blue") var test_weak_packed_string_array = temp_packed_string_array

@export_enum("Red", "Green", "Blue") var test_hard_variant: Variant
@export_enum("Red", "Green", "Blue") var test_hard_int: int
@export_enum("Red", "Green", "Blue") var test_hard_string: String
@export_enum("Red", "Green", "Blue") var test_hard_array_int: Array[int]
@export_enum("Red", "Green", "Blue") var test_hard_array_string: Array[String]

@export_enum("Red", "Green", "Blue") var test_variant_array_int: Variant = temp_array_int
@export_enum("Red", "Green", "Blue") var test_variant_packed_int32_array: Variant = temp_packed_int32_array
@export_enum("Red", "Green", "Blue") var test_variant_array_string: Variant = temp_array_string
@export_enum("Red", "Green", "Blue") var test_variant_packed_string_array: Variant = temp_packed_string_array

func test():
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_extended_info(property, self))
