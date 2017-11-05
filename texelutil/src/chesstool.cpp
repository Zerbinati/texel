/*
 * chesstool.cpp
 *
 *  Created on: Dec 24, 2013
 *      Author: petero
 */

#include "chesstool.hpp"
#include "search.hpp"
#include "textio.hpp"
#include "gametree.hpp"
#include "stloutput.hpp"

#include <queue>
#include <unordered_set>
#include <unistd.h>


ScoreToProb::ScoreToProb(double pawnAdvantage0)
    : pawnAdvantage(pawnAdvantage0) {
    for (int i = 0; i < MAXCACHE; i++) {
        cache[i] = -1;
        logCacheP[i] = logCacheN[i] = 2;
    }
}

double
ScoreToProb::getProb(int score) {
    bool neg = false;
    if (score < 0) {
        score = -score;
        neg = true;
    }
    double ret = -1;
    if (score < MAXCACHE) {
        if (cache[score] < 0)
            cache[score] = computeProb(score);
        ret = cache[score];
    }
    if (ret < 0)
        ret = computeProb(score);
    if (neg)
        ret = 1 - ret;
    return ret;
}

double
ScoreToProb::getLogProb(int score) {
    if ((score >= 0) && (score < MAXCACHE)) {
        if (logCacheP[score] > 1)
            logCacheP[score] = log(getProb(score));
        return logCacheP[score];
    }
    if ((score < 0) && (score > -MAXCACHE)) {
        if (logCacheN[-score] > 1)
            logCacheN[-score] = log(getProb(score));
        return logCacheN[-score];
    }
    return log(getProb(score));
}

// --------------------------------------------------------------------------------

std::vector<std::string>
ChessTool::readFile(const std::string& fname) {
    std::ifstream is(fname);
    return readStream(is);
}

std::vector<std::string>
ChessTool::readStream(std::istream& is) {
    std::vector<std::string> ret;
    while (true) {
        std::string line;
        std::getline(is, line);
        if (!is || is.eof())
            break;
        ret.push_back(line);
    }
    return ret;
}

const int UNKNOWN_SCORE = -32767; // Represents unknown static eval score

void
ChessTool::pgnToFen(std::istream& is) {
    static std::vector<U64> nullHist(200);
    static TranspositionTable tt(19);
    static ParallelData pd(tt);
    static KillerTable kt;
    static History ht;
    static auto et = Evaluate::getEvalHashTables();
    static Search::SearchTables st(tt, kt, ht, *et);
    static TreeLogger treeLog;

    Position pos;
    const int mate0 = SearchConst::MATE0;
    Search sc(pos, nullHist, 0, st, pd, nullptr, treeLog);
    const int plyScale = SearchConst::plyScale;

    GameTree gt(is);
    int gameNo = 0;
    while (gt.readPGN()) {
        gameNo++;
        GameTree::Result result = gt.getResult();
        if (result == GameTree::UNKNOWN)
            continue;
        double rScore = 0;
        switch (result) {
        case GameTree::WHITE_WIN: rScore = 1.0; break;
        case GameTree::BLACK_WIN: rScore = 0.0; break;
        case GameTree::DRAW:      rScore = 0.5; break;
        default: break;
        }
        GameNode gn = gt.getRootNode();
        while (true) {
            pos = gn.getPos();
            std::string fen = TextIO::toFEN(pos);
            if (gn.nChildren() == 0)
                break;
            gn.goForward(0);
//            std::string move = TextIO::moveToUCIString(gn.getMove());
            std::string comment = gn.getComment();
            int commentScore;
            if (!getCommentScore(comment, commentScore))
                continue;

            sc.init(pos, nullHist, 0);
            sc.q0Eval = UNKNOWN_SCORE;
            int score = sc.quiesce(-mate0, mate0, 0, 0*plyScale, MoveGen::inCheck(pos));
            if (!pos.getWhiteMove()) {
                score = -score;
                commentScore = -commentScore;
            }

            std::cout << fen << " : " << rScore << " : " << commentScore << " : " << score << " : " << gameNo << '\n';
        }
    }
    std::cout << std::flush;
}

void
ChessTool::fenToPgn(std::istream& is) {
    std::vector<std::string> lines = readStream(is);
    for (const std::string& line : lines) {
        Position pos(TextIO::readFEN(line));
        writePGN(pos);
    }
}

void
ChessTool::pawnAdvTable(std::istream& is) {
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    qEval(positions);
    for (int pawnAdvantage = 1; pawnAdvantage <= 400; pawnAdvantage += 1) {
        ScoreToProb sp(pawnAdvantage);
        double avgErr = computeAvgError(positions, sp);
        std::stringstream ss;
        ss << "pa:" << pawnAdvantage << " err:" << std::setprecision(14) << avgErr;
        std::cout << ss.str() << std::endl;
    }
}

// --------------------------------------------------------------------------------

void
ChessTool::filterScore(std::istream& is, int scLimit, double prLimit) {
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    ScoreToProb sp;
    Position pos;
    for (const PositionInfo& pi : positions) {
        double p1 = sp.getProb(pi.searchScore);
        double p2 = sp.getProb(pi.qScore);
        if ((std::abs(p1 - p2) < prLimit) && (std::abs(pi.searchScore - pi.qScore) < scLimit)) {
            pos.deSerialize(pi.posData);
            std::string fen = TextIO::toFEN(pos);
            std::cout << fen << " : " << pi.result << " : " << pi.searchScore << " : " << pi.qScore
                      << " : " << pi.gameNo << '\n';
        }
    }
    std::cout << std::flush;
}

static int
swapSquareY(int square) {
    int x = Position::getX(square);
    int y = Position::getY(square);
    return Position::getSquare(x, 7-y);
}

