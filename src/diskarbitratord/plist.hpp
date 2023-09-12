/***************************************************************************
 *   plist.hpp  --  This file is part of diskarbitratord.                  *
 *                                                                         *
 *   Copyright (C) 2023 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   diskarbitratord is free software: you can redistribute it and/or      *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation, either version 3 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   diskarbitratord is distributed in the hope that it will be useful,    *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLIST_HPP_
#define PLIST_HPP_

#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>

// Simple plist abstraction class. There's no fancy templating here: all types
// are returned as strings and it's the caller that will have to convert that
// to whichever type they want.

// Every element of the Plist structure is considered a Plist node, with this
// you can:
// - Get the node's value
// - If it's an array, get its length or access the element at index i.
// - If it's a dictionary, retrieve one its keys.
// 
// We're not covering every case, but this does a good enough job for what we
// want, while being correct and easy to use.
class PlistNode {
  public:
    PlistNode(CFTypeRef& v, const std::string& key) : value(v), keyName(key) {};
    PlistNode& operator[](const std::string& key);
    PlistNode& operator[](long long int i);
    size_t size();

    template<typename T>
    T get() {
      return this->get<T>();
    }

  private:
    std::vector<std::unique_ptr<PlistNode>> generatedNodes;
    CFTypeRef value;
    const std::string keyName;
};

// The plist wrapper. Accessing one of its elements returns a PlistNode object.
// An example for using this would be:
// const T value = plist["keyName"]["nestedKeyName"][4].get<T>();
// This would be accessing key "keyName", which would be a CFDictionary, then
// its "nestedKeyName", which would be an array, and finally position 4 of
// that array. The value is converted to a string if supported, otherwise it
// throws an exception.
class Plist {
  public:
    Plist(const std::string& data);
    ~Plist();
    PlistNode& operator[](const std::string& key);

  private:
    CFPropertyListRef plist;
    std::vector<std::unique_ptr<PlistNode>> generatedNodes;
};

#endif
