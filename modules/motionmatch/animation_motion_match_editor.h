#ifndef ANIMATION_MOTION_MATCH_EDITOR_H
#define ANIMATION_MOTION_MATCH_EDITOR_H

#ifdef TOOLS_ENABLED

#include "frame_model.h"

#include "core/reference.h"
#include "editor/plugins/animation_tree_editor_plugin.h"
#include "modules/motionmatch/animation_node_motion_match.h"

class AnimationNodeMotionMatchEditor : public AnimationTreeNodeEditorPlugin {
	GDCLASS(AnimationNodeMotionMatchEditor, AnimationTreeNodeEditorPlugin)

	Ref<AnimationNodeMotionMatch> motion_match;

	AcceptDialog *match_tracks_dialog;
	Tree *match_tracks;

	Button *edit_match_tracks;
	Button *update_tracks;
	Button *clear_tree;

	SpinBox *snap_x;

	Skeleton3D *skeleton;

	bool updating;
	Vector<frame_model *> *keys = new Vector<frame_model *>();

	void _match_tracks_edited();

	void _edit_match_tracks();
	void _update_match_tracks();
	void _update_tracks();
	void _clear_tree();
	int max_key_track;
	void _update_vel();

protected:
	static void _bind_methods();

public:
	bool can_edit(const Ref<AnimationNode> &p_node) override;
	virtual void edit(const Ref<AnimationNode> &p_node) override;

	int fill_tracks(AnimationPlayer *player, Animation *anim, NodePath &root);

	AnimationNodeMotionMatchEditor();
};
#endif // ANIMATION_MOTION_MATCH_EDITOR_H
#endif