static Position
swapColors(const Position& pos) {
    Position sym;
    sym.setWhiteMove(!pos.getWhiteMove());
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            int sq = Position::getSquare(x, y);
            int p = pos.getPiece(sq);
            p = Piece::isWhite(p) ? Piece::makeBlack(p) : Piece::makeWhite(p);
            sym.setPiece(swapSquareY(sq), p);
        }
    }

    int castleMask = 0;
    if (pos.a1Castle()) castleMask |= 1 << Position::A8_CASTLE;
    if (pos.h1Castle()) castleMask |= 1 << Position::H8_CASTLE;
    if (pos.a8Castle()) castleMask |= 1 << Position::A1_CASTLE;
    if (pos.h8Castle()) castleMask |= 1 << Position::H1_CASTLE;
    sym.setCastleMask(castleMask);

    if (pos.getEpSquare() >= 0)
        sym.setEpSquare(swapSquareY(pos.getEpSquare()));

    sym.setHalfMoveClock(pos.getHalfMoveClock());
    sym.setFullMoveCounter(pos.getFullMoveCounter());

    return sym;
}

static int nPieces(const Position& pos, Piece::Type piece) {
    return BitBoard::bitCount(pos.pieceTypeBB(piece));
}

static bool isMatch(int mtrlDiff, bool compare, int patternDiff) {
    return !compare || (mtrlDiff == patternDiff);
}

void
ChessTool::filterMtrlBalance(std::istream& is, bool minorEqual,
                             const std::vector<std::pair<bool,int>>& mtrlPattern) {
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    Position pos;
    int mtrlDiff[5];
    for (const PositionInfo& pi : positions) {
        pos.deSerialize(pi.posData);
        mtrlDiff[0] = nPieces(pos, Piece::WQUEEN)  - nPieces(pos, Piece::BQUEEN);
        mtrlDiff[1] = nPieces(pos, Piece::WROOK)   - nPieces(pos, Piece::BROOK);
        int nComp;
        if (minorEqual) {
            mtrlDiff[2] = nPieces(pos, Piece::WBISHOP) - nPieces(pos, Piece::BBISHOP) +
                          nPieces(pos, Piece::WKNIGHT) - nPieces(pos, Piece::BKNIGHT);
            mtrlDiff[3] = nPieces(pos, Piece::WPAWN)   - nPieces(pos, Piece::BPAWN);
            nComp = 4;
        } else {
            mtrlDiff[2] = nPieces(pos, Piece::WBISHOP) - nPieces(pos, Piece::BBISHOP);
            mtrlDiff[3] = nPieces(pos, Piece::WKNIGHT) - nPieces(pos, Piece::BKNIGHT);
            mtrlDiff[4] = nPieces(pos, Piece::WPAWN)   - nPieces(pos, Piece::BPAWN);
            nComp = 5;
        }
        int inc1 = true, inc2 = true;
        for (int i = 0; i < nComp; i++) {
            if (!isMatch(mtrlDiff[i], mtrlPattern[i].first, mtrlPattern[i].second))
                inc1 = false;
            if (!isMatch(mtrlDiff[i], mtrlPattern[i].first, -mtrlPattern[i].second))
                inc2 = false;
        }
        int sign = 1;
        if (inc2 && !inc1) {
            pos = swapColors(pos);
            sign = -1;
        }
        if (inc1 || inc2) {
            std::string fen = TextIO::toFEN(pos);
            std::cout << fen << " : " << ((sign>0)?pi.result:(1-pi.result)) << " : "
                      << pi.searchScore * sign << " : " << pi.qScore * sign
                      << " : " << pi.gameNo << '\n';
        }
    }
    std::cout << std::flush;
}

void
ChessTool::outliers(std::istream& is, int threshold) {
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    qEval(positions);
    Position pos;
    for (const PositionInfo& pi : positions) {
        if (((pi.qScore >=  threshold) && (pi.result < 1.0)) ||
            ((pi.qScore <= -threshold) && (pi.result > 0.0))) {
            pos.deSerialize(pi.posData);
            std::string fen = TextIO::toFEN(pos);
            std::cout << fen << " : " << pi.result << " : " << pi.searchScore << " : " << pi.qScore
                      << " : " << pi.gameNo << '\n';
        }
    }
    std::cout << std::flush;
}

// --------------------------------------------------------------------------------

void
ChessTool::paramEvalRange(std::istream& is, ParamDomain& pd) {
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);

    ScoreToProb sp;
    double bestVal = 1e100;
    for (int i = pd.minV; i <= pd.maxV; i += pd.step) {
        Parameters::instance().set(pd.name, num2Str(i));
        qEval(positions);
        double avgErr = computeAvgError(positions, sp);
        bool best = avgErr < bestVal;
        bestVal = std::min(bestVal, avgErr);
        std::stringstream ss;
        ss << "i:" << i << " err:" << std::setprecision(14) << avgErr << (best?" *":"");
        std::cout << ss.str() << std::endl;
    }
}

struct PrioParam {
    PrioParam(ParamDomain& pd0) : priority(1), pd(&pd0) {}
    double priority;
    ParamDomain* pd;
    bool operator<(const PrioParam& o) const {
        return priority < o.priority;
    }
};

// --------------------------------------------------------------------------------

void
ChessTool::accumulateATA(std::vector<PositionInfo>& positions, int beg, int end,
                         ScoreToProb& sp,
                         std::vector<ParamDomain>& pdVec,
                         arma::mat& aTa, arma::mat& aTb,
                         arma::mat& ePos, arma::mat& eNeg) {
    Parameters& uciPars = Parameters::instance();
    const int M = end - beg;
    const int N = pdVec.size();
    const double w = 1.0 / positions.size();

    arma::mat b(M, 1);
    qEval(positions, beg, end);
    for (int i = beg; i < end; i++)
        b.at(i-beg,0) = positions[i].getErr(sp) * w;

    arma::mat A(M, N);
    for (int j = 0; j < N; j++) {
        ParamDomain& pd = pdVec[j];
        std::cout << "j:" << j << " beg:" << beg << " name:" << pd.name << std::endl;
        const int v0 = pd.value;
        const int vPos = std::min(pd.maxV, pd.value + 1);
        const int vNeg = std::max(pd.minV, pd.value - 1);
        assert(vPos > vNeg);

        uciPars.set(pd.name, num2Str(vPos));
        qEval(positions, beg, end);
        double EPos = 0;
        for (int i = beg; i < end; i++) {
            const double err = positions[i].getErr(sp);
            A.at(i-beg,j) = err;
            EPos += err * err;
        }
        ePos.at(j, 0) += sqrt(EPos * w);

        uciPars.set(pd.name, num2Str(vNeg));
        qEval(positions, beg, end);
        double ENeg = 0;
        for (int i = beg; i < end; i++) {
            const double err = positions[i].getErr(sp);
            A.at(i-beg,j) = (A.at(i-beg,j) - err) / (vPos - vNeg) * w;
            ENeg += err * err;
        }
        eNeg.at(j, 0) += sqrt(ENeg * w);

        uciPars.set(pd.name, num2Str(v0));
    }

    aTa += A.t() * A;
    aTb += A.t() * b;
}

