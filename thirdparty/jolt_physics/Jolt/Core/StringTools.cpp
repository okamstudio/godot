// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt/Jolt.h>

#include <Jolt/Core/StringTools.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <cstdarg>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

String StringFormat(const char *inFMT, ...)
{
	char buffer[1024];

	// Format the string
	va_list list;
	va_start(list, inFMT);
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	return String(buffer);
}

void StringReplace(String &ioString, const string_view &inSearch, const string_view &inReplace)
{
	size_t index = 0;
	for (;;)
	{
		 index = ioString.find(inSearch, index);
		 if (index == String::npos)
			 break;

		 ioString.replace(index, inSearch.size(), inReplace);

		 index += inReplace.size();
	}
}

void StringToVector(const string_view &inString, Array<String> &outVector, const string_view &inDelimiter, bool inClearVector)
{
	JPH_ASSERT(inDelimiter.size() > 0);

	// Ensure vector empty
	if (inClearVector)
		outVector.clear();

	// No string? no elements
	if (inString.empty())
		return;

	// Start with initial string
	String s(inString);

	// Add to vector while we have a delimiter
	size_t i;
	while (!s.empty() && (i = s.find(inDelimiter)) != String::npos)
	{
		outVector.push_back(s.substr(0, i));
		s.erase(0, i + inDelimiter.length());
	}

	// Add final element
	outVector.push_back(s);
}

void VectorToString(const Array<String> &inVector, String &outString, const string_view &inDelimiter)
{
	// Ensure string empty
	outString.clear();

	for (const String &s : inVector)
	{
		// Add delimiter if not first element
		if (!outString.empty())
			outString.append(inDelimiter);

		// Add element
		outString.append(s);
	}
}

String ToLower(const string_view &inString)
{
	String out;
	out.reserve(inString.length());
	for (char c : inString)
		out.push_back((char)tolower(c));
	return out;
}

const char *NibbleToBinary(uint32 inNibble)
{
	static const char *nibbles[] = { "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111", "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111" };
	return nibbles[inNibble & 0xf];
}

JPH_NAMESPACE_END
