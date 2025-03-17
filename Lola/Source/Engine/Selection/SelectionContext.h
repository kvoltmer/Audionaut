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

namespace audium
{

//==============================================================================
/**
    These enums indicate the selection context.
*/
enum SelectionContextType
{
    invalid_context = 0,
    play_list_item = 1,     /**<  */
    audio_region = 2,       /**< . */
    audio_track = 3,        /**< . */
    audio_channel = 4,      /**< . */
    sub_group = 5,          /**< . */
    
};

} // namespace audium
