@tool

@export_storage @no_storage var int_number = 0
@export_custom(PROPERTY_HINT_NONE, "") @no_storage var float_number = 0.0
@export_tool_button("") @no_storage var callable = test
@no_storage var string = ""
@no_storage @export var string_name = &""
@export @no_storage @no_storage var bool_var = false

func test():
    pass
