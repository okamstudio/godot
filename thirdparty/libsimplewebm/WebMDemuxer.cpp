/*
	MIT License

	Copyright (c) 2016 Błażej Szczygieł

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "WebMDemuxer.hpp"

#include "mkvparser/mkvparser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

WebMFrame::WebMFrame() :
	bufferSize(0), bufferCapacity(0),
	buffer(NULL),
	time(0),
	key(false)
{}
WebMFrame::~WebMFrame()
{
	free(buffer);
}

/**/

WebMDemuxer::WebMDemuxer(mkvparser::IMkvReader *reader, int videoTrack, int audioTrack) :
	m_reader(reader),
	m_segment(NULL),
	m_cluster(NULL), m_block(NULL), m_blockEntry(NULL),
	m_blockFrameIndex(0),
	m_videoTrack(NULL), m_vCodec(NO_VIDEO),
	m_audioTrack(NULL), m_aCodec(NO_AUDIO),
	m_seekTime(0.0f),
	m_isSeek(false),
	m_isOpen(false),
	m_eos(false)
{
	long long pos = 0;
	if (mkvparser::EBMLHeader().Parse(m_reader, pos))
		return;

	if (mkvparser::Segment::CreateInstance(m_reader, pos, m_segment))
		return;

	if (m_segment->Load() < 0)
		return;

	const mkvparser::Tracks *tracks = m_segment->GetTracks();
	const unsigned long tracksCount = tracks->GetTracksCount();
	int currVideoTrack = -1, currAudioTrack = -1;
	for (unsigned long i = 0; i < tracksCount; ++i)
	{
		const mkvparser::Track *track = tracks->GetTrackByIndex(i);
		if (const char *codecId = track->GetCodecId())
		{
			if ((!m_videoTrack || currVideoTrack != videoTrack) && track->GetType() == mkvparser::Track::kVideo)
			{
				if (!strcmp(codecId, "V_VP8"))
					m_vCodec = VIDEO_VP8;
				else if (!strcmp(codecId, "V_VP9"))
					m_vCodec = VIDEO_VP9;
				if (m_vCodec != NO_VIDEO)
					m_videoTrack = static_cast<const mkvparser::VideoTrack *>(track);
				++currVideoTrack;
			}
			if ((!m_audioTrack || currAudioTrack != audioTrack) && track->GetType() == mkvparser::Track::kAudio)
			{
				if (!strcmp(codecId, "A_VORBIS"))
					m_aCodec = AUDIO_VORBIS;
				else if (!strcmp(codecId, "A_OPUS"))
					m_aCodec = AUDIO_OPUS;
				if (m_aCodec != NO_AUDIO)
					m_audioTrack = static_cast<const mkvparser::AudioTrack *>(track);
				++currAudioTrack;
			}
		}
	}
	if (!m_videoTrack && !m_audioTrack)
		return;

	m_isOpen = true;
}
WebMDemuxer::~WebMDemuxer()
{
	delete m_segment;
	delete m_reader;
}

double WebMDemuxer::getLength() const
{
	return m_segment->GetDuration() / 1e9;
}

WebMDemuxer::VIDEO_CODEC WebMDemuxer::getVideoCodec() const
{
	return m_vCodec;
}
int WebMDemuxer::getWidth() const
{
	return m_videoTrack->GetWidth();
}
int WebMDemuxer::getHeight() const
{
	return m_videoTrack->GetHeight();
}

WebMDemuxer::AUDIO_CODEC WebMDemuxer::getAudioCodec() const
{
	return m_aCodec;
}
const unsigned char *WebMDemuxer::getAudioExtradata(size_t &size) const
{
	return m_audioTrack->GetCodecPrivate(size);
}
double WebMDemuxer::getSampleRate() const
{
	return m_audioTrack->GetSamplingRate();
}
int WebMDemuxer::getChannels() const
{
	return m_audioTrack->GetChannels();
}
int WebMDemuxer::getAudioDepth() const
{
	return m_audioTrack->GetBitDepth();
}

