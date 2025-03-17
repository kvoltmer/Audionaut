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

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

struct LoopData
{
    // loop start
    double loopStartPositionClocks  = 96.0;
    
    // loop end
    double loopEndPositionClocks    = 288.0;
    
    // the smallest possible loop length
    double minimumLoopLengthClocks  = 1.0;
    
    // loop active
    bool loopActive                 = false;
    
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LoopData,
                                                loopStartPositionClocks,
                                                loopEndPositionClocks,
                                                loopActive);

} // namespace audium
