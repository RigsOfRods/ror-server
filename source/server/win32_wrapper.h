/*
    This source file is part of Rigs of Rods
    Copyright 2005-2012 Pierre-Michel Ricordel
    Copyright 2007-2012 Thomas Fischer
    Copyright 2013-2025 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

///@file win32_wrapper.h
///@brief Windows specific includes (and conflict resolutions)

#pragma once

#ifdef _WIN32
#   include <windows.h>
#   include <time.h>

    // Defined in <wingdi.h> ~ clashes with AngelScript addons
#   ifdef GetObject
#      undef GetObject
#   endif

    // Defined in <winnt.h> ~ clashes with `HttpMethod::DELETE`
#   ifdef DELETE
#      undef DELETE
#   endif
#endif


