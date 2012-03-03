/*
 * gameTest.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "gameTest.hpp"
#include "game.hpp"
#include "humanPlayer.hpp"
#include "textio.hpp"

#include <iostream>

#include "cute.h"

/**
 * Test of haveDrawOffer method, of class Game.
 */
static void
testHaveDrawOffer() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(false, game.haveDrawOffer());

    bool res = game.processString("e4");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(false, game.haveDrawOffer());

    res = game.processString("draw offer e5");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Draw offer does not imply draw
    ASSERT_EQUAL(Piece::BPAWN, game.pos.getPiece(Position::getSquare(4, 4))); // e5 move made

    res = game.processString("draw offer Nf3");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Draw offer does not imply draw
    ASSERT_EQUAL(Piece::WKNIGHT, game.pos.getPiece(Position::getSquare(5, 2))); // Nf3 move made

    res = game.processString("Nc6");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(false, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    ASSERT_EQUAL(Piece::BKNIGHT, game.pos.getPiece(Position::getSquare(2, 5))); // Nc6 move made

    res = game.processString("draw offer Bb5");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    ASSERT_EQUAL(Piece::WBISHOP, game.pos.getPiece(Position::getSquare(1, 4))); // Bb5 move made

    res = game.processString("draw accept");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Game::DRAW_AGREE, game.getGameState());    // Draw by agreement

    res = game.processString("undo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Piece::EMPTY, game.pos.getPiece(Position::getSquare(1, 4))); // Bb5 move undone
    ASSERT_EQUAL(false, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    res = game.processString("undo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Piece::EMPTY, game.pos.getPiece(Position::getSquare(2, 5))); // Nc6 move undone
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());

    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Piece::BKNIGHT, game.pos.getPiece(Position::getSquare(2, 5))); // Nc6 move redone
    ASSERT_EQUAL(false, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Piece::WBISHOP, game.pos.getPiece(Position::getSquare(1, 4))); // Bb5 move redone
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(true, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Can't redo draw accept

    // Test draw offer in connection with invalid move
    res = game.processString("new");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(false, game.haveDrawOffer());
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());

    res = game.processString("draw offer e5");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(TextIO::startPosFEN, TextIO::toFEN(game.pos));   // Move invalid, not executed
    res = game.processString("e4");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(true, game.haveDrawOffer());   // Previous draw offer still valid
    ASSERT_EQUAL(Piece::WPAWN, game.pos.getPiece(Position::getSquare(4, 3))); // e4 move made

    // Undo/redo shall clear "pendingDrawOffer".
    game.processString("new");
    game.processString("e4");
    game.processString("draw offer e4");       // Invalid black move
    ASSERT_EQUAL(true, game.pendingDrawOffer);
    game.processString("undo");
    game.processString("redo");
    game.processString("e5");
    ASSERT_EQUAL(true,game.pos.whiteMove);
    ASSERT_EQUAL(false, game.haveDrawOffer());
}

/**
 * Test of draw by 50 move rule, of class Game.
 */
static void
testDraw50() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(false, game.haveDrawOffer());
    bool res = game.processString("draw 50");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Draw claim invalid
    res = game.processString("e4");
    ASSERT_EQUAL(true, game.haveDrawOffer());   // Invalid claim converted to draw offer

    std::string cmd = "setpos 8/4k3/8/P7/8/8/8/1N2K2R w K - 99 83";
    res = game.processString(cmd);
    ASSERT_EQUAL(true, res);
    res = game.processString("draw 50");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());      // Draw claim invalid

    game.processString(cmd);
    game.processString("draw 50 Nc3");
    ASSERT_EQUAL(Game::DRAW_50, game.getGameState());    // Draw claim valid
    ASSERT_EQUAL("Game over, draw by 50 move rule! [Nc3]", game.getGameStateString());

    game.processString(cmd);
    game.processString("draw 50 a6");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());      // Pawn move resets counter
    ASSERT_EQUAL(Piece::WPAWN, game.pos.getPiece(Position::getSquare(0, 5))); // Move a6 made

    game.processString(cmd);
    game.processString("draw 50 O-O");
    ASSERT_EQUAL(Game::DRAW_50, game.getGameState());    // Castling doesn't reset counter

    game.processString(cmd);
    game.processString("draw 50 Kf2");
    ASSERT_EQUAL(Game::DRAW_50, game.getGameState());    // Loss of castling right doesn't reset counter

    game.processString(cmd);
    game.processString("draw 50 Ke3");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Ke3 is invalid
    ASSERT_EQUAL(true,game.pos.whiteMove);
    game.processString("a6");
    ASSERT_EQUAL(true, game.haveDrawOffer());   // Previous invalid claim converted to offer
    game.processString("draw 50");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());  // 50 move counter reset.
    res = game.processString("draw accept");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(Game::DRAW_AGREE, game.getGameState()); // Can accept previous implicit offer

    cmd = "setpos 3k4/R7/3K4/8/8/8/8/8 w - - 99 78";
    game.processString(cmd);
    game.processString("Ra8");
    ASSERT_EQUAL(Game::WHITE_MATE, game.getGameState());
    game.processString("draw 50");
    ASSERT_EQUAL(Game::WHITE_MATE, game.getGameState()); // Can't claim draw when game over
    ASSERT_EQUAL(Game::ALIVE, game.drawState);
}

