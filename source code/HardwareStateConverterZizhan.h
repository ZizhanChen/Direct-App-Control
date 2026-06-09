/*
 * Copyright 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <optional>
#include <set>

#include <utils/Timers.h>

#include "inputzizhan/EventHubZizhan.h"
#include "inputzizhan/InputDeviceZizhan.h"
#include "CursorButtonAccumulatorZizhan.h"
#include "MultiTouchMotionAccumulatorZizhan.h"
#include "TouchButtonAccumulatorZizhan.h"

#include "include/gestures.h"

namespace zizhan {

// A HardwareState struct, but bundled with a vector to contain its FingerStates, so you don't have
// to worry about where that memory is allocated.
struct SelfContainedHardwareState {
    HardwareState state;
    std::vector<FingerState> fingers;
};

// Converts RawEvents into the HardwareState structs used by the gestures library.
class HardwareStateConverter {
public:
    HardwareStateConverter(const InputDeviceContext& deviceContext,
                           MultiTouchMotionAccumulator& motionAccumulator);

    std::optional<SelfContainedHardwareState> processRawEvent(const RawEvent* event);
    void reset();

private:
    SelfContainedHardwareState produceHardwareState(nsecs_t when);

    const InputDeviceContext& mDeviceContext;
    CursorButtonAccumulator mCursorButtonAccumulator;
    MultiTouchMotionAccumulator& mMotionAccumulator;
    TouchButtonAccumulator mTouchButtonAccumulator;
    int32_t mMscTimestamp = 0;
};

} // namespace zizhan
