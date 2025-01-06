/**************************************************************************/
/*  audio_driver_coreaudio.mm                                             */
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

#import "audio_driver_coreaudio.h"

#ifdef COREAUDIO_ENABLED

#include "core/config/project_settings.h"
#include "core/os/os.h"

#define kOutputBus 0
#define kInputBus 1

#ifdef MACOS_ENABLED
OSStatus AudioDriverCoreAudio::input_device_address_cb(AudioObjectID inObjectID,
		UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses,
		void *inClientData) {
	AudioDriverCoreAudio *driver = static_cast<AudioDriverCoreAudio *>(inClientData);

	// If our selected input device is the Default, call set_input_device to update the
	// kAudioOutputUnitProperty_CurrentDevice property
	if (driver->input_device_name == "Default") {
		driver->set_input_device("Default");
	}

	return noErr;
}

OSStatus AudioDriverCoreAudio::output_device_address_cb(AudioObjectID inObjectID,
		UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses,
		void *inClientData) {
	AudioDriverCoreAudio *driver = static_cast<AudioDriverCoreAudio *>(inClientData);

	// If our selected output device is the Default call set_output_device to update the
	// kAudioOutputUnitProperty_CurrentDevice property
	if (driver->output_device_name == "Default") {
		driver->set_output_device("Default");
	}

	return noErr;
}

// Switch to kAudioObjectPropertyElementMain everywhere to remove deprecated warnings.
#if (TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED < 120000) || (TARGET_OS_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED < 150000)
#define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
#endif
#endif

Error AudioDriverCoreAudio::init() {
	AudioComponentDescription desc;
	memset(&desc, 0, sizeof(desc));
	desc.componentType = kAudioUnitType_Output;
#ifdef MACOS_ENABLED
	desc.componentSubType = kAudioUnitSubType_HALOutput;
#else
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	ERR_FAIL_NULL_V(comp, FAILED);

	OSStatus result = AudioComponentInstanceNew(comp, &audio_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

#ifdef MACOS_ENABLED
	AudioObjectPropertyAddress prop;
	prop.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = kAudioObjectPropertyElementMain;

	result = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop, &output_device_address_cb, this);
	ERR_FAIL_COND_V(result != noErr, FAILED);
#endif

	AudioStreamBasicDescription strdesc;

	memset(&strdesc, 0, sizeof(strdesc));
	UInt32 size = sizeof(strdesc);
	result = AudioUnitGetProperty(audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kOutputBus, &strdesc, &size);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	switch (strdesc.mChannelsPerFrame) {
		case 2: // Stereo
		case 4: // Surround 3.1
		case 6: // Surround 5.1
		case 8: // Surround 7.1
			channels = strdesc.mChannelsPerFrame;
			break;

		default:
			// Unknown number of channels, default to stereo
			channels = 2;
			break;
	}

#ifdef MACOS_ENABLED
	AudioDeviceID device_id;
	UInt32 dev_id_size = sizeof(AudioDeviceID);

	AudioObjectPropertyAddress property_dev_id = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster };
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property_dev_id, 0, nullptr, &dev_id_size, &device_id);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	double hw_mix_rate;
	UInt32 hw_mix_rate_size = sizeof(hw_mix_rate);

	AudioObjectPropertyAddress property_sr = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
	result = AudioObjectGetPropertyData(device_id, &property_sr, 0, nullptr, &hw_mix_rate_size, &hw_mix_rate);
	ERR_FAIL_COND_V(result != noErr, FAILED);
#else
	double hw_mix_rate = [AVAudioSession sharedInstance].sampleRate;
