#include <linux/hiddev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <dirent.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define ACDPROBE_VERSION "0.5"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* HID usage pages used by Apple USB monitor controls. */
static const unsigned int kUsagePageUsbMonitor          = 0x0080U;
static const unsigned int kUsagePageVesaVirtualControls = 0x0082U;
static const unsigned int kUsagePageAppleVendorPrivate  = 0xff92U;

/* HID usage IDs.
 *
 * Confirmed on Apple LED Cinema Display 27-inch (05ac:9226):
 *   report 16  / usage 0x00820010 = Brightness
 *   report 102 / usage 0x00820066 = Ambient Light Sensor / auto-brightness
 *
 * Tentative labels from controlled reverse-engineering:
 *   report 225 / usage 0xff9200e1 = vendor-private boolean
 *   report 236 / usage 0xff9200ec = vendor-private status/data
 */
static const unsigned int kUsageIdMonitorControl       = 0x0001U;
static const unsigned int kUsageIdBrightness           = 0x0010U;
static const unsigned int kUsageIdAmbientLightSensor   = 0x0066U;
static const unsigned int kUsageIdVendorPrivateBool    = 0x00e1U;
static const unsigned int kUsageIdVendorPrivateStatus  = 0x00ecU;

/* Known report IDs for Apple LED Cinema Display 27-inch (05ac:9226). */
static const unsigned int kFeatureReportBrightness          = 16U;
static const unsigned int kFeatureReportAmbientLightSensor  = 102U;
static const unsigned int kFeatureReportVendorPrivateBool   = 225U;
static const unsigned int kFeatureReportVendorPrivateStatus = 236U;

struct StringEntry {
    int index;
    std::string value;
};

struct UsageInfo {
    unsigned int usage_index;
    unsigned int usage_code;
    int value;
    bool have_value;
    bool get_code_ok;
    bool get_value_ok;
};

struct FieldInfoWrap {
    hiddev_field_info info;
    std::vector<UsageInfo> usages;
};

struct ReportInfoWrap {
    hiddev_report_info info;
    std::vector<FieldInfoWrap> fields;
};

struct CandidateHint {
    std::string key;
    std::string level;
    std::string text;
};

struct FeatureSetRequest {
    bool enabled;
    unsigned int report_id;
    unsigned int field_index;
    unsigned int usage_index;
    int value;
    unsigned int readback_delay_ms;

    FeatureSetRequest()
        : enabled(false), report_id(0), field_index(0), usage_index(0), value(0), readback_delay_ms(0) {}
};

struct FeatureSetResult {
    bool attempted;
    bool success;
    std::string error;
    unsigned int report_id;
    unsigned int field_index;
    unsigned int usage_index;
    unsigned int usage_code;
    int logical_minimum;
    int logical_maximum;
    int before_value;
    int requested_value;
    int after_value;
    int delayed_value;
    bool have_before;
    bool have_after;
    bool have_delayed;

    FeatureSetResult()
        : attempted(false), success(false), report_id(0), field_index(0), usage_index(0), usage_code(0),
          logical_minimum(0), logical_maximum(0), before_value(0), requested_value(0), after_value(0),
          delayed_value(0), have_before(false), have_after(false), have_delayed(false) {}
};

struct ProbeData {
    int hid_version;
    hiddev_devinfo devinfo;
    std::string device_node;
    std::string hid_name;
    std::string hid_phys;
    std::vector<StringEntry> strings;
    std::vector<unsigned int> applications;
    std::vector<ReportInfoWrap> input_reports;
    std::vector<ReportInfoWrap> output_reports;
    std::vector<ReportInfoWrap> feature_reports;

    std::string sysfs_hiddev_realpath;
    std::string sysfs_devchar_realpath;
    std::string sysfs_device_realpath;
    std::string sysfs_report_descriptor_path;
    std::string report_descriptor_hex;

    std::string vid_pid_key;
    std::string base_filename;
};

static std::string now_utc_iso8601() {
    std::time_t now = std::time(NULL);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_now) == 0) {
        return "1970-01-01T00:00:00Z";
    }
    return buf;
}

static std::string escape_json(const std::string& s) {
    std::ostringstream out;
    for (std::string::size_type i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<unsigned int>(c)
                        << std::dec << std::setfill(' ');
                } else {
                    out << s[i];
                }
        }
    }
    return out.str();
}

static std::string csv_escape(const std::string& s) {
    bool need_quotes = false;
    for (std::string::size_type i = 0; i < s.size(); ++i) {
        if (s[i] == ',' || s[i] == '"' || s[i] == '\n' || s[i] == '\r') {
            need_quotes = true;
            break;
        }
    }

    if (!need_quotes) {
        return s;
    }

    std::ostringstream out;
    out << '"';
    for (std::string::size_type i = 0; i < s.size(); ++i) {
        if (s[i] == '"') {
            out << '"';
        }
        out << s[i];
    }
    out << '"';
    return out.str();
}