void
ChessTool::gnOptimize(std::istream& is, std::vector<ParamDomain>& pdVec) {
    double t0 = currentTime();
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    const int nPos = positions.size();

    const int N = pdVec.size();
    arma::mat bestP(N, 1);
    for (int i = 0; i < N; i++)
        bestP.at(i, 0) = pdVec[i].value;
    ScoreToProb sp;
    double bestAvgErr = computeAvgError(positions, sp, pdVec, bestP);
    {
        std::stringstream ss;
        ss << "Initial error: " << std::setprecision(14) << bestAvgErr;
        std::cout << ss.str() << std::endl;
    }

    const int chunkSize = 250000000 / N;

    while (true) {
        arma::mat aTa(N, N);  aTa.fill(0.0);
        arma::mat aTb(N, 1);  aTb.fill(0.0);
        arma::mat ePos(N, 1); ePos.fill(0.0);
        arma::mat eNeg(N, 1); eNeg.fill(0.0);

        for (int i = 0; i < nPos; i += chunkSize) {
            const int end = std::min(nPos, i + chunkSize);
            accumulateATA(positions, i, end, sp, pdVec, aTa, aTb, ePos, eNeg);
        }

        arma::mat delta = pinv(aTa) * aTb;
        bool improved = false;
        for (double alpha = 1.0; alpha >= 0.25; alpha /= 2) {
            arma::mat newP = bestP - delta * alpha;
            for (int i = 0; i < N; i++)
                newP.at(i, 0) = clamp((int)std::round(newP.at(i, 0)), pdVec[i].minV, pdVec[i].maxV);
            double avgErr = computeAvgError(positions, sp, pdVec, newP);
            for (int i = 0; i < N; i++) {
                ParamDomain& pd = pdVec[i];
                std::stringstream ss;
                ss << pd.name << ' ' << newP.at(i, 0) << ' ' << std::setprecision(14) << avgErr << ((avgErr < bestAvgErr) ? " *" : "");
                std::cout << ss.str() << std::endl;
            }
            if (avgErr < bestAvgErr) {
                bestP = newP;
                bestAvgErr = avgErr;
                improved = true;
                break;
            }
        }
        if (!improved)
            break;
    }
    double t1 = currentTime();
    ::usleep(100000);
    std::cerr << "Elapsed time: " << t1 - t0 << std::endl;
}

// --------------------------------------------------------------------------------

void
ChessTool::localOptimize(std::istream& is, std::vector<ParamDomain>& pdVec) {
    double t0 = currentTime();
    Parameters& uciPars = Parameters::instance();
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);

    std::priority_queue<PrioParam> queue;
    for (ParamDomain& pd : pdVec)
        queue.push(PrioParam(pd));

    ScoreToProb sp;
    qEval(positions);
    double bestAvgErr = computeAvgError(positions, sp);
    {
        std::stringstream ss;
        ss << "Initial error: " << std::setprecision(14) << bestAvgErr;
        std::cout << ss.str() << std::endl;
    }

    std::vector<PrioParam> tried;
    while (!queue.empty()) {
        PrioParam pp = queue.top(); queue.pop();
        ParamDomain& pd = *pp.pd;
        std::cout << pd.name << " prio:" << pp.priority << " q:" << queue.size()
                  << " min:" << pd.minV << " max:" << pd.maxV << " val:" << pd.value << std::endl;
        double oldBest = bestAvgErr;
        bool improved = false;
        for (int d = 0; d < 2; d++) {
            while (true) {
                const int newValue = pd.value + (d ? -1 : 1);
                if ((newValue < pd.minV) || (newValue > pd.maxV))
                    break;

                uciPars.set(pd.name, num2Str(newValue));
                qEval(positions);
                double avgErr = computeAvgError(positions, sp);
                uciPars.set(pd.name, num2Str(pd.value));

                std::stringstream ss;
                ss << pd.name << ' ' << newValue << ' ' << std::setprecision(14) << avgErr << ((avgErr < bestAvgErr) ? " *" : "");
                std::cout << ss.str() << std::endl;

                if (avgErr >= bestAvgErr)
                    break;
                bestAvgErr = avgErr;
                pd.value = newValue;
                uciPars.set(pd.name, num2Str(pd.value));
                improved = true;
            }
            if (improved)
                break;
        }
        double improvement = oldBest - bestAvgErr;
        std::cout << pd.name << " improvement:" << improvement << std::endl;
        pp.priority = pp.priority * 0.1 + improvement * 0.9;
        if (improved) {
            for (PrioParam& pp2 : tried)
                queue.push(pp2);
            tried.clear();
        }
        tried.push_back(pp);
    }

    double t1 = currentTime();
    ::usleep(100000);
    std::cerr << "Elapsed time: " << t1 - t0 << std::endl;
}

static void
updateMinMax(std::map<int,double>& funcValues, int bestV, int& minV, int& maxV) {
    auto it = funcValues.find(bestV);
    assert(it != funcValues.end());
    if (it != funcValues.begin()) {
        auto it2 = it; --it2;
        int nextMinV = it2->first;
        minV = std::max(minV, nextMinV);
    }
    auto it2 = it; ++it2;
    if (it2 != funcValues.end()) {
        int nextMaxV = it2->first;
        maxV = std::min(maxV, nextMaxV);
    }
}

