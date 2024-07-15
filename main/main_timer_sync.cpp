/**************************************************************************/
/*  main_timer_sync.cpp                                                   */
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

#include "main_timer_sync.h"

#include "core/os/os.h"
#include "servers/display_server.h"

void MainFrameTime::clamp_process_step(double min_process_step, double max_process_step) {
	if (process_step < min_process_step) {
		process_step = min_process_step;
	} else if (process_step > max_process_step) {
		process_step = max_process_step;
	}
}

/////////////////////////////////

void MainTimerSync::DeltaSmoother::update_refresh_rate_estimator(int64_t p_delta) {
	// the calling code should prevent 0 or negative values of delta
	// (preventing divide by zero)

	// note that if the estimate gets locked, and something external changes this
	// (e.g. user changes to non-vsync in the OS), then the results may be less than ideal,
	// but usually it will detect this via the FPS measurement and not attempt smoothing.
	// This should be a rare occurrence anyway, and will be cured next time user restarts game.
	if (_estimate_locked) {
		return;
	}

	// First average the delta over NUM_READINGS
	_estimator_total_delta += p_delta;
	_estimator_delta_readings++;

	const int NUM_READINGS = 60;

	if (_estimator_delta_readings < NUM_READINGS) {
		return;
	}

	// use average
	p_delta = _estimator_total_delta / NUM_READINGS;

	// reset the averager for next time
	_estimator_delta_readings = 0;
	_estimator_total_delta = 0;

	///////////////////////////////

	int fps = Math::round(1000000.0 / p_delta);

	// initial estimation, to speed up converging, special case we will estimate the refresh rate
	// from the first average FPS reading
	if (_estimated_fps == 0) {
		// below 50 might be chugging loading stuff, or else
		// dropping loads of frames, so the estimate will be inaccurate
		if (fps >= 50) {
			_estimated_fps = fps;
#ifdef GODOT_DEBUG_DELTA_SMOOTHER
			print_line("initial guess (average measured) refresh rate: " + itos(fps));
#endif
		} else {
			// can't get started until above 50
			return;
		}
	}

	// we hit our exact estimated refresh rate.
	// increase our confidence in the estimate.
	if (fps == _estimated_fps) {
		// note that each hit is an average of NUM_READINGS frames
		_hits_at_estimated++;

		if (_estimate_complete && _hits_at_estimated == 20) {
			_estimate_locked = true;
#ifdef GODOT_DEBUG_DELTA_SMOOTHER
			print_line("estimate LOCKED at " + itos(_estimated_fps) + " fps");
#endif
			return;
		}

		// if we are getting pretty confident in this estimate, decide it is complete
		// (it can still be increased later, and possibly lowered but only for a short time)
		if ((!_estimate_complete) && (_hits_at_estimated > 2)) {
			// when the estimate is complete we turn on smoothing
			if (_estimated_fps) {
				_estimate_complete = true;
				_vsync_delta = 1000000 / _estimated_fps;

#ifdef GODOT_DEBUG_DELTA_SMOOTHER
				print_line("estimate complete. vsync_delta " + itos(_vsync_delta) + ", fps " + itos(_estimated_fps));
#endif
			}
		}

#ifdef GODOT_DEBUG_DELTA_SMOOTHER
		if ((_hits_at_estimated % (400 / NUM_READINGS)) == 0) {
			String sz = "hits at estimated : " + itos(_hits_at_estimated) + ", above : " + itos(_hits_above_estimated) + "( " + itos(_hits_one_above_estimated) + " ), below : " + itos(_hits_below_estimated) + " (" + itos(_hits_one_below_estimated) + " )";

			print_line(sz);
		}
#endif

		return;
	}

	const int SIGNIFICANCE_UP = 1;
	const int SIGNIFICANCE_DOWN = 2;

	// we are not usually interested in slowing the estimate
	// but we may have overshot, so make it possible to reduce
	if (fps < _estimated_fps) {
		// micro changes
		if (fps == (_estimated_fps - 1)) {
			_hits_one_below_estimated++;

			if ((_hits_one_below_estimated > _hits_at_estimated) && (_hits_one_below_estimated > SIGNIFICANCE_DOWN)) {
				_estimated_fps--;
				made_new_estimate();
			}

			return;
		} else {
			_hits_below_estimated++;

			// don't allow large lowering if we are established at a refresh rate, as it will probably be dropped frames
			bool established = _estimate_complete && (_hits_at_estimated > 10);

			// macro changes
			// note there is a large barrier to macro lowering. That is because it is more likely to be dropped frames
			// than mis-estimation of the refresh rate.
			if (!established) {
				if (((_hits_below_estimated / 8) > _hits_at_estimated) && (_hits_below_estimated > SIGNIFICANCE_DOWN)) {
					// decrease the estimate
					_estimated_fps--;
					made_new_estimate();
				}
			}

			return;
		}
	}

	// Changes increasing the estimate.
	// micro changes
	if (fps == (_estimated_fps + 1)) {
		_hits_one_above_estimated++;

		if ((_hits_one_above_estimated > _hits_at_estimated) && (_hits_one_above_estimated > SIGNIFICANCE_UP)) {
			_estimated_fps++;
			made_new_estimate();
		}
		return;
	} else {
		_hits_above_estimated++;

		// macro changes
		if ((_hits_above_estimated > _hits_at_estimated) && (_hits_above_estimated > SIGNIFICANCE_UP)) {
			// increase the estimate
			int change = fps - _estimated_fps;
			change /= 2;
			change = MAX(1, change);

			_estimated_fps += change;
			made_new_estimate();
		}
		return;
	}
}

