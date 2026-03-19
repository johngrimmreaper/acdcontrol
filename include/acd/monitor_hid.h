#ifndef ACD_MONITOR_HID_H
#define ACD_MONITOR_HID_H

/*
 * Generic USB HID monitor-control definitions and helpers.
 *
 * Purpose
 * -------
 * This header is meant to hold only the parts that are generic, stable,
 * and reusable across multiple monitor models.
 *
 * In other words:
 *   - standard HID / USB monitor concepts belong here
 *   - model-specific reverse-engineered findings do NOT belong here
 *
 * Why split it this way?
 * ----------------------
 * Because it keeps the code honest.
 *
 * If something is defined by the HID monitor-control world in general,
 * it should live in a generic header.
 *
 * If something was discovered by probing one particular monitor model,
 * it should live in that monitor's own header.
 *
 * This avoids a common reverse-engineering problem:
 * mixing "spec fact" with "device-specific behavior".
 */

#include <cstddef>
#include <cstdint>

namespace acd {
namespace hid {
namespace monitor {

/*
 * Confidence level for meanings assigned to reports or fields.
 *
 * Why this exists
 * ---------------
 * During reverse engineering, some things are:
 *   - directly defined by a public standard
 *   - strongly suggested by repeated behavior
 *   - still unknown
 *
 * Encoding that confidence explicitly in code is very useful.
 * It turns the source tree into living documentation.
 */
enum MeaningConfidence {
    MEANING_UNKNOWN = 0,
    MEANING_INFERRED = 1,
    MEANING_CONFIRMED = 2
};

/*
 * Top-level HID usage pages relevant to USB monitors.
 *
 * Observed / commonly relevant pages
 * ----------------------------------
 * 0x80 : USB Monitor
 * 0x81 : USB Enumerated Values
 * 0x82 : VESA Virtual Controls
 * 0x83 : Reserved / uncommon in practice for our current work
 *
 * For your Apple LED Cinema Display, the descriptor begins with:
 *   Usage Page (0x80)
 *   Usage      (0x01)
 * which identifies the device as a monitor-control HID function.
 */
static const std::uint16_t USAGE_PAGE_USB_MONITOR = 0x0080;
static const std::uint16_t USAGE_PAGE_USB_ENUMERATED_VALUES = 0x0081;
static const std::uint16_t USAGE_PAGE_VESA_VIRTUAL_CONTROLS = 0x0082;
static const std::uint16_t USAGE_PAGE_RESERVED_83 = 0x0083;

/*
 * Common top-level USB Monitor usages.
 *
 * For now we only define the one we have directly observed and care about.
 */
static const std::uint16_t USAGE_MONITOR_CONTROL = 0x0001;

/*
 * Standard VESA virtual controls that we know we use.
 *
 * Important:
 * ----------
 * Only put constants here when their meaning is actually known.
 *
 * From your monitor's descriptor, usage 0x0010 on page 0x0082 is the
 * brightness control. That one is safe to name strongly.
 */
static const std::uint16_t USAGE_BRIGHTNESS = 0x0010;

/*
 * Standard logical brightness range observed on your display.
 *
 * Your descriptor says:
 *   Logical Minimum = 0
 *   Logical Maximum = 1023
 *
 * We keep those names generic because many monitor-control HID devices use
 * the same style of 10-bit logical scale, and it is a useful abstraction
 * even when the real panel behavior is not perfectly linear.
 */
static const std::uint16_t BRIGHTNESS_MIN = 0;
static const std::uint16_t BRIGHTNESS_MAX = 1023;

/*
 * Basic metadata about a known report.
 *
 * This is intentionally simple and old-compiler-friendly.
 * It is useful for:
 *   - debug dumps
 *   - developer-facing documentation
 *   - future CLI/report introspection
 */
struct KnownReportInfo {
    std::uint8_t report_id;
    const char* short_name;
    const char* meaning;
    MeaningConfidence confidence;
    std::uint16_t usage_page;
    std::uint16_t usage;
    std::size_t payload_size_bytes; /* not counting the leading report-id byte */
};

/*
 * Read a little-endian unsigned 16-bit integer from raw HID bytes.
 *
 * Why helper functions instead of struct casts?
 * ---------------------------------------------
 * HID report payloads are wire-format byte streams.
 * Casting them directly into C++ structs is brittle because of:
 *   - alignment concerns
 *   - packing issues
 *   - compiler differences
 *   - signedness confusion
 *
 * Explicit byte decoding is clearer and safer.
 */
inline std::uint16_t read_le_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        (static_cast<std::uint16_t>(p[1]) << 8)
    );
}

/*
 * Read a little-endian unsigned 32-bit integer from raw HID bytes.
 */
inline std::uint32_t read_le_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24)
    );
}

/*
 * Read a little-endian signed 32-bit integer from raw HID bytes.
 *
 * We first decode as unsigned and then cast to signed.
 * This preserves the bit pattern exactly.
 */
inline std::int32_t read_le_s32(const std::uint8_t* p) {
    return static_cast<std::int32_t>(read_le_u32(p));
}

/*
 * Write a little-endian unsigned 16-bit integer into raw HID bytes.
 *
 * This is useful when constructing feature reports to send to a device.
 */
inline void write_le_u16(std::uint8_t* p, std::uint16_t value) {
    p[0] = static_cast<std::uint8_t>(value & 0xFFu);
    p[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
}

/*
 * Returns true if a value is within an inclusive logical range.
 *
 * Useful for validating command-line input before attempting to send it to
 * the monitor.
 */
inline bool value_in_range_u16(std::uint16_t value,
                               std::uint16_t min_value,
                               std::uint16_t max_value) {
    return (value >= min_value) && (value <= max_value);
}

/*
 * Convert a logical value into a percentage of the given range.
 *
 * Notes
 * -----
 * - This is purely a convenience calculation for user interfaces.
 * - It does NOT imply that the monitor's actual light output is perfectly
 *   linear across the whole range.
 * - Still, it is a useful human-readable display.
 */
inline double value_to_percent_u16(std::uint16_t value,
                                   std::uint16_t min_value,
                                   std::uint16_t max_value) {
    if (max_value <= min_value) {
        return 0.0;
    }

    if (value <= min_value) {
        return 0.0;
    }

    if (value >= max_value) {
        return 100.0;
    }

    const double span = static_cast<double>(max_value - min_value);
    const double offset = static_cast<double>(value - min_value);
    return (offset * 100.0) / span;
}

/*
 * Helpers for common brightness calculations.
 */
inline bool brightness_value_is_valid(std::uint16_t value) {
    return value_in_range_u16(value, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
}

inline double brightness_value_to_percent(std::uint16_t value) {
    return value_to_percent_u16(value, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
}

} /* namespace monitor */
} /* namespace hid */
} /* namespace acd */

#endif /* ACD_MONITOR_HID_H */