static std::string hex_u32(unsigned int value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

static std::string hex_u16(unsigned int value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(4) << std::setfill('0') << (value & 0xffffU);
    return out.str();
}

static std::string vid_pid_dirname(unsigned int vendor, unsigned int product) {
    std::ostringstream out;
    out << std::hex << std::nouppercase
        << std::setw(4) << std::setfill('0') << (vendor & 0xffffU)
        << "_"
        << std::setw(4) << std::setfill('0') << (product & 0xffffU);
    return out.str();
}

static std::string report_type_name(unsigned int report_type) {
    switch (report_type) {
        case HID_REPORT_TYPE_INPUT: return "input";
        case HID_REPORT_TYPE_OUTPUT: return "output";
        case HID_REPORT_TYPE_FEATURE: return "feature";
        default: return "unknown";
    }
}

static std::string field_flags_to_string(unsigned int flags) {
    std::ostringstream out;
    bool first = true;

    struct FlagName {
        unsigned int bit;
        const char* name;
    } flag_names[] = {
        { HID_FIELD_CONSTANT, "constant" },
        { HID_FIELD_VARIABLE, "variable" },
        { HID_FIELD_RELATIVE, "relative" },
        { HID_FIELD_WRAP, "wrap" },
        { HID_FIELD_NONLINEAR, "nonlinear" },
        { HID_FIELD_NO_PREFERRED, "no_preferred" },
        { HID_FIELD_NULL_STATE, "null_state" },
        { HID_FIELD_VOLATILE, "volatile" },
        { HID_FIELD_BUFFERED_BYTE, "buffered_byte" }
    };

    for (unsigned int i = 0; i < sizeof(flag_names) / sizeof(flag_names[0]); ++i) {
        if (flags & flag_names[i].bit) {
            if (!first) {
                out << '|';
            }
            out << flag_names[i].name;
            first = false;
        }
    }

    if (first) {
        out << "none";
    }

    return out.str();
}

static std::string basename_string(const std::string& path) {
    std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

static std::string dirname_string(const std::string& path) {
    std::string::size_type pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static std::string realpath_string(const std::string& path) {
    char buf[PATH_MAX + 1];
    if (realpath(path.c_str(), buf) == NULL) {
        return "";
    }
    return std::string(buf);
}

static bool read_binary_file(const std::string& path, std::string& out) {
    std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream tmp;
    tmp << in.rdbuf();
    out = tmp.str();
    return true;
}

static std::string bytes_to_hex(const std::string& bytes) {
    std::ostringstream out;
    for (std::string::size_type i = 0; i < bytes.size(); ++i) {
        unsigned int c = static_cast<unsigned char>(bytes[i]);
        out << std::hex << std::setw(2) << std::setfill('0') << c;
        if ((i + 1) != bytes.size()) {
            out << ' ';
        }
    }
    return out.str();
}

static bool write_text_file(const std::string& path, const std::string& content) {
    std::ofstream out(path.c_str(), std::ios::out | std::ios::binary);
    if (!out) {
        return false;
    }
    out << content;
    return out.good();
}

static bool ensure_directory_recursive(const std::string& path) {
    if (path.empty() || path == ".") {
        return true;
    }

    if (is_directory(path)) {
        return true;
    }

    std::string parent = dirname_string(path);
    if (parent != path && !ensure_directory_recursive(parent)) {
        return false;
    }

    if (::mkdir(path.c_str(), 0755) == 0) {
        return true;
    }

    if (errno == EEXIST && is_directory(path)) {
        return true;
    }

    return false;
}

static std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (a[a.size() - 1] == '/') {
        return a + b;
    }
    return a + "/" + b;
}

static std::string collect_dirname(const std::string& save_dir, const ProbeData& data) {
    return join_path(save_dir, data.vid_pid_key);
}

static bool get_hid_name(int fd, std::string& out) {
    char buf[256];
    std::memset(buf, 0, sizeof(buf));
    int rc = ioctl(fd, HIDIOCGNAME(sizeof(buf)), buf);
    if (rc < 0) {
        return false;
    }
    out = buf;
    return true;
}

static bool get_hid_phys(int fd, std::string& out) {
    char buf[256];
    std::memset(buf, 0, sizeof(buf));
    int rc = ioctl(fd, HIDIOCGPHYS(sizeof(buf)), buf);
    if (rc < 0) {
        return false;
    }
    out = buf;
    return true;
}

static std::string usage_page_name(unsigned int page) {
    switch (page) {
        case kUsagePageUsbMonitor:          return "USB Monitor";
        case kUsagePageVesaVirtualControls: return "VESA Virtual Controls";
        case kUsagePageAppleVendorPrivate:  return "Vendor Private 0xff92";
        default: return "Unknown";
    }
}

static std::string decode_usage_code(unsigned int usage_code) {
    unsigned int page = (usage_code >> 16) & 0xffffU;
    unsigned int id = usage_code & 0xffffU;

    if (page == kUsagePageUsbMonitor && id == kUsageIdMonitorControl) {
        return "Monitor Control";
    }
    if (page == kUsagePageVesaVirtualControls && id == kUsageIdBrightness) {
        return "Brightness";
    }
    if (page == kUsagePageVesaVirtualControls && id == kUsageIdAmbientLightSensor) {
        return "Ambient Light Sensor";
    }
    if (page == kUsagePageAppleVendorPrivate && id == kUsageIdVendorPrivateBool) {
        return "Vendor-private boolean (tentative)";
    }
    if (page == kUsagePageAppleVendorPrivate && id == kUsageIdVendorPrivateStatus) {
        return "Vendor-private data/status (tentative)";
    }
    if ((page & 0xff00U) == 0xff00U) {
        return "Vendor-private usage";
    }

    return "Unknown";
}

static void enumerate_strings(int fd, ProbeData& data) {
    for (int idx = 1; idx <= 16; ++idx) {
        hiddev_string_descriptor desc;
        std::memset(&desc, 0, sizeof(desc));
        desc.index = idx;
        if (ioctl(fd, HIDIOCGSTRING, &desc) >= 0) {
            if (desc.value[0] != '\0') {
                StringEntry entry;
                entry.index = idx;
                entry.value = desc.value;
                data.strings.push_back(entry);
            }
        }
    }
}

static void enumerate_applications(int fd, const hiddev_devinfo& devinfo, ProbeData& data) {
    for (__u32 i = 0; i < devinfo.num_applications; ++i) {
        int app = ioctl(fd, HIDIOCAPPLICATION, i);
        if (app >= 0) {
            data.applications.push_back(static_cast<unsigned int>(app));
        }
    }
}

static void enumerate_report_type(int fd, unsigned int report_type, std::vector<ReportInfoWrap>& out_reports) {
    hiddev_report_info rep;
    std::memset(&rep, 0, sizeof(rep));
    rep.report_type = report_type;
    rep.report_id = HID_REPORT_ID_FIRST;

    while (ioctl(fd, HIDIOCGREPORTINFO, &rep) >= 0) {
        ReportInfoWrap rep_wrap;
        std::memset(&rep_wrap.info, 0, sizeof(rep_wrap.info));
        rep_wrap.info = rep;

        for (__u32 field_index = 0; field_index < rep.num_fields; ++field_index) {
            hiddev_field_info finfo;
            std::memset(&finfo, 0, sizeof(finfo));
            finfo.report_type = rep.report_type;
            finfo.report_id = rep.report_id;
            finfo.field_index = field_index;

            if (ioctl(fd, HIDIOCGFIELDINFO, &finfo) < 0) {
                continue;
            }

            FieldInfoWrap field_wrap;
            std::memset(&field_wrap.info, 0, sizeof(field_wrap.info));
            field_wrap.info = finfo;

            for (__u32 usage_index = 0; usage_index < finfo.maxusage; ++usage_index) {
                hiddev_usage_ref uref;
                std::memset(&uref, 0, sizeof(uref));
                uref.report_type = rep.report_type;
                uref.report_id = rep.report_id;
                uref.field_index = field_index;
                uref.usage_index = usage_index;

                UsageInfo usage;
                usage.usage_index = usage_index;
                usage.usage_code = 0;
                usage.value = 0;
                usage.have_value = false;
                usage.get_code_ok = false;
                usage.get_value_ok = false;

                if (ioctl(fd, HIDIOCGUCODE, &uref) >= 0) {
                    usage.usage_code = uref.usage_code;
                    usage.get_code_ok = true;
                }

                if (ioctl(fd, HIDIOCGUSAGE, &uref) >= 0) {
                    usage.value = uref.value;
                    usage.have_value = true;
                    usage.get_value_ok = true;
                    if (!usage.get_code_ok) {
                        usage.usage_code = uref.usage_code;
                    }
                }

                field_wrap.usages.push_back(usage);
            }

            rep_wrap.fields.push_back(field_wrap);
        }

        out_reports.push_back(rep_wrap);
        rep.report_id |= HID_REPORT_ID_NEXT;
    }
}

static bool find_named_file_recursive(const std::string& base, const char* wanted_name, int depth, std::string& found) {
    if (base.empty() || depth < 0 || !is_directory(base)) {
        return false;
    }

    DIR* dir = opendir(base.c_str());
    if (!dir) {
        return false;
    }

    bool ok = false;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        const char* name = ent->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
            continue;
        }

        std::string child = base + "/" + name;

        if (std::strcmp(name, wanted_name) == 0 && file_exists(child)) {
            found = child;
            ok = true;
            break;
        }

        if (depth > 0 && is_directory(child)) {
            if (find_named_file_recursive(child, wanted_name, depth - 1, found)) {
                ok = true;
                break;
            }
        }
    }

    closedir(dir);
    return ok;
}