static int
estimateMin(std::map<int,double>& funcValues, int bestV, int minV, int maxV) {
    return (minV + maxV) / 2;
}

void
ChessTool::localOptimize2(std::istream& is, std::vector<ParamDomain>& pdVec) {
    double t0 = currentTime();
    Parameters& uciPars = Parameters::instance();
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);

    std::priority_queue<PrioParam> queue;
    for (ParamDomain& pd : pdVec)
        queue.push(PrioParam(pd));

    ScoreToProb sp;
    qEval(positions);
    double bestAvgErr = computeAvgError(positions, sp);
    {
        std::stringstream ss;
        ss << "Initial error: " << std::setprecision(14) << bestAvgErr;
        std::cout << ss.str() << std::endl;
    }

    std::vector<PrioParam> tried;
    while (!queue.empty()) {
        PrioParam pp = queue.top(); queue.pop();
        ParamDomain& pd = *pp.pd;
        std::cout << pd.name << " prio:" << pp.priority << " q:" << queue.size()
                  << " min:" << pd.minV << " max:" << pd.maxV << " val:" << pd.value << std::endl;
        double oldBest = bestAvgErr;

        std::map<int, double> funcValues;
        funcValues[pd.value] = bestAvgErr;
        int minV = pd.minV;
        int maxV = pd.maxV;
        while (true) {
            bool improved = false;
            for (int d = 0; d < 2; d++) {
                const int newValue = pd.value + (d ? -1 : 1);
                if ((newValue < minV) || (newValue > maxV))
                    continue;
                if (funcValues.count(newValue) == 0) {
                    uciPars.set(pd.name, num2Str(newValue));
                    qEval(positions);
                    double avgErr = computeAvgError(positions, sp);
                    funcValues[newValue] = avgErr;
                    uciPars.set(pd.name, num2Str(pd.value));
                    std::stringstream ss;
                    ss << pd.name << ' ' << newValue << ' ' << std::setprecision(14) << avgErr << ((avgErr < bestAvgErr) ? " *" : "");
                    std::cout << ss.str() << std::endl;
                }
                if (funcValues[newValue] < bestAvgErr) {
                    bestAvgErr = funcValues[newValue];
                    pd.value = newValue;
                    uciPars.set(pd.name, num2Str(pd.value));
                    updateMinMax(funcValues, pd.value, minV, maxV);
                    improved = true;

                    const int estimatedMinValue = estimateMin(funcValues, pd.value, minV, maxV);
                    if ((estimatedMinValue >= minV) && (estimatedMinValue <= maxV) &&
                        (funcValues.count(estimatedMinValue) == 0)) {
                        uciPars.set(pd.name, num2Str(estimatedMinValue));
                        qEval(positions);
                        double avgErr = computeAvgError(positions, sp);
                        funcValues[estimatedMinValue] = avgErr;
                        uciPars.set(pd.name, num2Str(pd.value));
                        std::stringstream ss;
                        ss << pd.name << ' ' << estimatedMinValue << ' ' << std::setprecision(14) << avgErr << ((avgErr < bestAvgErr) ? " *" : "");
                        std::cout << ss.str() << std::endl;

                        if (avgErr < bestAvgErr) {
                            bestAvgErr = avgErr;
                            pd.value = estimatedMinValue;
                            uciPars.set(pd.name, num2Str(pd.value));
                            updateMinMax(funcValues, pd.value, minV, maxV);
                            break;
                        }
                    }
                }
            }
            if (!improved)
                break;
        }
        double improvement = oldBest - bestAvgErr;
        std::cout << pd.name << " improvement:" << improvement << std::endl;
        pp.priority = pp.priority * 0.1 + improvement * 0.9;
        if (improvement > 0) {
            for (PrioParam& pp2 : tried)
                queue.push(pp2);
            tried.clear();
        }
        tried.push_back(pp);
    }

    double t1 = currentTime();
    ::usleep(100000);
    std::cerr << "Elapsed time: " << t1 - t0 << std::endl;
}

// --------------------------------------------------------------------------------

template <int N>
static void
printTableNxN(const ParamTable<N*N>& pt, const std::string& name,
              std::ostream& os) {
    os << name << ":" << std::endl;
    for (int y = 0; y < N; y++) {
        os << "    " << ((y == 0) ? "{" : " ");
        for (int x = 0; x < N; x++) {
            os << std::setw(4) << pt[y*N+x] << (((y==N-1) && (x == N-1)) ? " }," : ",");
        }
        os << std::endl;
    }
}

template <int N>
static void
printTable(const ParamTable<N>& pt, const std::string& name, std::ostream& os) {
    os << name << ":" << std::endl;
    os << "    {";
    for (int i = 0; i < N; i++)
        os << std::setw(3) << pt[i] << ((i == N-1) ? " }," : ",");
    os << std::endl;
}

