#pragma once

#include "Utils/Types.h"

#include <algorithm>
#include <deque>
#include <limits>

template<typename T>
class TPerfValue
{
public:
    void Push(T value)
    {
        Values.push_back(value);
        while ((i32)Values.size() > MaxSamples)
            Values.pop_front();
    }

    f32 WindowedAverage()
    {
        T sum = 0.f;
        for (auto value : Values)
            sum += value;
        return (f32)sum / Values.size();
    }

    bool HasValue() { return !Values.empty(); }
    T Last() { return Values.back(); }

    T WindowedMin()
    {
        T min = std::numeric_limits<T>::max();
        for (auto value : Values)
            min = std::min(value, min);
        return min;
    }

    T WindowedMax()
    {
        T max = std::numeric_limits<T>::lowest();
        for (auto value : Values)
            max = std::max(value, max);
        return max;
    }

    i32 MaxSamples = 120;
    std::deque<T> Values;
};