static std::string find_report_descriptor_path_from_base(const std::string& base) {
    if (base.empty()) {
        return "";
    }

    std::string direct = base + "/report_descriptor";
    if (file_exists(direct)) {
        return direct;
    }

    std::string found;
    if (find_named_file_recursive(base, "report_descriptor", 5, found)) {
        return found;
    }

    std::string path = base;
    for (int i = 0; i < 8; ++i) {
        std::string parent = dirname_string(path);
        if (parent == path) {
            break;
        }
        path = parent;

        std::string candidate = path + "/report_descriptor";
        if (file_exists(candidate)) {
            return candidate;
        }

        found.clear();
        if (find_named_file_recursive(path, "report_descriptor", 3, found)) {
            return found;
        }

        if (path == "/") {
            break;
        }
    }

    return "";
}

static void detect_sysfs_paths(ProbeData& data) {
    std::string node_base = basename_string(data.device_node);
    std::string hiddev_sysfs = std::string("/sys/class/hiddev/") + node_base;
    data.sysfs_hiddev_realpath = realpath_string(hiddev_sysfs);

    struct stat st;
    if (stat(data.device_node.c_str(), &st) == 0) {
        std::ostringstream devchar;
        devchar << "/sys/dev/char/" << major(st.st_rdev) << ":" << minor(st.st_rdev);
        data.sysfs_devchar_realpath = realpath_string(devchar.str());
    }

    std::string descriptor_path;

    if (!data.sysfs_hiddev_realpath.empty()) {
        descriptor_path = find_report_descriptor_path_from_base(data.sysfs_hiddev_realpath);
        std::string maybe_device = realpath_string(data.sysfs_hiddev_realpath + "/device");
        if (!maybe_device.empty()) {
            data.sysfs_device_realpath = maybe_device;
        }
    }

    if (descriptor_path.empty() && !data.sysfs_devchar_realpath.empty()) {
        descriptor_path = find_report_descriptor_path_from_base(data.sysfs_devchar_realpath);
        if (data.sysfs_device_realpath.empty()) {
            std::string maybe_device = realpath_string(data.sysfs_devchar_realpath + "/device");
            if (!maybe_device.empty()) {
                data.sysfs_device_realpath = maybe_device;
            }
        }
    }

    if (descriptor_path.empty() && !data.sysfs_device_realpath.empty()) {
        descriptor_path = find_report_descriptor_path_from_base(data.sysfs_device_realpath);
    }

    if (!descriptor_path.empty()) {
        data.sysfs_report_descriptor_path = descriptor_path;
        std::string bytes;
        if (read_binary_file(descriptor_path, bytes)) {
            data.report_descriptor_hex = bytes_to_hex(bytes);
        }
    }
}

static void build_identity_keys(ProbeData& data) {
    data.vid_pid_key = vid_pid_dirname(static_cast<unsigned int>(data.devinfo.vendor),
                                       static_cast<unsigned int>(data.devinfo.product));

    std::ostringstream base;
    base << data.vid_pid_key << "-if" << data.devinfo.ifnum;
    data.base_filename = base.str();
}

static bool refresh_report(int fd, unsigned int report_type, unsigned int report_id, std::string& error) {
    hiddev_report_info rep;
    std::memset(&rep, 0, sizeof(rep));
    rep.report_type = report_type;
    rep.report_id = report_id;

    if (ioctl(fd, HIDIOCGREPORT, &rep) < 0) {
        std::ostringstream out;
        out << "HIDIOCGREPORT failed for report_id=" << report_id << ": " << std::strerror(errno);
        error = out.str();
        return false;
    }
    return true;
}

static bool get_feature_metadata_and_value(
    int fd,
    unsigned int report_id,
    unsigned int field_index,
    unsigned int usage_index,
    hiddev_field_info& field,
    hiddev_usage_ref& usage,
    std::string& error) {

    std::string refresh_error;
    if (!refresh_report(fd, HID_REPORT_TYPE_FEATURE, report_id, refresh_error)) {
        error = refresh_error;
        return false;
    }

    std::memset(&field, 0, sizeof(field));
    field.report_type = HID_REPORT_TYPE_FEATURE;
    field.report_id = report_id;
    field.field_index = field_index;

    if (ioctl(fd, HIDIOCGFIELDINFO, &field) < 0) {
        std::ostringstream out;
        out << "HIDIOCGFIELDINFO failed for report_id=" << report_id
            << " field_index=" << field_index << ": " << std::strerror(errno);
        error = out.str();
        return false;
    }

    if (usage_index >= field.maxusage) {
        std::ostringstream out;
        out << "usage_index " << usage_index << " out of range for report_id=" << report_id
            << " field_index=" << field_index << " (maxusage=" << field.maxusage << ")";
        error = out.str();
        return false;
    }

    std::memset(&usage, 0, sizeof(usage));
    usage.report_type = HID_REPORT_TYPE_FEATURE;
    usage.report_id = report_id;
    usage.field_index = field_index;
    usage.usage_index = usage_index;

    if (ioctl(fd, HIDIOCGUCODE, &usage) < 0) {
        std::ostringstream out;
        out << "HIDIOCGUCODE failed for report_id=" << report_id
            << " field_index=" << field_index << " usage_index=" << usage_index
            << ": " << std::strerror(errno);
        error = out.str();
        return false;
    }

    if (ioctl(fd, HIDIOCGUSAGE, &usage) < 0) {
        std::ostringstream out;
        out << "HIDIOCGUSAGE failed for report_id=" << report_id
            << " field_index=" << field_index << " usage_index=" << usage_index
            << ": " << std::strerror(errno);
        error = out.str();
        return false;
    }

    return true;
}

