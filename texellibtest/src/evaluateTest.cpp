/*
 * evaluateTest.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "evaluateTest.hpp"
#include "position.hpp"

#include "cute.h"


#if 0
    /**
     * Test of evalPos method, of class Evaluate.
     */
    public void testEvalPos() {
        Position pos = TextIO::readFEN(TextIO::startPosFEN);
        UndoInfo ui = new UndoInfo();
        pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "e5"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Nf3"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Nc6"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Bb5"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Nge7"), ui);
        ASSERT(moveScore(pos, "O-O") > 0);      // Castling is good
        ASSERT(moveScore(pos, "Ke2") < 0);      // Losing right to castle is bad
        ASSERT(moveScore(pos, "Kf1") < 0);
        ASSERT(moveScore(pos, "Rg1") < 0);
        ASSERT(moveScore(pos, "Rf1") < 0);

        pos = TextIO::readFEN("8/8/8/1r3k2/4pP2/4P3/8/4K2R w K - 0 1");
        ASSERT_EQUAL(true, pos.h1Castle());
        int cs1 = evalWhite(pos);
        pos.setCastleMask(pos.getCastleMask() & ~(1 << Position::H1_CASTLE));
        ASSERT_EQUAL(false, pos.h1Castle());
        int cs2 = evalWhite(pos);
        ASSERT(cs2 >= cs1);    // No bonus for useless castle right

        // Test rook open file bonus
        pos = TextIO::readFEN("r4rk1/1pp1qppp/3b1n2/4p3/2B1P1b1/1QN2N2/PP3PPP/R3R1K1 w - - 0 1");
        int ms1 = moveScore(pos, "Red1");
        int ms2 = moveScore(pos, "Rec1");
        int ms3 = moveScore(pos, "Rac1");
        int ms4 = moveScore(pos, "Rad1");
        ASSERT(ms1 > 0);        // Good to have rook on open file
        ASSERT(ms2 > 0);        // Good to have rook on half-open file
        ASSERT(ms1 > ms2);      // Open file better than half-open file
        ASSERT(ms3 > 0);
        ASSERT(ms4 > 0);
        ASSERT(ms4 > ms1);
        ASSERT(ms3 > ms2);

        pos = TextIO::readFEN("r3kb1r/p3pp1p/bpPq1np1/4N3/2pP4/2N1PQ2/P1PB1PPP/R3K2R b KQkq - 0 12");
        ASSERT(moveScore(pos, "O-O-O") > 0);    // Black long castle is bad for black
        pos.makeMove(TextIO::stringToMove(pos, "O-O-O"), ui);
        ASSERT(moveScore(pos, "O-O") > 0);      // White short castle is good for white

        pos = TextIO::readFEN("8/3k4/2p5/1pp5/1P1P4/3K4/8/8 w - - 0 1");
        int sc1 = moveScore(pos, "bxc5");
        int sc2 = moveScore(pos, "dxc5");
        ASSERT(sc1 < sc2);      // Don't give opponent a passed pawn.

        pos = TextIO::readFEN("8/pp1bk3/8/8/8/8/PPPBK3/8 w - - 0 1");
        sc1 = evalWhite(pos);
        pos.setPiece(Position::getSquare(3, 1), Piece::EMPTY);
        pos.setPiece(Position::getSquare(3, 0), Piece::WBISHOP);
        sc2 = evalWhite(pos);
        ASSERT(sc2 > sc1);      // Easier to win if bishops on same color

        // Test bishop mobility
        pos = TextIO::readFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
        sc1 = moveScore(pos, "Bd3");
        sc2 = moveScore(pos, "Bc4");
        ASSERT(sc2 > sc1);
    }

    /**
     * Test of pieceSquareEval method, of class Evaluate.
     */
    public void testPieceSquareEval() {
        Position pos = TextIO::readFEN(TextIO::startPosFEN);
        int score = evalWhite(pos);
        ASSERT_EQUAL(0, score);    // Should be zero, by symmetry
        UndoInfo ui = new UndoInfo();
        pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
        score = evalWhite(pos);
        ASSERT(score > 0);     // Centralizing a pawn is a good thing
        pos.makeMove(TextIO::stringToMove(pos, "e5"), ui);
        score = evalWhite(pos);
        ASSERT_EQUAL(0, score);    // Should be zero, by symmetry
        ASSERT(moveScore(pos, "Nf3") > 0);      // Developing knight is good
        pos.makeMove(TextIO::stringToMove(pos, "Nf3"), ui);
        ASSERT(moveScore(pos, "Nc6") < 0);      // Developing knight is good
        pos.makeMove(TextIO::stringToMove(pos, "Nc6"), ui);
        ASSERT(moveScore(pos, "Bb5") > 0);      // Developing bishop is good
        pos.makeMove(TextIO::stringToMove(pos, "Bb5"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Nge7"), ui);
        ASSERT(moveScore(pos, "Qe2") > 0);      // Queen away from edge is good
        score = evalWhite(pos);
        pos.makeMove(TextIO::stringToMove(pos, "Bxc6"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Nxc6"), ui);
        int score2 = evalWhite(pos);
        ASSERT(score2 < score);                 // Bishop worth more than knight in this case

        pos = TextIO::readFEN("5k2/4nppp/p1n5/1pp1p3/4P3/2P1BN2/PP3PPP/3R2K1 w - - 0 1");
        ASSERT(moveScore(pos, "Rd7") > 0);      // Rook on 7:th rank is good
        ASSERT(moveScore(pos, "Rd8") <= 0);     // Rook on 8:th rank not particularly good
        pos.setPiece(TextIO::getSquare("a1"), Piece::WROOK);
        ASSERT(moveScore(pos, "Rac1") > 0);     // Rook on c-f files considered good

        pos = TextIO::readFEN("r4rk1/pppRRppp/1q4b1/n7/8/2N3B1/PPP1QPPP/6K1 w - - 0 1");
        score = evalWhite(pos);
        ASSERT(score > 100); // Two rooks on 7:th rank is very good
    }

    /**
     * Test of tradeBonus method, of class Evaluate.
     */
    public void testTradeBonus() {
        std::string fen = "8/5k2/6r1/2p1p3/3p4/2P2N2/3PPP2/4K1R1 w - - 0 1";
        Position pos = TextIO::readFEN(fen);
        int score1 = evalWhite(pos);
        UndoInfo ui = new UndoInfo();
        pos.makeMove(TextIO::stringToMove(pos, "Rxg6"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Kxg6"), ui);
        int score2 = evalWhite(pos);
        ASSERT(score2 > score1);    // White ahead, trading pieces is good

        pos = TextIO::readFEN(fen);
        pos.makeMove(TextIO::stringToMove(pos, "cxd4"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "cxd4"), ui);
        score2 = evalWhite(pos);
        ASSERT(score2 < score1);    // White ahead, trading pawns is bad

        pos = TextIO::readFEN("8/8/1b2b3/4kp2/5N2/4NKP1/6B1/8 w - - 0 62");
        score1 = evalWhite(pos);
        pos.makeMove(TextIO::stringToMove(pos, "Nxe6"), ui);
        pos.makeMove(TextIO::stringToMove(pos, "Kxe6"), ui);
        score2 = evalWhite(pos);
        ASSERT(score2 > score1); // White ahead, trading pieces is good
    }

    /**
     * Test of material method, of class Evaluate.
     */
    public void testMaterial() {
        Position pos = TextIO::readFEN(TextIO::startPosFEN);
        ASSERT_EQUAL(0, Evaluate::material(pos));

        final int pV = Evaluate::pV;
        final int qV = Evaluate::qV;
        ASSERT(pV != 0);
        ASSERT(qV != 0);
        ASSERT(qV > pV);

        UndoInfo ui = new UndoInfo();
        pos.makeMove(TextIO::stringToMove(pos, "e4"), ui);
        ASSERT_EQUAL(0, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "d5"), ui);
        ASSERT_EQUAL(0, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "exd5"), ui);
        ASSERT_EQUAL(pV, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "Qxd5"), ui);
        ASSERT_EQUAL(0, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "Nc3"), ui);
        ASSERT_EQUAL(0, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "Qxd2"), ui);
        ASSERT_EQUAL(-pV, Evaluate::material(pos));
        pos.makeMove(TextIO::stringToMove(pos, "Qxd2"), ui);
        ASSERT_EQUAL(-pV+qV, Evaluate::material(pos));
    }

    /**
     * Test of kingSafety method, of class Evaluate.
     */
    public void testKingSafety() {
        Position pos = TextIO::readFEN("r3kb1r/p1p1pppp/b2q1n2/4N3/3P4/2N1PQ2/P2B1PPP/R3R1K1 w kq - 0 1");
        int s1 = evalWhite(pos);
        pos.setPiece(TextIO::getSquare("g7"), Piece::EMPTY);
        pos.setPiece(TextIO::getSquare("b7"), Piece::BPAWN);
        int s2 = evalWhite(pos);
        ASSERT(s2 < s1);    // Half-open g-file is bad for white

        // Trapping rook with own king is bad
        pos = TextIO::readFEN("rnbqk1nr/pppp1ppp/8/8/1bBpP3/8/PPP2PPP/RNBQK1NR w KQkq - 2 4");
        s1 = evalWhite(pos);
        pos.setPiece(TextIO::getSquare("e1"), Piece::EMPTY);
        pos.setPiece(TextIO::getSquare("f1"), Piece::WKING);
        s2 = evalWhite(pos);
        ASSERT(s2 < s1);

        pos = TextIO::readFEN("rnbqk1nr/pppp1ppp/8/8/1bBpPB2/8/PPP1QPPP/RN1K2NR w kq - 0 1");
        s1 = evalWhite(pos);
        pos.setPiece(TextIO::getSquare("d1"), Piece::EMPTY);
        pos.setPiece(TextIO::getSquare("c1"), Piece::WKING);
        s2 = evalWhite(pos);
        ASSERT(s2 < s1);
    }

    /**
     * Test of endGameEval method, of class Evaluate.
     */
    public void testEndGameEval() {
        Position pos = new Position();
        pos.setPiece(Position::getSquare(4, 1), Piece::WKING);
        pos.setPiece(Position::getSquare(4, 6), Piece::BKING);
        int score = evalWhite(pos);
        ASSERT_EQUAL(0, score);

        pos.setPiece(Position::getSquare(3, 1), Piece::WBISHOP);
        score = evalWhite(pos);
        ASSERT(Math.abs(score) < 50);   // Insufficient material to mate

        pos.setPiece(Position::getSquare(3, 1), Piece::WKNIGHT);
        score = evalWhite(pos);
        ASSERT(Math.abs(score) < 50);   // Insufficient material to mate

        pos.setPiece(Position::getSquare(3, 1), Piece::WROOK);
        score = evalWhite(pos);
        final int rV = Evaluate::rV;
        ASSERT(Math.abs(score) > rV + 100);   // Enough material to force mate

        pos.setPiece(Position::getSquare(3, 6), Piece::BBISHOP);
        score = evalWhite(pos);
        final int bV = Evaluate::bV;
        ASSERT(score >= 0);
        ASSERT(score < rV - bV);   // Insufficient excess material to mate

        pos.setPiece(Position::getSquare(5, 6), Piece::BROOK);
        score = evalWhite(pos);
        ASSERT(score <= 0);
        ASSERT(-score < bV);

        pos.setPiece(Position::getSquare(2, 6), Piece::BBISHOP);
        score = evalWhite(pos);
        ASSERT(-score > bV * 2 + 100);

        // KrpKn is win for white
        pos = TextIO::readFEN("8/3bk3/8/8/8/3P4/3RK3/8 w - - 0 1");
        score = evalWhite(pos);
        final int pV = Evaluate::pV;
        ASSERT(score > rV + pV - bV - 100);

        // KNNK is a draw
        pos = TextIO::readFEN("8/8/4k3/8/8/3NK3/3N4/8 w - - 0 1");
        score = evalWhite(pos);
        ASSERT(Math.abs(score) < 50);

        pos = TextIO::readFEN("8/8/3k4/8/8/3NK3/2B5/8 b - - 0 1");
        score = evalWhite(pos);
        final int nV = Evaluate::nV;
        ASSERT(score > bV + nV + 150);  // KBNK is won, should have a bonus
        score = moveScore(pos, "Kc6");
        ASSERT(score > 0);      // Black king going into wrong corner, good for white
        score = moveScore(pos, "Ke6");
        ASSERT(score < 0);      // Black king going away from wrong corner, good for black

        // KRN vs KR is generally drawn
        pos = TextIO::readFEN("rk/p/8/8/8/8/NKR/8 w - - 0 1");
        score = evalWhite(pos);
        ASSERT(score < nV - 2 * pV);

        // KRKB, defending king should prefer corner that bishop cannot attack
        pos = TextIO::readFEN("6B1/8/8/8/8/2k5/4r3/2K5 w - - 0 93");
        score = evalWhite(pos);
        ASSERT(score >= -pV);
        score = moveScore(pos, "Kd1");
        ASSERT(score < 0);
        score = moveScore(pos, "Kb1");
        ASSERT(score > 0);
    }

    /**
     * Test of endGameEval method, of class Evaluate.
     */
    public void testPassedPawns() {
        Position pos = TextIO::readFEN("8/8/8/P3k/8/8/p/K w");
        int score = evalWhite(pos);
        ASSERT(score > 300); // Unstoppable passed pawn
        pos.whiteMove = false;
        score = evalWhite(pos);
        ASSERT(score <= 0); // Not unstoppable

        pos = TextIO::readFEN("4R3/8/8/3K4/8/4pk2/8/8 w - - 0 1");
        score = evalWhite(pos);
        pos.setPiece(TextIO::getSquare("d5"), Piece::EMPTY);
        pos.setPiece(TextIO::getSquare("d4"), Piece::WKING);
        int score2 = evalWhite(pos);
        ASSERT(score2 > score); // King closer to passed pawn promotion square

        // Connected passed pawn test. Disabled because it didn't help in tests
//        pos = TextIO::readFEN("4k3/8/8/4P3/3P1K2/8/8/8 w - - 0 1");
//        score = evalWhite(pos);
//        pos.setPiece(TextIO::getSquare("d4"), Piece::EMPTY);
//        pos.setPiece(TextIO::getSquare("d5"), Piece::WPAWN);
//        score2 = evalWhite(pos);
//        ASSERT(score2 > score); // Advancing passed pawn is good
    }

    /**
     * Test of endGameEval method, of class Evaluate.
     */
    public void testBishAndRookPawns() {
        final int pV = Evaluate::pV;
        final int bV = Evaluate::bV;
        final int winScore = pV + bV;
        final int drawish = (pV + bV) / 20;
        Position pos = TextIO::readFEN("k7/8/8/8/2B5/2K5/P7/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("k7/8/8/8/3B4/2K5/P7/8 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);

        pos = TextIO::readFEN("8/2k5/8/8/3B4/2K5/P7/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("8/2k5/8/8/3B4/2K4P/8/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("8/2k5/8/8/4B3/2K4P/8/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/8/8 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);

        pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/7P/8 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);

        pos = TextIO::readFEN("8/6k1/8/8/2B1B3/2K4P/7P/8 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);

        pos = TextIO::readFEN("8/6k1/8/2B5/4B3/2K4P/7P/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("8/6k1/8/8/4B3/2K4P/P7/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        pos = TextIO::readFEN("8/6k1/8/8/4B3/2K3PP/8/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);
    }

    public void testTrappedBishop() {
        Position pos = TextIO::readFEN("r2q1rk1/ppp2ppp/3p1n2/8/3P4/1P1Q1NP1/b1P2PBP/2KR3R w - - 0 1");
        ASSERT(evalWhite(pos) > 0); // Black has trapped bishop

        pos = TextIO::readFEN("r2q2k1/pp1b1p1p/2p2np1/3p4/3P4/1BNQ2P1/PPPB1P1b/2KR4 w - - 0 1");
        ASSERT(evalWhite(pos) > 0); // Black has trapped bishop
    }

    /**
     * Test of endGameEval method, of class Evaluate.
     */
    public void testKQKP() {
        final int pV = Evaluate::pV;
        final int qV = Evaluate::qV;
        final int winScore = qV - pV - 200;
        final int drawish = (pV + qV) / 20;

        // Pawn on a2
        Position pos = TextIO::readFEN("8/8/1K6/8/8/Q7/p7/1k6 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);
        pos = TextIO::readFEN("8/8/8/1K6/8/Q7/p7/1k6 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);
        pos = TextIO::readFEN("3Q4/8/8/8/K7/8/1kp5/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);

        // Pawn on c2
        pos = TextIO::readFEN("3Q4/8/8/8/3K4/8/1kp5/8 w - - 0 1");
        ASSERT(evalWhite(pos) < drawish);
        pos = TextIO::readFEN("3Q4/8/8/8/8/4K3/1kp5/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);
    }

    public void testKRKP() {
        final int pV = Evaluate::pV;
        final int rV = Evaluate::rV;
        final int winScore = rV - pV;
        final int drawish = (pV + rV) / 20;
        Position pos = TextIO::readFEN("6R1/8/8/8/5K2/2kp4/8/8 w - - 0 1");
        ASSERT(evalWhite(pos) > winScore);
        pos.whiteMove = !pos.whiteMove;
        ASSERT(evalWhite(pos) < drawish);
    }

    public void testCantWin() {
        Position pos = TextIO::readFEN("8/8/8/3k4/3p4/3K4/4N3/8 w - - 0 1");
        int score1 = evalWhite(pos);
        ASSERT(score1 < 0);
        UndoInfo ui = new UndoInfo();
        pos.makeMove(TextIO::stringToMove(pos, "Nxd4"), ui);
        int score2 = evalWhite(pos);
        ASSERT(score2 <= 0);
        ASSERT(score2 > score1);
    }

    /** Return static evaluation score for white, regardless of whose turn it is to move. */
    final static int evalWhite(const Position& pos) {
        Evaluate eval = new Evaluate();
        int ret = eval.evalPos(pos);
        Position symPos = swapColors(pos);
        int symScore = eval.evalPos(symPos);
        ASSERT_EQUAL(ret, symScore);
        if (!pos.whiteMove) {
            ret = -ret;
        }
        return ret;
    }
#endif

Position
swapColors(const Position& pos) {
    Position sym;
    sym.whiteMove = !pos.whiteMove;
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            int p = pos.getPiece(Position::getSquare(x, y));
            p = Piece::isWhite(p) ? Piece::makeBlack(p) : Piece::makeWhite(p);
            sym.setPiece(Position::getSquare(x, 7-y), p);
        }
    }

    int castleMask = 0;
    if (pos.a1Castle()) castleMask |= 1 << Position::A8_CASTLE;
    if (pos.h1Castle()) castleMask |= 1 << Position::H8_CASTLE;
    if (pos.a8Castle()) castleMask |= 1 << Position::A1_CASTLE;
    if (pos.h8Castle()) castleMask |= 1 << Position::H1_CASTLE;
    sym.setCastleMask(castleMask);

    if (pos.getEpSquare() >= 0) {
        int x = Position::getX(pos.getEpSquare());
        int y = Position::getY(pos.getEpSquare());
        sym.setEpSquare(Position::getSquare(x, 7-y));
    }

    sym.halfMoveClock = pos.halfMoveClock;
    sym.fullMoveCounter = pos.fullMoveCounter;

    return sym;
}

#if 0
    /** Compute change in eval score for white after making "moveStr" in position "pos". */
    private final int moveScore(const Position& pos, const std::string& moveStr) {
        int score1 = evalWhite(pos);
        Position tmpPos = new Position(pos);
        UndoInfo ui = new UndoInfo();
        tmpPos.makeMove(TextIO::stringToMove(tmpPos, moveStr), ui);
        int score2 = evalWhite(tmpPos);
//        printf("move:%s s1:%d s2:%d\n", moveStr, score1, score2);
        return score2 - score1;
    }
#endif



cute::suite
EvaluateTest::getSuite() const {
    cute::suite s;
//    s.push_back(CUTE());
#if 0
    s.push_back(CUTE(testEvalPos));
    s.push_back(CUTE(testPieceSquareEval));
    s.push_back(CUTE(testTradeBonus));
    s.push_back(CUTE(testMaterial));
    s.push_back(CUTE(testKingSafety));
    s.push_back(CUTE(testEndGameEval));
    s.push_back(CUTE(testPassedPawns));
    s.push_back(CUTE(testBishAndRookPawns));
    s.push_back(CUTE(testTrappedBishop));
    s.push_back(CUTE(testKQKP));
    s.push_back(CUTE(testKRKP));
    s.push_back(CUTE(testCantWin));
#endif
    return s;
}