void
ChessTool::printParams() {
    std::ostream& os = std::cout;
    printTableNxN<8>(kt1b, "kt1b", os);
    printTableNxN<8>(kt2b, "kt2b", os);
    printTableNxN<8>(pt1b, "pt1b", os);
    printTableNxN<8>(pt2b, "pt2b", os);
    printTableNxN<8>(nt1b, "nt1b", os);
    printTableNxN<8>(nt2b, "nt2b", os);
    printTableNxN<8>(bt1b, "bt1b", os);
    printTableNxN<8>(bt2b, "bt2b", os);
    printTableNxN<8>(qt1b, "qt1b", os);
    printTableNxN<8>(qt2b, "qt2b", os);
    printTableNxN<8>(rt1b, "rt1b", os);
    printTableNxN<8>(knightOutpostBonus, "knightOutpostBonus", os);

    printTable(rookMobScore, "rookMobScore", os);
    printTable(bishMobScore, "bishMobScore", os);
    printTable(queenMobScore, "queenMobScore", os);
    printTableNxN<4>(majorPieceRedundancy, "majorPieceRedundancy", os);
    printTable(passedPawnBonus, "passedPawnBonus", os);
    printTable(candidatePassedBonus, "candidatePassedBonus", os);
    printTable(QvsRRBonus, "QvsRRBonus", os);
    printTable(RvsMBonus, "RvsMBonus", os);
    printTable(RvsMMBonus, "RvsMMBonus", os);
    printTable(bishopPairValue, "bishopPairValue", os);
    printTable(pawnShelterTable, "pawnShelterTable", os);
    printTable(pawnStormTable, "pawnStormTable", os);

    os << "pV : " << pV << std::endl;
    os << "nV : " << nV << std::endl;
    os << "bV : " << bV << std::endl;
    os << "rV : " << rV << std::endl;
    os << "qV : " << qV << std::endl;

    os << "pawnDoubledPenalty     : " << pawnDoubledPenalty << std::endl;
    os << "pawnIslandPenalty      : " << pawnIslandPenalty << std::endl;
    os << "pawnIsolatedPenalty    : " << pawnIsolatedPenalty << std::endl;
    os << "pawnBackwardPenalty    : " << pawnBackwardPenalty << std::endl;
    os << "pawnGuardedPassedBonus : " << pawnGuardedPassedBonus << std::endl;
    os << "pawnRaceBonus          : " << pawnRaceBonus << std::endl;

    os << "QvsRMBonus1         : " << QvsRMBonus1 << std::endl;
    os << "QvsRMBonus2         : " << QvsRMBonus2 << std::endl;
    os << "knightVsQueenBonus1 : " << knightVsQueenBonus1 << std::endl;
    os << "knightVsQueenBonus2 : " << knightVsQueenBonus2 << std::endl;
    os << "knightVsQueenBonus3 : " << knightVsQueenBonus3 << std::endl;

    os << "pawnTradePenalty    : " << pawnTradePenalty << std::endl;
    os << "pieceTradeBonus     : " << pieceTradeBonus << std::endl;
    os << "pawnTradeThreshold  : " << pawnTradeThreshold << std::endl;
    os << "pieceTradeThreshold : " << pieceTradeThreshold << std::endl;

    os << "threatBonus1     : " << threatBonus1 << std::endl;
    os << "threatBonus2     : " << threatBonus2 << std::endl;

    os << "rookHalfOpenBonus     : " << rookHalfOpenBonus << std::endl;
    os << "rookOpenBonus         : " << rookOpenBonus << std::endl;
    os << "rookDouble7thRowBonus : " << rookDouble7thRowBonus << std::endl;
    os << "trappedRookPenalty    : " << trappedRookPenalty << std::endl;

    os << "bishopPairPawnPenalty : " << bishopPairPawnPenalty << std::endl;
    os << "trappedBishopPenalty1 : " << trappedBishopPenalty1 << std::endl;
    os << "trappedBishopPenalty2 : " << trappedBishopPenalty2 << std::endl;
    os << "oppoBishopPenalty     : " << oppoBishopPenalty << std::endl;

    os << "kingAttackWeight         : " << kingAttackWeight << std::endl;
    os << "kingSafetyHalfOpenBCDEFG : " << kingSafetyHalfOpenBCDEFG << std::endl;
    os << "kingSafetyHalfOpenAH     : " << kingSafetyHalfOpenAH << std::endl;
    os << "kingSafetyWeight         : " << kingSafetyWeight << std::endl;
    os << "pawnStormBonus           : " << pawnStormBonus << std::endl;

    os << "pawnLoMtrl          : " << pawnLoMtrl << std::endl;
    os << "pawnHiMtrl          : " << pawnHiMtrl << std::endl;
    os << "minorLoMtrl         : " << minorLoMtrl << std::endl;
    os << "minorHiMtrl         : " << minorHiMtrl << std::endl;
    os << "castleLoMtrl        : " << castleLoMtrl << std::endl;
    os << "castleHiMtrl        : " << castleHiMtrl << std::endl;
    os << "queenLoMtrl         : " << queenLoMtrl << std::endl;
    os << "queenHiMtrl         : " << queenHiMtrl << std::endl;
    os << "passedPawnLoMtrl    : " << passedPawnLoMtrl << std::endl;
    os << "passedPawnHiMtrl    : " << passedPawnHiMtrl << std::endl;
    os << "kingSafetyLoMtrl    : " << kingSafetyLoMtrl << std::endl;
    os << "kingSafetyHiMtrl    : " << kingSafetyHiMtrl << std::endl;
    os << "oppoBishopLoMtrl    : " << oppoBishopLoMtrl << std::endl;
    os << "oppoBishopHiMtrl    : " << oppoBishopHiMtrl << std::endl;
    os << "knightOutpostLoMtrl : " << knightOutpostLoMtrl << std::endl;
    os << "knightOutpostHiMtrl : " << knightOutpostHiMtrl << std::endl;
}

static bool strContains(const std::string& str, const std::string& sub) {
    return str.find(sub) != std::string::npos;
}

static int
findLine(const std::string start, const std::string& contain, const std::vector<std::string>& lines) {
    for (int i = 0; i < (int)lines.size(); i++) {
        const std::string& line = lines[i];
        if (startsWith(line, start) && strContains(line, contain))
            return i;
    }
    return -1;
}

std::vector<std::string> splitLines(const std::string& lines) {
    std::vector<std::string> ret;
    int start = 0;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (lines[i] == '\n') {
            ret.push_back(lines.substr(start, i - start));
            start = i + 1;
        }
    }
    return ret;
}

template <int N>
static void
replaceTableNxN(const ParamTable<N*N>& pt, const std::string& name,
             std::vector<std::string>& cppFile) {
    int lineNo = findLine("ParamTable", " " + name + " ", cppFile);
    if (lineNo < 0)
        throw ChessParseError(name + " not found");
    if (lineNo + N >= (int)cppFile.size())
        throw ChessParseError("unexpected end of file");

    std::stringstream ss;
    printTableNxN<N>(pt, name, ss);
    std::vector<std::string> replaceLines = splitLines(ss.str());
    if (replaceLines.size() != N + 1)
        throw ChessParseError("Wrong number of replacement lines");
    for (int i = 1; i <= N; i++)
        cppFile[lineNo + i] = replaceLines[i];
}