/**
 * Test of draw by repetition, of class Game.
 */
static void
testDrawRep() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(false, game.haveDrawOffer());
    game.processString("Nc3");
    game.processString("Nc6");
    game.processString("Nb1");
    game.processString("Nb8");
    game.processString("Nf3");
    game.processString("Nf6");
    game.processString("Ng1");
    ASSERT_EQUAL(false, game.haveDrawOffer());
    game.processString("draw rep");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Claim not valid, one more move needed
    game.processString("draw rep Nc6");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());    // Claim not valid, wrong move claimed
    ASSERT_EQUAL(Piece::BKNIGHT, game.pos.getPiece(Position::getSquare(2, 5)));   // Move Nc6 made
    ASSERT_EQUAL(true, game.haveDrawOffer());
    game.processString("undo");
    ASSERT_EQUAL(false, game.haveDrawOffer());
    ASSERT_EQUAL(Piece::EMPTY, game.pos.getPiece(Position::getSquare(2, 5)));
    game.processString("draw rep Ng8");
    ASSERT_EQUAL(Game::DRAW_REP, game.getGameState());
    ASSERT_EQUAL(Piece::EMPTY, game.pos.getPiece(Position::getSquare(6, 7))); // Ng8 not played

    // Test draw by repetition when a "potential ep square but not real ep square" position is present.
    game.processString("new");
    game.processString("e4");   // e3 is not a real epSquare here
    game.processString("Nf6");
    game.processString("Nf3");
    game.processString("Ng8");
    game.processString("Ng1");
    game.processString("Nf6");
    game.processString("Nf3");
    game.processString("Ng8");
    game.processString("draw rep Ng1");
    ASSERT_EQUAL(Game::DRAW_REP, game.getGameState());

    // Now check the case when e3 *is* an epSquare
    game.processString("new");
    game.processString("Nf3");
    game.processString("d5");
    game.processString("Ng1");
    game.processString("d4");
    game.processString("e4");   // Here e3 is a real epSquare
    game.processString("Nf6");
    game.processString("Nf3");
    game.processString("Ng8");
    game.processString("Ng1");
    game.processString("Nf6");
    game.processString("Nf3");
    game.processString("Ng8");
    game.processString("draw rep Ng1");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());

    // EP capture not valid because it would leave the king in check. Therefore
    // the position has been repeated three times at the end of the move sequence.
    game.processString("setpos 4k2n/8/8/8/4p3/8/3P4/3KR2N w - - 0 1");
    game.processString("d4");
    game.processString("Ng6");
    game.processString("Ng3");
    game.processString("Nh8");
    game.processString("Nh1");
    game.processString("Ng6");
    game.processString("Ng3");
    game.processString("Nh8");
    game.processString("draw rep Nh1");
    ASSERT_EQUAL(Game::DRAW_REP, game.getGameState());
}

/**
 * Test of resign command, of class Game.
 */
static void
testResign() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.processString("f3");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.processString("resign");
    ASSERT_EQUAL(Game::RESIGN_BLACK, game.getGameState());
    game.processString("undo");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.processString("f3");
    game.processString("e5");
    game.processString("resign");
    ASSERT_EQUAL(Game::RESIGN_WHITE, game.getGameState());
    game.processString("undo");
    game.processString("e5");
    game.processString("g4");
    game.processString("Qh4");
    ASSERT_EQUAL(Game::BLACK_MATE, game.getGameState());
    game.processString("resign");
    ASSERT_EQUAL(Game::BLACK_MATE, game.getGameState());   // Can't resign after game over
}

/**
 * Test of processString method, of class Game.
 */