#endif
	mix_rate = hw_mix_rate;

	memset(&strdesc, 0, sizeof(strdesc));
	strdesc.mFormatID = kAudioFormatLinearPCM;
	strdesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	strdesc.mChannelsPerFrame = channels;
	strdesc.mSampleRate = mix_rate;
	strdesc.mFramesPerPacket = 1;
	strdesc.mBitsPerChannel = 16;
	strdesc.mBytesPerFrame = strdesc.mBitsPerChannel * strdesc.mChannelsPerFrame / 8;
	strdesc.mBytesPerPacket = strdesc.mBytesPerFrame * strdesc.mFramesPerPacket;

	result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, kOutputBus, &strdesc, sizeof(strdesc));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	int latency = Engine::get_singleton()->get_audio_output_latency();
	// Sample rate is independent of channels (ref: https://stackoverflow.com/questions/11048825/audio-sample-frequency-rely-on-channels)
	buffer_frames = closest_power_of_2(latency * mix_rate / 1000);

#ifdef MACOS_ENABLED
	result = AudioUnitSetProperty(audio_unit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, kOutputBus, &buffer_frames, sizeof(UInt32));
	ERR_FAIL_COND_V(result != noErr, FAILED);
#endif

	unsigned int buffer_size = buffer_frames * channels;
	samples_in.resize(buffer_size);

	print_verbose("CoreAudio: detected " + itos(channels) + " channels");
	print_verbose("CoreAudio: output sampling rate: " + itos(mix_rate) + " Hz");
	print_verbose("CoreAudio: output audio buffer frames: " + itos(buffer_frames) + " calculated latency: " + itos(buffer_frames * 1000 / mix_rate) + "ms");

	AURenderCallbackStruct callback;
	memset(&callback, 0, sizeof(AURenderCallbackStruct));
	callback.inputProc = &AudioDriverCoreAudio::output_callback;
	callback.inputProcRefCon = this;
	result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputBus, &callback, sizeof(callback));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	result = AudioUnitInitialize(audio_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	if (GLOBAL_GET("audio/driver/enable_input")) {
		return init_input_device();
	}
	return OK;
}

OSStatus AudioDriverCoreAudio::output_callback(void *inRefCon,
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData) {
	AudioDriverCoreAudio *ad = static_cast<AudioDriverCoreAudio *>(inRefCon);

	if (!ad->active || !ad->try_lock()) {
		for (unsigned int i = 0; i < ioData->mNumberBuffers; i++) {
			AudioBuffer *abuf = &ioData->mBuffers[i];
			memset(abuf->mData, 0, abuf->mDataByteSize);
		}
		return 0;
	}

	ad->start_counting_ticks();

	for (unsigned int i = 0; i < ioData->mNumberBuffers; i++) {
		AudioBuffer *abuf = &ioData->mBuffers[i];
		unsigned int frames_left = inNumberFrames;
		int16_t *out = (int16_t *)abuf->mData;

		while (frames_left) {
			unsigned int frames = MIN(frames_left, ad->buffer_frames);
			ad->audio_server_process(frames, ad->samples_in.ptrw());

			for (unsigned int j = 0; j < frames * ad->channels; j++) {
				out[j] = ad->samples_in[j] >> 16;
			}

			frames_left -= frames;
			out += frames * ad->channels;
		}
	}

	ad->stop_counting_ticks();
	ad->unlock();

	return 0;
}

OSStatus AudioDriverCoreAudio::input_callback(void *inRefCon,
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber, UInt32 inNumberFrames,
		AudioBufferList *ioData) {
	AudioDriverCoreAudio *ad = static_cast<AudioDriverCoreAudio *>(inRefCon);
	if (!ad->active) {
		return 0;
	}

	ad->lock();
	ad->start_counting_ticks();

	AudioBufferList bufferList;
	bufferList.mNumberBuffers = 1;
	bufferList.mBuffers[0].mData = ad->input_buf.ptrw();
	bufferList.mBuffers[0].mNumberChannels = ad->capture_channels;
	bufferList.mBuffers[0].mDataByteSize = ad->input_buf.size() * sizeof(int16_t);

	OSStatus result = AudioUnitRender(ad->input_unit, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, &bufferList);
	if (result == noErr) {
		for (unsigned int i = 0; i < inNumberFrames * ad->capture_channels; i++) {
			int32_t sample = ad->input_buf[i] << 16;
			ad->input_buffer_write(sample);

			if (ad->capture_channels == 1) {
				// In case input device is single channel convert it to Stereo
				ad->input_buffer_write(sample);
			}
		}
	} else {
		ERR_PRINT("AudioUnitRender failed, code: " + itos(result));
	}

	ad->stop_counting_ticks();
	ad->unlock();

	return result;
}