static bool perform_feature_set(const std::string& device_node, const FeatureSetRequest& req, FeatureSetResult& result) {
    result.attempted = true;
    result.report_id = req.report_id;
    result.field_index = req.field_index;
    result.usage_index = req.usage_index;
    result.requested_value = req.value;

    int fd = open(device_node.c_str(), O_RDWR);
    if (fd < 0) {
        std::ostringstream out;
        out << "open(" << device_node << ") failed: " << std::strerror(errno);
        result.error = out.str();
        return false;
    }

    hiddev_field_info field;
    hiddev_usage_ref usage;
    std::string error;

    if (!get_feature_metadata_and_value(fd, req.report_id, req.field_index, req.usage_index, field, usage, error)) {
        close(fd);
        result.error = error;
        return false;
    }

    result.usage_code = usage.usage_code;
    result.logical_minimum = field.logical_minimum;
    result.logical_maximum = field.logical_maximum;
    result.before_value = usage.value;
    result.have_before = true;

    if (req.value < field.logical_minimum || req.value > field.logical_maximum) {
        std::ostringstream out;
        out << "requested value " << req.value << " is outside logical range "
            << field.logical_minimum << ".." << field.logical_maximum
            << " for usage " << hex_u32(usage.usage_code);
        close(fd);
        result.error = out.str();
        return false;
    }

    usage.value = req.value;
    if (ioctl(fd, HIDIOCSUSAGE, &usage) < 0) {
        std::ostringstream out;
        out << "HIDIOCSUSAGE failed for report_id=" << req.report_id
            << " field_index=" << req.field_index << " usage_index=" << req.usage_index
            << ": " << std::strerror(errno);
        close(fd);
        result.error = out.str();
        return false;
    }

    hiddev_report_info rep;
    std::memset(&rep, 0, sizeof(rep));
    rep.report_type = HID_REPORT_TYPE_FEATURE;
    rep.report_id = req.report_id;

    if (ioctl(fd, HIDIOCSREPORT, &rep) < 0) {
        std::ostringstream out;
        out << "HIDIOCSREPORT failed for report_id=" << req.report_id
            << ": " << std::strerror(errno);
        close(fd);
        result.error = out.str();
        return false;
    }

    if (!get_feature_metadata_and_value(fd, req.report_id, req.field_index, req.usage_index, field, usage, error)) {
        close(fd);
        result.error = std::string("write succeeded but immediate readback failed: ") + error;
        return false;
    }

    result.after_value = usage.value;
    result.have_after = true;

    if (req.readback_delay_ms > 0) {
        usleep(req.readback_delay_ms * 1000U);
        if (!get_feature_metadata_and_value(fd, req.report_id, req.field_index, req.usage_index, field, usage, error)) {
            close(fd);
            result.error = std::string("delayed readback failed: ") + error;
            return false;
        }
        result.delayed_value = usage.value;
        result.have_delayed = true;
    }

    close(fd);
    result.success = true;
    return true;
}

