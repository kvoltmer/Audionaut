//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <atomic>
#include <cassert>

namespace audium
{

class SampleTimer {
    
public:
    SampleTimer() = default;
    
    // returns true once timer is due
    bool process(int numSamples, int& offset) {
        if (active) {
            if (sampleCounter - numSamples <= 0) {
                offset = sampleCounter;
                sampleCounter = 0;
                active = false;
                return true;
            }
            else {
                sampleCounter -= numSamples;
            }
        }
        return false;
    }
    
    void schedule(int numSamples = 0) {
        sampleCounter = numSamples;
        active = true;
    }
    
    void invalidate() {
        active = false;
    }
    
    bool isActive() const noexcept {
        return active;
    }
    
private:
    int sampleCounter = 0;
    std::atomic<bool> active = false;
};

} // namespace audium