void AudioDriverCoreAudio::start() {
	if (!active && audio_unit != nullptr) {
		OSStatus result = AudioOutputUnitStart(audio_unit);
		if (result != noErr) {
			ERR_PRINT("AudioOutputUnitStart failed, code: " + itos(result));
		} else {
			active = true;
		}
	}
}

void AudioDriverCoreAudio::stop() {
	if (active) {
		OSStatus result = AudioOutputUnitStop(audio_unit);
		if (result != noErr) {
			ERR_PRINT("AudioOutputUnitStop failed, code: " + itos(result));
		} else {
			active = false;
		}
	}
}

int AudioDriverCoreAudio::get_mix_rate() const {
	return mix_rate;
}

int AudioDriverCoreAudio::get_input_mix_rate() const {
	return capture_mix_rate;
}

AudioDriver::SpeakerMode AudioDriverCoreAudio::get_speaker_mode() const {
	return get_speaker_mode_by_total_channels(channels);
}

void AudioDriverCoreAudio::lock() {
	mutex.lock();
}

void AudioDriverCoreAudio::unlock() {
	mutex.unlock();
}

bool AudioDriverCoreAudio::try_lock() {
	return mutex.try_lock();
}

void AudioDriverCoreAudio::finish() {
	finish_input_device();

	if (audio_unit) {
		OSStatus result;

		lock();

		AURenderCallbackStruct callback;
		memset(&callback, 0, sizeof(AURenderCallbackStruct));
		result = AudioUnitSetProperty(audio_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputBus, &callback, sizeof(callback));
		if (result != noErr) {
			ERR_PRINT("AudioUnitSetProperty failed");
		}

		if (active) {
			result = AudioOutputUnitStop(audio_unit);
			if (result != noErr) {
				ERR_PRINT("AudioOutputUnitStop failed");
			}

			active = false;
		}

		result = AudioUnitUninitialize(audio_unit);
		if (result != noErr) {
			ERR_PRINT("AudioUnitUninitialize failed");
		}

#ifdef MACOS_ENABLED
		AudioObjectPropertyAddress prop;
		prop.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
		prop.mScope = kAudioObjectPropertyScopeGlobal;
		prop.mElement = kAudioObjectPropertyElementMain;

		result = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop, &output_device_address_cb, this);
		if (result != noErr) {
			ERR_PRINT("AudioObjectRemovePropertyListener failed");
		}
#endif

		result = AudioComponentInstanceDispose(audio_unit);
		if (result != noErr) {
			ERR_PRINT("AudioComponentInstanceDispose failed");
		}

		audio_unit = nullptr;
		unlock();
	}
}

Error AudioDriverCoreAudio::init_input_device() {
	AudioComponentDescription desc;
	memset(&desc, 0, sizeof(desc));
	desc.componentType = kAudioUnitType_Output;
#ifdef MACOS_ENABLED
	desc.componentSubType = kAudioUnitSubType_HALOutput;
#else
	desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	ERR_FAIL_NULL_V(comp, FAILED);

	OSStatus result = AudioComponentInstanceNew(comp, &input_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

#ifdef MACOS_ENABLED
	AudioObjectPropertyAddress prop;
	prop.mSelector = kAudioHardwarePropertyDefaultInputDevice;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = kAudioObjectPropertyElementMain;

	result = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &prop, &input_device_address_cb, this);
	ERR_FAIL_COND_V(result != noErr, FAILED);
#endif

	UInt32 flag = 1;
	result = AudioUnitSetProperty(input_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, kInputBus, &flag, sizeof(flag));
	ERR_FAIL_COND_V(result != noErr, FAILED);
	flag = 0;
	result = AudioUnitSetProperty(input_unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, kOutputBus, &flag, sizeof(flag));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	UInt32 size;
#ifdef MACOS_ENABLED
	AudioDeviceID device_id;
	size = sizeof(AudioDeviceID);
	AudioObjectPropertyAddress property = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, nullptr, &size, &device_id);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	result = AudioUnitSetProperty(input_unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device_id, sizeof(AudioDeviceID));
	ERR_FAIL_COND_V(result != noErr, FAILED);
