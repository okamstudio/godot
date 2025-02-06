/*
 * Copyright (c) 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgCommon.h"
#include "thorvg_lottie.h"
#include "tvgLottieLoader.h"
#include "tvgAnimation.h"


/************************************************************************/
/* Internal Class Implementation                                        */
/************************************************************************/

/************************************************************************/
/* External Class Implementation                                        */
/************************************************************************/

LottieAnimation::~LottieAnimation()
{
}


Result LottieAnimation::override(const char* slot) noexcept
{
    if (!pImpl->picture->pImpl->loader) return Result::InsufficientCondition;

    if (static_cast<LottieLoader*>(pImpl->picture->pImpl->loader)->override(slot)) return Result::Success;

    return Result::InvalidArguments;
}


Result LottieAnimation::segment(const char* marker) noexcept
{
    auto loader = pImpl->picture->pImpl->loader;
    if (!loader) return Result::InsufficientCondition;

    if (!marker) {
        static_cast<FrameModule*>(loader)->segment(0.0f, 1.0f);
        return Result::Success;
    }
    
    float begin, end;
    if (!static_cast<LottieLoader*>(loader)->segment(marker, begin, end)) return Result::InvalidArguments;

    return static_cast<Animation*>(this)->segment(begin, end);
}


uint32_t LottieAnimation::markersCnt() noexcept
{
    auto loader = pImpl->picture->pImpl->loader;
    if (!loader) return 0;
    return static_cast<LottieLoader*>(loader)->markersCnt();
}


const char* LottieAnimation::marker(uint32_t idx) noexcept
{
    auto loader = pImpl->picture->pImpl->loader;
    if (!loader) return nullptr;
    return static_cast<LottieLoader*>(loader)->markers(idx);
}


unique_ptr<LottieAnimation> LottieAnimation::gen() noexcept
{
    return unique_ptr<LottieAnimation>(new LottieAnimation);
}