bool MainTimerSync::DeltaSmoother::fps_allows_smoothing(int64_t p_delta) {
	_measurement_time += p_delta;
	_measurement_frame_count++;

	if (_measurement_frame_count == _measurement_end_frame) {
		// only switch on or off if the estimate is complete
		if (_estimate_complete) {
			int64_t time_passed = _measurement_time - _measurement_start_time;

			// average delta
			time_passed /= MEASURE_FPS_OVER_NUM_FRAMES;

			// estimate fps
			if (time_passed) {
				double fps = 1000000.0 / time_passed;
				double ratio = fps / (double)_estimated_fps;

				//print_line("ratio : " + String(Variant(ratio)));

				if ((ratio > 0.95) && (ratio < 1.05)) {
					_measurement_allows_smoothing = true;
				} else {
					_measurement_allows_smoothing = false;
				}
			}
		} // estimate complete

		// new start time for next iteration
		_measurement_start_time = _measurement_time;
		_measurement_end_frame += MEASURE_FPS_OVER_NUM_FRAMES;
	}

	return _measurement_allows_smoothing;
}

int64_t MainTimerSync::DeltaSmoother::smooth_delta(int64_t p_delta) {
	// Conditions to disable smoothing.
	// Note that vsync is a request, it cannot be relied on, the OS may override this.
	// If the OS turns vsync on without vsync in the app, smoothing will not be enabled.
	// If the OS turns vsync off with sync enabled in the app, the smoothing must detect this
	// via the error metric and switch off.
	// Also only try smoothing if vsync is enabled (classical vsync, not new types) ..
	// This condition is currently checked before calling smooth_delta().
	if (!OS::get_singleton()->is_delta_smoothing_enabled() || Engine::get_singleton()->is_editor_hint()) {
		return p_delta;
	}

	// only attempt smoothing if vsync is selected
	DisplayServer::VSyncMode vsync_mode = DisplayServer::get_singleton()->window_get_vsync_mode(DisplayServer::MAIN_WINDOW_ID);
	if (vsync_mode != DisplayServer::VSYNC_ENABLED) {
		return p_delta;
	}

	// Very important, ignore long deltas and pass them back unmodified.
	// This is to deal with resuming after suspend for long periods.
	if (p_delta > 1000000) {
		return p_delta;
	}

	// keep a running guesstimate of the FPS, and turn off smoothing if
	// conditions not close to the estimated FPS
	if (!fps_allows_smoothing(p_delta)) {
		return p_delta;
	}

	// we can't cope with negative deltas .. OS bug on some hardware
	// and also very small deltas caused by vsync being off.
	// This could possibly be part of a hiccup, this value isn't fixed in stone...
	if (p_delta < 1000) {
		return p_delta;
	}

	// note still some vsync off will still get through to this point...
	// and we need to cope with it by not converging the estimator / and / or not smoothing
	update_refresh_rate_estimator(p_delta);

	// no smoothing until we know what the refresh rate is
	if (!_estimate_complete) {
		return p_delta;
	}

	// accumulate the time we have available to use
	_leftover_time += p_delta;

	// how many vsyncs units can we fit?
	int64_t units = _leftover_time / _vsync_delta;

	// a delta must include minimum 1 vsync
	// (if it is less than that, it is either random error or we are no longer running at the vsync rate,
	// in which case we should switch off delta smoothing, or re-estimate the refresh rate)
	units = MAX(units, 1);

	_leftover_time -= units * _vsync_delta;
	// print_line("units " + itos(units) + ", leftover " + itos(_leftover_time/1000) + " ms");

	return units * _vsync_delta;
}