#endif

	AudioStreamBasicDescription strdesc;
	memset(&strdesc, 0, sizeof(strdesc));
	size = sizeof(strdesc);
	result = AudioUnitGetProperty(input_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kInputBus, &strdesc, &size);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	switch (strdesc.mChannelsPerFrame) {
		case 1: // Mono
			capture_channels = 1;
			break;

		case 2: // Stereo
			capture_channels = 2;
			break;

		default:
			// Unknown number of channels, default to stereo
			capture_channels = 2;
			break;
	}

#ifdef MACOS_ENABLED
	double hw_mix_rate;
	UInt32 hw_mix_rate_size = sizeof(hw_mix_rate);

	AudioObjectPropertyAddress property_sr = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
	result = AudioObjectGetPropertyData(device_id, &property_sr, 0, nullptr, &hw_mix_rate_size, &hw_mix_rate);
	ERR_FAIL_COND_V(result != noErr, FAILED);
#else
	double hw_mix_rate = [AVAudioSession sharedInstance].sampleRate;
#endif
	capture_mix_rate = hw_mix_rate;

	memset(&strdesc, 0, sizeof(strdesc));
	strdesc.mFormatID = kAudioFormatLinearPCM;
	strdesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	strdesc.mChannelsPerFrame = capture_channels;
	strdesc.mSampleRate = capture_mix_rate;
	strdesc.mFramesPerPacket = 1;
	strdesc.mBitsPerChannel = 16;
	strdesc.mBytesPerFrame = strdesc.mBitsPerChannel * strdesc.mChannelsPerFrame / 8;
	strdesc.mBytesPerPacket = strdesc.mBytesPerFrame * strdesc.mFramesPerPacket;

	result = AudioUnitSetProperty(input_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kInputBus, &strdesc, sizeof(strdesc));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	int latency = Engine::get_singleton()->get_audio_output_latency();
	// Sample rate is independent of channels (ref: https://stackoverflow.com/questions/11048825/audio-sample-frequency-rely-on-channels)
	capture_buffer_frames = closest_power_of_2(latency * capture_mix_rate / 1000);

	unsigned int buffer_size = capture_buffer_frames * capture_channels;
	input_buf.resize(buffer_size);

	AURenderCallbackStruct callback;
	memset(&callback, 0, sizeof(AURenderCallbackStruct));
	callback.inputProc = &AudioDriverCoreAudio::input_callback;
	callback.inputProcRefCon = this;
	result = AudioUnitSetProperty(input_unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, kInputBus, &callback, sizeof(callback));
	ERR_FAIL_COND_V(result != noErr, FAILED);

	result = AudioUnitInitialize(input_unit);
	ERR_FAIL_COND_V(result != noErr, FAILED);

	print_verbose("CoreAudio: input sampling rate: " + itos(capture_mix_rate) + " Hz");
	print_verbose("CoreAudio: input audio buffer frames: " + itos(capture_buffer_frames) + " calculated latency: " + itos(capture_buffer_frames * 1000 / capture_mix_rate) + "ms");

	return OK;
}

