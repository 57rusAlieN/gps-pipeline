#include "filter/FirLowPassFilter.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

FirLowPassFilter::FirLowPassFilter(double cutoffNyquistNorm, int numTaps)
{
    if (numTaps < 3)
        throw std::invalid_argument("FirLowPassFilter: numTaps must be >= 3");
    if (cutoffNyquistNorm <= 0.0 || cutoffNyquistNorm > 1.0)
        throw std::invalid_argument("FirLowPassFilter: cutoff must be in (0, 1]");

    m_coefficients = computeCoefficients(cutoffNyquistNorm, numTaps);
}

// ---------------------------------------------------------------------------
// IGpsFilter::process
// ---------------------------------------------------------------------------

FilterResult FirLowPassFilter::process(GpsPoint& point)
{
    point.lat       = filter(m_lat,   point.lat);
    point.lon       = filter(m_lon,   point.lon);
    point.speed_kmh = filter(m_speed, point.speed_kmh);
    point.altitude  = filter(m_alt,   point.altitude);
    return {FilterStatus::Pass, ""};
}

// ---------------------------------------------------------------------------
// Apply filter to one signal channel
// ---------------------------------------------------------------------------

double FirLowPassFilter::filter(std::deque<double>& history, double x)
{
    const std::size_t N = m_coefficients.size();

    if (history.empty())
    {
        // Pre-fill with first value to avoid initial transient
        history.assign(N, x);
    }
    else
    {
        history.push_back(x);
        history.pop_front();
    }

    // Causal convolution:  y[n] = Σ h[k] * x[n-k]
    // history[N-1] = x[n] (newest), history[0] = x[n-(N-1)] (oldest)
    double out = 0.0;
    for (std::size_t k = 0; k < N; ++k)
        out += m_coefficients[k] * history[N - 1 - k];

    return out;
}

// ---------------------------------------------------------------------------
// Compute windowed-sinc coefficients
//
// Cutoff fc is normalised to the Nyquist frequency (fc=1 ≡ fs/2).
//
// Ideal LPF impulse response (Nyquist-normalised):
//   h_ideal[n] = sin(π · fc · m) / (π · m)   m = n - M/2, m ≠ 0
//   h_ideal[M/2] = fc                          (limit as m → 0)
//
// Hamming window:
//   w[n] = 0.54 − 0.46 · cos(2π · n / M)
//
// Final coefficients are normalised to unit DC gain (Σ h[n] = 1).
// ---------------------------------------------------------------------------

std::vector<double> FirLowPassFilter::computeCoefficients(double fc, int numTaps)
{
    const int    M  = numTaps - 1;
    const double pi = std::numbers::pi;

    std::vector<double> h(static_cast<std::size_t>(numTaps));

    for (int n = 0; n < numTaps; ++n)
    {
        const double m = n - M / 2.0;

        double sinc;
        if (std::abs(m) < 1e-12)
            sinc = fc;
        else
            sinc = std::sin(pi * fc * m) / (pi * m);

        const double hamming = 0.54 - 0.46 * std::cos(2.0 * pi * n / M);
        h[static_cast<std::size_t>(n)] = sinc * hamming;
    }

    // Normalise so Σ h[n] = 1  (unity DC gain)
    const double sum = std::accumulate(h.begin(), h.end(), 0.0);
    if (std::abs(sum) > 1e-12)
        for (auto& c : h) c /= sum;

    return h;
}