static bool probe_device(const std::string& device_node, ProbeData& data, std::string& error) {
    data.device_node = device_node;

    int fd = open(device_node.c_str(), O_RDWR);
    if (fd < 0) {
        std::ostringstream out;
        out << "open(" << device_node << ") failed: " << std::strerror(errno);
        error = out.str();
        return false;
    }

    std::memset(&data.devinfo, 0, sizeof(data.devinfo));

    if (ioctl(fd, HIDIOCGVERSION, &data.hid_version) < 0) {
        error = std::string("HIDIOCGVERSION failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    if (ioctl(fd, HIDIOCGDEVINFO, &data.devinfo) < 0) {
        error = std::string("HIDIOCGDEVINFO failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    if (ioctl(fd, HIDIOCINITREPORT, 0) < 0) {
        error = std::string("HIDIOCINITREPORT failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    get_hid_name(fd, data.hid_name);
    get_hid_phys(fd, data.hid_phys);
    enumerate_strings(fd, data);
    enumerate_applications(fd, data.devinfo, data);

    enumerate_report_type(fd, HID_REPORT_TYPE_INPUT, data.input_reports);
    enumerate_report_type(fd, HID_REPORT_TYPE_OUTPUT, data.output_reports);
    enumerate_report_type(fd, HID_REPORT_TYPE_FEATURE, data.feature_reports);

    close(fd);

    detect_sysfs_paths(data);
    build_identity_keys(data);
    return true;
}

static std::vector<CandidateHint> collect_candidate_hints(const ProbeData& data) {
    std::vector<CandidateHint> hints;
    std::vector<std::string> seen_keys;

    for (std::vector<ReportInfoWrap>::const_iterator r = data.feature_reports.begin();
         r != data.feature_reports.end(); ++r) {
        for (std::vector<FieldInfoWrap>::const_iterator f = r->fields.begin();
             f != r->fields.end(); ++f) {
            for (std::vector<UsageInfo>::const_iterator u = f->usages.begin();
                 u != f->usages.end(); ++u) {
                unsigned int page = (u->usage_code >> 16) & 0xffffU;
                unsigned int id = u->usage_code & 0xffffU;

                CandidateHint hint;
                bool add = false;

                if (page == kUsagePageVesaVirtualControls && id == kUsageIdBrightness) {
                    hint.key = std::string("known:") + hex_u32(u->usage_code) + ":" + hex_u32(r->info.report_id);
                    hint.level = "known";
                    std::ostringstream t;
                    t << "feature report " << r->info.report_id
                      << " usage " << hex_u32(u->usage_code)
                      << " is the known brightness path; current value=";
                    if (u->have_value) {
                        t << u->value;
                    } else {
                        t << "<unavailable>";
                    }
                    hint.text = t.str();
                    add = true;
                } else if (page == kUsagePageVesaVirtualControls && id == kUsageIdAmbientLightSensor) {
                    hint.key = std::string("known:") + hex_u32(u->usage_code) + ":" + hex_u32(r->info.report_id);
                    hint.level = "known";
                    std::ostringstream t;
                    t << "feature report " << r->info.report_id
                      << " usage " << hex_u32(u->usage_code)
                      << " is the confirmed ambient-light-sensor / auto-brightness control"
                      << " (logical range " << f->info.logical_minimum
                      << ".." << f->info.logical_maximum << "); current value=";
                    if (u->have_value) {
                        t << u->value;
                    } else {
                        t << "<unavailable>";
                    }
                    t << ".";
                    hint.text = t.str();
                    add = true;
                } else if (page == kUsagePageAppleVendorPrivate && id == kUsageIdVendorPrivateBool) {
                    hint.key = std::string("candidate:") + hex_u32(u->usage_code) + ":" + hex_u32(r->info.report_id);
                    hint.level = "candidate";
                    std::ostringstream t;
                    t << "feature report " << r->info.report_id
                      << " usage " << hex_u32(u->usage_code)
                      << " is a tentative vendor-private boolean"
                      << " (logical range " << f->info.logical_minimum
                      << ".." << f->info.logical_maximum << "); current value=";
                    if (u->have_value) {
                        t << u->value;
                    } else {
                        t << "<unavailable>";
                    }
                    t << ". Controlled tests suggest it is not the primary auto-brightness toggle.";
                    hint.text = t.str();
                    add = true;
                } else if (page == kUsagePageAppleVendorPrivate && id == kUsageIdVendorPrivateStatus) {
                    std::ostringstream key;
                    key << "note:" << hex_u32(u->usage_code) << ":" << hex_u32(r->info.report_id);
                    hint.key = key.str();
                    hint.level = "note";
                    std::ostringstream t;
                    t << "feature report " << r->info.report_id
                      << " usage " << hex_u32(u->usage_code)
                      << " is tentative vendor-private data/status under investigation.";
                    hint.text = t.str();
                    add = true;
                } else if (page == kUsagePageAppleVendorPrivate) {
                    std::ostringstream key;
                    key << "note:" << hex_u32(u->usage_code) << ":" << hex_u32(r->info.report_id);
                    hint.key = key.str();
                    hint.level = "note";
                    std::ostringstream t;
                    t << "feature report " << r->info.report_id
                      << " usage " << hex_u32(u->usage_code)
                      << " is vendor-private and currently unresolved.";
                    hint.text = t.str();
                    add = true;
                }

                if (add) {
                    bool seen = false;
                    for (std::vector<std::string>::const_iterator it = seen_keys.begin();
                         it != seen_keys.end(); ++it) {
                        if (*it == hint.key) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) {
                        seen_keys.push_back(hint.key);
                        hints.push_back(hint);
                    }
                }
            }
        }
    }

    return hints;
}

static void append_feature_set_text(std::ostream& out, const FeatureSetResult& set_result) {
    if (!set_result.attempted) {
        return;
    }

    out << "feature_write:\n";
    out << "  success: " << (set_result.success ? "true" : "false") << "\n";
    out << "  report_id: " << set_result.report_id << "\n";
    out << "  field_index: " << set_result.field_index << "\n";
    out << "  usage_index: " << set_result.usage_index << "\n";
    out << "  usage_code: " << hex_u32(set_result.usage_code)
        << " (" << decode_usage_code(set_result.usage_code) << ")\n";
    out << "  logical_range: " << set_result.logical_minimum << ".." << set_result.logical_maximum << "\n";
    if (set_result.have_before) {
        out << "  before_value: " << set_result.before_value << "\n";
    }
    out << "  requested_value: " << set_result.requested_value << "\n";
    if (set_result.have_after) {
        out << "  after_value: " << set_result.after_value << "\n";
    }
    if (set_result.have_delayed) {
        out << "  delayed_value: " << set_result.delayed_value << "\n";
    }
    if (!set_result.error.empty()) {
        out << "  error: " << set_result.error << "\n";
    }
}

static void append_reports_text(std::ostream& out, const char* title, const std::vector<ReportInfoWrap>& reports) {
    out << title << " reports: " << reports.size() << "\n";
    for (std::vector<ReportInfoWrap>::size_type r = 0; r < reports.size(); ++r) {
        const ReportInfoWrap& rep = reports[r];
        out << "  report_id=" << rep.info.report_id
            << " num_fields=" << rep.info.num_fields
            << " type=" << report_type_name(rep.info.report_type) << "\n";

        for (std::vector<FieldInfoWrap>::size_type f = 0; f < rep.fields.size(); ++f) {
            const FieldInfoWrap& field = rep.fields[f];
            out << "    field_index=" << field.info.field_index
                << " maxusage=" << field.info.maxusage
                << " flags=" << field_flags_to_string(field.info.flags)
                << " application=" << hex_u32(field.info.application)
                << " (" << decode_usage_code(field.info.application) << ")"
                << " logical=" << hex_u32(field.info.logical)
                << " physical=" << hex_u32(field.info.physical)
                << " logical_range=" << field.info.logical_minimum << ".." << field.info.logical_maximum
                << " physical_range=" << field.info.physical_minimum << ".." << field.info.physical_maximum
                << " unit_exponent=" << field.info.unit_exponent
                << " unit=" << field.info.unit
                << "\n";

            for (std::vector<UsageInfo>::size_type u = 0; u < field.usages.size(); ++u) {
                const UsageInfo& usage = field.usages[u];
                unsigned int page = (usage.usage_code >> 16) & 0xffffU;
                out << "      usage_index=" << usage.usage_index
                    << " usage_code=" << hex_u32(usage.usage_code)
                    << " page=" << hex_u16(page)
                    << " (" << usage_page_name(page) << ")"
                    << " decoded=\"" << decode_usage_code(usage.usage_code) << "\"";

                if (usage.have_value) {
                    out << " value=" << usage.value;
                } else {
                    out << " value=<unavailable>";
                }

                if (!usage.get_code_ok || !usage.get_value_ok) {
                    out << " status=";
                    if (!usage.get_code_ok && !usage.get_value_ok) {
                        out << "code+value-unavailable";
                    } else if (!usage.get_code_ok) {
                        out << "code-unavailable";
                    } else {
                        out << "value-unavailable";
                    }
                }

                out << "\n";
            }
        }
    }
}

static std::string build_text_report(const ProbeData& data, const FeatureSetResult& set_result) {
    std::ostringstream out;
    std::vector<CandidateHint> hints = collect_candidate_hints(data);

    out << "acdprobe text report\n";
    out << "timestamp_utc: " << now_utc_iso8601() << "\n";
    out << "device_node: " << data.device_node << "\n";
    out << "hid_version: 0x" << std::hex << data.hid_version << std::dec << "\n";
    out << "vendor: " << hex_u16(static_cast<unsigned int>(data.devinfo.vendor)) << "\n";
    out << "product: " << hex_u16(static_cast<unsigned int>(data.devinfo.product)) << "\n";
    out << "version: " << hex_u16(static_cast<unsigned int>(data.devinfo.version)) << "\n";
    out << "busnum: " << data.devinfo.busnum << " devnum: " << data.devinfo.devnum
        << " ifnum: " << data.devinfo.ifnum << "\n";
    out << "num_applications: " << data.devinfo.num_applications << "\n";
    out << "vid_pid_key: " << data.vid_pid_key << "\n";
    out << "base_filename: " << data.base_filename << "\n";
    out << "hid_name: " << (data.hid_name.empty() ? std::string("<unavailable>") : data.hid_name) << "\n";
    out << "hid_phys: " << (data.hid_phys.empty() ? std::string("<unavailable>") : data.hid_phys) << "\n";

    append_feature_set_text(out, set_result);

    out << "strings:\n";
    if (data.strings.empty()) {
        out << "  <none>\n";
    } else {
        for (std::vector<StringEntry>::size_type i = 0; i < data.strings.size(); ++i) {
            out << "  [" << data.strings[i].index << "] " << data.strings[i].value << "\n";
        }
    }

    out << "applications:\n";
    if (data.applications.empty()) {
        out << "  <none>\n";
    } else {
        for (std::vector<unsigned int>::size_type i = 0; i < data.applications.size(); ++i) {
            unsigned int app = data.applications[i];
            unsigned int page = (app >> 16) & 0xffffU;
            out << "  [" << i << "] " << hex_u32(app)
                << " page=" << hex_u16(page)
                << " (" << usage_page_name(page) << ")"
                << " decoded=\"" << decode_usage_code(app) << "\"\n";
        }
    }

    out << "sysfs_hiddev_realpath: "
        << (data.sysfs_hiddev_realpath.empty() ? std::string("<unavailable>") : data.sysfs_hiddev_realpath) << "\n";
    out << "sysfs_devchar_realpath: "
        << (data.sysfs_devchar_realpath.empty() ? std::string("<unavailable>") : data.sysfs_devchar_realpath) << "\n";
    out << "sysfs_device_realpath: "
        << (data.sysfs_device_realpath.empty() ? std::string("<unavailable>") : data.sysfs_device_realpath) << "\n";
    out << "sysfs_report_descriptor_path: "
        << (data.sysfs_report_descriptor_path.empty() ? std::string("<unavailable>") : data.sysfs_report_descriptor_path) << "\n";

    if (!data.report_descriptor_hex.empty()) {
        out << "report_descriptor_hex: " << data.report_descriptor_hex << "\n";
    }

    out << "decoded_hints:\n";
    if (hints.empty()) {
        out << "  <none>\n";
    } else {
        for (std::vector<CandidateHint>::size_type i = 0; i < hints.size(); ++i) {
            out << "  [" << hints[i].level << "] " << hints[i].text << "\n";
        }
    }

    append_reports_text(out, "input", data.input_reports);
    append_reports_text(out, "output", data.output_reports);
    append_reports_text(out, "feature", data.feature_reports);

    return out.str();
}

static void append_reports_json(std::ostream& out, const std::vector<ReportInfoWrap>& reports, int indent) {
    std::string pad(indent, ' ');

    for (std::vector<ReportInfoWrap>::size_type r = 0; r < reports.size(); ++r) {
        const ReportInfoWrap& rep = reports[r];
        out << pad << "{\n";
        out << pad << "  \"report_id\": " << rep.info.report_id << ",\n";
        out << pad << "  \"report_type\": \"" << report_type_name(rep.info.report_type) << "\",\n";
        out << pad << "  \"num_fields\": " << rep.info.num_fields << ",\n";
        out << pad << "  \"fields\": [\n";

        for (std::vector<FieldInfoWrap>::size_type f = 0; f < rep.fields.size(); ++f) {
            const FieldInfoWrap& field = rep.fields[f];
            out << pad << "    {\n";
            out << pad << "      \"field_index\": " << field.info.field_index << ",\n";
            out << pad << "      \"maxusage\": " << field.info.maxusage << ",\n";
            out << pad << "      \"flags\": " << field.info.flags << ",\n";
            out << pad << "      \"flags_text\": \"" << escape_json(field_flags_to_string(field.info.flags)) << "\",\n";
            out << pad << "      \"application\": \"" << hex_u32(field.info.application) << "\",\n";
            out << pad << "      \"application_decoded\": \"" << escape_json(decode_usage_code(field.info.application)) << "\",\n";
            out << pad << "      \"logical\": \"" << hex_u32(field.info.logical) << "\",\n";
            out << pad << "      \"physical\": \"" << hex_u32(field.info.physical) << "\",\n";
            out << pad << "      \"logical_minimum\": " << field.info.logical_minimum << ",\n";
            out << pad << "      \"logical_maximum\": " << field.info.logical_maximum << ",\n";
            out << pad << "      \"physical_minimum\": " << field.info.physical_minimum << ",\n";
            out << pad << "      \"physical_maximum\": " << field.info.physical_maximum << ",\n";
            out << pad << "      \"unit_exponent\": " << field.info.unit_exponent << ",\n";
            out << pad << "      \"unit\": " << field.info.unit << ",\n";
            out << pad << "      \"usages\": [\n";

            for (std::vector<UsageInfo>::size_type u = 0; u < field.usages.size(); ++u) {
                const UsageInfo& usage = field.usages[u];
                unsigned int page = (usage.usage_code >> 16) & 0xffffU;
                out << pad << "        {\n";
                out << pad << "          \"usage_index\": " << usage.usage_index << ",\n";
                out << pad << "          \"usage_code\": \"" << hex_u32(usage.usage_code) << "\",\n";
                out << pad << "          \"usage_page\": \"" << hex_u16(page) << "\",\n";
                out << pad << "          \"usage_page_name\": \"" << escape_json(usage_page_name(page)) << "\",\n";
                out << pad << "          \"usage_decoded\": \"" << escape_json(decode_usage_code(usage.usage_code)) << "\",\n";
                out << pad << "          \"get_code_ok\": " << (usage.get_code_ok ? "true" : "false") << ",\n";
                out << pad << "          \"get_value_ok\": " << (usage.get_value_ok ? "true" : "false") << ",\n";
                if (usage.have_value) {
                    out << pad << "          \"value\": " << usage.value << "\n";
                } else {
                    out << pad << "          \"value\": null\n";
                }
                out << pad << "        }";
                if ((u + 1) != field.usages.size()) {
                    out << ",";
                }
                out << "\n";
            }

            out << pad << "      ]\n";
            out << pad << "    }";
            if ((f + 1) != rep.fields.size()) {
                out << ",";
            }
            out << "\n";
        }

        out << pad << "  ]\n";
        out << pad << "}";
        if ((r + 1) != reports.size()) {
            out << ",";
        }
        out << "\n";
    }
}

static std::string build_json_report(const ProbeData& data, const FeatureSetResult& set_result) {
    std::ostringstream out;
    std::vector<CandidateHint> hints = collect_candidate_hints(data);

    out << "{\n";
    out << "  \"schema\": \"urn:acdcontrol:schema:probe:raw:v1\",\n";
    out << "  \"timestamp_utc\": \"" << now_utc_iso8601() << "\",\n";
    out << "  \"probe_tool\": {\n";
    out << "    \"name\": \"acdprobe\",\n";
    out << "    \"version\": \"" << ACDPROBE_VERSION << "\"\n";
    out << "  },\n";

    out << "  \"submission\": {\n";
    out << "    \"vid_pid_key\": \"" << escape_json(data.vid_pid_key) << "\",\n";
    out << "    \"base_filename\": \"" << escape_json(data.base_filename) << "\"\n";
    out << "  },\n";

    out << "  \"target\": {\n";
    out << "    \"device_node\": \"" << escape_json(data.device_node) << "\",\n";
    out << "    \"usb_vendor_id\": \"" << hex_u16(static_cast<unsigned int>(data.devinfo.vendor)) << "\",\n";
    out << "    \"usb_product_id\": \"" << hex_u16(static_cast<unsigned int>(data.devinfo.product)) << "\",\n";
    out << "    \"usb_version\": \"" << hex_u16(static_cast<unsigned int>(data.devinfo.version)) << "\",\n";
    out << "    \"busnum\": " << data.devinfo.busnum << ",\n";
    out << "    \"devnum\": " << data.devinfo.devnum << ",\n";
    out << "    \"ifnum\": " << data.devinfo.ifnum << "\n";
    out << "  },\n";

    out << "  \"identity\": {\n";
    out << "    \"hid_name\": \"" << escape_json(data.hid_name) << "\",\n";
    out << "    \"hid_phys\": \"" << escape_json(data.hid_phys) << "\",\n";
    out << "    \"strings\": [\n";
    for (std::vector<StringEntry>::size_type i = 0; i < data.strings.size(); ++i) {
        out << "      { \"index\": " << data.strings[i].index
            << ", \"value\": \"" << escape_json(data.strings[i].value) << "\" }";
        if ((i + 1) != data.strings.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";

    out << "  \"feature_write\": {\n";
    out << "    \"attempted\": " << (set_result.attempted ? "true" : "false") << ",\n";
    out << "    \"success\": " << (set_result.success ? "true" : "false") << ",\n";
    out << "    \"report_id\": " << set_result.report_id << ",\n";
    out << "    \"field_index\": " << set_result.field_index << ",\n";
    out << "    \"usage_index\": " << set_result.usage_index << ",\n";
    out << "    \"usage_code\": \"" << hex_u32(set_result.usage_code) << "\",\n";
    out << "    \"usage_decoded\": \"" << escape_json(decode_usage_code(set_result.usage_code)) << "\",\n";
    out << "    \"logical_minimum\": " << set_result.logical_minimum << ",\n";
    out << "    \"logical_maximum\": " << set_result.logical_maximum << ",\n";
    if (set_result.have_before) {
        out << "    \"before_value\": " << set_result.before_value << ",\n";
    } else {
        out << "    \"before_value\": null,\n";
    }
    out << "    \"requested_value\": " << set_result.requested_value << ",\n";
    if (set_result.have_after) {
        out << "    \"after_value\": " << set_result.after_value << ",\n";
    } else {
        out << "    \"after_value\": null,\n";
    }
    if (set_result.have_delayed) {
        out << "    \"delayed_value\": " << set_result.delayed_value << ",\n";
    } else {
        out << "    \"delayed_value\": null,\n";
    }
    out << "    \"error\": \"" << escape_json(set_result.error) << "\"\n";
    out << "  },\n";

    out << "  \"applications\": [\n";
    for (std::vector<unsigned int>::size_type i = 0; i < data.applications.size(); ++i) {
        unsigned int app = data.applications[i];
        unsigned int page = (app >> 16) & 0xffffU;
        out << "    { \"index\": " << i
            << ", \"usage\": \"" << hex_u32(app) << "\""
            << ", \"usage_page\": \"" << hex_u16(page) << "\""
            << ", \"usage_page_name\": \"" << escape_json(usage_page_name(page)) << "\""
            << ", \"decoded\": \"" << escape_json(decode_usage_code(app)) << "\""
            << " }";
        if ((i + 1) != data.applications.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"sysfs\": {\n";
    out << "    \"hiddev_realpath\": \"" << escape_json(data.sysfs_hiddev_realpath) << "\",\n";
    out << "    \"devchar_realpath\": \"" << escape_json(data.sysfs_devchar_realpath) << "\",\n";
    out << "    \"device_realpath\": \"" << escape_json(data.sysfs_device_realpath) << "\",\n";
    out << "    \"report_descriptor_path\": \"" << escape_json(data.sysfs_report_descriptor_path) << "\",\n";
    out << "    \"report_descriptor_hex\": \"" << escape_json(data.report_descriptor_hex) << "\"\n";
    out << "  },\n";

    out << "  \"decoded_hints\": [\n";
    for (std::vector<CandidateHint>::size_type i = 0; i < hints.size(); ++i) {
        out << "    { \"level\": \"" << escape_json(hints[i].level)
            << "\", \"text\": \"" << escape_json(hints[i].text) << "\" }";
        if ((i + 1) != hints.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"reports\": {\n";
    out << "    \"input\": [\n";
    append_reports_json(out, data.input_reports, 6);
    out << "    ],\n";
    out << "    \"output\": [\n";
    append_reports_json(out, data.output_reports, 6);
    out << "    ],\n";
    out << "    \"feature\": [\n";
    append_reports_json(out, data.feature_reports, 6);
    out << "    ]\n";
    out << "  }\n";

    out << "}\n";
    return out.str();
}

static std::string build_csv_report(const ProbeData& data) {
    std::ostringstream out;

    out << "device_node,vendor_id,product_id,ifnum,report_type,report_id,field_index,usage_index,usage_code,usage_page,usage_page_name,usage_decoded,value,get_code_ok,get_value_ok,application,application_decoded,logical,physical,logical_minimum,logical_maximum,physical_minimum,physical_maximum,flags,flags_text\n";

    const std::vector<ReportInfoWrap>* sets[] = {
        &data.input_reports,
        &data.output_reports,
        &data.feature_reports
    };

    for (unsigned int s = 0; s < 3; ++s) {
        const std::vector<ReportInfoWrap>& reports = *sets[s];
        for (std::vector<ReportInfoWrap>::size_type r = 0; r < reports.size(); ++r) {
            const ReportInfoWrap& rep = reports[r];
            for (std::vector<FieldInfoWrap>::size_type f = 0; f < rep.fields.size(); ++f) {
                const FieldInfoWrap& field = rep.fields[f];
                for (std::vector<UsageInfo>::size_type u = 0; u < field.usages.size(); ++u) {
                    const UsageInfo& usage = field.usages[u];
                    unsigned int page = (usage.usage_code >> 16) & 0xffffU;

                    out << csv_escape(data.device_node) << ','
                        << csv_escape(hex_u16(static_cast<unsigned int>(data.devinfo.vendor))) << ','
                        << csv_escape(hex_u16(static_cast<unsigned int>(data.devinfo.product))) << ','
                        << data.devinfo.ifnum << ','
                        << csv_escape(report_type_name(rep.info.report_type)) << ','
                        << rep.info.report_id << ','
                        << field.info.field_index << ','
                        << usage.usage_index << ','
                        << csv_escape(hex_u32(usage.usage_code)) << ','
                        << csv_escape(hex_u16(page)) << ','
                        << csv_escape(usage_page_name(page)) << ','
                        << csv_escape(decode_usage_code(usage.usage_code)) << ',';

                    if (usage.have_value) {
                        out << usage.value;
                    }

                    out << ','
                        << (usage.get_code_ok ? "true" : "false") << ','
                        << (usage.get_value_ok ? "true" : "false") << ','
                        << csv_escape(hex_u32(field.info.application)) << ','
                        << csv_escape(decode_usage_code(field.info.application)) << ','
                        << csv_escape(hex_u32(field.info.logical)) << ','
                        << csv_escape(hex_u32(field.info.physical)) << ','
                        << field.info.logical_minimum << ','
                        << field.info.logical_maximum << ','
                        << field.info.physical_minimum << ','
                        << field.info.physical_maximum << ','
                        << field.info.flags << ','
                        << csv_escape(field_flags_to_string(field.info.flags))
                        << '\n';
                }
            }
        }
    }

    return out.str();
}

static std::string usage_summary(const ProbeData& data) {
    std::ostringstream out;
    out << "Summary: "
        << data.input_reports.size() << " input reports, "
        << data.output_reports.size() << " output reports, "
        << data.feature_reports.size() << " feature reports";
    return out.str();
}

static void print_help(const char* argv0) {
    std::cout
        << "acdprobe " << ACDPROBE_VERSION << "\n"
        << "Usage:\n"
        << "  " << argv0 << " /dev/acdctl4\n"
        << "  " << argv0 << " /dev/acdctl4 --no-save\n"
        << "  " << argv0 << " --collect /dev/acdctl4\n"
        << "  " << argv0 << " /dev/acdctl4 --set-feature "
        << kFeatureReportAmbientLightSensor << " 0 0 2\n"
        << "  " << argv0 << " /dev/acdctl4 --set-feature "
        << kFeatureReportAmbientLightSensor << " 0 0 2 --readback-delay-ms 500\n\n"
        << "Default save layout:\n"
        << "  probes/raw/VID_PID/VID_PID-ifN.json\n"
        << "  probes/raw/VID_PID/VID_PID-ifN.txt\n"
        << "  probes/raw/VID_PID/VID_PID-ifN.csv\n"
        << "  probes/raw/VID_PID/VID_PID-ifN.rdesc.hex\n\n"
        << "Options:\n"
        << "  --save-dir DIR            Base raw output directory (default: probes/raw)\n"
        << "  --collect                 Save a standardized report bundle under probes/VID_PID/\n"
        << "  --json PATH               Save full JSON report\n"
        << "  --text PATH               Save human-readable text report\n"
        << "  --csv PATH                Save flat CSV of all usages\n"
        << "  --descriptor PATH         Save raw report descriptor hex dump\n"
        << "  --set-feature R F U V     Set one feature usage and read it back\n"
        << "  --readback-delay-ms N     Optional delayed readback after write\n"
        << "  --no-save                 Print only; do not auto-save files\n"
        << "  --quiet                   Do not print the text report to stdout\n"
        << "  --help                    Show this help\n";
}

static bool parse_uint_arg(const std::string& s, unsigned int& out) {
    char* end = NULL;
    errno = 0;
    unsigned long value = std::strtoul(s.c_str(), &end, 0);
    if (errno != 0 || end == NULL || *end != '\0') {
        return false;
    }
    out = static_cast<unsigned int>(value);
    return true;
}

static bool parse_int_arg(const std::string& s, int& out) {
    char* end = NULL;
    errno = 0;
    long value = std::strtol(s.c_str(), &end, 0);
    if (errno != 0 || end == NULL || *end != '\0') {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

int main(int argc, char** argv) {
    std::string device_node;
    std::string save_dir = "probes/raw";
    std::string json_path;
    std::string text_path;
    std::string csv_path;
    std::string descriptor_path;
    bool no_save = false;
    bool collect_mode = false;
    bool quiet = false;
    FeatureSetRequest set_request;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--save-dir" && i + 1 < argc) {
            save_dir = argv[++i];
        } else if (arg == "--collect") {
            collect_mode = true;
        } else if (arg == "--json" && i + 1 < argc) {
            json_path = argv[++i];
        } else if (arg == "--text" && i + 1 < argc) {
            text_path = argv[++i];
        } else if (arg == "--csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--descriptor" && i + 1 < argc) {
            descriptor_path = argv[++i];
        } else if (arg == "--set-feature" && i + 4 < argc) {
            set_request.enabled = true;
            if (!parse_uint_arg(argv[++i], set_request.report_id) ||
                !parse_uint_arg(argv[++i], set_request.field_index) ||
                !parse_uint_arg(argv[++i], set_request.usage_index) ||
                !parse_int_arg(argv[++i], set_request.value)) {
                std::cerr << "acdprobe: invalid arguments for --set-feature\n";
                return 2;
            }
        } else if (arg == "--readback-delay-ms" && i + 1 < argc) {
            unsigned int delay_ms = 0;
            if (!parse_uint_arg(argv[++i], delay_ms)) {
                std::cerr << "acdprobe: invalid value for --readback-delay-ms\n";
                return 2;
            }
            set_request.readback_delay_ms = delay_ms;
        } else if (arg == "--no-save") {
            no_save = true;
        } else if (arg == "--quiet" || arg == "-q") {
            quiet = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            print_help(argv[0]);
            return 2;
        } else if (device_node.empty()) {
            device_node = arg;
        } else {
            std::cerr << "Unexpected argument: " << arg << "\n";
            print_help(argv[0]);
            return 2;
        }
    }

    if (device_node.empty()) {
        print_help(argv[0]);
        return 2;
    }

    FeatureSetResult set_result;
    if (set_request.enabled) {
        if (!perform_feature_set(device_node, set_request, set_result)) {
            std::cerr << "acdprobe: feature write failed: " << set_result.error << "\n";
            return 1;
        }
    }

    ProbeData data;
    std::string error;

    if (!probe_device(device_node, data, error)) {
        std::cerr << "acdprobe: " << error << "\n";
        return 1;
    }

    if (!no_save) {
        std::string default_dir = join_path(save_dir, data.vid_pid_key);
        std::string output_dir = collect_mode ? collect_dirname(save_dir, data) : default_dir;

        if (!ensure_directory_recursive(output_dir)) {
            std::cerr << "acdprobe: failed to create output directory: " << output_dir << "\n";
            return 1;
        }

        if (collect_mode) {
            if (json_path.empty()) {
                json_path = join_path(output_dir, "report.json");
            }
            if (text_path.empty()) {
                text_path = join_path(output_dir, "report.txt");
            }
            if (csv_path.empty()) {
                csv_path = join_path(output_dir, "report.csv");
            }
            if (descriptor_path.empty()) {
                descriptor_path = join_path(output_dir, "report_descriptor.hex");
            }
        } else {
            if (json_path.empty()) {
                json_path = join_path(output_dir, data.base_filename + ".json");
            }
            if (text_path.empty()) {
                text_path = join_path(output_dir, data.base_filename + ".txt");
            }
            if (csv_path.empty()) {
                csv_path = join_path(output_dir, data.base_filename + ".csv");
            }
            if (descriptor_path.empty()) {
                descriptor_path = join_path(output_dir, data.base_filename + ".rdesc.hex");
            }
        }
    }

    std::string text_report = build_text_report(data, set_result);
    std::string json_report = build_json_report(data, set_result);
    std::string csv_report = build_csv_report(data);

    if (!quiet) {
        std::cout << text_report;
        std::cout << usage_summary(data) << "\n";
    }

    if (!json_path.empty() && !write_text_file(json_path, json_report)) {
        std::cerr << "acdprobe: failed to write JSON file: " << json_path << "\n";
        return 1;
    }

    if (!text_path.empty() && !write_text_file(text_path, text_report)) {
        std::cerr << "acdprobe: failed to write text file: " << text_path << "\n";
        return 1;
    }

    if (!csv_path.empty() && !write_text_file(csv_path, csv_report)) {
        std::cerr << "acdprobe: failed to write CSV file: " << csv_path << "\n";
        return 1;
    }

    if (!descriptor_path.empty()) {
        if (data.report_descriptor_hex.empty()) {
            std::cerr << "acdprobe: report descriptor was not found in sysfs; skipping "
                      << descriptor_path << "\n";
        } else if (!write_text_file(descriptor_path, data.report_descriptor_hex + "\n")) {
            std::cerr << "acdprobe: failed to write descriptor file: " << descriptor_path << "\n";
            return 1;
        }
    }

    if (!json_path.empty()) {
        std::cout << "Saved JSON: " << json_path << "\n";
    }
    if (!text_path.empty()) {
        std::cout << "Saved text: " << text_path << "\n";
    }
    if (!csv_path.empty()) {
        std::cout << "Saved CSV: " << csv_path << "\n";
    }
    if (!descriptor_path.empty() && !data.report_descriptor_hex.empty()) {
        std::cout << "Saved descriptor hex: " << descriptor_path << "\n";
    }

    return 0;
}