float WebMDemuxer::seek(float p_time){
	
	if(p_time < 0)
		return 0.0f;

	m_seekTime = p_time;
	m_isSeek = true;

 	const mkvparser::BlockEntry *blockEntry = NULL;
	m_videoTrack->Seek(m_seekTime * 1e9, blockEntry);

	if(!blockEntry || !blockEntry->GetBlock() || !blockEntry->GetCluster())
		return 0.0f;

	return blockEntry->GetBlock()->GetTime(blockEntry->GetCluster()) / 1e9;
}

bool WebMDemuxer::readFrame(WebMFrame *videoFrame, WebMFrame *audioFrame)
{
	const long videoTrackNumber = (videoFrame && m_videoTrack) ? m_videoTrack->GetNumber() : 0;
	const long audioTrackNumber = (audioFrame && m_audioTrack) ? m_audioTrack->GetNumber() : 0;
	bool blockEntryEOS = false;

	if (videoFrame)
		videoFrame->bufferSize = 0;
	if (audioFrame)
		audioFrame->bufferSize = 0;

	if (videoTrackNumber == 0 && audioTrackNumber == 0)
		return false;

	if (m_eos)
		return false;

	if (!m_cluster)
		m_cluster = m_segment->GetFirst();

	do
	{
		bool getNewBlock = false;
		long status = 0;
		if (!m_blockEntry && !blockEntryEOS)
		{
			status = m_cluster->GetFirst(m_blockEntry);
			getNewBlock = true;
		}
		else if (blockEntryEOS || m_blockEntry->EOS())
		{
			m_cluster = m_segment->GetNext(m_cluster);
			if (!m_cluster || m_cluster->EOS())
			{
				m_eos = true;
				return false;
			}
			status = m_cluster->GetFirst(m_blockEntry);
			blockEntryEOS = false;
			getNewBlock = true;
		}
		else if (!m_block || m_blockFrameIndex == m_block->GetFrameCount() || notSupportedTrackNumber(videoTrackNumber, audioTrackNumber))
		{
			if(m_isSeek)
			{
				m_videoTrack->Seek(m_seekTime * 1e9, m_blockEntry);
				m_cluster = m_blockEntry->GetCluster();
				m_isSeek = false;
			}
			else
			{
				status = m_cluster->GetNext(m_blockEntry, m_blockEntry);
			}
			if (!m_blockEntry  || m_blockEntry->EOS())
			{
				blockEntryEOS = true;
				continue;
			}
			getNewBlock = true;
		}
		if (status || !m_blockEntry)
			return false;
		if (getNewBlock)
		{
			m_block = m_blockEntry->GetBlock();
			m_blockFrameIndex = 0;
		}
	} while (blockEntryEOS || notSupportedTrackNumber(videoTrackNumber, audioTrackNumber));

	WebMFrame *frame = NULL;

	const long trackNumber = m_block->GetTrackNumber();
	if (trackNumber == videoTrackNumber)
		frame = videoFrame;
	else if (trackNumber == audioTrackNumber)
		frame = audioFrame;
	else
	{
		//Should not be possible
		assert(trackNumber == videoTrackNumber || trackNumber == audioTrackNumber);
		return false;
	}

	const mkvparser::Block::Frame &blockFrame = m_block->GetFrame(m_blockFrameIndex++);
	if (blockFrame.len > frame->bufferCapacity)
	{
		unsigned char *newBuff = (unsigned char *)realloc(frame->buffer, frame->bufferCapacity = blockFrame.len);
		if (newBuff)
			frame->buffer = newBuff;
		else // Out of memory
			return false;
	}
	frame->bufferSize = blockFrame.len;

	frame->time = m_block->GetTime(m_cluster) / 1e9;
	frame->key  = m_block->IsKey();

	return !blockFrame.Read(m_reader, frame->buffer);
}

inline bool WebMDemuxer::notSupportedTrackNumber(long videoTrackNumber, long audioTrackNumber) const
{
	const long trackNumber = m_block->GetTrackNumber();
	return (trackNumber != videoTrackNumber && trackNumber != audioTrackNumber);
}
