/*
 
 Copyright (C) 2012 Grame
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 
 Grame Research Laboratory, 9 rue du Garet, 69001 Lyon - France
 research@grame.fr
 
 */


#ifndef __utilities__
#define __utilities__

#include <vector>
#include <map>

namespace guidohttpd
{
    typedef std::pair<std::string, std::string>	TArg;
    typedef std::vector<TArg>					TArgs;
    
    std::string rand_alnum_str (std::string::size_type sz);
    
} // end namespoace

#endif