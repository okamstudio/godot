/**************************************************************************/
/*  tts_macos.mm                                                          */
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

#import "tts_macos.h"

@implementation TTS_MacOS

- (id)init {
	self = [super init];
	self->speaking = false;
	self->have_utterance = false;
	self->last_utterance = -1;
	self->paused = false;
	if (@available(macOS 10.14, *)) {
		self->synth = [[AVSpeechSynthesizer alloc] init];
		[self->synth setDelegate:self];
		print_verbose("Text-to-Speech: AVSpeechSynthesizer initialized.");
	} else {
		self->synth = [[NSSpeechSynthesizer alloc] init];
		[self->synth setDelegate:self];
		print_verbose("Text-to-Speech: NSSpeechSynthesizer initialized.");
	}
	return self;
}

// AVSpeechSynthesizer callback (macOS 10.14+)

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth willSpeakRangeOfSpeechString:(NSRange)characterRange utterance:(AVSpeechUtterance *)utterance API_AVAILABLE(macosx(10.14)) {
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

// AVSpeechSynthesizer callback (macOS 10.14+)

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth didCancelSpeechUtterance:(AVSpeechUtterance *)utterance API_AVAILABLE(macosx(10.14)) {
	DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, ids[utterance]);
	ids.erase(utterance);
	speaking = false;
	[self update];
}

// AVSpeechSynthesizer callback (macOS 10.14+)

- (void)speechSynthesizer:(AVSpeechSynthesizer *)av_synth didFinishSpeechUtterance:(AVSpeechUtterance *)utterance API_AVAILABLE(macosx(10.14)) {
	DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_ENDED, ids[utterance]);
	ids.erase(utterance);
	speaking = false;
	[self update];
}

// NSSpeechSynthesizer callback (macOS 10.4+)

- (void)speechSynthesizer:(NSSpeechSynthesizer *)ns_synth willSpeakWord:(NSRange)characterRange ofString:(NSString *)string {
	if (!paused && have_utterance) {
		// Convert from UTF-16 to UTF-32 position.
		int pos = 0;
		for (NSUInteger i = 0; i < MIN(characterRange.location, string.length); i++) {
			unichar c = [string characterAtIndex:i];
			if ((c & 0xfffffc00) == 0xd800) {
				i++;
			}
			pos++;
		}

		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_BOUNDARY, last_utterance, pos);
	}
}

- (void)speechSynthesizer:(NSSpeechSynthesizer *)ns_synth didFinishSpeaking:(BOOL)success {
	if (!paused && have_utterance) {
		if (success) {
			DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_ENDED, last_utterance);
		} else {
			DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, last_utterance);
		}
		have_utterance = false;
	}
	speaking = false;
	[self update];
}

- (void)update {
	if (!speaking && queue.size() > 0) {
		DisplayServer::TTSUtterance &message = queue.front()->get();

		if (@available(macOS 10.14, *)) {
			AVSpeechSynthesizer *av_synth = synth;
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
		} else {
			NSSpeechSynthesizer *ns_synth = synth;
			[ns_synth setObject:nil forProperty:NSSpeechResetProperty error:nil];
			[ns_synth setVoice:[NSString stringWithUTF8String:message.voice.utf8().get_data()]];
			int base_pitch = [[ns_synth objectForProperty:NSSpeechPitchBaseProperty error:nil] intValue];
			[ns_synth setObject:[NSNumber numberWithInt:(base_pitch * (message.pitch / 2.f + 0.5f))] forProperty:NSSpeechPitchBaseProperty error:nullptr];
			[ns_synth setVolume:(Math::remap(message.volume, 0.f, 100.f, 0.f, 1.f))];
			[ns_synth setRate:(message.rate * 200)];

			last_utterance = message.id;
			have_utterance = true;
			[ns_synth startSpeakingString:[NSString stringWithUTF8String:message.text.utf8().get_data()]];
		}
		queue.pop_front();

		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_STARTED, message.id);
		speaking = true;
	}
}

- (void)pauseSpeaking {
	if (@available(macOS 10.14, *)) {
		AVSpeechSynthesizer *av_synth = synth;
		[av_synth pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
	} else {
		NSSpeechSynthesizer *ns_synth = synth;
		[ns_synth pauseSpeakingAtBoundary:NSSpeechImmediateBoundary];
	}
	paused = true;
}

- (void)resumeSpeaking {
	if (@available(macOS 10.14, *)) {
		AVSpeechSynthesizer *av_synth = synth;
		[av_synth continueSpeaking];
	} else {
		NSSpeechSynthesizer *ns_synth = synth;
		[ns_synth continueSpeaking];
	}
	paused = false;
}

- (void)stopSpeaking {
	for (DisplayServer::TTSUtterance &message : queue) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, message.id);
	}
	queue.clear();
	if (@available(macOS 10.14, *)) {
		AVSpeechSynthesizer *av_synth = synth;
		[av_synth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
	} else {
		NSSpeechSynthesizer *ns_synth = synth;
		if (have_utterance) {
			DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, last_utterance);
		}
		[ns_synth stopSpeaking];
	}
	have_utterance = false;
	speaking = false;
	paused = false;
}

- (bool)isSpeaking {
	return speaking || (queue.size() > 0);
}

- (bool)isPaused {
	if (@available(macOS 10.14, *)) {
		AVSpeechSynthesizer *av_synth = synth;
		return [av_synth isPaused];
	} else {
		return paused;
	}
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
	if (@available(macOS 10.14, *)) {
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
	} else {
		for (NSString *voiceIdentifierString in [NSSpeechSynthesizer availableVoices]) {
			NSString *voiceLocaleIdentifier = [[NSSpeechSynthesizer attributesForVoice:voiceIdentifierString] objectForKey:NSVoiceLocaleIdentifier];
			NSString *voiceName = [[NSSpeechSynthesizer attributesForVoice:voiceIdentifierString] objectForKey:NSVoiceName];
			Dictionary voice_d;
			voice_d["name"] = String([voiceName UTF8String]);
			voice_d["id"] = String([voiceIdentifierString UTF8String]);
			voice_d["language"] = String([voiceLocaleIdentifier UTF8String]);
			list.push_back(voice_d);
		}
	}
	return list;
}

@end
