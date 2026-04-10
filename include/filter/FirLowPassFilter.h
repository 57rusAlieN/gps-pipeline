#pragma once

#include "filter/IGpsFilter.h"

#include <deque>
#include <vector>

// FIR low-pass filter — КИХ (конечная импульсная характеристика).
// Design: windowed-sinc with Hamming window.
// fc is normalised to the Nyquist frequency: fc=1.0 ≡ fs/2 (all-pass limit).
// History is pre-filled with the first sample to avoid initial transient.
class FirLowPassFilter final : public IGpsFilter
{
public:
    // cutoffNyquistNorm ∈ (0, 1], numTaps must be odd and >= 3
    FirLowPassFilter(double cutoffNyquistNorm, int numTaps = 21);

    FilterResult process(GpsPoint& point) override;

private:
    std::vector<double> m_coefficients;

    std::deque<double> m_lat;
    std::deque<double> m_lon;
    std::deque<double> m_speed;
    std::deque<double> m_alt;

    double filter(std::deque<double>& history, double x);

    static std::vector<double> computeCoefficients(double fc, int numTaps);
};
