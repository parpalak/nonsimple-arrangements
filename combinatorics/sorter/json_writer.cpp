// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "json_writer.hpp"

#include <sstream>
#include <string>

std::string json_escape_string(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (uc < 0x20) {
                    static const char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(uc >> 4) & 0xf];
                    out += hex[uc & 0xf];
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

namespace {

void write_perm(std::ostringstream& os, const LinePermutation& p) {
    os << '[';
    const auto& a = p.as_array();
    for (size_t i = 0; i < a.size(); ++i) {
        if (i > 0)
            os << ',';
        os << a[i];
    }
    os << ']';
}

void write_perm_list(std::ostringstream& os, const std::vector<LinePermutation>& list) {
    os << '[';
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0)
            os << ',';
        write_perm(os, list[i]);
    }
    os << ']';
}

void write_size_array(std::ostringstream& os, const std::vector<size_t>& v) {
    os << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0)
            os << ',';
        os << v[i];
    }
    os << ']';
}

void write_order_histogram(std::ostringstream& os, const std::map<size_t, size_t>& h) {
    os << '{';
    bool first = true;
    for (const auto& [k, v] : h) {
        if (!first)
            os << ',';
        first = false;
        os << '"' << k << "\":" << v;
    }
    os << '}';
}

void write_e_class(std::ostringstream& os, const ELocalClass& ec, bool include_full_perms) {
    os << '{';
    os << "\"eid\":\"" << json_escape_string(ec.eid) << '"';
    os << ",\"m\":" << ec.cut_count;
    os << ",\"reps\":";
    if (ec.representatives_defined)
        os << ec.representatives;
    else
        os << "null";
    os << ",\"he_type\":\"" << json_escape_string(ec.stabilizer_type) << '"';
    os << ",\"he_ord\":" << ec.stabilizer_order;
    if (ec.stabilizer_image_order != 0 &&
        ec.stabilizer_image_order != ec.stabilizer_order) {
        os << ",\"he_img_ord\":" << ec.stabilizer_image_order;
        os << ",\"he_kernel_ord\":" << ec.stabilizer_kernel_order;
    }
    os << ",\"he_gens\":";
    write_perm_list(os, ec.e_sym_generators);
    if (include_full_perms) {
        os << ",\"he_elts\":";
        write_perm_list(os, ec.e_sym_elements);
    }
    os << '}';
}

}  // namespace

std::string write_analysis_json(const SymmetryAnalysisResult& result,
                                const JsonWriterOptions& opts) {
    std::ostringstream os;
    os << '{';
    os << "\"n\":" << result.n;
    os << ",\"pid\":\"" << json_escape_string(result.pid) << '"';
    if (opts.include_gens) {
        os << ",\"gens\":\"" << json_escape_string(opts.gens_string) << '"';
    }
    os << ",\"gp_ord\":" << result.p_sym_ord;
    os << ",\"gp_type\":\"" << json_escape_string(result.p_sym_type) << '"';
    os << ",\"gp_gens\":";
    write_perm_list(os, result.p_sym_generators);
    if (opts.include_full_perms) {
        os << ",\"gp_elts\":";
        write_perm_list(os, result.p_sym_elements);
    }
    os << ",\"gp_elt_orders\":";
    write_order_histogram(os, result.p_sym_element_orders);
    os << ",\"chart_to_e\":";
    write_size_array(os, result.cut_to_e_class);
    os << ",\"e_classes\":[";
    for (size_t i = 0; i < result.e_classes.size(); ++i) {
        if (i > 0)
            os << ',';
        write_e_class(os, result.e_classes[i], opts.include_full_perms);
    }
    os << ']';
    os << ",\"checks\":{";
    auto bstr = [](bool b) { return b ? "true" : "false"; };
    os << "\"sum_m_ok\":" << bstr(result.checks.sum_m_ok);
    os << ",\"orbit_stabilizer_ok\":" << bstr(result.checks.orbit_stabilizer_ok);
    os << ",\"representatives_ok\":" << bstr(result.checks.representatives_ok);
    if (!result.checks.representatives_applicable)
        os << ",\"representatives_applicable\":false";
    os << ",\"generators_close\":" << bstr(result.checks.generators_close);
    os << ",\"e_generators_close\":" << bstr(result.checks.e_generators_close);
    os << ",\"chart_to_e_ok\":" << bstr(result.checks.chart_to_e_ok);
    os << '}';
    os << '}';
    return os.str();
}
