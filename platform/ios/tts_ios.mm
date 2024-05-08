/**************************************************************************/
/*  tts_ios.mm                                                            */
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

#import "tts_ios.h"

@implementation TTS_IOS

- (id)init {
	self = [super init];
	self->speaking = false;
	self->av_synth = [[AVSpeechSynthesizer alloc] init];
	[self->av_synth setDelegate:self];
	print_verbose("Text-to-Speech: AVSpeechSynthesizer initialized.");
	return self;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth willSpeakRangeOfSpeechString:(NSRange)characterRange utterance:(AVSpeechUtterance *)utterance {
	NSString *string = [utterance speechString];

	// Convert from UTF-16 to UTF-32 position.
	int pos = 0;
	for (NSUInteger i = 0; i < MIN(characterRange.location, string.length); i++) {
		unichar c = [string characterAtIndex:i];
		if ((c & 0xfffffc00) == 0xd800) {
			i++;
		}
		pos++;
	}

	DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_BOUNDARY, ids[utterance], pos);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth didCancelSpeechUtterance:(AVSpeechUtterance *)utterance {
	DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, ids[utterance]);
	ids.erase(utterance);
	speaking = false;
	[self update];
}

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth didFinishSpeechUtterance:(AVSpeechUtterance *)utterance {
	DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_ENDED, ids[utterance]);
	ids.erase(utterance);
	speaking = false;
	[self update];
}

- (void)update {
	if (!speaking && queue.size() > 0) {
		DisplayServer::TTSUtterance &message = queue.get_front();

		AVSpeechUtterance *new_utterance = [[AVSpeechUtterance alloc] initWithString:[NSString stringWithUTF8String:message.text.utf8().get_data()]];
		[new_utterance setVoice:[AVSpeechSynthesisVoice voiceWithIdentifier:[NSString stringWithUTF8String:message.voice.utf8().get_data()]]];
		if (message.rate > 1.f) {
			[new_utterance setRate:Math::remap(message.rate, 1.f, 10.f, AVSpeechUtteranceDefaultSpeechRate, AVSpeechUtteranceMaximumSpeechRate)];
		} else if (message.rate < 1.f) {
			[new_utterance setRate:Math::remap(message.rate, 0.1f, 1.f, AVSpeechUtteranceMinimumSpeechRate, AVSpeechUtteranceDefaultSpeechRate)];
		}
		[new_utterance setPitchMultiplier:message.pitch];
		[new_utterance setVolume:(Math::remap(message.volume, 0.f, 100.f, 0.f, 1.f))];

		ids[new_utterance] = message.id;
		[av_synth speakUtterance:new_utterance];

		queue.pop_front();

		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_STARTED, message.id);
		speaking = true;
	}
}

- (void)pauseSpeaking {
	[av_synth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
}

- (void)resumeSpeaking {
	[av_synth continueSpeaking];
}

- (void)stopSpeaking {
	for (DisplayServer::TTSUtterance &message : queue) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, message.id);
	}
	queue.clear();
	[av_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
	speaking = false;
}

- (bool)isSpeaking {
	return speaking || (queue.size() > 0);
}

- (bool)isPaused {
	return [av_synth isPaused];
}

- (void)speak:(const String &)text voice:(const String &)voice volume:(int)volume pitch:(float)pitch rate:(float)rate utterance_id:(int)utterance_id interrupt:(bool)interrupt {
	if (interrupt) {
		[self stopSpeaking];
	}

	if (text.is_empty()) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, utterance_id);
		return;
	}

	DisplayServer::TTSUtterance message;
	message.text = text;
	message.voice = voice;
	message.volume = CLAMP(volume, 0, 100);
	message.pitch = CLAMP(pitch, 0.f, 2.f);
	message.rate = CLAMP(rate, 0.1f, 10.f);
	message.id = utterance_id;
	queue.push_back(message);

	if ([self isPaused]) {
		[self resumeSpeaking];
	} else {
		[self update];
	}
}

- (Array)getVoices {
	Array list;
	for (AVSpeechSynthesisVoice *voice in [AVSpeechSynthesisVoice speechVoices]) {
		NSString *voiceIdentifierString = [voice identifier];
		NSString *voiceLocaleIdentifier = [voice language];
		NSString *voiceName = [voice name];
		Dictionary voice_d;
		voice_d["name"] = String::utf8([voiceName UTF8String]);
		voice_d["id"] = String::utf8([voiceIdentifierString UTF8String]);
		voice_d["language"] = String::utf8([voiceLocaleIdentifier UTF8String]);
		list.push_back(voice_d);
	}
	return list;
}

@end
