#ifndef FRAME_MODEL_H
#define FRAME_MODEL_H

#include "scene/main/node.h"

struct frame_model {
	Vector<Vector<float>> *bone_data = new Vector<Vector<float>>();
	Vector<float> traj;
	float time = 0.0f;
	int anim_num = 0;
};

#endif
