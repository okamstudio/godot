// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

JPH_NAMESPACE_BEGIN

/// Helper class that either contains a valid result or an error
template <class Type>
class Result
{
public:
	/// Default constructor
						Result()									{ }

	/// Copy constructor
						Result(const Result<Type> &inRHS) :
		mState(inRHS.mState)
	{
		switch (inRHS.mState)
		{
		case EState::Valid:
			::new (&mResult) Type (inRHS.mResult);
			break;

		case EState::Error:
			::new (&mError) String(inRHS.mError);
			break;

		case EState::Invalid:
			break;
		}
	}

	/// Move constructor
						Result(Result<Type> &&inRHS) noexcept :
		mState(inRHS.mState)
	{
		switch (inRHS.mState)
		{
		case EState::Valid:
			::new (&mResult) Type (std::move(inRHS.mResult));
			break;

		case EState::Error:
			::new (&mError) String(std::move(inRHS.mError));
			break;

		case EState::Invalid:
			break;
		}

		// Don't reset the state of inRHS, the destructors still need to be called after a move operation
	}

	/// Destructor
						~Result()									{ Clear(); }

	/// Copy assignment
	Result<Type> &		operator = (const Result<Type> &inRHS)
	{
		Clear();

		mState = inRHS.mState;

		switch (inRHS.mState)
		{
		case EState::Valid:
			::new (&mResult) Type (inRHS.mResult);
			break;

		case EState::Error:
			::new (&mError) String(inRHS.mError);
			break;

		case EState::Invalid:
			break;
		}

		return *this;
	}

	/// Move assignment
	Result<Type> &		operator = (Result<Type> &&inRHS) noexcept
	{
		Clear();

		mState = inRHS.mState;

		switch (inRHS.mState)
		{
		case EState::Valid:
			::new (&mResult) Type (std::move(inRHS.mResult));
			break;

		case EState::Error:
			::new (&mError) String(std::move(inRHS.mError));
			break;

		case EState::Invalid:
			break;
		}

		// Don't reset the state of inRHS, the destructors still need to be called after a move operation

		return *this;
	}

	/// Clear result or error
	void				Clear()
	{
		switch (mState)
		{
		case EState::Valid:
			mResult.~Type();
			break;

		case EState::Error:
			mError.~String();
			break;

		case EState::Invalid:
			break;
		}

		mState = EState::Invalid;
	}

	/// Checks if the result is still uninitialized
	bool				IsEmpty() const								{ return mState == EState::Invalid; }

	/// Checks if the result is valid
	bool				IsValid() const								{ return mState == EState::Valid; }

	/// Get the result value
	const Type &		Get() const									{ JPH_ASSERT(IsValid()); return mResult; }

	/// Set the result value
	void				Set(const Type &inResult)					{ Clear(); ::new (&mResult) Type(inResult); mState = EState::Valid; }

	/// Set the result value (move value)
	void				Set(Type &&inResult)						{ Clear(); ::new (&mResult) Type(std::move(inResult)); mState = EState::Valid; }

	/// Check if we had an error
	bool				HasError() const							{ return mState == EState::Error; }

	/// Get the error value
	const String &		GetError() const							{ JPH_ASSERT(HasError()); return mError; }

	/// Set an error value
	void				SetError(const char *inError)				{ Clear(); ::new (&mError) String(inError); mState = EState::Error; }
	void				SetError(const string_view &inError)		{ Clear(); ::new (&mError) String(inError); mState = EState::Error; }
	void				SetError(String &&inError)					{ Clear(); ::new (&mError) String(std::move(inError)); mState = EState::Error; }

private:
	union
	{
		Type			mResult;									///< The actual result object
		String			mError;										///< The error description if the result failed
	};

	/// State of the result
	enum class EState : uint8
	{
		Invalid,
		Valid,
		Error
	};

	EState				mState = EState::Invalid;
};

JPH_NAMESPACE_END
