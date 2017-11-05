/*
 * texelutil.cpp
 *
 *  Created on: Dec 22, 2013
 *      Author: petero
 */

#include "chesstool.hpp"
#include "posgen.hpp"
#include "parameters.hpp"

#include <iostream>
#include <fstream>
#include <string>

void setInitialValues(const std::string& fname) {
    Parameters& uciPars = Parameters::instance();
    std::vector<std::string> lines = ChessTool::readFile(fname);
    for (const std::string& line : lines) {
        std::vector<std::string> fields;
        splitString(line, fields);
        if ((fields.size() < 2) || !uciPars.getParam(fields[0]))
            continue;
        int value;
        if (str2Num(fields[1], value))
            uciPars.set(fields[0], fields[1]);
    }
}

void usage() {
    std::cerr << "Usage: texelutil [-iv file] cmd params" << std::endl;
    std::cerr << " -iv file : Set initial parameter values" << std::endl;
    std::cerr << "cmd is one of:" << std::endl;
    std::cerr << " p2f      : Convert from PGN to FEN" << std::endl;
    std::cerr << " f2p      : Convert from FEN to PGN" << std::endl;
    std::cerr << " filter   : Remove positions where qScore and search score deviate too much" << std::endl;
    std::cerr << " pawnadv  : Compute evaluation error for different pawn advantage" << std::endl;
    std::cerr << " parrange p a b c   : Compare evaluation error for different parameter values" << std::endl;
    std::cerr << " localopt p1 p2 ... : Optimize parameters using local search" << std::endl;
    std::cerr << " printpar : Print evaluation tables and parameters" << std::endl;
    std::cerr << " evalstat p1 p2 ... : Print parameter statistics" << std::endl;
    std::cerr << " genfen qvsn : Generate all positions of a given type" << std::endl;
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

void getParams(int argc, char* argv[], std::vector<ParamDomain>& params) {
    Parameters& uciPars = Parameters::instance();
    for (int i = 2; i < argc; i++) {
        ParamDomain pd;
        std::string parName(argv[i]);
        if (uciPars.getParam(parName)) {
            pd.name = parName;
            params.push_back(pd);
        } else if (uciPars.getParam(parName + "1")) {
            for (int n = 1; ; n++) {
                pd.name = parName + num2Str(n);
                if (!uciPars.getParam(pd.name))
                    break;
                params.push_back(pd);
            }
        } else {
            std::cerr << "No such parameter:" << pd.name << std::endl;
            ::exit(2);
        }
    }
    for (ParamDomain& pd : params) {
        std::shared_ptr<Parameters::ParamBase> p = uciPars.getParam(pd.name);
        const Parameters::SpinParamBase& sp = dynamic_cast<const Parameters::SpinParamBase&>(*p.get());
        pd.minV = sp.getMinValue();
        pd.step = 1;
        pd.maxV = sp.getMaxValue();
        pd.value = sp.getIntPar();
    }
}

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);

    if ((argc >= 3) && (std::string(argv[1]) == "-iv")) {
        setInitialValues(argv[2]);
        argc -= 2;
        argv += 2;
    }

    if (argc < 2)
        usage();

    std::string cmd = argv[1];
    if (cmd == "p2f") {
        ChessTool::pgnToFen(std::cin);
    } else if (cmd == "f2p") {
        ChessTool::fenToPgn(std::cin);
    } else if (cmd == "pawnadv") {
        ChessTool::pawnAdvTable(std::cin);
    } else if (cmd == "filter") {
        ChessTool::filterFEN(std::cin);
    } else if (cmd == "parrange") {
        std::vector<ParamDomain> params;
        parseParamDomains(argc, argv, params);
        if (params.size() != 1)
            usage();
        ChessTool::paramEvalRange(std::cin, params[0]);
    } else if (cmd == "localopt") {
        std::vector<ParamDomain> params;
        getParams(argc, argv, params);
        ChessTool::localOptimize(std::cin, params);
    } else if (cmd == "printpar") {
        ChessTool::printParams();
    } else if (cmd == "evalstat") {
        std::vector<ParamDomain> params;
        getParams(argc, argv, params);
        ChessTool::evalStat(std::cin, params);
    } else if (cmd == "genfen") {
        if ((argc < 3) || !PosGenerator::generate(argv[2]))
            usage();
    } else {
        ScoreToProb sp(300.0);
        for (int i = -100; i <= 100; i++)
            std::cout << "i:" << i << " p:" << sp.getProb(i) << std::endl;
    }
    return 0;
}
