// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Jolt/Core/StreamIn.h>
#include <Jolt/Core/StreamOut.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <ostream>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

/// Wrapper around std::ostream
class StreamOutWrapper : public StreamOut
{
public:
	/// Constructor
						StreamOutWrapper(ostream &ioWrapped)						: mWrapped(ioWrapped) { }

	/// Write a string of bytes to the binary stream
	virtual void		WriteBytes(const void *inData, size_t inNumBytes) override	{ mWrapped.write((const char *)inData, inNumBytes); }

	/// Returns true if there was an IO failure
	virtual bool		IsFailed() const override									{ return mWrapped.fail(); }

private:
	ostream &			mWrapped;
};

/// Wrapper around std::istream
class StreamInWrapper : public StreamIn
{
public:
	/// Constructor
						StreamInWrapper(istream &ioWrapped)							: mWrapped(ioWrapped) { }

	/// Write a string of bytes to the binary stream
	virtual void		ReadBytes(void *outData, size_t inNumBytes) override		{ mWrapped.read((char *)outData, inNumBytes); }

	/// Returns true when an attempt has been made to read past the end of the file
	virtual bool		IsEOF() const override										{ return mWrapped.eof(); }

	/// Returns true if there was an IO failure
	virtual bool		IsFailed() const override									{ return mWrapped.fail(); }

private:
	istream &			mWrapped;
};

JPH_NAMESPACE_END
