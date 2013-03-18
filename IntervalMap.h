/*
   This file is part of memview, a real-time memory trace visualization
   application.

   Copyright (C) 2013 Andrew Clinton

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#ifndef IntervalMap_H
#define IntervalMap_H

#include <QMutex>
#include "Math.h"
#include <unordered_set>
#include <map>
#include <string>
#include <assert.h>
#include <iostream>

// This class stores a map of non-overlapping intervals [start, end).  The
// manipulator methods ensure that the interval map is always
// non-overlapping.
template <typename T>
class IntervalMap {
private:
    struct Entry {
	uint64	start;
	T	obj;
    };

    typedef std::map<uint64, Entry> MapType;

public:
    void    insert(uint64 start, uint64 end, const T &val)
    {
	QMutexLocker lock(&myLock);

	clearOverlappingIntervals(start, end);

	myMap[end].start = start;
	myMap[end].obj = val;
    }

    void    erase(uint64 start, uint64 end)
    {
	QMutexLocker lock(&myLock);

	clearOverlappingIntervals(start, end);
    }
    size_t  size() const { return myMap.size(); }

    //
    // Note that these methods return elements by value.  This is to ensure
    // thread safety in the case where another thread erases or overwrites
    // an element after it has been queried by the display thread.
    //

    // Finds the element above and below the query address, and returns the
    // closer of the two.
    T    findClosest(uint64 addr) const
    {
	QMutexLocker lock(&myLock);
	auto hi = myMap.upper_bound(addr);
	if (hi != myMap.end())
	{
	    auto lo = hi;
	    --lo;
	    if (lo != myMap.end())
	    {
		return dist2(hi, addr) <= dist2(lo, addr) ?
		    hi->second.obj : lo->second.obj;
	    }
	    return hi->second.obj;
	}
	else if (myMap.size())
	    return myMap.rbegin()->second.obj;

	return T();
    }

    // Returns the element whose interval contains addr if it exists -
    // otherwise 0.
    T    find(uint64 addr) const
    {
	QMutexLocker lock(&myLock);
	auto it = myMap.upper_bound(addr);

	if (it != myMap.end() && !dist2(it, addr))
	    return it->second.obj;

	return T();
    }

    void    dump() const
    {
	for (auto it = myMap.begin(); it != myMap.end(); ++it)
	{
	    std::cerr
		<< "[" << it->second.start
		<< ", " << it->first << "): "
		<< it->second.obj << "\n";
	}
    }

private:
    // Find the distance from an address to an interval
    template <typename IT>
    uint64 dist2(const IT &e, uint64 addr) const
    {
	if (addr < e->second.start)
	    return e->second.start - addr;
	if (addr >= e->first)
	    return addr - e->first + 1;
	return 0;
    }

    void clearOverlappingIntervals(uint64 start, uint64 end)
    {
	// Find any overlapping intervals and either erase them or shorten
	// them to make them non-overlapping.

	auto it = myMap.upper_bound(start);

	while (it != myMap.end() && it->second.start < end)
	{
	    auto next = it;
	    next++;

	    if (it->second.start < start)
	    {
		// We used upper_bound so this should always be true
		assert(it->first > start);

		Entry   val = it->second;

		// Test whether the existing interval needs to be split or
		// just truncated
		if (it->first > end)
		    it->second.start = end;
		else
		    myMap.erase(it);

		myMap[start] = val;
	    }
	    else if (it->first <= end)
		myMap.erase(it);
	    else
		it->second.start = end;

	    it = next;
	}
    }

private:
    MapType	      myMap;
    mutable QMutex    myLock;
};

typedef IntervalMap<std::string> StackTraceMap;
typedef IntervalMap<std::string> MMapMap;

#endif
