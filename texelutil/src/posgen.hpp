/*
 * posgen.hpp
 *
 *  Created on: Jan 6, 2014
 *      Author: petero
 */

#ifndef POSGEN_HPP_
#define POSGEN_HPP_

#include <string>
#include <vector>

class PosGenerator {
public:
    /** Generate a FEN containing all (or a sample of) positions of a certain type. */
    static bool generate(const std::string& type);

    /** Print all tablebase types containing a given number of pieces. */
    static void tbList(int nPieces);

    /** Generate tablebase DTM statistics. */
    static void dtmStat(const std::vector<std::string>& tbTypes);

    /** Generate tablebase DTZ statistics. */
    static void dtzStat(const std::vector<std::string>& tbTypes);

    /**
     * Generate WDL statistics for an endgame type, indexed by the positions of the
     * pieces specified in pieceTypes.
     * A pieceType string has the format [wb][kqrbnp]
     */
    static void egStat(const std::string& tbType, const std::vector<std::string>& pieceTypes);

    /** Compare RTB probe results to GTB probe results, report any differences. */
    static void wdlTest(const std::vector<std::string>& tbTypes);

    /** Compare RTB DTZ probe results to GTB DTM probe results, report any unexpected differences. */
    static void dtzTest(const std::vector<std::string>& tbTypes);

    /** Compare tbgen probe results to GTB DTM probe results, report any differences. */
    static void tbgenTest(const std::vector<std::string>& tbTypes);

private:
    static void genQvsN();
};


#endif /* POSGEN_HPP_ */
