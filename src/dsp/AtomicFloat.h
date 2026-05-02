#pragma once

#include <atomic>
#include <bit>
#include <cstdint>

namespace nebula::dsp
{
class AtomicFloat final
{
public:
    AtomicFloat() noexcept = default;
    explicit AtomicFloat(float initial) noexcept { store(initial); }

    void store(float value) noexcept
    {
        bits.store(std::bit_cast<std::uint32_t>(value), std::memory_order_release);
    }

    [[nodiscard]] float load() const noexcept
    {
        return std::bit_cast<float>(bits.load(std::memory_order_acquire));
    }

private:
    std::atomic<std::uint32_t> bits { std::bit_cast<std::uint32_t>(0.0f) };
};

struct MeterFrame final
{
    AtomicFloat detection { 0.0f };
    AtomicFloat detectionMax { -160.0f };
    AtomicFloat reduction { 0.0f };
    AtomicFloat reductionMax { 0.0f };
};
} // namespace nebula::dsp
