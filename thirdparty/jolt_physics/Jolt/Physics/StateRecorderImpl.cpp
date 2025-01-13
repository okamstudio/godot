// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Physics/StateRecorderImpl.h>

JPH_NAMESPACE_BEGIN

void StateRecorderImpl::WriteBytes(const void *inData, size_t inNumBytes)
{
	mStream.write((const char *)inData, inNumBytes);
}

void StateRecorderImpl::Rewind()
{
	mStream.seekg(0, std::stringstream::beg);
}

void StateRecorderImpl::Clear()
{
	mStream.clear();
	mStream.str({});
}

void StateRecorderImpl::ReadBytes(void *outData, size_t inNumBytes)
{
	if (IsValidating())
	{
		// Read data in temporary buffer to compare with current value
		void *data = JPH_STACK_ALLOC(inNumBytes);
		mStream.read((char *)data, inNumBytes);
		if (memcmp(data, outData, inNumBytes) != 0)
		{
			// Mismatch, print error
			Trace("Mismatch reading %u bytes", (uint)inNumBytes);
			for (size_t i = 0; i < inNumBytes; ++i)
			{
				int b1 = reinterpret_cast<uint8 *>(outData)[i];
				int b2 = reinterpret_cast<uint8 *>(data)[i];
				if (b1 != b2)
					Trace("Offset %d: %02X -> %02X", i, b1, b2);
			}
			JPH_BREAKPOINT;
		}

		// Copy the temporary data to the final destination
		memcpy(outData, data, inNumBytes);
		return;
	}

	mStream.read((char *)outData, inNumBytes);
}

bool StateRecorderImpl::IsEqual(StateRecorderImpl &inReference)
{
	// Get length of new state
	mStream.seekg(0, std::stringstream::end);
	std::streamoff this_len = mStream.tellg();
	mStream.seekg(0, std::stringstream::beg);

	// Get length of old state
	inReference.mStream.seekg(0, std::stringstream::end);
	std::streamoff reference_len = inReference.mStream.tellg();
	inReference.mStream.seekg(0, std::stringstream::beg);

	// Compare size
	bool fail = reference_len != this_len;
	if (fail)
	{
		Trace("Failed to properly recover state, different stream length!");
		return false;
	}

	// Compare byte by byte
	for (std::streamoff i = 0, l = this_len; !fail && i < l; ++i)
	{
		fail = inReference.mStream.get() != mStream.get();
		if (fail)
		{
			Trace("Failed to properly recover state, different at offset %d!", (int)i);
			return false;
		}
	}

	return true;
}

JPH_NAMESPACE_END
