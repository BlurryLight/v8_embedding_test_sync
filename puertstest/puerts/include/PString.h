/*
 * Tencent is pleased to support the open source community by making Puerts available.
 * Copyright (C) 2020 THL A29 Limited, a Tencent company.  All rights reserved.
 * Puerts is licensed under the BSD 3-Clause License, except for the third-party components listed in the file 'LICENSE' which may
 * be subject to their corresponding license terms. This file is subject to the terms and conditions defined in file 'LICENSE',
 * which is part of this source code package.
 */

#pragma once

#include <cstring>
#include <functional>
#include "NamespaceDef.h"

namespace PUERTS_NAMESPACE
{
class PString
{
public:
    PString() : data_(new char[1]{'\0'}), size_(0)
    {
    }

    PString(const char* str)
    {
        assign(str, str ? std::strlen(str) : 0);
    }

    PString(const char* str, size_t length)
    {
        assign(str, str ? length : 0);
    }

    PString(const PString& other)
    {
        assign(other.data_, other.size_);
    }

    PString& operator=(const PString& other)
    {
        if (this != &other)
        {
            delete[] data_;
            assign(other.data_, other.size_);
        }
        return *this;
    }

    ~PString()
    {
        delete[] data_;
    }

    PString operator+(const PString& other) const
    {
        PString result;
        delete[] result.data_;
        result.size_ = size_ + other.size_;
        result.data_ = new char[result.size_ + 1];
#ifdef _MSC_VER
        strncpy_s(result.data_, result.size_ + 1, data_, size_);
        strncpy_s(result.data_ + size_, result.size_ - size_ + 1, other.data_, other.size_);
#else
        std::strncpy(result.data_, data_, size_);
        std::strncpy(result.data_ + size_, other.data_, other.size_);
#endif
        result.data_[result.size_] = '\0';
        return result;
    }

    friend PString operator+(const char* lhs, const PString& rhs)
    {
        return PString(lhs) + rhs;
    }

    const char* c_str() const
    {
        return data_;
    }

    size_t size() const
    {
        return size_;
    }

    bool empty() const
    {
        return size_ == 0;
    }

    bool operator<(const PString& other) const
    {
        return std::strcmp(data_, other.data_) < 0;
    }

    bool operator==(const PString& other) const
    {
        return std::strcmp(data_, other.data_) == 0;
    }

private:
    void assign(const char* str, size_t length)
    {
        size_ = length;
        data_ = new char[size_ + 1];
        if (str && size_ > 0)
        {
#ifdef _MSC_VER
            strncpy_s(data_, size_ + 1, str, size_);
#else
            std::strncpy(data_, str, size_);
#endif
        }
        data_[size_] = '\0';
    }

    char* data_;
    size_t size_;
};
}    // namespace PUERTS_NAMESPACE

namespace std
{
template <>
struct hash<PUERTS_NAMESPACE::PString>
{
    size_t operator()(const PUERTS_NAMESPACE::PString& str) const
    {
        size_t hash = 5381;
        for (size_t i = 0; i < str.size(); ++i)
        {
            hash = ((hash << 5) + hash) + str.c_str()[i];
        }
        return hash;
    }
};
}    // namespace std
