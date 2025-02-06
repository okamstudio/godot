@export_dir var test_dir: Array[String]
@export_dir var test_dir_packed: PackedStringArray
@export_file var test_file: Array[String]
@export_file var test_file_packed: PackedStringArray
@export_global_dir var test_global_dir: Array[String]
@export_global_dir var test_global_dir_packed: PackedStringArray
@export_global_file var test_global_file: Array[String]
@export_global_file var test_global_file_packed: PackedStringArray
@export_flags("A", "B", "C") var test_bit_flag: Array[int]
@export_flags("A", "B", "C") var test_bit_flag_packed_byte: PackedByteArray
@export_flags("A", "B", "C") var test_bit_flag_packed32: PackedInt32Array
@export_flags("A", "B", "C") var test_bit_flag_packed64: PackedInt64Array
@export_flags_2d_navigation var test_bit_flag_2d_nav: Array[int]
@export_flags_2d_navigation var test_bit_flag_2d_nav_packed_byte: PackedByteArray
@export_flags_2d_navigation var test_bit_flag_2d_nav_packed32: PackedInt32Array
@export_flags_2d_navigation var test_bit_flag_2d_nav_packed64: PackedInt64Array
@export_flags_2d_physics var test_bit_flag_2d_phys: Array[int]
@export_flags_2d_physics var test_bit_flag_2d_phys_packed_byte: PackedByteArray
@export_flags_2d_physics var test_bit_flag_2d_phys_packed32: PackedInt32Array
@export_flags_2d_physics var test_bit_flag_2d_phys_packed64: PackedInt64Array
@export_flags_2d_render var test_bit_flag_2d_render: Array[int]
@export_flags_2d_render var test_bit_flag_2d_render_packed_byte: PackedByteArray
@export_flags_2d_render var test_bit_flag_2d_render_packed32: PackedInt32Array
@export_flags_2d_render var test_bit_flag_2d_render_packed64: PackedInt64Array
@export_flags_3d_navigation var test_bit_flag_3d_nav: Array[int]
@export_flags_3d_navigation var test_bit_flag_3d_nav_packed_byte: PackedByteArray
@export_flags_3d_navigation var test_bit_flag_3d_nav_packed32: PackedInt32Array
@export_flags_3d_navigation var test_bit_flag_3d_nav_packed64: PackedInt64Array
@export_flags_3d_physics var test_bit_flag_3d_phys: Array[int]
@export_flags_3d_physics var test_bit_flag_3d_phys_packed_byte: PackedByteArray
@export_flags_3d_physics var test_bit_flag_3d_phys_packed32: PackedInt32Array
@export_flags_3d_physics var test_bit_flag_3d_phys_packed64: PackedInt64Array
@export_flags_3d_render var test_bit_flag_3d_render: Array[int]
@export_flags_3d_render var test_bit_flag_3d_render_packed_byte: PackedByteArray
@export_flags_3d_render var test_bit_flag_3d_render_packed32: PackedInt32Array
@export_flags_3d_render var test_bit_flag_3d_render_packed64: PackedInt64Array
@export_multiline var test_multiline: Array[String]
@export_multiline var test_multiline_packed: PackedStringArray
@export_placeholder("Placeholder") var test_placeholder: Array[String]
@export_placeholder("Placeholder") var test_placeholder_packed: PackedStringArray
@export_range(1, 10) var test_range_int: Array[int]
@export_range(1, 10) var test_range_int_packed_byte: PackedByteArray
@export_range(1, 10) var test_range_int_packed32: PackedInt32Array
@export_range(1, 10) var test_range_int_packed64: PackedInt64Array
@export_range(1, 10, 0.01) var test_range_int_float_step: Array[int]
@export_range(1.0, 10.0) var test_range_float: Array[float]
@export_range(1.0, 10.0) var test_range_float_packed32: PackedFloat32Array
@export_range(1.0, 10.0) var test_range_float_packed64: PackedFloat64Array
@export_exp_easing var test_exp_easing: Array[float]
@export_exp_easing var test_exp_easing_packed32: PackedFloat32Array
@export_exp_easing var test_exp_easing_packed64: PackedFloat64Array
@export_node_path var test_node_path: Array[NodePath]
@export_color_no_alpha var test_color: Array[Color]
@export_color_no_alpha var test_color_packed: PackedColorArray

var temp_packed_byte_array: PackedByteArray
var temp_packed_int32_array: PackedInt32Array
var temp_packed_int64_array: PackedInt64Array
var temp_packed_float32_array: PackedFloat32Array
var temp_packed_float64_array: PackedFloat64Array
var temp_packed_color_array: PackedColorArray
var temp_packed_vector2_array: PackedVector2Array
var temp_packed_vector3_array: PackedVector3Array
var temp_packed_vector4_array: PackedVector4Array

@export var test_weak_packed_byte_array = temp_packed_byte_array
@export var test_weak_packed_int32_array = temp_packed_int32_array
@export var test_weak_packed_int64_array = temp_packed_int64_array
@export var test_weak_packed_float32_array = temp_packed_float32_array
@export var test_weak_packed_float64_array = temp_packed_float64_array
@export var test_weak_packed_color_array = temp_packed_color_array
@export var test_weak_packed_vector2_array = temp_packed_vector2_array
@export var test_weak_packed_vector3_array = temp_packed_vector3_array
@export var test_weak_packed_vector4_array = temp_packed_vector4_array

@export_range(1, 10) var test_range_weak_packed_byte_array = temp_packed_byte_array
@export_range(1, 10) var test_range_weak_packed_int32_array = temp_packed_int32_array
@export_range(1, 10) var test_range_weak_packed_int64_array = temp_packed_int64_array
@export_range(1, 10) var test_range_weak_packed_float32_array = temp_packed_float32_array
@export_range(1, 10) var test_range_weak_packed_float64_array = temp_packed_float64_array
@export_color_no_alpha var test_noalpha_weak_packed_color_array = temp_packed_color_array

func test():
	for property in get_property_list():
		if str(property.name).begins_with("test_"):
			print(Utils.get_property_extended_info(property))
