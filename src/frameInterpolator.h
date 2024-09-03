#ifndef FRAME_INTERPOLATOR_H
#define FRAME_INTERPOLATOR_H

#include <unordered_map>
#include <iostream>
#include <limits>

class FrameInterpolator
{
public:

    static constexpr float UNINITIALIZED = -std::numeric_limits<float>::max();

    FrameInterpolator() {}

    void init(int first, int last)
    {
        if (didInit) 
        {
            std::cerr << "FrameInterpolator has already been initialized." << std::endl;
            return;
        }

        interpolationRange.i = first;
        interpolationRange.f = last;
        interpolationValue = first;
        
        didInit = true;
    }

    bool updateValues(int stall = 1)
    {
        // Make sure the interpolator was initialized
        if (!didInit) return false;
        
        // End after interpolation value reaches end
        if (interpolationValue > interpolationRange.f)
        {
            didInit = false;
            return false;
        }

        static int callsSinceLastUpdate = 1;
        if (callsSinceLastUpdate < stall)
        {
            callsSinceLastUpdate++;
            return false;
        }
        callsSinceLastUpdate = 1;

        // Calculate current alpha value. Prevent division by zero
        float alpha = (interpolationRange.f != interpolationRange.i)
        ? (interpolationValue - interpolationRange.i) / (float)(interpolationRange.f - interpolationRange.i)
        : 1.0f;

        // Update every value
        for (auto it = rangeMap.begin(); it != rangeMap.end(); it++)
        {
            float *key = it->first;
            updateValue(key, alpha);
        }

        interpolationValue++;
        return true;
    }


    // * MAP HANDLERS

    void set(float *target, float iVal, float fVal)
    {
        rangeMap[target] = { iVal, fVal };
    }

    void setUninitialized(float *target)
    {
        rangeMap[target] = { UNINITIALIZED, UNINITIALIZED };
    }

    void setInitial(float *target, float iVal)
    {
        if (!isTargetInMap(target)) rangeMap[target].f = UNINITIALIZED;
        rangeMap[target].i = iVal;
    }

    void setFinal(float *target, float fVal)
    {
        if (!isTargetInMap(target)) rangeMap[target].i = UNINITIALIZED;
        rangeMap[target].f = fVal;
    }

    void clear()
    {
        rangeMap.clear();
        didInit = false;
    }


    // * STATES

    bool isTargetInMap(float *target)
    {
        return rangeMap.find(target) != rangeMap.end();
    }

    bool isTargetActive(float *target)
    {
        if (!isTargetInMap(target)) return false;
        return rangeMap[target].i != UNINITIALIZED && rangeMap[target].f != UNINITIALIZED;
    }

    bool isInitialValueActive(float *target)
    {
        if (!isTargetInMap(target)) return false;
        return rangeMap[target].i != UNINITIALIZED;
    }

    bool isFinalValueActive(float *target)
    {
        if (!isTargetInMap(target)) return false;
        return rangeMap[target].f != UNINITIALIZED;
    }

    bool isActive()
    {
        return didInit;
    }

    // * Getters

    int getInterpolationValue()
    {
        return interpolationValue;
    }

    int getMaxInterpolationValue()
    {
        return interpolationRange.f;
    }

    int getMinInterpolationValue()
    {
        return interpolationRange.i;
    }

    float getInitialValue(float *target)
    {
        if (!isInitialValueActive(target)) return UNINITIALIZED;
        return rangeMap[target].i;
    }

    float getFinalValue(float *target)
    {
        if (!isFinalValueActive(target)) return UNINITIALIZED;
        return rangeMap[target].f;
    }

private:

    template <typename T> struct Range { T i, f; };
    bool didInit = false;
    std::unordered_map<float*, Range<float>> rangeMap;  // Inclusive
    Range<int> interpolationRange;  // Inclusive
    int interpolationValue = 0;

    void updateValue(float *target, float alpha)
    {
        float iVal = rangeMap[target].i;
        float fVal = rangeMap[target].f;

        // If either end of the range is uninitialized, then don't update
        if (iVal == UNINITIALIZED || fVal == UNINITIALIZED) return;
        
        // Update value
        *target = iVal*(1.0 - alpha) + fVal*alpha;
    }

};

#endif