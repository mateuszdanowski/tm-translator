#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include "turing_machine.h"

using namespace std;

static void print_usage(string error) {
    cerr << "ERROR: " << error << "\n"
         << "Usage: tm_translator [-q|--quiet] <input_file>\n";
    exit(1);
}

int main(int argc, char* argv[]) {
    string input_filename;
    string output_filename;
    int ok = 0;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (ok == 0)
            input_filename = arg;
        else if (ok == 1)
            output_filename = arg;
        else
            print_usage("Too many arguments");
        ++ok;
    }
    if (ok != 2)
        print_usage("Not enough arguments");

    FILE *f = fopen(input_filename.c_str(), "r");
    if (!f) {
        cerr << "ERROR: File " << input_filename << " does not exist\n";
        return 1;
    }
    TuringMachine tm = read_tm_from_file(f);
    if (tm.num_tapes != 2) {
        cerr << "ERROR: The translator only translates two-tape Turing machines\n";
        return 1;
    }

    TuringMachine one_tape_tm = translate_tm(tm);

    std::ofstream out;
    out.open(output_filename);
    if (!out) {
        cerr << "ERROR: File " << output_filename << " could not be opened\n";
        return 1;
    }
    out << one_tape_tm;
    out.close();

    return 0;
}