void AudioDriverCoreAudio::finish_input_device() {
	if (input_unit) {
		lock();

		AURenderCallbackStruct callback;
		memset(&callback, 0, sizeof(AURenderCallbackStruct));
		OSStatus result = AudioUnitSetProperty(input_unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &callback, sizeof(callback));
		if (result != noErr) {
			ERR_PRINT("AudioUnitSetProperty failed");
		}

		result = AudioUnitUninitialize(input_unit);
		if (result != noErr) {
			ERR_PRINT("AudioUnitUninitialize failed");
		}

#ifdef MACOS_ENABLED
		AudioObjectPropertyAddress prop;
		prop.mSelector = kAudioHardwarePropertyDefaultInputDevice;
		prop.mScope = kAudioObjectPropertyScopeGlobal;
		prop.mElement = kAudioObjectPropertyElementMain;

		result = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &prop, &input_device_address_cb, this);
		if (result != noErr) {
			ERR_PRINT("AudioObjectRemovePropertyListener failed");
		}
#endif

		result = AudioComponentInstanceDispose(input_unit);
		if (result != noErr) {
			ERR_PRINT("AudioComponentInstanceDispose failed");
		}

		input_unit = nullptr;
		unlock();
	}
}

Error AudioDriverCoreAudio::input_start() {
	input_buffer_init(capture_buffer_frames);

	OSStatus result = AudioOutputUnitStart(input_unit);
	if (result != noErr) {
		ERR_PRINT("AudioOutputUnitStart failed, code: " + itos(result));
	}

	return OK;
}

Error AudioDriverCoreAudio::input_stop() {
	if (input_unit) {
		OSStatus result = AudioOutputUnitStop(input_unit);
		if (result != noErr) {
			ERR_PRINT("AudioOutputUnitStop failed, code: " + itos(result));
		}
	}

	return OK;
}

#ifdef MACOS_ENABLED

PackedStringArray AudioDriverCoreAudio::_get_device_list(bool input) {
	PackedStringArray list;

	list.push_back("Default");

	AudioObjectPropertyAddress prop;

	prop.mSelector = kAudioHardwarePropertyDevices;
	prop.mScope = kAudioObjectPropertyScopeGlobal;
	prop.mElement = kAudioObjectPropertyElementMain;

	UInt32 size = 0;
	AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &prop, 0, nullptr, &size);
	AudioDeviceID *audioDevices = (AudioDeviceID *)memalloc(size);
	ERR_FAIL_NULL_V_MSG(audioDevices, list, "Out of memory.");
	AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, 0, nullptr, &size, audioDevices);

	UInt32 deviceCount = size / sizeof(AudioDeviceID);
	for (UInt32 i = 0; i < deviceCount; i++) {
		prop.mScope = input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
		prop.mSelector = kAudioDevicePropertyStreamConfiguration;

		AudioObjectGetPropertyDataSize(audioDevices[i], &prop, 0, nullptr, &size);
		AudioBufferList *bufferList = (AudioBufferList *)memalloc(size);
		ERR_FAIL_NULL_V_MSG(bufferList, list, "Out of memory.");
		AudioObjectGetPropertyData(audioDevices[i], &prop, 0, nullptr, &size, bufferList);

		UInt32 channelCount = 0;
		for (UInt32 j = 0; j < bufferList->mNumberBuffers; j++) {
			channelCount += bufferList->mBuffers[j].mNumberChannels;
		}

		memfree(bufferList);

		if (channelCount >= 1) {
			CFStringRef cfname;

			size = sizeof(CFStringRef);
			prop.mSelector = kAudioObjectPropertyName;

			AudioObjectGetPropertyData(audioDevices[i], &prop, 0, nullptr, &size, &cfname);

			CFIndex length = CFStringGetLength(cfname);
			CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
			char *buffer = (char *)memalloc(maxSize);
			ERR_FAIL_NULL_V_MSG(buffer, list, "Out of memory.");
			if (CFStringGetCString(cfname, buffer, maxSize, kCFStringEncodingUTF8)) {
				// Append the ID to the name in case we have devices with duplicate name
				list.push_back(String::utf8(buffer) + " (" + itos(audioDevices[i]) + ")");
			}

			memfree(buffer);
		}
	}

	memfree(audioDevices);

	return list;
}

