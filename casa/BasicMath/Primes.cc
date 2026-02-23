//# Primes.cc:  This class provides some prime number operations using a cached table
//# Copyright (C) 1994,1995,1998
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

//# Includes

#include <casacore/casa/BasicMath/Primes.h>
#include <casacore/casa/BasicMath/Math.h>			// For sqrt only

namespace casacore { //# NAMESPACE CASACORE - BEGIN

//The minimum size for the cacheTable
const uInt MINSIZE = 31; 

std::vector<uInt>  Primes::cacheTable;
std::mutex   Primes::theirMutex;


Bool Primes::isPrime(uInt number)
{
    if (number < 2) return False;
    return(smallestPrimeFactor(number)==number ? True : False);
}

uInt Primes::aLargerPrimeThan( uInt number ) 
{    
    std::lock_guard<std::mutex> lock(theirMutex);
    // If number is equal to or larger than the last (and largest) element in 
    // the table of primes, this function returns zero; otherwise, this 
    // function returns the next higher prime in the table.
    
    if ( cacheTable.size() < MINSIZE ) initializeCache();

    if ( number >= cacheTable[cacheTable.size() - 1] ) return 0;

    Int index = -1;
    for ( uInt i = cacheTable.size(); i > 0; i-- ) {
	if ( cacheTable[(i-1)] > number ) {
	    index =(i-1);
	}
    }
    return cacheTable[ index ];
}

uInt Primes::nextLargerPrimeThan( uInt number ) 
{
    std::lock_guard<std::mutex> lock(theirMutex);
    uInt i;
    // This function increments number until it is prime.  It finds the next
    // entry in the table of primes which is larger, and stores this entry's
    // index number.  The table is resized to accomodate another entry, and
    // every entry after the stored index is moved over by one.  The new prime
    // number is inserted to the spot marked by the stored index.

    if ( cacheTable.size() < MINSIZE ) {
	initializeCache();
    }
    while( !isPrime( ++number ) ) {}
    uInt index = cacheTable.size();
    for( i = cacheTable.size(); i > 0; i-- ) {
	if ( cacheTable[(i-1)] == number ) {
	    return number;
	}
	if ( cacheTable[(i-1)] > number ) {
	    index =(i-1);
	}
    }
    cacheTable.insert(cacheTable.begin() + index, number);
    return number;
}
 
uInt Primes::smallestPrimeFactor( uInt number ) 
{
    // This function checks for factors: if found, the first (smallest) one is
    // returned, otherwise the original value is returned.
    
    // This algorithm is not the best, but checks for divisability by 6n +/- 1

    if (number == 0) return 0;
    if ((number % 2) == 0) return 2;
    if ((number % 3) == 0) return 3;

    for (uInt i=5,k=7,sq=(uInt)(sqrt(Double(number))+1); i<sq; i=i+6,k=k+6) {
	if ((number % i) == 0) return i;
	if ((number % k) == 0) return k;
    }
    return number;
} 

Block<uInt> Primes::factor( uInt number ) 
{
    //If number is zero or one, this function returns a one-cell block
    //containing number; otherwise this fuction continues to resize the 
    //block by one and store the next smallest factor of number in the 
    //block until number equals the product of all the factors stored 
    //in the block.

    std::vector<uInt> multiples;

    if (number < 2) {
	multiples.resize(1);
	multiples[0] = number;
    } else {
	for (uInt index=0; number > 1; index++) {
	    multiples.resize( index+1 );
	    multiples[index] = smallestPrimeFactor(number);
	    number = number / multiples[index];
	}
    }
    return multiples;
}

void Primes::initializeCache()
{
    // This function resets the cache to a block of 30, which
    // contains the next prime greater than each power of two.

    cacheTable = {
        3, 5, 11, 17, 37, 67, 131, 257, 521, 1031,
        2053, 4099, 8209, 16411, 32771, 65537, 131101, 262147, 524309, 1048583,
        2097169, 4194319, 8388617, 16777259, 33554467, 67108879, 134217757,
        268435459, 536870923, 1073741827
    };
}

} //# NAMESPACE CASACORE - END