/////////////////////////////////////

// returns the fraction of p_physics_step required for the timer to overshoot
// before advance_core considers changing the physics_steps return from
// the typical values as defined by typical_physics_steps
double MainTimerSync::get_physics_jitter_fix() {
	return Engine::get_singleton()->get_physics_jitter_fix();
}

// gets our best bet for the average number of physics steps per render frame
// return value: number of frames back this data is consistent
int MainTimerSync::get_average_physics_steps(double &p_min, double &p_max) {
	p_min = typical_physics_steps[0];
	p_max = p_min + 1;

	for (int i = 1; i < CONTROL_STEPS; ++i) {
		const double typical_lower = typical_physics_steps[i];
		const double current_min = typical_lower / (i + 1);
		if (current_min > p_max) {
			return i; // bail out if further restrictions would void the interval
		} else if (current_min > p_min) {
			p_min = current_min;
		}
		const double current_max = (typical_lower + 1) / (i + 1);
		if (current_max < p_min) {
			return i;
		} else if (current_max < p_max) {
			p_max = current_max;
		}
	}

	return CONTROL_STEPS;
}

// advance physics clock by p_process_step, return appropriate number of steps to simulate
MainFrameTime MainTimerSync::advance_core(double p_physics_step, int p_physics_ticks_per_second, double p_process_step) {
	MainFrameTime ret;

	ret.process_step = p_process_step;

	// simple determination of number of physics iteration
	time_accum += ret.process_step;
	ret.physics_steps = Math::floor(time_accum * p_physics_ticks_per_second);

	int min_typical_steps = typical_physics_steps[0];
	int max_typical_steps = min_typical_steps + 1;

	// given the past recorded steps and typical steps to match, calculate bounds for this
	// step to be typical
	bool update_typical = false;

	for (int i = 0; i < CONTROL_STEPS - 1; ++i) {
		int steps_left_to_match_typical = typical_physics_steps[i + 1] - accumulated_physics_steps[i];
		if (steps_left_to_match_typical > max_typical_steps ||
				steps_left_to_match_typical + 1 < min_typical_steps) {
			update_typical = true;
			break;
		}

		if (steps_left_to_match_typical > min_typical_steps) {
			min_typical_steps = steps_left_to_match_typical;
		}
		if (steps_left_to_match_typical + 1 < max_typical_steps) {
			max_typical_steps = steps_left_to_match_typical + 1;
		}
	}

#ifdef DEBUG_ENABLED
	if (max_typical_steps < 0) {
		WARN_PRINT_ONCE("`max_typical_steps` is negative. This could hint at an engine bug or system timer misconfiguration.");
	}
#endif

	// try to keep it consistent with previous iterations
	if (ret.physics_steps < min_typical_steps) {
		const int max_possible_steps = Math::floor((time_accum)*p_physics_ticks_per_second + get_physics_jitter_fix());
		if (max_possible_steps < min_typical_steps) {
			ret.physics_steps = max_possible_steps;
			update_typical = true;
		} else {
			ret.physics_steps = min_typical_steps;
		}
	} else if (ret.physics_steps > max_typical_steps) {
		const int min_possible_steps = Math::floor((time_accum)*p_physics_ticks_per_second - get_physics_jitter_fix());
		if (min_possible_steps > max_typical_steps) {
			ret.physics_steps = min_possible_steps;
			update_typical = true;
		} else {
			ret.physics_steps = max_typical_steps;
		}
	}

	if (ret.physics_steps < 0) {
		ret.physics_steps = 0;
	}

	time_accum -= ret.physics_steps * p_physics_step;

	// keep track of accumulated step counts
	for (int i = CONTROL_STEPS - 2; i >= 0; --i) {
		accumulated_physics_steps[i + 1] = accumulated_physics_steps[i] + ret.physics_steps;
	}
	accumulated_physics_steps[0] = ret.physics_steps;

	if (update_typical) {
		for (int i = CONTROL_STEPS - 1; i >= 0; --i) {
			if (typical_physics_steps[i] > accumulated_physics_steps[i]) {
				typical_physics_steps[i] = accumulated_physics_steps[i];
			} else if (typical_physics_steps[i] < accumulated_physics_steps[i] - 1) {
				typical_physics_steps[i] = accumulated_physics_steps[i] - 1;
			}
		}
	}

	return ret;
}

