#pragma once

#include <vector>

namespace nebula::dsp::metrics
{
struct AudioMetrics final
{
    double rmsDb {};
    double peakDb {};
    double lufsIntegrated {};
    double spectralCentroidHz {};
    double highBandRatio {};
    double transientScore {};
    double harmonicMusicality {};
    double stereoCoherence {};
    double nullResidualDb {};
};

[[nodiscard]] AudioMetrics analyseStereo(const std::vector<double>& left,
                                         const std::vector<double>& right,
                                         double sampleRate);

[[nodiscard]] double nullResidualDb(const std::vector<double>& reference,
                                    const std::vector<double>& test);
} // namespace nebula::dsp::metrics
