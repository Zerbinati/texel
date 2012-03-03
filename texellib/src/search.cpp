/*
 * search.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "search.hpp"
#include "treeLogger.hpp"

#include <cmath>

using namespace SearchConst;

const int UNKNOWN_SCORE = -32767; // Represents unknown static eval score

Search::Search(const Position& pos0, const std::vector<U64>& posHashList0,
               int posHashListSize0, TranspositionTable& tt0)
    : tt(tt0)
{
    init(pos, posHashList0, posHashListSize0);
}

void
Search::init(const Position& pos0, const std::vector<U64>& posHashList0,
             int posHashListSize0) {
    posHashFirstNew = posHashListSize;
    initNodeStats();
    minTimeMillis = -1;
    maxTimeMillis = -1;
    searchNeedMoreTime = false;
    maxNodes = -1;
    nodesBetweenTimeCheck = 5000;
    strength = 1000;
    weak = false;
    randomSeed = 0;
}

void
Search::timeLimit(int minTimeLimit, int maxTimeLimit) {
    minTimeMillis = minTimeLimit;
    maxTimeMillis = maxTimeLimit;
}

void
Search::setStrength(int strength, U64 randomSeed) {
    if (strength < 0) strength = 0;
    if (strength > 1000) strength = 1000;
    this->strength = strength;
    weak = strength < 1000;
    this->randomSeed = randomSeed;
}

Move
Search::iterativeDeepening(const MoveGen::MoveList& scMovesIn,
                           int maxDepth, U64 initialMaxNodes, bool verbose) {
    tStart = currentTimeMillis();
    log.open("/home/petero/treelog.dmp", pos);
    totalNodes = 0;
    if (scMovesIn.size <= 0)
        return Move(); // No moves to search

    std::vector<MoveInfo> scMoves;
    {
        // If strength is < 10%, only include a subset of the root moves.
        // At least one move is always included though.
        bool includedMoves[scMovesIn.size];
        U64 rndL = pos.zobristHash() ^ randomSeed;
        includedMoves[(int)(rndL % scMovesIn.size)] = true;
        int nIncludedMoves = 1;
        double pIncl = (strength < 100) ? strength * strength * 1e-4 : 1.0;
        for (int mi = 0; mi < scMovesIn.size; mi++) {
            rndL = 6364136223846793005ULL * rndL + 1442695040888963407ULL;
            double rnd = ((rndL & 0x7fffffffffffffffULL) % 1000000000) / 1e9;
            if (!includedMoves[mi] && (rnd < pIncl)) {
                includedMoves[mi] = true;
                nIncludedMoves++;
            }
        }
        for (int mi = 0; mi < scMovesIn.size; mi++) {
            if (includedMoves[mi]) {
                const Move& m = scMovesIn.m[mi];
                scMoves.push_back(MoveInfo(m, 0));
            }
        }
    }
    maxNodes = initialMaxNodes;
    nodesToGo = 0;
    Position origPos(pos);
    int bestScoreLastIter = 0;
    bool firstIteration = true;
    Move bestMove = scMoves[0].move;
    this->verbose = verbose;
    if ((maxDepth < 0) || (maxDepth > 100)) {
        maxDepth = 100;
    }
    for (size_t i = 0; i < COUNT_OF(searchTreeInfo); i++)
        searchTreeInfo[i].allowNullMove = true;
    try {
    for (int depthS = plyScale; ; depthS += plyScale, firstIteration = false) {
        initNodeStats();
        listener.notifyDepth(depthS/plyScale);
        int aspirationDelta = (std::abs(bestScoreLastIter) <= MATE0 / 2) ? 20 : 1000;
        int alpha = firstIteration ? -MATE0 : std::max(bestScoreLastIter - aspirationDelta, -MATE0);
        int bestScore = -MATE0;
        UndoInfo ui;
        bool needMoreTime = false;
        for (size_t mi = 0; mi < scMoves.size(); mi++) {
            searchNeedMoreTime = (mi > 0);
            Move& m = scMoves[mi].move;
            if (currentTimeMillis() - tStart >= 1000)
                listener.notifyCurrMove(m, mi + 1);
            nodes = qNodes = 0;
            posHashList[posHashListSize++] = pos.zobristHash();
            bool givesCheck = MoveGen::givesCheck(pos, m);
            int beta;
            if (firstIteration)
                beta = MATE0;
            else
                beta = (mi == 0) ? std::min(bestScoreLastIter + aspirationDelta, MATE0) : alpha + 1;

            int lmrS = 0;
            bool isCapture = (pos.getPiece(m.to()) != Piece::EMPTY);
            bool isPromotion = (m.promoteTo() != Piece::EMPTY);
            if ((depthS >= 3*plyScale) && !isCapture && !isPromotion) {
                if (!givesCheck && !passedPawnPush(pos, m)) {
                    if (mi >= 3)
                        lmrS = plyScale;
                }
            }
/*                U64 nodes0 = nodes;
            U64 qNodes0 = qNodes;
            printf("%2d %5s %5d %5d %6s %6s ",
                   mi, "-", alpha, beta, "-", "-");
            printf("%-6s...\n", TextIO::moveToUCIString(m)); */
            pos.makeMove(m, ui);
            SearchTreeInfo sti = searchTreeInfo[0];
            sti.currentMove = m;
            sti.lmr = lmrS;
            sti.nodeIdx = -1;
            int score = -negaScout(-beta, -alpha, 1, depthS - lmrS - plyScale, -1, givesCheck);
            if ((lmrS > 0) && (score > alpha)) {
                sti.lmr = 0;
                score = -negaScout(-beta, -alpha, 1, depthS - plyScale, -1, givesCheck);
            }
            U64 nodesThisMove = nodes + qNodes;
            posHashListSize--;
            pos.unMakeMove(m, ui);
            {
                int type = TType::T_EXACT;
                if (score <= alpha) {
                    type = TType::T_LE;
                } else if (score >= beta) {
                    type = TType::T_GE;
                }
                m.setScore(score);
                tt.insert(pos.historyHash(), m, type, 0, depthS, UNKNOWN_SCORE);
            }
            if (score >= beta) {
                int retryDelta = aspirationDelta * 2;
                while (score >= beta) {
                    beta = std::min(score + retryDelta, MATE0);
                    retryDelta = MATE0 * 2;
                    if (mi != 0)
                        needMoreTime = true;
                    bestMove = m;
                    if (verbose)
                        printf("%-6s %6d %6ld %6ld >=\n", TextIO::moveToString(pos, m, false).c_str(),
                               score, nodes, qNodes);
                    notifyPV(depthS/plyScale, score, false, true, m);
                    nodes = qNodes = 0;
                    posHashList[posHashListSize++] = pos.zobristHash();
                    pos.makeMove(m, ui);
                    int score2 = -negaScout(-beta, -score, 1, depthS - plyScale, -1, givesCheck);
                    score = std::max(score, score2);
                    nodesThisMove += nodes + qNodes;
                    posHashListSize--;
                    pos.unMakeMove(m, ui);
                }
            } else if ((mi == 0) && (score <= alpha)) {
                int retryDelta = MATE0 * 2;
                while (score <= alpha) {
                    alpha = std::max(score - retryDelta, -MATE0);
                    retryDelta = MATE0 * 2;
                    needMoreTime = searchNeedMoreTime = true;
                    if (verbose)
                        printf("%-6s %6d %6ld %6ld <=\n", TextIO::moveToString(pos, m, false).c_str(),
                               score, nodes, qNodes);
                    notifyPV(depthS/plyScale, score, true, false, m);
                    nodes = qNodes = 0;
                    posHashList[posHashListSize++] = pos.zobristHash();
                    pos.makeMove(m, ui);
                    score = -negaScout(-score, -alpha, 1, depthS - plyScale, -1, givesCheck);
                    nodesThisMove += nodes + qNodes;
                    posHashListSize--;
                    pos.unMakeMove(m, ui);
                }
            }
            if (verbose || !firstIteration) {
                bool havePV = false;
                std::string PV;
                if ((score > alpha) || (mi == 0)) {
                    havePV = true;
                    if (verbose) {
                        PV = TextIO::moveToString(pos, m, false) + " ";
                        pos.makeMove(m, ui);
                        PV += tt.extractPV(pos);
                        pos.unMakeMove(m, ui);
                    }
                }
                if (verbose) {
/*                    printf("%2d %5d %5d %5d %6d %6d ",
                           mi, score, alpha, beta, nodes-nodes0, qNodes-qNodes0);
                    printf("%-6s\n", TextIO::moveToUCIString(m)); */
                    printf("%-6s %6d %6ld %6ld%s %s\n",
                            TextIO::moveToString(pos, m, false).c_str(), score,
                            nodes, qNodes, (score > alpha ? " *" : ""), PV.c_str());
                }
                if (havePV && !firstIteration)
                    notifyPV(depthS/plyScale, score, false, false, m);
            }
            scMoves[mi].move.setScore(score);
            scMoves[mi].nodes = nodesThisMove;
            bestScore = std::max(bestScore, score);
            if (!firstIteration) {
                if ((score > alpha) || (mi == 0)) {
                    alpha = score;
                    MoveInfo tmp = scMoves[mi];
                    for (int i = mi - 1; i >= 0;  i--) {
                        scMoves[i + 1] = scMoves[i];
                    }
                    scMoves[0] = tmp;
                    bestMove = scMoves[0].move;
                }
            }
            if (!firstIteration) {
                U64 timeLimit = needMoreTime ? maxTimeMillis : minTimeMillis;
                if (timeLimit >= 0) {
                    U64 tNow = currentTimeMillis();
                    if (tNow - tStart >= timeLimit)
                        break;
                }
            }
        }
        if (firstIteration) {
            std::sort(scMoves.begin(), scMoves.end(), MoveInfo::SortByScore());
            bestMove = scMoves[0].move;
            notifyPV(depthS/plyScale, bestMove.score(), false, false, bestMove);
        }
        U64 tNow = currentTimeMillis();
        if (verbose) {
            for (int i = 0; i < 20; i++)
                printf("%2d %7d %7d\n", i, nodesPlyVec[i], nodesDepthVec[i]);
            printf("Time: %.3f depth:%.2f nps:%d\n", (tNow - tStart) * .001, depthS/(double)plyScale,
                   (int)(totalNodes / ((tNow - tStart) * .001)));
        }
        if (maxTimeMillis >= 0) {
            if (tNow - tStart >= minTimeMillis)
                break;
        }
        if (depthS >= maxDepth * plyScale)
            break;
        if (maxNodes >= 0) {
            if (totalNodes >= maxNodes)
                break;
        }
        int plyToMate = MATE0 - std::abs(bestScore);
        if (depthS >= plyToMate * plyScale)
            break;
        bestScoreLastIter = bestScore;

        if (!firstIteration) {
            // Moves that were hard to search should be searched early in the next iteration
            std::sort(scMoves.begin()+1, scMoves.end(), MoveInfo::SortByNodes());
        }
    }
    } catch (const StopSearch& ss) {
        pos = origPos;
    }
    notifyStats();

    log.close();
    return bestMove;
}