// calls advance_core, keeps track of deficit it adds to animaption_step, make sure the deficit sum stays close to zero
MainFrameTime MainTimerSync::advance_checked(double p_physics_step, int p_physics_ticks_per_second, double p_process_step) {
	if (fixed_fps != -1) {
		p_process_step = 1.0 / fixed_fps;
	}

	float min_output_step = p_process_step / 8;
	min_output_step = MAX(min_output_step, 1E-6);

	// compensate for last deficit
	p_process_step += time_deficit;

	MainFrameTime ret = advance_core(p_physics_step, p_physics_ticks_per_second, p_process_step);

	// we will do some clamping on ret.process_step and need to sync those changes to time_accum,
	// that's easiest if we just remember their fixed difference now
	const double process_minus_accum = ret.process_step - time_accum;

	// first, least important clamping: keep ret.process_step consistent with typical_physics_steps.
	// this smoothes out the process steps and culls small but quick variations.
	{
		double min_average_physics_steps, max_average_physics_steps;
		int consistent_steps = get_average_physics_steps(min_average_physics_steps, max_average_physics_steps);
		if (consistent_steps > 3) {
			ret.clamp_process_step(min_average_physics_steps * p_physics_step, max_average_physics_steps * p_physics_step);
		}
	}

	// second clamping: keep abs(time_deficit) < jitter_fix * frame_slise
	double max_clock_deviation = get_physics_jitter_fix() * p_physics_step;
	ret.clamp_process_step(p_process_step - max_clock_deviation, p_process_step + max_clock_deviation);

	// last clamping: make sure time_accum is between 0 and p_physics_step for consistency between physics and process
	ret.clamp_process_step(process_minus_accum, process_minus_accum + p_physics_step);

	// all the operations above may have turned ret.p_process_step negative or zero, keep a minimal value
	if (ret.process_step < min_output_step) {
		ret.process_step = min_output_step;
	}

	// restore time_accum
	time_accum = ret.process_step - process_minus_accum;

	// forcing ret.process_step to be positive may trigger a violation of the
	// promise that time_accum is between 0 and p_physics_step
#ifdef DEBUG_ENABLED
	if (time_accum < -1E-7) {
		WARN_PRINT_ONCE("Intermediate value of `time_accum` is negative. This could hint at an engine bug or system timer misconfiguration.");
	}
#endif

	if (time_accum > p_physics_step) {
		const int extra_physics_steps = Math::floor(time_accum * p_physics_ticks_per_second);
		time_accum -= extra_physics_steps * p_physics_step;
		ret.physics_steps += extra_physics_steps;
	}

#ifdef DEBUG_ENABLED
	if (time_accum < -1E-7) {
		WARN_PRINT_ONCE("Final value of `time_accum` is negative. It should always be between 0 and `p_physics_step`. This hints at an engine bug.");
	}
	if (time_accum > p_physics_step + 1E-7) {
		WARN_PRINT_ONCE("Final value of `time_accum` is larger than `p_physics_step`. It should always be between 0 and `p_physics_step`. This hints at an engine bug.");
	}
#endif

	// track deficit
	time_deficit = p_process_step - ret.process_step;

	// p_physics_step is 1.0 / iterations_per_sec
	// i.e. the time in seconds taken by a physics tick
	ret.interpolation_fraction = time_accum / p_physics_step;

	return ret;
}

// determine wall clock step since last iteration
double MainTimerSync::get_cpu_process_step() {
	uint64_t cpu_ticks_elapsed = current_cpu_ticks_usec - last_cpu_ticks_usec;
	last_cpu_ticks_usec = current_cpu_ticks_usec;

	cpu_ticks_elapsed = _delta_smoother.smooth_delta(cpu_ticks_elapsed);

	return cpu_ticks_elapsed / 1000000.0;
}

MainTimerSync::MainTimerSync() {
	for (int i = CONTROL_STEPS - 1; i >= 0; --i) {
		typical_physics_steps[i] = i;
		accumulated_physics_steps[i] = i;
	}
}

// start the clock
void MainTimerSync::init(uint64_t p_cpu_ticks_usec) {
	current_cpu_ticks_usec = last_cpu_ticks_usec = p_cpu_ticks_usec;
}

// set measured wall clock time
void MainTimerSync::set_cpu_ticks_usec(uint64_t p_cpu_ticks_usec) {
	current_cpu_ticks_usec = p_cpu_ticks_usec;
}

void MainTimerSync::set_fixed_fps(int p_fixed_fps) {
	fixed_fps = p_fixed_fps;
}

// advance one physics frame, return timesteps to take
MainFrameTime MainTimerSync::advance(double p_physics_step, int p_physics_ticks_per_second) {
	double cpu_process_step = get_cpu_process_step();

	return advance_checked(p_physics_step, p_physics_ticks_per_second, cpu_process_step);
}