template <int N>
static void
replaceTable(const ParamTable<N>& pt, const std::string& name,
           std::vector<std::string>& cppFile) {
    int lineNo = findLine("ParamTable", " " + name + " ", cppFile);
    if (lineNo < 0)
        throw ChessParseError(name + " not found");
    if (lineNo + 1 >= (int)cppFile.size())
        throw ChessParseError("unexpected end of file");

    std::stringstream ss;
    printTable<N>(pt, name, ss);
    std::vector<std::string> replaceLines = splitLines(ss.str());
    if (replaceLines.size() != 2)
        throw ChessParseError("Wrong number of replacement lines");
    cppFile[lineNo + 1] = replaceLines[1];
}

template <typename ParType>
static void replaceValue(const ParType& par, const std::string& name,
                         std::vector<std::string>& hppFile) {
    int lineNo = findLine("DECLARE_PARAM", "(" + name + ", ", hppFile);
    if (lineNo < 0)
        throw ChessParseError(name + " not found");

    const std::string& line = hppFile[lineNo];
    const int len = line.length();
    for (int i = 0; i < len; i++) {
        if (line[i] == ',') {
            for (int j = i + 1; j < len; j++) {
                if (line[j] != ' ') {
                    int p1 = j;
                    for (int k = p1 + 1; k < len; k++) {
                        if (line[k] == ',') {
                            int p2 = k;
                            int val = par;
                            hppFile[lineNo] = line.substr(0, p1) +
                                              num2Str(val) +
                                              line.substr(p2);
                            return;
                        }
                    }
                    goto error;
                }
            }
            goto error;
        }
    }
 error:
    throw ChessParseError("Failed to patch name : " + name);
}

void
ChessTool::patchParams(const std::string& directory) {
    std::vector<std::string> cppFile = readFile(directory + "/parameters.cpp");
    std::vector<std::string> hppFile = readFile(directory + "/parameters.hpp");

    replaceTableNxN<8>(kt1b, "kt1b", cppFile);
    replaceTableNxN<8>(kt2b, "kt2b", cppFile);
    replaceTableNxN<8>(pt1b, "pt1b", cppFile);
    replaceTableNxN<8>(pt2b, "pt2b", cppFile);
    replaceTableNxN<8>(nt1b, "nt1b", cppFile);
    replaceTableNxN<8>(nt2b, "nt2b", cppFile);
    replaceTableNxN<8>(bt1b, "bt1b", cppFile);
    replaceTableNxN<8>(bt2b, "bt2b", cppFile);
    replaceTableNxN<8>(qt1b, "qt1b", cppFile);
    replaceTableNxN<8>(qt2b, "qt2b", cppFile);
    replaceTableNxN<8>(rt1b, "rt1b", cppFile);
    replaceTableNxN<8>(knightOutpostBonus, "knightOutpostBonus", cppFile);

    replaceTable(rookMobScore, "rookMobScore", cppFile);
    replaceTable(bishMobScore, "bishMobScore", cppFile);
    replaceTable(queenMobScore, "queenMobScore", cppFile);
    replaceTableNxN<4>(majorPieceRedundancy, "majorPieceRedundancy", cppFile);
    replaceTable(passedPawnBonus, "passedPawnBonus", cppFile);
    replaceTable(candidatePassedBonus, "candidatePassedBonus", cppFile);
    replaceTable(QvsRRBonus, "QvsRRBonus", cppFile);
    replaceTable(RvsMBonus, "RvsMBonus", cppFile);
    replaceTable(RvsMMBonus, "RvsMMBonus", cppFile);
    replaceTable(bishopPairValue, "bishopPairValue", cppFile);
    replaceTable(pawnShelterTable, "pawnShelterTable", cppFile);
    replaceTable(pawnStormTable, "pawnStormTable", cppFile);

    replaceValue(pV, "pV", hppFile);
    replaceValue(nV, "nV", hppFile);
    replaceValue(bV, "bV", hppFile);
    replaceValue(rV, "rV", hppFile);
    replaceValue(qV, "qV", hppFile);

    replaceValue(pawnDoubledPenalty, "pawnDoubledPenalty", hppFile);
    replaceValue(pawnIslandPenalty, "pawnIslandPenalty", hppFile);
    replaceValue(pawnIsolatedPenalty, "pawnIsolatedPenalty", hppFile);
    replaceValue(pawnBackwardPenalty, "pawnBackwardPenalty", hppFile);
    replaceValue(pawnGuardedPassedBonus, "pawnGuardedPassedBonus", hppFile);
    replaceValue(pawnRaceBonus, "pawnRaceBonus", hppFile);

    replaceValue(QvsRMBonus1, "QvsRMBonus1", hppFile);
    replaceValue(QvsRMBonus2, "QvsRMBonus2", hppFile);
    replaceValue(knightVsQueenBonus1, "knightVsQueenBonus1", hppFile);
    replaceValue(knightVsQueenBonus2, "knightVsQueenBonus2", hppFile);
    replaceValue(knightVsQueenBonus3, "knightVsQueenBonus3", hppFile);

    replaceValue(pawnTradePenalty, "pawnTradePenalty", hppFile);
    replaceValue(pieceTradeBonus, "pieceTradeBonus", hppFile);
    replaceValue(pawnTradeThreshold, "pawnTradeThreshold", hppFile);
    replaceValue(pieceTradeThreshold, "pieceTradeThreshold", hppFile);

    replaceValue(threatBonus1, "threatBonus1", hppFile);
    replaceValue(threatBonus2, "threatBonus2", hppFile);

    replaceValue(rookHalfOpenBonus, "rookHalfOpenBonus", hppFile);
    replaceValue(rookOpenBonus, "rookOpenBonus", hppFile);
    replaceValue(rookDouble7thRowBonus, "rookDouble7thRowBonus", hppFile);
    replaceValue(trappedRookPenalty, "trappedRookPenalty", hppFile);

    replaceValue(bishopPairPawnPenalty, "bishopPairPawnPenalty", hppFile);
    replaceValue(trappedBishopPenalty1, "trappedBishopPenalty1", hppFile);
    replaceValue(trappedBishopPenalty2, "trappedBishopPenalty2", hppFile);
    replaceValue(oppoBishopPenalty, "oppoBishopPenalty", hppFile);

    replaceValue(kingAttackWeight, "kingAttackWeight", hppFile);
    replaceValue(kingSafetyHalfOpenBCDEFG, "kingSafetyHalfOpenBCDEFG", hppFile);
    replaceValue(kingSafetyHalfOpenAH, "kingSafetyHalfOpenAH", hppFile);
    replaceValue(kingSafetyWeight, "kingSafetyWeight", hppFile);
    replaceValue(pawnStormBonus, "pawnStormBonus", hppFile);

    replaceValue(pawnLoMtrl, "pawnLoMtrl", hppFile);
    replaceValue(pawnHiMtrl, "pawnHiMtrl", hppFile);
    replaceValue(minorLoMtrl, "minorLoMtrl", hppFile);
    replaceValue(minorHiMtrl, "minorHiMtrl", hppFile);
    replaceValue(castleLoMtrl, "castleLoMtrl", hppFile);
    replaceValue(castleHiMtrl, "castleHiMtrl", hppFile);
    replaceValue(queenLoMtrl, "queenLoMtrl", hppFile);
    replaceValue(queenHiMtrl, "queenHiMtrl", hppFile);
    replaceValue(passedPawnLoMtrl, "passedPawnLoMtrl", hppFile);
    replaceValue(passedPawnHiMtrl, "passedPawnHiMtrl", hppFile);
    replaceValue(kingSafetyLoMtrl, "kingSafetyLoMtrl", hppFile);
    replaceValue(kingSafetyHiMtrl, "kingSafetyHiMtrl", hppFile);
    replaceValue(oppoBishopLoMtrl, "oppoBishopLoMtrl", hppFile);
    replaceValue(oppoBishopHiMtrl, "oppoBishopHiMtrl", hppFile);
    replaceValue(knightOutpostLoMtrl, "knightOutpostLoMtrl", hppFile);
    replaceValue(knightOutpostHiMtrl, "knightOutpostHiMtrl", hppFile);

    std::ofstream osc(directory + "/parameters.cpp");
    for (const std::string& line : cppFile)
        osc << line << std::endl;

    std::ofstream osh(directory + "/parameters.hpp");
    for (const std::string& line : hppFile)
        osh << line << std::endl;
}

