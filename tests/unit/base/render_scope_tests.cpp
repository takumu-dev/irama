// SPDX-FileCopyrightText: 2026 takumu-dev
// SPDX-License-Identifier: Zlib

#include <irama/engine/base/render_scope.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Only x86-64 has a floating-point environment that a render scope sets.
#if defined(__x86_64__) || defined(_M_X64)

#include <xmmintrin.h>

#include <limits>

namespace {

// The bits of MXCSR that flush a denormal result to zero, and read a denormal operand as zero.
constexpr unsigned int flush_to_zero = 1U << 15U;
constexpr unsigned int denormals_are_zero = 1U << 6U;
// The bits that choose rounding toward zero, which a render scope leaves as they are.
constexpr unsigned int round_toward_zero = 3U << 13U;

// Puts the thread's MXCSR back as it was when the test case began, however the case ends.
class KeptControl {
  public:
    KeptControl() = default;
    KeptControl(const KeptControl&) = delete;
    KeptControl& operator=(const KeptControl&) = delete;
    KeptControl(KeptControl&&) = delete;
    KeptControl& operator=(KeptControl&&) = delete;

    ~KeptControl() { _mm_setcsr(saved_); }

    // The thread's MXCSR as the case began, with neither denormal bit set.
    [[nodiscard]] unsigned int without_denormal_bits() const {
        return saved_ & ~(flush_to_zero | denormals_are_zero);
    }

  private:
    unsigned int saved_ = _mm_getcsr();
};

} // namespace

TEST_CASE("a render scope sets flush-to-zero and denormals-are-zero", "[base]") {
    const KeptControl kept;
    _mm_setcsr(kept.without_denormal_bits());
    unsigned int inside = 0;
    {
        const irama::RenderScope scope;
        inside = _mm_getcsr();
    }
    REQUIRE((inside & flush_to_zero) != 0);
    REQUIRE((inside & denormals_are_zero) != 0);
    REQUIRE((inside & ~(flush_to_zero | denormals_are_zero)) == kept.without_denormal_bits());
}

TEST_CASE("a render scope reads a denormal as zero", "[base]") {
    const KeptControl kept;
    _mm_setcsr(kept.without_denormal_bits());
    // Volatile, so that the compiler can't work out the products itself.
    const volatile float denormal = std::numeric_limits<float>::denorm_min();
    const volatile float two = 2.0F;
    volatile float inside = 0.0F;
    {
        const irama::RenderScope scope;
        inside = denormal * two;
    }
    const float outside = denormal * two;
    REQUIRE(inside == 0.0F);
    REQUIRE(outside != 0.0F);
}

TEST_CASE("closing a render scope restores the MXCSR the thread had", "[base]") {
    const KeptControl kept;
    const unsigned int before =
        GENERATE_REF(kept.without_denormal_bits(), kept.without_denormal_bits() | round_toward_zero,
                     kept.without_denormal_bits() | flush_to_zero);
    _mm_setcsr(before);
    {
        const irama::RenderScope scope;
    }
    REQUIRE(_mm_getcsr() == before);
}

TEST_CASE("a nested render scope restores the MXCSR of the scope around it", "[base]") {
    const KeptControl kept;
    _mm_setcsr(kept.without_denormal_bits());
    unsigned int before_inner = 0;
    unsigned int after_inner = 0;
    {
        const irama::RenderScope outer;
        // Code inside the outer scope may change the environment, which the inner scope keeps.
        _mm_setcsr(_mm_getcsr() | round_toward_zero);
        before_inner = _mm_getcsr();
        {
            const irama::RenderScope inner;
        }
        after_inner = _mm_getcsr();
    }
    REQUIRE(after_inner == before_inner);
    REQUIRE(_mm_getcsr() == kept.without_denormal_bits());
}

#endif