void
Search::notifyPV(int depth, int score, bool uBound, bool lBound, const Move& m) {
    bool isMate = false;
    if (score > MATE0 / 2) {
        isMate = true;
        score = (MATE0 - score) / 2;
    } else if (score < -MATE0 / 2) {
        isMate = true;
        score = -((MATE0 + score - 1) / 2);
    }
    U64 tNow = currentTimeMillis();
    int time = (int) (tNow - tStart);
    int nps = (time > 0) ? (int)(totalNodes / (time / 1000.0)) : 0;
    std::vector<Move> pv;
    tt.extractPVMoves(pos, m, pv);
    listener.notifyPV(depth, score, time, totalNodes, nps, isMate, uBound, lBound, pv);
}

void
Search::notifyStats() {
    U64 tNow = currentTimeMillis();
    int time = (int) (tNow - tStart);
    int nps = (time > 0) ? (int)(totalNodes / (time / 1000.0)) : 0;
    listener.notifyStats(totalNodes, nps, time);
    tLastStats = tNow;
}

int
Search::negaScout(int alpha, int beta, int ply, int depth, int recaptureSquare,
                  const bool inCheck) {
    {
        const SearchTreeInfo& sti = searchTreeInfo[ply-1];
        U64 idx = log.logNodeStart(sti.nodeIdx, sti.currentMove, alpha, beta, ply, depth/plyScale);
        searchTreeInfo[ply].nodeIdx = idx;
    }
    if (--nodesToGo <= 0) {
        nodesToGo = nodesBetweenTimeCheck;
        U64 tNow = currentTimeMillis();
        U64 timeLimit = searchNeedMoreTime ? maxTimeMillis : minTimeMillis;
        if (    ((timeLimit >= 0) && (tNow - tStart >= timeLimit)) ||
                ((maxNodes >= 0) && (totalNodes >= maxNodes))) {
            throw StopSearch();
        }
        if (tNow - tLastStats >= 1000) {
            notifyStats();
        }
    }

    // Collect statistics
    if (verbose) {
        if (ply < 20) nodesPlyVec[ply]++;
        if (depth < 20*plyScale) nodesDepthVec[depth/plyScale]++;
    }
    const U64 hKey = pos.historyHash();

    // Draw tests
    if (canClaimDraw50(pos)) {
        if (MoveGen::canTakeKing(pos)) {
            int score = MATE0 - ply;
            log.logNodeEnd(searchTreeInfo[ply].nodeIdx, score, TType::T_EXACT, UNKNOWN_SCORE, hKey);
            return score;
        }
        if (inCheck) {
            MoveGen::MoveList moves;
            MoveGen::pseudoLegalMoves(pos, moves);
            MoveGen::removeIllegal(pos, moves);
            if (moves.size == 0) {            // Can't claim draw if already check mated.
                int score = -(MATE0-(ply+1));
                log.logNodeEnd(searchTreeInfo[ply].nodeIdx, score, TType::T_EXACT, UNKNOWN_SCORE, hKey);
                return score;
            }
        }
        log.logNodeEnd(searchTreeInfo[ply].nodeIdx, 0, TType::T_EXACT, UNKNOWN_SCORE, hKey);
        return 0;
    }
    if (canClaimDrawRep(pos, posHashList, posHashListSize, posHashFirstNew)) {
        log.logNodeEnd(searchTreeInfo[ply].nodeIdx, 0, TType::T_EXACT, UNKNOWN_SCORE, hKey);
        return 0;            // No need to test for mate here, since it would have been
                             // discovered the first time the position came up.
    }

    int evalScore = UNKNOWN_SCORE;
    // Check transposition table
    TranspositionTable::TTEntry ent;
    tt.probe(hKey, ent);
    Move hashMove;
    SearchTreeInfo sti = searchTreeInfo[ply];
    if (ent.type != TType::T_EMPTY) {
        int score = ent.getScore(ply);
        evalScore = ent.evalScore;
        int plyToMate = MATE0 - std::abs(score);
        int eDepth = ent.getDepth();
        if ((beta == alpha + 1) && ((eDepth >= depth) || (eDepth >= plyToMate*plyScale))) {
            if (     (ent.type == TType::T_EXACT) ||
                    ((ent.type == TType::T_GE) && (score >= beta)) ||
                    ((ent.type == TType::T_LE) && (score <= alpha))) {
                if (score >= beta) {
                    hashMove = sti.hashMove;
                    ent.getMove(hashMove);
                    if (!hashMove.isEmpty())
                        if (pos.getPiece(hashMove.to()) == Piece::EMPTY)
                            kt.addKiller(ply, hashMove);
                }
                log.logNodeEnd(searchTreeInfo[ply].nodeIdx, score, ent.type, evalScore, hKey);
                return score;
            }
        }
        hashMove = sti.hashMove;
        ent.getMove(hashMove);
    }

    int posExtend = inCheck ? plyScale : 0; // Check extension

    // If out of depth, perform quiescence search
    if (depth + posExtend <= 0) {
        q0Eval = evalScore;
        int score = quiesce(alpha, beta, ply, 0, inCheck);
        int type = TType::T_EXACT;
        if (score <= alpha) {
            type = TType::T_LE;
        } else if (score >= beta) {
            type = TType::T_GE;
        }
        emptyMove.setScore(score);
        tt.insert(hKey, emptyMove, type, ply, depth, q0Eval);
        log.logNodeEnd(sti.nodeIdx, score, type, q0Eval, hKey);
        return score;
    }

    // Razoring
    if ((std::abs(alpha) <= MATE0 / 2) && (depth < 4*plyScale) && (beta == alpha + 1)) {
        if (evalScore == UNKNOWN_SCORE) {
            evalScore = eval.evalPos(pos);
        }
        const int razorMargin = 250;
        if (evalScore < beta - razorMargin) {
            q0Eval = evalScore;
            int score = quiesce(alpha-razorMargin, beta-razorMargin, ply, 0, inCheck);
            if (score <= alpha-razorMargin) {
                emptyMove.setScore(score);
                tt.insert(hKey, emptyMove, TType::T_LE, ply, depth, q0Eval);
                log.logNodeEnd(sti.nodeIdx, score, TType::T_LE, q0Eval, hKey);
                return score;
            }
        }
    }

    // Reverse futility pruning
    if (!inCheck && (depth < 5*plyScale) && (posExtend == 0) &&
        (std::abs(alpha) <= MATE0 / 2) && (std::abs(beta) <= MATE0 / 2)) {
        bool mtrlOk;
        if (pos.whiteMove) {
            mtrlOk = (pos.wMtrl > pos.wMtrlPawns) && (pos.wMtrlPawns > 0);
        } else {
            mtrlOk = (pos.bMtrl > pos.bMtrlPawns) && (pos.bMtrlPawns > 0);
        }
        if (mtrlOk) {
            int margin;
            if (depth <= plyScale)        margin = 204;
            else if (depth <= 2*plyScale) margin = 420;
            else if (depth <= 3*plyScale) margin = 533;
            else                          margin = 788;
            if (evalScore == UNKNOWN_SCORE)
                evalScore = eval.evalPos(pos);
            if (evalScore - margin >= beta) {
                emptyMove.setScore(evalScore - margin);
                tt.insert(hKey, emptyMove, TType::T_GE, ply, depth, evalScore);
                log.logNodeEnd(sti.nodeIdx, evalScore - margin, TType::T_GE, evalScore, hKey);
                return evalScore - margin;
            }
        }
    }

    // Try null-move pruning
    // FIXME! Try null-move verification in late endgames. See loss in round 21.
    sti.currentMove = emptyMove;
    if (    (depth >= 3*plyScale) && !inCheck && sti.allowNullMove &&
            (std::abs(beta) <= MATE0 / 2)) {
        if (MoveGen::canTakeKing(pos)) {
            int score = MATE0 - ply;
            log.logNodeEnd(sti.nodeIdx, score, TType::T_EXACT, evalScore, hKey);
            return score;
        }
        bool nullOk;
        if (pos.whiteMove) {
            nullOk = (pos.wMtrl > pos.wMtrlPawns) && (pos.wMtrlPawns > 0);
        } else {
            nullOk = (pos.bMtrl > pos.bMtrlPawns) && (pos.bMtrlPawns > 0);
        }
        if (nullOk) {
            if (evalScore == UNKNOWN_SCORE)
                evalScore = eval.evalPos(pos);
            if (evalScore < beta)
                nullOk = false;
        }
        if (nullOk) {
            const int R = (depth > 6*plyScale) ? 4*plyScale : 3*plyScale;
            pos.setWhiteMove(!pos.whiteMove);
            int epSquare = pos.getEpSquare();
            pos.setEpSquare(-1);
            searchTreeInfo[ply+1].allowNullMove = false;
            int score = -negaScout(-beta, -(beta - 1), ply + 1, depth - R, -1, false);
            searchTreeInfo[ply+1].allowNullMove = true;
            pos.setEpSquare(epSquare);
            pos.setWhiteMove(!pos.whiteMove);
            if (score >= beta) {
                if (score > MATE0 / 2)
                    score = beta;
                emptyMove.setScore(score);
                tt.insert(hKey, emptyMove, TType::T_GE, ply, depth, evalScore);
                log.logNodeEnd(sti.nodeIdx, score, TType::T_GE, evalScore, hKey);
                return score;
            } else {
                if ((searchTreeInfo[ply-1].lmr > 0) && (depth < 5*plyScale)) {
                    Move m1 = searchTreeInfo[ply-1].currentMove;
                    Move m2 = searchTreeInfo[ply+1].bestMove; // threat move
                    if (relatedMoves(m1, m2)) {
                        // if the threat move was made possible by a reduced
                        // move on the previous ply, the reduction was unsafe.
                        // Return alpha to trigger a non-reduced re-search.
                        log.logNodeEnd(sti.nodeIdx, alpha, TType::T_LE, evalScore, hKey);
                        return alpha;
                    }
                }
            }
        }
    }

    bool futilityPrune = false;
    int futilityScore = alpha;
    if (!inCheck && (depth < 5*plyScale) && (posExtend == 0)) {
        if ((std::abs(alpha) <= MATE0 / 2) && (std::abs(beta) <= MATE0 / 2)) {
            int margin;
            if (depth <= plyScale)        margin = 61;
            else if (depth <= 2*plyScale) margin = 144;
            else if (depth <= 3*plyScale) margin = 268;
            else                          margin = 334;
            if (evalScore == UNKNOWN_SCORE)
                evalScore = eval.evalPos(pos);
            futilityScore = evalScore + margin;
            if (futilityScore <= alpha)
                futilityPrune = true;
        }
    }

    if ((depth > 4*plyScale) && hashMove.isEmpty()) {
        bool isPv = beta > alpha + 1;
        if (isPv || (depth > 8 * plyScale)) {
            // No hash move. Try internal iterative deepening.
            long savedNodeIdx = sti.nodeIdx;
            int newDepth = isPv ? depth  - 2 * plyScale : depth * 3 / 8;
            negaScout(alpha, beta, ply, newDepth, -1, inCheck);
            sti.nodeIdx = savedNodeIdx;
            tt.probe(hKey, ent);
            if (ent.type != TType::T_EMPTY) {
                hashMove = sti.hashMove;
                ent.getMove(hashMove);
            }
        }
    }

    // Start searching move alternatives
    // FIXME! Try hash move before generating move list.
    MoveGen::MoveList moves;
    if (inCheck)
        MoveGen::checkEvasions(pos, moves);
    else
        MoveGen::pseudoLegalMoves(pos, moves);
    bool seeDone = false;
    bool hashMoveSelected = true;
    if (!selectHashMove(moves, hashMove)) {
        scoreMoveList(moves, ply);
        seeDone = true;
        hashMoveSelected = false;
    }

    UndoInfo ui = sti.undoInfo;
    bool haveLegalMoves = false;
    int illegalScore = -(MATE0-(ply+1));
    int b = beta;
    int bestScore = illegalScore;
    int bestMove = -1;
    int lmrCount = 0;
    for (int mi = 0; mi < moves.size; mi++) {
        if ((mi == 1) && !seeDone) {
            scoreMoveList(moves, ply, 1);
            seeDone = true;
        }
        if ((mi > 0) || !hashMoveSelected)
            selectBest(moves, mi);
        Move& m = moves.m[mi];
        if (pos.getPiece(m.to()) == (pos.whiteMove ? Piece::BKING : Piece::WKING)) {
            int score = MATE0-ply;
            log.logNodeEnd(sti.nodeIdx, score, TType::T_EXACT, evalScore, hKey);
            return score;       // King capture
        }
        int newCaptureSquare = -1;
        bool isCapture = (pos.getPiece(m.to()) != Piece::EMPTY);
        bool isPromotion = (m.promoteTo() != Piece::EMPTY);
        int sVal = std::numeric_limits<int>::min();
        // FIXME! Test extending pawn pushes to 7:th rank
        bool mayReduce = (m.score() < 53) && (!isCapture || m.score() < 0) && !isPromotion;
        bool givesCheck = MoveGen::givesCheck(pos, m);
        bool doFutility = false;
        if (mayReduce && haveLegalMoves && !givesCheck && !passedPawnPush(pos, m)) {
            int moveCountLimit;
            if (depth <= plyScale)          moveCountLimit = 3;
            else if (depth <= 2 * plyScale) moveCountLimit = 6;
            else if (depth <= 3 * plyScale) moveCountLimit = 12;
            else if (depth <= 4 * plyScale) moveCountLimit = 24;
            else moveCountLimit = 256;
            if (mi >= moveCountLimit)
                continue; // Late move pruning
            if (futilityPrune)
                doFutility = true;
        }
        int score;
        if (doFutility) {
            score = futilityScore;
        } else {
            int moveExtend = 0;
            if (posExtend == 0) {
                const int pV = Evaluate::pV;
                if ((m.to() == recaptureSquare)) {
                    if (sVal == std::numeric_limits<int>::min()) sVal = SEE(m);
                    int tVal = Evaluate::pieceValue[pos.getPiece(m.to())];
                    if (sVal > tVal - pV / 2)
                        moveExtend = plyScale;
                }
                if ((moveExtend < plyScale) && isCapture && (pos.wMtrlPawns + pos.bMtrlPawns > pV)) {
                    // Extend if going into pawn endgame
                    int capVal = Evaluate::pieceValue[pos.getPiece(m.to())];
                    if (pos.whiteMove) {
                        if ((pos.wMtrl == pos.wMtrlPawns) && (pos.bMtrl - pos.bMtrlPawns == capVal))
                            moveExtend = plyScale;
                    } else {
                        if ((pos.bMtrl == pos.bMtrlPawns) && (pos.wMtrl - pos.wMtrlPawns == capVal))
                            moveExtend = plyScale;
                    }
                }
            }
            int extend = std::max(posExtend, moveExtend);
            int lmr = 0;
            if ((depth >= 3*plyScale) && mayReduce && (extend == 0)) {
                if (!givesCheck && !passedPawnPush(pos, m)) {
                    lmrCount++;
                    if ((lmrCount > 3) && (depth > 3*plyScale) && !isCapture) {
                        lmr = 2*plyScale;
                    } else {
                        lmr = 1*plyScale;
                    }
                }
            }
            int newDepth = depth - plyScale + extend - lmr;
            if (isCapture && (givesCheck || (depth + extend) > plyScale)) {
                // Compute recapture target square, but only if we are not going
                // into q-search at the next ply.
                int fVal = Evaluate::pieceValue[pos.getPiece(m.from())];
                int tVal = Evaluate::pieceValue[pos.getPiece(m.to())];
                const int pV = Evaluate::pV;
                if (std::abs(tVal - fVal) < pV / 2) {    // "Equal" capture
                    sVal = SEE(m);
                    if (std::abs(sVal) < pV / 2)
                        newCaptureSquare = m.to();
                }
            }
            posHashList[posHashListSize++] = pos.zobristHash();
            pos.makeMove(m, ui);
            nodes++;
            totalNodes++;
            sti.currentMove = m;
/*            long nodes0 = nodes;
            long qNodes0 = qNodes;
            if ((ply < 3) && (newDepth > plyScale)) {
                printf("%2d %5s %5d %5d %6s %6s ",
                       mi, "-", alpha, beta, "-", "-");
                for (int i = 0; i < ply; i++)
                    printf("      ");
                printf("%-6s...\n", TextIO::moveToUCIString(m));
            } */
            sti.lmr = lmr;
            score = -negaScout(-b, -alpha, ply + 1, newDepth, newCaptureSquare, givesCheck);
            if (((lmr > 0) && (score > alpha)) ||
                ((score > alpha) && (score < beta) && (b != beta) && (score != illegalScore))) {
                sti.lmr = 0;
                newDepth += lmr;
                score = -negaScout(-beta, -alpha, ply + 1, newDepth, newCaptureSquare, givesCheck);
            }
/*            if (ply <= 3) {
                printf("%2d %5d %5d %5d %6d %6d ",
                       mi, score, alpha, beta, nodes-nodes0, qNodes-qNodes0);
                for (int i = 0; i < ply; i++)
                    printf("      ");
                printf("%-6s\n", TextIO::moveToUCIString(m));
            }*/
            posHashListSize--;
            pos.unMakeMove(m, ui);
        }
        if (weak && haveLegalMoves)
            if (weakPlaySkipMove(pos, m, ply))
                score = illegalScore;
        m.setScore(score);

        if (score != illegalScore)
            haveLegalMoves = true;
        bestScore = std::max(bestScore, score);
        if (score > alpha) {
            alpha = score;
            bestMove = mi;
            sti.bestMove.setMove(m.from(), m.to(), m.promoteTo(), sti.bestMove.score());
        }
        if (alpha >= beta) {
            if (pos.getPiece(m.to()) == Piece::EMPTY) {
                kt.addKiller(ply, m);
                ht.addSuccess(pos, m, depth/plyScale);
                for (int mi2 = mi - 1; mi2 >= 0; mi2--) {
                    Move m2 = moves.m[mi2];
                    if (pos.getPiece(m2.to()) == Piece::EMPTY)
                        ht.addFail(pos, m2, depth/plyScale);
                }
            }
            tt.insert(hKey, m, TType::T_GE, ply, depth, evalScore);
            log.logNodeEnd(sti.nodeIdx, alpha, TType::T_GE, evalScore, hKey);
            return alpha;
        }
        b = alpha + 1;
    }
    if (!haveLegalMoves && !inCheck) {
        log.logNodeEnd(sti.nodeIdx, 0, TType::T_EXACT, evalScore, hKey);
        return 0;       // Stale-mate
    }
    if (bestMove >= 0) {
        tt.insert(hKey, moves.m[bestMove], TType::T_EXACT, ply, depth, evalScore);
        log.logNodeEnd(sti.nodeIdx, bestScore, TType::T_EXACT, evalScore, hKey);
    } else {
        emptyMove.setScore(bestScore);
        tt.insert(hKey, emptyMove, TType::T_LE, ply, depth, evalScore);
        log.logNodeEnd(sti.nodeIdx, bestScore, TType::T_LE, evalScore, hKey);
    }
    return bestScore;
}

