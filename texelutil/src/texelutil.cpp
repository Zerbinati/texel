/*
 * texelutil.cpp
 *
 *  Created on: Dec 22, 2013
 *      Author: petero
 */

#include "chesstool.hpp"
#include "parameters.hpp"

#include <iostream>
#include <fstream>
#include <string>

/** Read a whole file into a string. */
std::string readFile(const std::string& fname) {
    std::ifstream is(fname);
    std::string data;
    while (true) {
        std::string line;
        std::getline(is, line);
        if (!is || is.eof())
            break;
        data += line;
        data += '\n';
    }
    return data;
}

void usage() {
    std::cerr << "Usage: texelutil [-p2f] [-pawnadv] [-filter] [-parrange p a b c]" << std::endl;
    std::cerr << " -p2f      : Convert from PGN to FEN" << std::endl;
    std::cerr << " -pawnadv  : Compute evaluation error for different pawn advantage" << std::endl;
    std::cerr << " -parrange : Compate evaluation error for different parameter values" << std::endl;
    std::cerr << " -filter   : Remove positions where qScore and search score deviate too much" << std::endl;
    ::exit(2);
}

void parseParamDomains(int argc, char* argv[], std::vector<ParamDomain>& params) {
    int i = 2;
    while (i + 3 < argc) {
        ParamDomain pd;
        pd.name = argv[i];
        if (!str2Num(std::string(argv[i+1]), pd.minV) ||
            !str2Num(std::string(argv[i+2]), pd.step) || (pd.step <= 0) ||
            !str2Num(std::string(argv[i+3]), pd.maxV))
            usage();
        if (!Parameters::instance().getParam(pd.name)) {
            std::cerr << "No such parameter:" << pd.name << std::endl;
            ::exit(2);
        }
        pd.value = Parameters::instance().getIntPar(pd.name);
        pd.value = (pd.value - pd.minV) / pd.step * pd.step + pd.minV;
        params.push_back(pd);
        i += 4;
    }
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    if (argc < 2)
        usage();

    std::string cmd = argv[1];
    if (cmd == "-p2f") {
        ChessTool::pgnToFen(std::cin);
    } else if (cmd == "-pawnadv") {
        ChessTool::pawnAdvTable(std::cin);
    } else if (cmd == "-filter") {
        ChessTool::filterFEN(std::cin);
    } else if (cmd == "-parrange") {
        std::vector<ParamDomain> params;
        parseParamDomains(argc, argv, params);
        if (params.size() != 1)
            usage();
        ChessTool::paramEvalRange(std::cin, params[0]);
    } else {
        ScoreToProb sp(300.0);
        for (int i = -100; i <= 100; i++)
            std::cout << "i:" << i << " p:" << sp.getProb(i) << std::endl;
    }
    return 0;
}