static void
testProcessString() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(TextIO::startPosFEN, TextIO::toFEN(game.pos));
    bool res = game.processString("Nf3");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(1, game.pos.halfMoveClock);
    ASSERT_EQUAL(1, game.pos.fullMoveCounter);
    res = game.processString("d5");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(0, game.pos.halfMoveClock);
    ASSERT_EQUAL(2, game.pos.fullMoveCounter);

    res = game.processString("undo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(1, game.pos.halfMoveClock);
    ASSERT_EQUAL(1, game.pos.fullMoveCounter);
    res = game.processString("undo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(TextIO::startPosFEN, TextIO::toFEN(game.pos));
    res = game.processString("undo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(TextIO::startPosFEN, TextIO::toFEN(game.pos));

    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(1, game.pos.halfMoveClock);
    ASSERT_EQUAL(1, game.pos.fullMoveCounter);
    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(0, game.pos.halfMoveClock);
    ASSERT_EQUAL(2, game.pos.fullMoveCounter);
    res = game.processString("redo");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(0, game.pos.halfMoveClock);
    ASSERT_EQUAL(2, game.pos.fullMoveCounter);

    res = game.processString("new");
    ASSERT_EQUAL(true, res);
    ASSERT_EQUAL(TextIO::startPosFEN, TextIO::toFEN(game.pos));

    std::string fen = "8/8/8/4k3/8/8/2p5/5K2 b - - 47 68";
    Position pos = TextIO::readFEN(fen);
    res = game.processString("setpos " + fen);
    ASSERT_EQUAL(true, res);
    ASSERT(pos.equals(game.pos));

    res = game.processString("junk");
    ASSERT_EQUAL(false, res);
}

/**
 * Test of getGameState method, of class Game.
 */
static void
testGetGameState() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.processString("f3");
    game.processString("e5");
    game.processString("g4");
    game.processString("Qh4");
    ASSERT_EQUAL(Game::BLACK_MATE, game.getGameState());

    game.processString("setpos 5k2/5P2/5K2/8/8/8/8/8 b - - 0 1");
    ASSERT_EQUAL(Game::BLACK_STALEMATE, game.getGameState());
}

/**
 * Test of insufficientMaterial method, of class Game.
 */
static void
testInsufficientMaterial() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.processString("setpos 4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());
    const int a1 = Position::getSquare(0, 0);
    game.pos.setPiece(a1, Piece::WROOK);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.pos.setPiece(a1, Piece::BQUEEN);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.pos.setPiece(a1, Piece::WPAWN);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.pos.setPiece(a1, Piece::BKNIGHT);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());
    game.pos.setPiece(a1, Piece::WBISHOP);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());

    const int c1 = Position::getSquare(2, 0);
    game.pos.setPiece(c1, Piece::WKNIGHT);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
    game.pos.setPiece(c1, Piece::BBISHOP);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());
    game.pos.setPiece(c1, Piece::WBISHOP);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());

    const int b2 = Position::getSquare(1, 1);
    game.pos.setPiece(b2, Piece::WBISHOP);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());
    game.pos.setPiece(b2, Piece::BBISHOP);
    ASSERT_EQUAL(Game::DRAW_NO_MATE, game.getGameState());

    const int b3 = Position::getSquare(1, 2);
    game.pos.setPiece(b3, Piece::WBISHOP);
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());

    // Can't force mate with KNNK, but still not an automatic draw.
    game.processString("setpos 8/8/8/8/8/8/8/K3nnk1 w - - 0 1");
    ASSERT_EQUAL(Game::ALIVE, game.getGameState());
}

static void
doTestPerfT(Position& pos, int maxDepth, U64 expectedNodeCounts[]) {
    for (int d = 1; d <= maxDepth; d++) {
        S64 t0 = currentTimeMillis();
        U64 nodes = Game::perfT(pos, d);
        S64 t1 = currentTimeMillis();
        std::stringstream ss;
        ss.precision(3);
        ss << "perft(" << d << ") = " << nodes
           << ", t=" << std::fixed << ((t1 - t0)*1e-3) << 's';
        std::cout << ss.str() << std::endl;
        ASSERT_EQUAL(expectedNodeCounts[d-1], nodes);
    }
}

/**
 * Test of perfT method, of class Game.
 */
static void
testPerfT() {
    HumanPlayer hp1, hp2;
    Game game(hp1, hp2);
    game.processString("new");
    U64 n1[] = { 20, 400, 8902, 197281, 4865609, 119060324, 3195901860ULL, 84998978956ULL};
    doTestPerfT(game.pos, 5, n1);

    game.processString("setpos 8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
    U64 n2[] = { 14, 191, 2812, 43238, 674624, 11030083, 178633661 };
    doTestPerfT(game.pos, 5, n2);

    game.processString("setpos r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
    U64 n3[] = { 48, 2039, 97862, 4085603, 193690690 };
    doTestPerfT(game.pos, 4, n3);
}


cute::suite
GameTest::getSuite() const {
    cute::suite s;
    s.push_back(CUTE(testHaveDrawOffer));
    s.push_back(CUTE(testDraw50));
    s.push_back(CUTE(testDrawRep));
    s.push_back(CUTE(testResign));
    s.push_back(CUTE(testProcessString));
    s.push_back(CUTE(testGetGameState));
    s.push_back(CUTE(testInsufficientMaterial));
    s.push_back(CUTE(testPerfT));
    return s;
}