// --------------------------------------------------------------------------------

void
ChessTool::evalStat(std::istream& is, std::vector<ParamDomain>& pdVec) {
    Parameters& uciPars = Parameters::instance();
    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    const int nPos = positions.size();

    qEval(positions);
    std::vector<int> qScores0;
    for (const PositionInfo& pi : positions)
        qScores0.push_back(pi.qScore);
    ScoreToProb sp;
    const double avgErr0 = computeAvgError(positions, sp);

    for (ParamDomain& pd : pdVec) {
        int newVal1 = (pd.value - pd.minV) > (pd.maxV - pd.value) ? pd.minV : pd.maxV;
        uciPars.set(pd.name, num2Str(newVal1));
        qEval(positions);
        double avgErr = computeAvgError(positions, sp);
        uciPars.set(pd.name, num2Str(pd.value));

        double nChanged = 0;
        std::unordered_set<int> games, changedGames;
        for (int i = 0; i < nPos; i++) {
            int gameNo = positions[i].gameNo;
            games.insert(gameNo);
            if (positions[i].qScore - qScores0[i]) {
                nChanged++;
                changedGames.insert(gameNo);
            }
        }
        double errChange1 = avgErr - avgErr0;
        double nChangedGames = changedGames.size();
        double nGames = games.size();

        double errChange2;
        int newVal2 = clamp(0, pd.minV, pd.maxV);
        if (newVal2 != newVal1) {
            uciPars.set(pd.name, num2Str(newVal2));
            qEval(positions);
            double avgErr2 = computeAvgError(positions, sp);
            uciPars.set(pd.name, num2Str(pd.value));
            errChange2 = avgErr2 - avgErr0;
        } else {
            errChange2 = errChange1;
        }

        std::cout << pd.name << " nMod:" << (nChanged / nPos)
                  << " nModG:" << (nChangedGames / nGames)
                  << " err1:" << errChange1 << " err2:" << errChange2 << std::endl;
    }
}

void
ChessTool::printResiduals(std::istream& is, const std::string& xTypeStr, bool includePosGameNr) {
    enum XType {
        MTRL_SUM,
        MTRL_DIFF,
        PAWN_SUM,
        PAWN_DIFF,
        EVAL
    };
    XType xType;
    if (xTypeStr == "mtrlsum") {
        xType = MTRL_SUM;
    } else if (xTypeStr == "mtrldiff") {
        xType = MTRL_DIFF;
    } else if (xTypeStr == "pawnsum") {
        xType = PAWN_SUM;
    } else if (xTypeStr == "pawndiff") {
        xType = PAWN_DIFF;
    } else if (xTypeStr == "eval") {
        xType = EVAL;
    } else {
        throw ChessParseError("Invalid X axis type");
    }

    std::vector<PositionInfo> positions;
    readFENFile(is, positions);
    qEval(positions);
    const int nPos = positions.size();
    Position pos;
    ScoreToProb sp;
    for (int i = 0; i < nPos; i++) {
        const PositionInfo& pi = positions[i];
        pos.deSerialize(pi.posData);
        int x;
        switch (xType) {
        case MTRL_SUM:
            x = pos.wMtrl() + pos.bMtrl();
            break;
        case MTRL_DIFF:
            x = pos.wMtrl() - pos.bMtrl();
            break;
        case PAWN_SUM:
            x = pos.wMtrlPawns() + pos.bMtrlPawns();
            break;
        case PAWN_DIFF:
            x = pos.wMtrlPawns() - pos.bMtrlPawns();
            break;
        case EVAL:
            x = pi.qScore;
            break;
        }
        double r = pi.result - sp.getProb(pi.qScore);
        if (includePosGameNr)
            std::cout << i << ' ' << pi.gameNo << ' ';
        std::cout << x << ' ' << r << '\n';
    }
    std::cout << std::flush;
}