void AudioDriverCoreAudio::_set_device(const String &output_device, bool input) {
	AudioDeviceID device_id;
	bool found = false;
	if (output_device != "Default") {
		AudioObjectPropertyAddress prop;

		prop.mSelector = kAudioHardwarePropertyDevices;
		prop.mScope = kAudioObjectPropertyScopeGlobal;
		prop.mElement = kAudioObjectPropertyElementMain;

		UInt32 size = 0;
		AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &prop, 0, nullptr, &size);
		AudioDeviceID *audioDevices = (AudioDeviceID *)memalloc(size);
		ERR_FAIL_NULL_MSG(audioDevices, "Out of memory.");
		AudioObjectGetPropertyData(kAudioObjectSystemObject, &prop, 0, nullptr, &size, audioDevices);

		UInt32 deviceCount = size / sizeof(AudioDeviceID);
		for (UInt32 i = 0; i < deviceCount && !found; i++) {
			prop.mScope = input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
			prop.mSelector = kAudioDevicePropertyStreamConfiguration;

			AudioObjectGetPropertyDataSize(audioDevices[i], &prop, 0, nullptr, &size);
			AudioBufferList *bufferList = (AudioBufferList *)memalloc(size);
			ERR_FAIL_NULL_MSG(bufferList, "Out of memory.");
			AudioObjectGetPropertyData(audioDevices[i], &prop, 0, nullptr, &size, bufferList);

			UInt32 channelCount = 0;
			for (UInt32 j = 0; j < bufferList->mNumberBuffers; j++) {
				channelCount += bufferList->mBuffers[j].mNumberChannels;
			}

			memfree(bufferList);

			if (channelCount >= 1) {
				CFStringRef cfname;

				size = sizeof(CFStringRef);
				prop.mSelector = kAudioObjectPropertyName;

				AudioObjectGetPropertyData(audioDevices[i], &prop, 0, nullptr, &size, &cfname);

				CFIndex length = CFStringGetLength(cfname);
				CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
				char *buffer = (char *)memalloc(maxSize);
				ERR_FAIL_NULL_MSG(buffer, "Out of memory.");
				if (CFStringGetCString(cfname, buffer, maxSize, kCFStringEncodingUTF8)) {
					String name = String::utf8(buffer) + " (" + itos(audioDevices[i]) + ")";
					if (name == output_device) {
						device_id = audioDevices[i];
						found = true;
					}
				}

				memfree(buffer);
			}
		}

		memfree(audioDevices);
	}

	if (!found) {
		// If we haven't found the desired device get the system default one
		UInt32 size = sizeof(AudioDeviceID);
		UInt32 elem = input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice;
		AudioObjectPropertyAddress property = { elem, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

		OSStatus result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &property, 0, nullptr, &size, &device_id);
		ERR_FAIL_COND(result != noErr);

		found = true;
	}

	if (found) {
		OSStatus result = AudioUnitSetProperty(input ? input_unit : audio_unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device_id, sizeof(AudioDeviceID));
		ERR_FAIL_COND(result != noErr);

		if (input) {
			// Reset audio input to keep synchronization.
			input_position = 0;
			input_size = 0;
		}
	}
}

PackedStringArray AudioDriverCoreAudio::get_output_device_list() {
	return _get_device_list();
}

String AudioDriverCoreAudio::get_output_device() {
	return output_device_name;
}

void AudioDriverCoreAudio::set_output_device(const String &p_name) {
	output_device_name = p_name;
	if (active) {
		_set_device(output_device_name);
	}
}

PackedStringArray AudioDriverCoreAudio::get_input_device_list() {
	return _get_device_list(true);
}

String AudioDriverCoreAudio::get_input_device() {
	return input_device_name;
}

void AudioDriverCoreAudio::set_input_device(const String &p_name) {
	input_device_name = p_name;
	if (active) {
		_set_device(input_device_name, true);
	}
}

#endif

AudioDriverCoreAudio::AudioDriverCoreAudio() {
	samples_in.clear();
}

#endif // COREAUDIO_ENABLED
