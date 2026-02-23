//# BlockCompat.h: Compatibility helpers for Block<T> to std::vector<T> migration
//# Copyright (C) 2026
//# Associated Universities, Inc. Washington DC, USA.
//#
//# This library is free software; you can redistribute it and/or modify it
//# under the terms of the GNU Library General Public License as published by
//# the Free Software Foundation; either version 2 of the License, or (at your
//# option) any later version.
//#
//# This library is distributed in the hope that it will be useful, but WITHOUT
//# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
//# License for more details.
//#
//# You should have received a copy of the GNU Library General Public License
//# along with this library; if not, write to the Free Software Foundation,
//# Inc., 675 Massachusetts Ave, Cambridge, MA 02139, USA.
//#
//# Correspondence concerning AIPS++ should be addressed as follows:
//#        Internet email: casa-feedback@nrao.edu.
//#        Postal address: AIPS++ Project Office
//#                        National Radio Astronomy Observatory
//#                        520 Edgemont Road
//#                        Charlottesville, VA 22903-2475 USA

#ifndef CASA_BLOCKCOMPAT_H
#define CASA_BLOCKCOMPAT_H

// Temporary compatibility header for the Block<T> -> std::vector<T> migration.
// Provides free-function adapters so that code migrated to std::vector can use
// the same call patterns that were common with Block. This header will be
// deleted once the migration is complete.

#include <casacore/casa/aips.h>
#include <casacore/casa/Containers/Block.h>
#include <vector>
#include <algorithm>

namespace casacore { //# NAMESPACE CASACORE - BEGIN

// Convert a Block<T> to a std::vector<T> (copy).
template<typename T>
std::vector<T> toVector(const Block<T>& block) {
    return std::vector<T>(block.begin(), block.end());
}

// Convert a std::vector<T> to a Block<T> (copy).
template<typename T>
Block<T> toBlock(const std::vector<T>& vec) {
    Block<T> block(vec.size());
    std::copy(vec.begin(), vec.end(), block.begin());
    return block;
}

} //# NAMESPACE CASACORE - END

#endif
