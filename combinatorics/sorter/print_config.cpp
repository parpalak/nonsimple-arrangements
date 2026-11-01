// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Roman Parpalak and Denis Utkin

#include "print_config.hpp"

#include "wire_print.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

std::string normalize_uniq_mode(std::string uniq_mode) {
    std::transform(uniq_mode.begin(), uniq_mode.end(), uniq_mode.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return uniq_mode;
}

bool apply_output_transformations(Configuration& conf, const Params& params) {
    if (!conf.o && (params.mirror || params.rotate > 0)) {
        std::cout << "#INVALID CONFIGURATION" << std::endl;
        return false;
    }

    bool matrix_updated = false;

    if (conf.has_multiple_points() && (params.mirror || params.rotate > 0)) {
        std::cout << "#UNSUPPORTED TRANSFORMATION FOR MULTIPLE POINTS" << std::endl;
        return false;
    }

    if (params.mirror) {
        conf.o = conf.o->mirror();
        matrix_updated = true;
    }

    if (params.rotate > 0) {
        auto new_o = conf.o->rotate(params.rotate);
        if (!new_o) {
            std::cout << "#INVALID ROTATION" << std::endl;
            return false;
        }
        conf.o = *new_o;
        matrix_updated = true;
    }

    if (matrix_updated) {
        conf.gens = conf.o->get_generators();
        conf.grouped_word.clear();
        conf.grouped_o.reset();
        conf.projective_o.reset();
        conf.grouped_identifiers.reset();
    }

    return true;
}

void print_configuration(Configuration& conf, const Params& params, const StreamPrintOverrides* overrides) {
    bool need_space = false;
    for (auto& print_cmd : params.print) {
        if (print_cmd == "gens") {
            if (conf.has_multiple_points()) {
                if (need_space) std::cout << " ";
                std::cout << addline::format_word(conf.grouped_word);
            } else {
                for (auto g : conf.gens) {
                    if (need_space) {
                        std::cout << " ";
                    }
                    std::cout << g;
                    need_space = true;
                }
            }
            std::cout << std::endl;
            need_space = false;
        }
        else if (print_cmd == "omatrix") {
            if (conf.o) {
                print_OMatrix(*conf.o);
            } else {
                std::cout << "#OMATRIX UNSUPPORTED FOR MULTIPLE POINTS" << std::endl;
            }
            need_space = false;
        }
        else if (print_cmd == "n") {
            if (need_space) std::cout << " ";
            const std::size_t n = conf.grouped_o
                ? static_cast<std::size_t>(conf.grouped_o->n)
                : (conf.o ? conf.o->intersections.size() : 0);
            std::cout << "[n=" << n << "]";
            need_space = true;
        }
        else if (print_cmd == "k") {
            if (need_space) std::cout << " ";
            std::size_t parallel_pairs = 0;
            if (conf.grouped_o) {
                const std::size_t n = static_cast<std::size_t>(conf.grouped_o->n);
                std::size_t finite_entries = 0;
                for (const auto& row : conf.grouped_o->rows) {
                    finite_entries += row.size();
                }
                parallel_pairs = (n * (n - 1) - finite_entries) / 2;
            } else if (conf.o) {
                parallel_pairs = conf.o->parallels.size() / 2;
            }
            std::cout << "[k=" << parallel_pairs << "]";
            need_space = true;
        }
        else if (print_cmd == "A") {
            if (need_space) std::cout << " ";
            std::cout << "[A=" << conf.get_A() << "]";
            need_space = true;
        }
        else if (print_cmd == "pos") {
            if (need_space) std::cout << " ";
            std::cout << "#" << conf.line_number;
            need_space = true;
        }
        else if (print_cmd == "did") {
            if (need_space) std::cout << " ";
            std::cout << conf.get_did();
            need_space = true;
        }
        else if (print_cmd == "eid") {
            if (need_space) std::cout << " ";
            if (overrides && overrides->eid.has_value())
                std::cout << *overrides->eid;
            else
                std::cout << conf.get_eid();
            need_space = true;
        }
        else if (print_cmd == "pid") {
            if (need_space) std::cout << " ";
            if (overrides && overrides->pid.has_value())
                std::cout << *overrides->pid;
            else
                std::cout << conf.get_pid();
            need_space = true;
        }
        else if (print_cmd == "perm") {
            std::vector<std::size_t> perm = conf.get_perm();
            if (need_space) std::cout << " ";
            std::cout << "[perm=";
            for (std::size_t i = 0; i < perm.size(); ++i) {
                if (i > 0) std::cout << " ";
                std::cout << perm[i];
            }
            std::cout << "]";
            need_space = true;
        }
        else if (print_cmd == "w0") {
            W0Validation validation = conf.get_w0_validation();
            if (need_space) std::cout << " ";
            std::cout << "[w0=" << validation.status << "]";
            need_space = true;
        }
        else if (print_cmd == "w0_reason") {
            W0Validation validation = conf.get_w0_validation();
            if (need_space) std::cout << " ";
            std::cout << "[w0: " << validation.reason << "]";
            need_space = true;
        }
        else if (print_cmd == "lf") {
            std::cout << std::endl;
            need_space = false;
        }
        else if (print_cmd == "wire_condensed") {
            print_wire_condensed(conf.gens);
            std::cout << std::endl;
            need_space = false;
        }
        else if (print_cmd == "wire") {
            print_wire(conf.gens);
            std::cout << std::endl;
            need_space = false;
        }
        else {
            if (need_space) std::cout << " ";
            std::cout << print_cmd;
            need_space = true;
        }
    }
}
