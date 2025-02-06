# meta-description: Basic import script template

@tool
class_name _CLASS_
extends _BASE_


# Called by the editor when a scene has this script set as the import script in the import tab.
func _post_import(scene: Node) -> Object:
	# Modify the contents of the scene upon import.
	return scene # Return the modified root node when you're done.
