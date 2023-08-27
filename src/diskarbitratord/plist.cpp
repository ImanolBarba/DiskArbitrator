/***************************************************************************
 *   plist.cpp  --  This file is part of diskarbitratord.                  *
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

#include "plist.hpp"
#include "scope_guard.hpp"

// Construct the main CFPropertyList object
Plist::Plist(const std::string& data) {
  CFStringRef plistStringCF = CFStringCreateWithCString(NULL, data.c_str(), kCFStringEncodingUTF8);
  if(plistStringCF == NULL) {
    throw std::runtime_error("Unable to create CF string with plist data");
  }
  ScopeGuard freeStringGuard([plistStringCF]() {
    CFRelease(plistStringCF);
  });

  CFDataRef plistData = CFStringCreateExternalRepresentation(NULL, plistStringCF, kCFStringEncodingUTF8, 0);
  if(plistData == NULL) {
    throw std::runtime_error("Unable to encode plist data");
  }
  ScopeGuard freePlistGuard([plistData]() {
    CFRelease(plistData);
  });

  this->plist = CFPropertyListCreateWithData(NULL, plistData, kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
  if(this->plist == NULL) {
    throw std::runtime_error("Unable to parse plist");
  }
}

// Free the plist reference
Plist::~Plist() {
  CFRelease(this->plist);
}

// Access one of the Plist's keys. Sine a CFPropertyListRef is essentially a
// CFDictionaryRef, there's no need to check for the type. Throws an exception
// if the key doesn't exist
PlistNode& Plist::operator[](const std::string& key) {
  CFStringRef keyCFStr = CFStringCreateWithCString(NULL, key.c_str(), kCFStringEncodingUTF8);
  if(keyCFStr == NULL) {
    throw std::runtime_error("Unable to create CF string with key");
  }
  ScopeGuard freeStringGuard([keyCFStr]() {
    CFRelease(keyCFStr);
  });
  
  if(!CFDictionaryContainsKey((CFDictionaryRef)this->plist, keyCFStr)) {
    throw std::runtime_error("Key " + key + " not found");
  }
  CFTypeRef value = CFDictionaryGetValue((CFDictionaryRef)this->plist, keyCFStr);
  
  PlistNode* n = new PlistNode(value, key);
  this->generatedNodes.push_back(std::unique_ptr<PlistNode>(n));

  return *n;
}

// Access a value's key if it's a CFDictionary. Throws an exception if the key
// doesn't exist
PlistNode& PlistNode::operator[](const std::string& key) {
  CFTypeID type = CFGetTypeID(this->value);
  if (type != CFDictionaryGetTypeID()) {
    throw std::runtime_error("Key " + this->keyName + " is not a dictionary. Can't access nested key " + key);
  }
  
  CFStringRef keyCFStr = CFStringCreateWithCString(NULL, key.c_str(), kCFStringEncodingUTF8);
  if(keyCFStr == NULL) {
    throw std::runtime_error("Unable to create CF string with key");
  }
  ScopeGuard freeStringGuard([keyCFStr]() {
    CFRelease(keyCFStr);
  });

  if(!CFDictionaryContainsKey((CFDictionaryRef)this->value, keyCFStr)) {
    throw std::runtime_error("Key " + key + " not found");
  }
  CFTypeRef nestedValue = CFDictionaryGetValue((CFDictionaryRef)this->value, keyCFStr);
  
  PlistNode* n = new PlistNode(nestedValue, key);
  this->generatedNodes.push_back(std::unique_ptr<PlistNode>(n));

  return *n;
}

// Converts the value to string representation
PlistNode::operator std::string() const {
  std::string strValue = "";

  CFTypeID type = CFGetTypeID(this->value);
  if(type == CFStringGetTypeID()) {
    strValue = std::string(CFStringGetCStringPtr((CFStringRef)this->value, kCFStringEncodingUTF8));
  } else if(type == CFNumberGetTypeID()) {
    int intValue;
    CFNumberGetValue((CFNumberRef)this->value, kCFNumberIntType, &intValue);
    strValue = std::to_string(intValue);
  } else if(type == CFBooleanGetTypeID()) {
    strValue = "false";
    if(CFBooleanGetValue((CFBooleanRef)this->value)) {
      strValue = "true";
    }
  } else {
    throw std::invalid_argument("Unsupported type");
  }

  return strValue;
}

// TODO: Maybe support negative indexes? Not really useful here...
// Access the value in index i if the node's value is a CFArray. Throws an 
// exception if the index is out of bounds
PlistNode& PlistNode::operator[](unsigned int i) {
  CFTypeID type = CFGetTypeID(this->value);
  if (type != CFArrayGetTypeID()) {
    throw std::runtime_error("Key " + this->keyName + " is not a list. Can't access elements");
  }

  if(this->size() < i) {
    throw std::out_of_range("Index out of bounds: " + std::to_string(i));
  }

  CFTypeRef nestedValue = CFArrayGetValueAtIndex((CFArrayRef)this->value, i);
  
  PlistNode* n = new PlistNode(nestedValue, this->keyName);
  this->generatedNodes.push_back(std::unique_ptr<PlistNode>(n));

  return *n;
}

size_t PlistNode::size() {
  CFTypeID type = CFGetTypeID(this->value);
  if (type != CFArrayGetTypeID()) {
    throw std::runtime_error("Key " + this->keyName + " is not a list. Can't get length");
  }

  CFIndex len = CFArrayGetCount((CFArrayRef)this->value);
  return (size_t)len;
}
