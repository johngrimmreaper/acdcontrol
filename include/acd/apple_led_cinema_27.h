#ifndef ACD_APPLE_LED_CINEMA_27_H
#define ACD_APPLE_LED_CINEMA_27_H

/*
 * Apple LED Cinema Display 27" model-specific HID findings.
 *
 * Scope
 * -----
 * This header stores only what has been observed for the Apple
 * LED Cinema Display control HID device:
 *
 *   Vendor  : 0x05ac
 *   Product : 0x9226
 *
 * Philosophy
 * ----------
 * We keep names conservative on purpose.
 *
 * Example:
 *   Report 0x10 is strongly named "brightness" because that meaning is
 *   confirmed both by the descriptor and by live experiments.
 *
 * But report 0x66 is NOT named "auto_brightness" here, because although
 * that may turn out to be true, we have not yet proven it conclusively.
 *
 * Reverse engineering becomes much easier to maintain when the code itself
 * clearly distinguishes:
 *   - confirmed meaning
 *   - inferred meaning
 *   - unknown meaning
 */

#include "acd/monitor_hid.h"

namespace acd {
namespace apple {
namespace led_cinema_27 {

/*
 * USB identity of the monitor-control HID function.
 *
 * Important:
 * ----------
 * This is NOT the internal USB hub in the display.
 * This is the actual HID control interface we have been probing.
 */
static const std::uint16_t VENDOR_ID = 0x05ac;
static const std::uint16_t PRODUCT_ID = 0x9226;

/*
 * Human-readable name observed in Linux hidraw enumeration.
 */
static const char* const HID_NAME = "Apple Inc. Apple LED Cinema Display";

/*
 * Vendor-defined usage page observed in the descriptor for report IDs
 * 0xE1 and 0xEC.
 *
 * Because it is vendor-defined, the HID descriptor tells us the shape
 * of the data but not its semantic meaning.
 */
static const std::uint16_t VENDOR_USAGE_PAGE = 0xFF92;

/*
 * Report IDs discovered so far.
 *
 * Current status summary
 * ----------------------
 * 0x10 : confirmed brightness report
 * 0x66 : confirmed 2-state 1-byte monitor-page control, meaning still unknown
 * 0xE1 : confirmed vendor-defined 1-byte flag, meaning unknown
 * 0xEC : confirmed vendor-defined report with two 32-bit values;
 *        first value changes with brightness-related state,
 *        second value remained constant in current tests
 */
static const std::uint8_t REPORT_ID_BRIGHTNESS = 0x10;
static const std::uint8_t REPORT_ID_MODE_2STATE = 0x66;
static const std::uint8_t REPORT_ID_VENDOR_FLAG = 0xE1;
static const std::uint8_t REPORT_ID_VENDOR_STATE = 0xEC;

/*
 * Usage values associated with the reports, where known.
 *
 * REPORT_ID_BRIGHTNESS
 * --------------------
 * Descriptor evidence:
 *   Usage Page 0x0082 (VESA Virtual Controls)
 *   Usage      0x0010 (Brightness)
 *
 * REPORT_ID_MODE_2STATE
 * ---------------------
 * Descriptor evidence:
 *   Usage Page 0x0082
 *   Usage      0x0066
 *
 * The report exists and the allowed logical values are 1..2, but the exact
 * human meaning of usage 0x0066 is not yet treated as confirmed here.
 *
 * REPORT_ID_VENDOR_FLAG / REPORT_ID_VENDOR_STATE
 * ----------------------------------------------
 * These live on vendor-defined usage page 0xFF92 and use their report IDs
 * as the short usage numbers:
 *   Usage 0x00E1
 *   Usage 0x00EC
 *
 * That naming is practical for documentation even though it is not yet
 * semantically descriptive.
 */
static const std::uint16_t USAGE_BRIGHTNESS =
    acd::hid::monitor::USAGE_BRIGHTNESS;
static const std::uint16_t USAGE_MODE_2STATE = 0x0066;
static const std::uint16_t USAGE_VENDOR_FLAG = 0x00E1;
static const std::uint16_t USAGE_VENDOR_STATE = 0x00EC;

/*
 * Payload sizes for each report, not counting the leading report-id byte.
 *
 * These come directly from the descriptor structure and from successful
 * HIDIOCGFEATURE probing.
 */
static const std::size_t REPORT_PAYLOAD_SIZE_BRIGHTNESS = 2;
static const std::size_t REPORT_PAYLOAD_SIZE_MODE_2STATE = 1;
static const std::size_t REPORT_PAYLOAD_SIZE_VENDOR_FLAG = 1;
static const std::size_t REPORT_PAYLOAD_SIZE_VENDOR_STATE = 8;

/*
 * Total buffer sizes including the report-id byte at offset 0.
 *
 * These are convenient when calling HIDIOCGFEATURE / HIDIOCSFEATURE.
 */
static const std::size_t REPORT_BUFFER_SIZE_BRIGHTNESS =
    1 + REPORT_PAYLOAD_SIZE_BRIGHTNESS;
static const std::size_t REPORT_BUFFER_SIZE_MODE_2STATE =
    1 + REPORT_PAYLOAD_SIZE_MODE_2STATE;
static const std::size_t REPORT_BUFFER_SIZE_VENDOR_FLAG =
    1 + REPORT_PAYLOAD_SIZE_VENDOR_FLAG;
static const std::size_t REPORT_BUFFER_SIZE_VENDOR_STATE =
    1 + REPORT_PAYLOAD_SIZE_VENDOR_STATE;

/*
 * Confirmed logical limits for brightness.
 *
 * These came from the HID report descriptor:
 *   Logical Minimum = 0
 *   Logical Maximum = 1023
 */
static const std::uint16_t BRIGHTNESS_MIN =
    acd::hid::monitor::BRIGHTNESS_MIN;
static const std::uint16_t BRIGHTNESS_MAX =
    acd::hid::monitor::BRIGHTNESS_MAX;

/*
 * Legal values observed for report 0x66.
 *
 * The descriptor clearly says:
 *   Logical Minimum = 1
 *   Logical Maximum = 2
 *
 * So we expose these with neutral names.
 */
static const std::uint8_t MODE_2STATE_VALUE_1 = 0x01;
static const std::uint8_t MODE_2STATE_VALUE_2 = 0x02;

/*
 * Report metadata table.
 *
 * This is useful both as documentation and as a source for future code
 * that may print report descriptions during probing or debugging.
 */
static const acd::hid::monitor::KnownReportInfo KNOWN_REPORTS[] = {
    {
        REPORT_ID_BRIGHTNESS,
        "brightness",
        "Standard brightness control on VESA virtual controls page",
        acd::hid::monitor::MEANING_CONFIRMED,
        acd::hid::monitor::USAGE_PAGE_VESA_VIRTUAL_CONTROLS,
        USAGE_BRIGHTNESS,
        REPORT_PAYLOAD_SIZE_BRIGHTNESS
    },
    {
        REPORT_ID_MODE_2STATE,
        "mode_2state",
        "Two-state 1-byte monitor control; exact meaning not yet confirmed",
        acd::hid::monitor::MEANING_UNKNOWN,
        acd::hid::monitor::USAGE_PAGE_VESA_VIRTUAL_CONTROLS,
        USAGE_MODE_2STATE,
        REPORT_PAYLOAD_SIZE_MODE_2STATE
    },
    {
        REPORT_ID_VENDOR_FLAG,
        "vendor_flag",
        "Vendor-defined 1-byte flag on Apple page 0xFF92; exact meaning unknown",
        acd::hid::monitor::MEANING_UNKNOWN,
        VENDOR_USAGE_PAGE,
        USAGE_VENDOR_FLAG,
        REPORT_PAYLOAD_SIZE_VENDOR_FLAG
    },
    {
        REPORT_ID_VENDOR_STATE,
        "vendor_state",
        "Vendor-defined report with two 32-bit values; first changes with brightness-related state",
        acd::hid::monitor::MEANING_INFERRED,
        VENDOR_USAGE_PAGE,
        USAGE_VENDOR_STATE,
        REPORT_PAYLOAD_SIZE_VENDOR_STATE
    }
};

static const std::size_t KNOWN_REPORT_COUNT =
    sizeof(KNOWN_REPORTS) / sizeof(KNOWN_REPORTS[0]);

/*
 * Wire-format notes
 * -----------------
 * HID raw report buffers are byte arrays shaped like:
 *
 *   buffer[0] = report_id
 *   buffer[1..] = payload bytes
 *
 * We intentionally DO NOT map them directly onto packed structs because:
 *   - that is fragile
 *   - it hides endian details
 *   - it tends to break silently later
 *
 * Instead we define small decoded representations and helper functions.
 */

/*
 * Decoded brightness report.
 *
 * Example raw bytes:
 *   10 2c 01  -> brightness = 300
 *   10 20 03  -> brightness = 800
 */
struct BrightnessReport {
    std::uint8_t report_id;
    std::uint16_t brightness;
};

/*
 * Decoded two-state report.
 *
 * Current observed values:
 *   0x01 or 0x02
 *
 * Exact semantic meaning remains under investigation.
 */
struct Mode2StateReport {
    std::uint8_t report_id;
    std::uint8_t value;
};

/*
 * Decoded 1-byte vendor flag report.
 *
 * Current observed baseline:
 *   0x00
 */
struct VendorFlagReport {
    std::uint8_t report_id;
    std::uint8_t value;
};

/*
 * Decoded vendor state report.
 *
 * Raw payload layout:
 *   byte 0 : report id (0xEC)
 *   bytes 1..4 : first 32-bit little-endian value
 *   bytes 5..8 : second 32-bit little-endian value
 *
 * Observed behavior so far:
 *   - first value changes when brightness is changed
 *   - second value stayed constant in the tests performed so far
 *
 * We use signed 32-bit fields because the descriptor declares a signed
 * logical range for this report.
 */
struct VendorStateReport {
    std::uint8_t report_id;
    std::int32_t value0;
    std::int32_t value1;
};

/*
 * Return true if the vendor/product pair matches this monitor model.
 */
inline bool matches_device(std::uint16_t vendor_id, std::uint16_t product_id) {
    return (vendor_id == VENDOR_ID) && (product_id == PRODUCT_ID);
}

/*
 * Return metadata for a known report ID, or NULL if not found.
 *
 * This is helpful for debug output, probe tools, and future generic
 * "describe this report" code.
 */
inline const acd::hid::monitor::KnownReportInfo* find_known_report(
    std::uint8_t report_id
) {
    std::size_t i;
    for (i = 0; i < KNOWN_REPORT_COUNT; ++i) {
        if (KNOWN_REPORTS[i].report_id == report_id) {
            return &KNOWN_REPORTS[i];
        }
    }
    return 0;
}

/*
 * Validate a logical brightness value before sending it to the monitor.
 */
inline bool brightness_value_is_valid(std::uint16_t value) {
    return acd::hid::monitor::brightness_value_is_valid(value);
}

/*
 * Convert logical brightness to a UI-friendly percent.
 */
inline double brightness_value_to_percent(std::uint16_t value) {
    return acd::hid::monitor::brightness_value_to_percent(value);
}

/*
 * Decode a raw brightness feature/input report.
 *
 * Expected buffer layout:
 *   buf[0] = 0x10
 *   buf[1] = low byte of brightness
 *   buf[2] = high byte of brightness
 *
 * Returns false if the buffer does not look like the expected report.
 */
inline bool decode_brightness_report(const std::uint8_t* buf,
                                     std::size_t len,
                                     BrightnessReport& out) {
    if (buf == 0 || len < REPORT_BUFFER_SIZE_BRIGHTNESS) {
        return false;
    }
    if (buf[0] != REPORT_ID_BRIGHTNESS) {
        return false;
    }

    out.report_id = buf[0];
    out.brightness = acd::hid::monitor::read_le_u16(&buf[1]);
    return true;
}

/*
 * Build a raw brightness feature report for sending to the monitor.
 *
 * Expected output layout:
 *   buf[0] = 0x10
 *   buf[1] = low byte
 *   buf[2] = high byte
 *
 * Returns false if the caller passes an invalid brightness value or an
 * output buffer that is too small.
 */
inline bool encode_brightness_report(std::uint16_t brightness,
                                     std::uint8_t* buf,
                                     std::size_t len) {
    if (buf == 0 || len < REPORT_BUFFER_SIZE_BRIGHTNESS) {
        return false;
    }
    if (!brightness_value_is_valid(brightness)) {
        return false;
    }

    buf[0] = REPORT_ID_BRIGHTNESS;
    acd::hid::monitor::write_le_u16(&buf[1], brightness);
    return true;
}

/*
 * Decode report 0x66.
 *
 * Expected buffer layout:
 *   buf[0] = 0x66
 *   buf[1] = 0x01 or 0x02
 */
inline bool decode_mode_2state_report(const std::uint8_t* buf,
                                      std::size_t len,
                                      Mode2StateReport& out) {
    if (buf == 0 || len < REPORT_BUFFER_SIZE_MODE_2STATE) {
        return false;
    }
    if (buf[0] != REPORT_ID_MODE_2STATE) {
        return false;
    }

    out.report_id = buf[0];
    out.value = buf[1];
    return true;
}

/*
 * Decode report 0xE1.
 *
 * Expected buffer layout:
 *   buf[0] = 0xE1
 *   buf[1] = 1-byte vendor flag
 */
inline bool decode_vendor_flag_report(const std::uint8_t* buf,
                                      std::size_t len,
                                      VendorFlagReport& out) {
    if (buf == 0 || len < REPORT_BUFFER_SIZE_VENDOR_FLAG) {
        return false;
    }
    if (buf[0] != REPORT_ID_VENDOR_FLAG) {
        return false;
    }

    out.report_id = buf[0];
    out.value = buf[1];
    return true;
}

/*
 * Decode report 0xEC.
 *
 * Expected buffer layout:
 *   buf[0]     = 0xEC
 *   buf[1..4]  = little-endian signed 32-bit value0
 *   buf[5..8]  = little-endian signed 32-bit value1
 */
inline bool decode_vendor_state_report(const std::uint8_t* buf,
                                       std::size_t len,
                                       VendorStateReport& out) {
    if (buf == 0 || len < REPORT_BUFFER_SIZE_VENDOR_STATE) {
        return false;
    }
    if (buf[0] != REPORT_ID_VENDOR_STATE) {
        return false;
    }

    out.report_id = buf[0];
    out.value0 = acd::hid::monitor::read_le_s32(&buf[1]);
    out.value1 = acd::hid::monitor::read_le_s32(&buf[5]);
    return true;
}

/*
 * Convenience check for the currently known legal values of report 0x66.
 *
 * We do not assign a stronger meaning here yet.
 */
inline bool mode_2state_value_is_valid(std::uint8_t value) {
    return value == MODE_2STATE_VALUE_1 || value == MODE_2STATE_VALUE_2;
}

} /* namespace led_cinema_27 */
} /* namespace apple */
} /* namespace acd */

#endif /* ACD_APPLE_LED_CINEMA_27_H */