bool
Search::weakPlaySkipMove(const Position& pos, const Move& m, int ply) const {
    long rndL = pos.zobristHash() ^ Position::psHashKeys[0][m.from()] ^
                Position::psHashKeys[0][m.to()] ^ randomSeed;
    double rnd = ((rndL & 0x7fffffffffffffffULL) % 1000000000) / 1e9;

    double s = strength * 1e-3;
    double offs = (17 - 50 * s) / 3;
    double effPly = ply * Evaluate::interpolate(pos.wMtrl + pos.bMtrl, 0, 30, Evaluate::qV * 4, 100) * 1e-2;
    double t = effPly + offs;
    double p = 1/(1+exp(t)); // Probability to "see" move
    bool easyMove = ((pos.getPiece(m.to()) != Piece::EMPTY) ||
                     (ply < 2) || (searchTreeInfo[ply-2].currentMove.to() == m.from()));
    if (easyMove)
        p = 1-(1-p)*(1-p);
    if (rnd > p)
        return true;
    return false;
}

int
Search::quiesce(int alpha, int beta, int ply, int depth, const bool inCheck) {
    int score;
    if (inCheck) {
        score = -(MATE0 - (ply+1));
    } else {
        if ((depth == 0) && (q0Eval != UNKNOWN_SCORE)) {
            score = q0Eval;
        } else {
            score = eval.evalPos(pos);
            if (depth == 0)
                q0Eval = score;
        }
    }
    if (score >= beta) {
        if ((depth == 0) && (score < MATE0 - ply)) {
            if (MoveGen::canTakeKing(pos)) {
                // To make stale-mate detection work
                score = MATE0 - ply;
            }
        }
        return score;
    }
    const int evalScore = score;
    if (score > alpha)
        alpha = score;
    int bestScore = score;
    const bool tryChecks = (depth > -3);
    MoveGen::MoveList moves;
    if (inCheck) {
        MoveGen::checkEvasions(pos, moves);
    } else if (tryChecks) {
        MoveGen::pseudoLegalCapturesAndChecks(pos, moves);
    } else {
        MoveGen::pseudoLegalCaptures(pos, moves);
    }
    scoreMoveListMvvLva(moves);
    UndoInfo ui = searchTreeInfo[ply].undoInfo;
    for (int mi = 0; mi < moves.size; mi++) {
        if (mi < 8) {
            // If the first 8 moves didn't fail high, this is probably an ALL-node,
            // so spending more effort on move ordering is probably wasted time.
            selectBest(moves, mi);
        }
        const Move& m = moves.m[mi];
        if (pos.getPiece(m.to()) == (pos.whiteMove ? Piece::BKING : Piece::WKING))
            return MATE0-ply;       // King capture
        bool givesCheck = false;
        bool givesCheckComputed = false;
        if (inCheck) {
            // Allow all moves
        } else {
            if ((pos.getPiece(m.to()) == Piece::EMPTY) && (m.promoteTo() == Piece::EMPTY)) {
                // Non-capture
                if (!tryChecks)
                    continue;
                givesCheck = MoveGen::givesCheck(pos, m);
                givesCheckComputed = true;
                if (!givesCheck)
                    continue;
                if (negSEE(m)) // Needed because m.score() is not computed for non-captures
                    continue;
            } else {
                if (negSEE(m))
                    continue;
                int capt = Evaluate::pieceValue[pos.getPiece(m.to())];
                int prom = Evaluate::pieceValue[m.promoteTo()];
                int optimisticScore = evalScore + capt + prom + 200;
                if (optimisticScore < alpha) { // Delta pruning
                    if ((pos.wMtrlPawns > 0) && (pos.wMtrl > capt + pos.wMtrlPawns) &&
                        (pos.bMtrlPawns > 0) && (pos.bMtrl > capt + pos.bMtrlPawns)) {
                        if (depth -1 > -4) {
                            givesCheck = MoveGen::givesCheck(pos, m);
                            givesCheckComputed = true;
                        }
                        if (!givesCheck) {
                            if (optimisticScore > bestScore)
                                bestScore = optimisticScore;
                            continue;
                        }
                    }
                }
            }
        }

        if (!givesCheckComputed) {
            if (depth - 1 > -4) {
                givesCheck = MoveGen::givesCheck(pos, m);
            }
        }
        const bool nextInCheck = (depth - 1) > -4 ? givesCheck : false;

        pos.makeMove(m, ui);
        qNodes++;
        totalNodes++;
        score = -quiesce(-beta, -alpha, ply + 1, depth - 1, nextInCheck);
        pos.unMakeMove(m, ui);
        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta)
                    return alpha;
            }
        }
    }
    return bestScore;
}


