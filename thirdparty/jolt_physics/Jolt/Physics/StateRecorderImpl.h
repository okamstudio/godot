// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Physics/StateRecorder.h>

JPH_NAMESPACE_BEGIN

/// Implementation of the StateRecorder class that uses a stringstream as underlying store and that implements checking if the state doesn't change upon reading
class JPH_EXPORT StateRecorderImpl final : public StateRecorder
{
public:
	/// Constructor
						StateRecorderImpl() = default;
						StateRecorderImpl(StateRecorderImpl &&inRHS)				: StateRecorder(inRHS), mStream(std::move(inRHS.mStream)) { }

	/// Write a string of bytes to the binary stream
	virtual void		WriteBytes(const void *inData, size_t inNumBytes) override;

	/// Rewind the stream for reading
	void				Rewind();

	/// Clear the stream for reuse
	void				Clear();

	/// Read a string of bytes from the binary stream
	virtual void		ReadBytes(void *outData, size_t inNumBytes) override;

	// See StreamIn
	virtual bool		IsEOF() const override										{ return mStream.eof(); }

	// See StreamIn / StreamOut
	virtual bool		IsFailed() const override									{ return mStream.fail(); }

	/// Compare this state with a reference state and ensure they are the same
	bool				IsEqual(StateRecorderImpl &inReference);

	/// Convert the binary data to a string
	std::string			GetData() const												{ return mStream.str(); }

	/// Get size of the binary data in bytes
	size_t				GetDataSize()												{ return size_t(mStream.tellp()); }

private:
	std::stringstream	mStream;
};

JPH_NAMESPACE_END