bool
ChessTool::getCommentScore(const std::string& comment, int& score) {
    double fScore;
    if (!str2Num(comment, fScore))
        return false;
    score = (int)std::round(fScore * 100);
    return true;
}

void
splitString(const std::string& line, const std::string& delim, std::vector<std::string>& fields) {
    size_t start = 0;
    while (true) {
        size_t n = line.find(delim, start);
        if (n == std::string::npos)
            break;
        fields.push_back(line.substr(start, n - start));
        start = n + delim.length();
    }
    if (start < line.length())
        fields.push_back(line.substr(start));
}

void
ChessTool::readFENFile(std::istream& is, std::vector<PositionInfo>& data) {
    std::vector<std::string> lines = readStream(is);
    data.resize(lines.size());
    Position pos;
    PositionInfo pi;
    const int nLines = lines.size();
    std::atomic<bool> error(false);
#pragma omp parallel for default(none) shared(data,error,lines,std::cerr) private(pos,pi)
    for (int i = 0; i < nLines; i++) {
        if (error)
            continue;
        const std::string& line = lines[i];
        std::vector<std::string> fields;
        splitString(line, " : ", fields);
        bool localError = false;
        if ((fields.size() < 4) || (fields.size() > 5))
            localError = true;
        if (!localError) {
            try {
                pos = TextIO::readFEN(fields[0]);
            } catch (ChessParseError& cpe) {
                localError = true;
            }
        }
        if (!localError) {
            pos.serialize(pi.posData);
            if (!str2Num(fields[1], pi.result) ||
                !str2Num(fields[2], pi.searchScore) ||
                !str2Num(fields[3], pi.qScore)) {
                localError = true;
            }
        }
        if (!localError) {
            pi.gameNo = -1;
            if (fields.size() == 5)
                if (!str2Num(fields[4], pi.gameNo))
                    localError = true;
        }
        if (!localError)
            data[i] = pi;

        if (localError) {
#pragma omp critical
            if (!error) {
                std::cerr << "line:" << line << std::endl;
                std::cerr << "fields:" << fields << std::endl;
                error = true;
            }
        }
    }
    if (error)
        throw ChessParseError("Invalid file format");
}

void
ChessTool::writePGN(const Position& pos) {
    std::cout << "[Event \"?\"]" << std::endl;
    std::cout << "[Site \"?\"]" << std::endl;
    std::cout << "[Date \"????.??.??\"]" << std::endl;
    std::cout << "[Round \"?\"]" << std::endl;
    std::cout << "[White \"?\"]" << std::endl;
    std::cout << "[Black \"?\"]" << std::endl;
    std::cout << "[Result \"*\"]" << std::endl;
    std::cout << "[FEN \"" << TextIO::toFEN(pos) << "\"]" << std::endl;
    std::cout << "[SetUp \"1\"]" << std::endl;
    std::cout << "*" << std::endl;
}

void
ChessTool::qEval(std::vector<PositionInfo>& positions) {
    qEval(positions, 0, positions.size());
}

void
ChessTool::qEval(std::vector<PositionInfo>& positions, const int beg, const int end) {
    TranspositionTable tt(19);
    ParallelData pd(tt);

    std::vector<U64> nullHist(200);
    KillerTable kt;
    History ht;
    std::shared_ptr<Evaluate::EvalHashTables> et;
    TreeLogger treeLog;
    Position pos;

    const int chunkSize = 5000;

#pragma omp parallel for default(none) shared(positions,tt,pd) private(kt,ht,et,treeLog,pos) firstprivate(nullHist)
    for (int c = beg; c < end; c += chunkSize) {
        if (!et)
            et = Evaluate::getEvalHashTables();
        Search::SearchTables st(tt, kt, ht, *et);

        const int mate0 = SearchConst::MATE0;
        Search sc(pos, nullHist, 0, st, pd, nullptr, treeLog);
        const int plyScale = SearchConst::plyScale;

        for (int i = 0; i < chunkSize; i++) {
            if (c + i >= end)
                break;
            PositionInfo& pi = positions[c + i];
            pos.deSerialize(pi.posData);
            sc.init(pos, nullHist, 0);
            sc.q0Eval = UNKNOWN_SCORE;
            int score = sc.quiesce(-mate0, mate0, 0, 0*plyScale, MoveGen::inCheck(pos));
            if (!pos.getWhiteMove())
                score = -score;
            pi.qScore = score;
        }
    }
}

double
ChessTool::computeAvgError(std::vector<PositionInfo>& positions, ScoreToProb& sp,
                           const std::vector<ParamDomain>& pdVec, arma::mat& pdVal) {
    assert(pdVal.n_rows == pdVec.size());
    assert(pdVal.n_cols == 1);

    Parameters& uciPars = Parameters::instance();
    for (int i = 0; i < (int)pdVal.n_rows; i++)
        uciPars.set(pdVec[i].name, num2Str(pdVal.at(i, 0)));
    qEval(positions);
    return computeAvgError(positions, sp);
}

double
ChessTool::computeAvgError(const std::vector<PositionInfo>& positions, ScoreToProb& sp) {
    double errSum = 0;
    if (useEntropyErrorFunction) {
        for (const PositionInfo& pi : positions) {
            double err = -(pi.result * sp.getLogProb(pi.qScore) + (1 - pi.result) * sp.getLogProb(-pi.qScore));
            errSum += err;
        }
        return errSum / positions.size();
    } else {
        for (const PositionInfo& pi : positions) {
            double p = sp.getProb(pi.qScore);
            double err = p - pi.result;
            errSum += err * err;
        }
        return sqrt(errSum / positions.size());
    }
}