int
Search::SEE(const Move& m) {
    const int kV = Evaluate::kV;

    const int square = m.to();
    if (square == pos.getEpSquare()) {
        captures[0] = Evaluate::pV;
    } else {
        captures[0] = Evaluate::pieceValue[pos.getPiece(square)];
        if (captures[0] == kV)
            return kV;
    }
    int nCapt = 1;                  // Number of entries in captures[]

    pos.makeSEEMove(m, seeUi);
    bool white = pos.whiteMove;
    int valOnSquare = Evaluate::pieceValue[pos.getPiece(square)];
    U64 occupied = pos.whiteBB | pos.blackBB;
    while (true) {
        int bestValue = std::numeric_limits<int>::max();
        U64 atk;
        if (white) {
            atk = BitBoard::bPawnAttacks[square] & pos.pieceTypeBB[Piece::WPAWN] & occupied;
            if (atk != 0) {
                bestValue = Evaluate::pV;
            } else {
                atk = BitBoard::knightAttacks[square] & pos.pieceTypeBB[Piece::WKNIGHT] & occupied;
                if (atk != 0) {
                    bestValue = Evaluate::nV;
                } else {
                    U64 bAtk = BitBoard::bishopAttacks(square, occupied) & occupied;
                    atk = bAtk & pos.pieceTypeBB[Piece::WBISHOP];
                    if (atk != 0) {
                        bestValue = Evaluate::bV;
                    } else {
                        U64 rAtk = BitBoard::rookAttacks(square, occupied) & occupied;
                        atk = rAtk & pos.pieceTypeBB[Piece::WROOK];
                        if (atk != 0) {
                            bestValue = Evaluate::rV;
                        } else {
                            atk = (bAtk | rAtk) & pos.pieceTypeBB[Piece::WQUEEN];
                            if (atk != 0) {
                                bestValue = Evaluate::qV;
                            } else {
                                atk = BitBoard::kingAttacks[square] & pos.pieceTypeBB[Piece::WKING] & occupied;
                                if (atk != 0) {
                                    bestValue = kV;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        } else {
            atk = BitBoard::wPawnAttacks[square] & pos.pieceTypeBB[Piece::BPAWN] & occupied;
            if (atk != 0) {
                bestValue = Evaluate::pV;
            } else {
                atk = BitBoard::knightAttacks[square] & pos.pieceTypeBB[Piece::BKNIGHT] & occupied;
                if (atk != 0) {
                    bestValue = Evaluate::nV;
                } else {
                    U64 bAtk = BitBoard::bishopAttacks(square, occupied) & occupied;
                    atk = bAtk & pos.pieceTypeBB[Piece::BBISHOP];
                    if (atk != 0) {
                        bestValue = Evaluate::bV;
                    } else {
                        U64 rAtk = BitBoard::rookAttacks(square, occupied) & occupied;
                        atk = rAtk & pos.pieceTypeBB[Piece::BROOK];
                        if (atk != 0) {
                            bestValue = Evaluate::rV;
                        } else {
                            atk = (bAtk | rAtk) & pos.pieceTypeBB[Piece::BQUEEN];
                            if (atk != 0) {
                                bestValue = Evaluate::qV;
                            } else {
                                atk = BitBoard::kingAttacks[square] & pos.pieceTypeBB[Piece::BKING] & occupied;
                                if (atk != 0) {
                                    bestValue = kV;
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        captures[nCapt++] = valOnSquare;
        if (valOnSquare == kV)
            break;
        valOnSquare = bestValue;
        occupied &= ~(atk & -atk);
        white = !white;
    }
    pos.unMakeSEEMove(m, seeUi);

    int score = 0;
    for (int i = nCapt - 1; i > 0; i--) {
        score = std::max(0, captures[i] - score);
    }
    return captures[0] - score;
}

void
Search::scoreMoveList(MoveGen::MoveList& moves, int ply, int startIdx) {
    for (int i = startIdx; i < moves.size; i++) {
        Move& m = moves.m[i];
        bool isCapture = (pos.getPiece(m.to()) != Piece::EMPTY) || (m.promoteTo() != Piece::EMPTY);
        int score = 0;
        if (isCapture) {
            int seeScore = isCapture ? signSEE(m) : 0;
            int v = pos.getPiece(m.to());
            int a = pos.getPiece(m.from());
            score = Evaluate::pieceValue[v]/10 * 1000 - Evaluate::pieceValue[a]/10;
            if (seeScore > 0)
                score += 2000000;
            else if (seeScore == 0)
                score += 1000000;
            else
                score -= 1000000;
            score *= 100;
        }
        int ks = kt.getKillerScore(ply, m);
        if (ks > 0) {
            score += ks + 50;
        } else {
            int hs = ht.getHistScore(pos, m);
            score += hs;
        }
        m.setScore(score);
    }
}

void
Search::selectBest(MoveGen::MoveList& moves, int startIdx) {
    int bestIdx = startIdx;
    int bestScore = moves.m[bestIdx].score();
    for (int i = startIdx + 1; i < moves.size; i++) {
        int sc = moves.m[i].score();
        if (sc > bestScore) {
            bestIdx = i;
            bestScore = sc;
        }
    }
    if (bestIdx != startIdx) {
        const Move& m = moves.m[startIdx];
        moves.m[startIdx] = moves.m[bestIdx];
        moves.m[bestIdx] = m;
    }
}

/** If hashMove exists in the move list, move the hash move to the front of the list. */
bool
Search::selectHashMove(MoveGen::MoveList& moves, const Move& hashMove) {
    if (hashMove.isEmpty())
        return false;
    for (int i = 0; i < moves.size; i++) {
        Move& m = moves.m[i];
        if (m.equals(hashMove)) {
            moves.m[i] = moves.m[0];
            moves.m[0] = m;
            m.setScore(10000);
            return true;
        }
    }
    return false;
}

bool
Search::canClaimDrawRep(const Position& pos, const std::vector<U64>& posHashList,
                       int posHashListSize, int posHashFirstNew) {
    int reps = 0;
    for (int i = posHashListSize - 4; i >= 0; i -= 2) {
        if (pos.zobristHash() == posHashList[i]) {
            reps++;
            if (i >= posHashFirstNew) {
                reps++;
                break;
            }
        }
    }
    return (reps >= 2);
}

void
Search::initNodeStats() {
    nodes = qNodes = 0;
    for (size_t i = 0; i < COUNT_OF(nodesPlyVec); i++) {
        nodesPlyVec[i] = 0;
        nodesDepthVec[i] = 0;
    }
}
