/*
 * bookbuild.cpp
 *
 *  Created on: Apr 27, 2014
 *      Author: petero
 */

#include "bookbuild.hpp"
#include "gametree.hpp"
#include "moveGen.hpp"
#include "search.hpp"
#include "textio.hpp"

namespace BookBuild {

void
BookNode::updateNegaMax(const BookData& bookData,
                        bool updateThis, bool updateChildren, bool updateParents) {
    if (!updateThis && (negaMaxScore != INVALID_SCORE))
        return;
    if (updateChildren) {
        for (auto& e : children)
            e.second->updateNegaMax(bookData, false, true, false);
    }
    bool propagate = computeNegaMax(bookData);
    if (updateParents && propagate) {
        for (auto& e : parents) {
            std::shared_ptr<BookNode> parent = e.second.lock();
            assert(parent);
            parent->updateNegaMax(bookData, true, false, true);
        }
    }
}

bool
BookNode::computeNegaMax(const BookData& bookData) {
    const int oldNM = negaMaxScore;
    const int oldEW = expansionCostWhite;
    const int oldEB = expansionCostBlack;

    negaMaxScore = searchScore;
    auto it = children.find(bestNonBookMove.getCompressedMove());
    if (it != children.end()) {
        // Ignore searchScore if a child node contains information about the same move
        if (it->second->getNegaMaxScore() != INVALID_SCORE)
            negaMaxScore = IGNORE_SCORE;
    }
    if (negaMaxScore != INVALID_SCORE)
        for (const auto& e : children)
            negaMaxScore = std::max(negaMaxScore, negateScore(e.second->negaMaxScore));

    expansionCostWhite = IGNORE_SCORE;
    expansionCostBlack = IGNORE_SCORE;
    if (!bookData.isPending(hashKey)) {
        if (searchScore == INVALID_SCORE) {
            expansionCostWhite = INVALID_SCORE;
            expansionCostBlack = INVALID_SCORE;
        } else if (searchScore != IGNORE_SCORE) {
            expansionCostWhite = getExpansionCost(bookData, nullptr, true);
            expansionCostBlack = getExpansionCost(bookData, nullptr, false);
        }
    }
    for (const auto& e : children) {
        if (e.second->expansionCostWhite == INVALID_SCORE)
            expansionCostWhite = INVALID_SCORE;
        if (e.second->expansionCostBlack == INVALID_SCORE)
            expansionCostBlack = INVALID_SCORE;
    }

    for (const auto& e : children) {
        std::shared_ptr<BookNode> child = e.second;
        if ((expansionCostWhite != INVALID_SCORE) &&
            (child->expansionCostWhite != IGNORE_SCORE)) {
            int cost = getExpansionCost(bookData, child, true);
            if ((expansionCostWhite == IGNORE_SCORE) || (expansionCostWhite > cost))
                expansionCostWhite = cost;
        }
        if ((expansionCostBlack != INVALID_SCORE) &&
            (child->expansionCostBlack != IGNORE_SCORE)) {
            int cost = getExpansionCost(bookData, child, false);
            if ((expansionCostBlack == IGNORE_SCORE) || (expansionCostBlack > cost))
                expansionCostBlack = cost;
        }
    }

    return ((negaMaxScore != oldNM) ||
            (expansionCostWhite != oldEW) ||
            (expansionCostBlack != oldEB));
}

int
BookNode::getExpansionCost(const BookData& bookData, const std::shared_ptr<BookNode>& child,
                           bool white) const {
    const int ownCost = bookData.ownPathErrorCost();
    const int otherCost = bookData.otherPathErrorCost();
    if (child) {
        int moveError = (negaMaxScore == INVALID_SCORE) ? 1000 :
                        negaMaxScore - negateScore(child->negaMaxScore);
        assert(moveError >= 0);
        bool wtm = getDepth() % 2 == 0;
        int cost = white ? child->expansionCostWhite : child->expansionCostBlack;
        if (cost != IGNORE_SCORE && cost != INVALID_SCORE)
            cost += bookData.bookDepthCost() + moveError * (wtm == white ? ownCost : otherCost);
        return cost;
    } else {
        if (children.find(bestNonBookMove.getCompressedMove()) != children.end()) {
            return -10000; // bestNonBookMove is obsoleted by a child node
        } else {
            int moveError = negaMaxScore - searchScore;
            assert(moveError >= 0);
            bool wtm = getDepth() % 2 == 0;
            int ownCost = bookData.ownPathErrorCost();
            int otherCost = bookData.otherPathErrorCost();
            return moveError * (wtm == white ? ownCost : otherCost);
        }
    }
}

int
BookNode::negateScore(int score) {
    if (score == IGNORE_SCORE || score == INVALID_SCORE)
        return score; // No negation
    if (SearchConst::isWinScore(score))
        return -(score-1);
    if (SearchConst::isLoseScore(score))
        return -(score+1);
    return -score;
}

void
BookNode::setSearchResult(const BookData& bookData,
                          const Move& bestMove, int score, int time) {
    bestNonBookMove = bestMove;
    searchScore = score;
    searchTime = time;
    updateNegaMax(bookData);
}

void
BookNode::updateDepth() {
    bool updated = false;
    for (auto& e : parents) {
        std::shared_ptr<BookNode> parent = e.second.lock();
        assert(parent);
        if (parent->depth == INT_MAX)
            parent->updateDepth();
        assert(parent->depth >= 0);
        assert(parent->depth < INT_MAX);
        if (depth != INT_MAX)
            assert((depth - parent->depth) % 2 != 0);
        if (depth > parent->depth + 1) {
            depth = parent->depth + 1;
            updated = true;
        }
    }
    if (updated)
        for (auto& e : children)
            e.second->updateDepth();
}

// ----------------------------------------------------------------------------

Book::Book(const std::string& backupFile0, int bookDepthCost,
           int ownPathErrorCost, int otherPathErrorCost)
    : startPosHash(TextIO::readFEN(TextIO::startPosFEN).bookHash()),
      backupFile(backupFile0),
      bookData(bookDepthCost, ownPathErrorCost, otherPathErrorCost) {
    addRootNode();
    if (!backupFile.empty())
        writeToFile(backupFile);
}

void
Book::improve(const std::string& bookFile, int searchTime, const std::string& startMoves) {
    readFromFile(bookFile);

    Position startPos = TextIO::readFEN(TextIO::startPosFEN);
    UndoInfo ui;
    std::vector<std::string> startMoveVec;
    splitString(startMoves, startMoveVec);
    for (const auto& ms : startMoveVec) {
        Move m = TextIO::stringToMove(startPos, ms);
        startPos.makeMove(m, ui);
    }

    class DropoutSelector : public PositionSelector {
    public:
        DropoutSelector(Book& b, U64 startPosHash) :
            book(b), startHash(startPosHash), whiteBook(true) {
        }

        bool getNextPosition(Position& pos, Move& move) override {
            std::shared_ptr<BookNode> ptr = book.bookNodes.find(startHash)->second;
            while (true) {
                int cost = whiteBook ? ptr->getExpansionCostWhite() : ptr->getExpansionCostBlack();
                if (cost == IGNORE_SCORE)
                    return false;
                std::vector<std::shared_ptr<BookNode>> goodChildren;
                for (const auto& e : ptr->getChildren()) {
                    std::shared_ptr<BookNode> child = e.second;
                    int childCost = ptr->getExpansionCost(book.bookData, child, whiteBook);
                    if (cost == childCost)
                        goodChildren.push_back(child);
                }
                if (!goodChildren.empty()) {
                    std::random_shuffle(goodChildren.begin(), goodChildren.end());
                    ptr = goodChildren[0];
                } else
                    break;
            }
            move = ptr->getBestNonBookMove();
            if (ptr->getChildren().find(move.getCompressedMove()) != ptr->getChildren().end())
                move = Move();
            std::vector<Move> moveList;
            book.getPosition(ptr->getHashKey(), pos, moveList);
            whiteBook = !whiteBook;
            return true;
        }

    private:
        Book& book;
        U64 startHash;
        bool whiteBook;
    };

    DropoutSelector selector(*this, startPos.bookHash());
    extendBook(selector, searchTime);

    writeToFile(bookFile + ".out");
}

void
Book::importPGN(const std::string& bookFile, const std::string& pgnFile) {
    readFromFile(bookFile);

    // Create book nodes for all positions in the PGN file
    std::ifstream is(pgnFile);
    GameTree gt(is);
    int nGames = 0;
    int nAdded = 0;
    while (gt.readPGN()) {
        nGames++;
        GameNode gn = gt.getRootNode();
        std::function<void(void)> addToBook = [&]() {
            Position base = gn.getPos();
            for (int i = 0; i < gn.nChildren(); i++) {
                gn.goForward(i);
                if (bookNodes.find(gn.getPos().bookHash()) == bookNodes.end()) {
                    assert(!gn.getMove().isEmpty());
                    std::vector<U64> toSearch;
                    addPosToBook(base, gn.getMove(), toSearch);
                    nAdded++;
                }
                addToBook();
                gn.goBack();
            }
        };
        addToBook();
    }
    std::cout << "Added " << nAdded << " positions from " << nGames << " games" << std::endl;
}

void
Book::exportPolyglot(const std::string& bookFile, const std::string& polyglotFile,
                     int maxPathError) {
    readFromFile(bookFile);
}

void
Book::interactiveQuery(const std::string& bookFile) {
    readFromFile(bookFile);

    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    std::vector<Move> movePath;

    for (std::string prevStr, cmdStr; true; prevStr = cmdStr) {
        printBookInfo(pos, movePath);
        std::cout << "Command:" << std::flush;
        std::getline(std::cin, cmdStr);
        if (!std::cin)
            break;
        if (cmdStr.length() == 0)
            cmdStr = prevStr;

        if (cmdStr ==  "q")
            break;

        if (startsWith(cmdStr, "?")) {
            std::cout << "  d n        - Go to child \"n\"" << std::endl;
            std::cout << "  move       - Go to child \"move\"" << std::endl;
            std::cout << "  u [levels] - Move up" << std::endl;
            std::cout << "  h key      - Go to node with given hash key" << std::endl;
            std::cout << "  q          - Quit" << std::endl;
            std::cout << "  ?          - Print this help" << std::endl;
            continue;
        }
        std::vector<Move> childMoves;
        std::shared_ptr<BookNode> node = getBookNode(pos.bookHash());
        assert(node);
        getOrderedChildMoves(*node, childMoves);
        Move childMove;
        for (const Move& m : childMoves) {
            std::string moveS = TextIO::moveToString(pos, m, false);
            if (toLowerCase(moveS) == toLowerCase(cmdStr)) {
                childMove = m;
                break;
            }
        }
        if (childMove.isEmpty() && startsWith(cmdStr, "d")) {
            std::vector<std::string> split;
            splitString(cmdStr, split);
            if (split.size() > 1) {
                int moveNo = -1;
                if (str2Num(split[1], moveNo) && (moveNo >= 0) &&
                        (moveNo < (int)childMoves.size()))
                    childMove = childMoves[moveNo];
            }
        }
        if (!childMove.isEmpty()) {
            UndoInfo ui;
            pos.makeMove(childMove, ui);
            movePath.push_back(childMove);
        } else if (startsWith(cmdStr, "u")) {
            int levels = 1;
            std::vector<std::string> split;
            splitString(cmdStr, split);
            if (split.size() > 1)
                str2Num(split[1], levels);
            while (levels > 0 && !movePath.empty()) {
                movePath.pop_back();
                levels--;
            }
            pos = TextIO::readFEN(TextIO::startPosFEN);
            for (const Move& m : movePath) {
                UndoInfo ui;
                pos.makeMove(m, ui);
            }
        } else if (startsWith(cmdStr, "h")) {
            std::vector<std::string> split;
            splitString(cmdStr, split);
            if (split.size() > 1) {
                std::string tmp = split[1];
                if (startsWith(tmp, "0x"))
                    tmp = tmp.substr(2);
                U64 hashKey;
                if (hexStr2Num(tmp, hashKey)) {
                    Position tmpPos;
                    std::vector<Move> moveList;
                    if (getPosition(hashKey, tmpPos, moveList)) {
                        pos = tmpPos;
                        movePath = moveList;
                    }
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------

void
Book::addRootNode() {
    if (bookNodes.find(startPosHash) == bookNodes.end()) {
        auto rootNode(std::make_shared<BookNode>(startPosHash, true));
        rootNode->setState(BookNode::INITIALIZED);
        bookNodes[startPosHash] = rootNode;
        Position pos = TextIO::readFEN(TextIO::startPosFEN);
        setChildRefs(pos);
        writeBackup(*rootNode);
    }
}

void
Book::readFromFile(const std::string& filename) {
    bookNodes.clear();
    hashToParent.clear();
    bookData.clearPending();

    // Read all book entries
    std::ifstream is;
    is.open(filename.c_str(), std::ios_base::in |
                              std::ios_base::binary);

    std::set<U64> zeroTime;
    while (true) {
        BookNode::BookSerializeData bsd;
        is.read((char*)&bsd.data[0], sizeof(bsd.data));
        if (!is)
            break;
        auto bn(std::make_shared<BookNode>(0));
        bn->deSerialize(bsd);
        if (bn->getSearchTime() == 0) {
            zeroTime.insert(bn->getHashKey());
        } else {
            zeroTime.erase(bn->getHashKey());
        }
        if (bn->getHashKey() == startPosHash)
            bn->setRootNode();
        bookNodes[bn->getHashKey()] = bn;
    }
    is.close();

    // Find positions for all book entries by exploring moves from the starting position
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    initPositions(pos);
    addRootNode();

    // Initialize all negamax scores
    bookNodes.find(startPosHash)->second->updateNegaMax(bookData);

    if (!backupFile.empty())
        writeToFile(backupFile);

    std::cout << "nZeroTime:" << zeroTime.size() << std::endl;
}

void
Book::writeToFile(const std::string& filename) {
    std::ofstream os;
    os.open(filename.c_str(), std::ios_base::out |
                              std::ios_base::binary |
                              std::ios_base::trunc);
    os.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    for (const auto& e : bookNodes) {
        auto& node = e.second;
        BookNode::BookSerializeData bsd;
        node->serialize(bsd);
        os.write((const char*)&bsd.data[0], sizeof(bsd.data));
    }
    os.close();
}

void
Book::extendBook(PositionSelector& selector, int searchTime) {
    const int numThreads = 1;
    TranspositionTable tt(27);

    SearchScheduler scheduler;
    for (int i = 0; i < numThreads; i++) {
        auto sr = std::make_shared<SearchRunner>(i, tt);
        scheduler.addWorker(sr);
    }
    scheduler.startWorkers();

    int numPending = 0;
    const int desiredQueueLen = numThreads + 1;
    int workId = 0;   // Work unit ID number
    int commitId = 0; // Next work unit to be stored in opening book
    std::set<SearchScheduler::WorkUnit> completed; // Completed but not yet committed to book
    while (true) {
        bool workAdded = false;
        if (numPending < desiredQueueLen) {
            Position pos;
            Move move;
            if (selector.getNextPosition(pos, move)) {
                assert(bookNodes.find(pos.bookHash()) != bookNodes.end());
                std::vector<U64> toSearch;
                if (move.isEmpty()) {
                    toSearch.push_back(pos.bookHash());
                } else
                    addPosToBook(pos, move, toSearch);
                for (U64 hKey : toSearch) {
                    Position pos2;
                    std::vector<Move> moveList;
                    if (!getPosition(hKey, pos2, moveList))
                        assert(false);
                    SearchScheduler::WorkUnit wu;
                    wu.id = workId++;
                    wu.hashKey = hKey;
                    wu.gameMoves = moveList;
                    wu.movesToSearch = getMovesToSearch(pos2);
                    wu.searchTime = searchTime;
                    scheduler.addWorkUnit(wu);
                    numPending++;
                    addPending(hKey);
                    workAdded = true;
                }
            }
        }
        if (!workAdded && (numPending == 0))
            break;
        if (!workAdded || (numPending >= desiredQueueLen)) {
            SearchScheduler::WorkUnit wu;
            scheduler.getResult(wu);
            completed.insert(wu);
            while (!completed.empty() && completed.begin()->id == commitId) {
                wu = *completed.begin();
                completed.erase(completed.begin());
                numPending--;
                commitId++;
                removePending(wu.hashKey);
                auto it = bookNodes.find(wu.hashKey);
                assert(it != bookNodes.end());
                BookNode& bn = *it->second;
                bn.setSearchResult(bookData,
                                   wu.bestMove, wu.bestMove.score(), wu.searchTime);
                writeBackup(bn);
                scheduler.reportResult(wu);
            }
        }
    }
}

std::vector<Move>
Book::getMovesToSearch(Position& pos) {
    std::vector<Move> ret;
    MoveList moves;
    MoveGen::pseudoLegalMoves(pos, moves);
    MoveGen::removeIllegal(pos, moves);
    UndoInfo ui;
    for (int i = 0; i < moves.size; i++) {
        const Move& m = moves[i];
        pos.makeMove(m, ui);
        auto it = bookNodes.find(pos.bookHash());
        bool include = it == bookNodes.end();
        if (!include) {
            if (!bookData.isPending(pos.bookHash())) {
                BookNode& bn = *it->second;
                if (bn.getNegaMaxScore() == INVALID_SCORE)
                    include = true;
            }
        }
        if (include)
            ret.push_back(m);
        pos.unMakeMove(m, ui);
    }
    return ret;
}

void
Book::addPending(U64 hashKey) {
    bookData.addPending(hashKey);
    auto it = bookNodes.find(hashKey);
    assert(it != bookNodes.end());
    it->second->updateNegaMax(bookData);
}

void
Book::removePending(U64 hashKey) {
    bookData.removePending(hashKey);
    auto it = bookNodes.find(hashKey);
    assert(it != bookNodes.end());
    it->second->updateNegaMax(bookData);
}

void
Book::addPosToBook(Position& pos, const Move& move, std::vector<U64>& toSearch) {
    assert(bookNodes.find(pos.bookHash()) != bookNodes.end());

    UndoInfo ui;
    pos.makeMove(move, ui);
    U64 childHash = pos.bookHash();
    assert(bookNodes.find(childHash) == bookNodes.end());
    auto childNode = std::make_shared<BookNode>(childHash);

    bookNodes[childHash] = childNode;

    toSearch.push_back(pos.bookHash());

    auto b = hashToParent.lower_bound(std::make_pair(childHash, (U64)0));
    auto e = hashToParent.lower_bound(std::make_pair(childHash+1, (U64)0));
    int nParents = 0;
    for (auto p = b; p != e; ++p) {
        assert(p->first == childHash);
        U64 parentHash = p->second;
        auto it2 = bookNodes.find(parentHash);
        assert(it2 != bookNodes.end());
        nParents++;
        std::shared_ptr<BookNode> parent = it2->second;

        Position pos2;
        std::vector<Move> dummyMoves;
        bool ok = getPosition(parentHash, pos2, dummyMoves);
        assert(ok);

        MoveList moves;
        MoveGen::pseudoLegalMoves(pos2, moves);
        MoveGen::removeIllegal(pos2, moves);
        Move move2;
        bool found = false;
        for (int i = 0; i < moves.size; i++) {
            UndoInfo ui2;
            pos2.makeMove(moves[i], ui2);
            if (pos2.bookHash() == childHash) {
                move2 = moves[i];
                found = true;
                break;
            }
            pos2.unMakeMove(moves[i], ui2);
        }
        assert(found);

        parent->addChild(move2.getCompressedMove(), childNode);
        childNode->addParent(move2.getCompressedMove(), parent);
        toSearch.push_back(parent->getHashKey());
    }
    assert(nParents > 0);

    setChildRefs(pos);
    childNode->updateNegaMax(bookData);

    pos.unMakeMove(move, ui);
    childNode->setState(BookNode::State::INITIALIZED);
    writeBackup(*childNode);
}

bool
Book::getPosition(U64 hashKey, Position& pos, std::vector<Move>& moveList) const {
    if (hashKey == startPosHash) {
        pos = TextIO::readFEN(TextIO::startPosFEN);
        return true;
    }

    auto it = bookNodes.find(hashKey);
    if (it == bookNodes.end())
        return false;
    const auto& parents = it->second->getParents();
    assert(!parents.empty());
    std::shared_ptr<BookNode> parent = parents.begin()->second.lock();
    assert(parent);
    bool ok = getPosition(parent->getHashKey(), pos, moveList);
    assert(ok);

    Move m;
    m.setFromCompressed(parents.begin()->first);
    UndoInfo ui;
    pos.makeMove(m, ui);
    moveList.push_back(m);
    return true;
}

std::shared_ptr<BookNode>
Book::getBookNode(U64 hashKey) const {
    auto it = bookNodes.find(hashKey);
    if (it == bookNodes.end())
        return nullptr;
    return it->second;
}

void
Book::initPositions(Position& pos) {
    const U64 hash = pos.bookHash();
    const auto it = bookNodes.find(hash);
    if (it == bookNodes.end())
        return;
    std::shared_ptr<BookNode>& node = it->second;

    setChildRefs(pos);
    for (auto& e : node->getChildren()) {
        if (e.second->getState() == BookNode::DESERIALIZED) {
            UndoInfo ui;
            Move m;
            m.setFromCompressed(e.first);
            pos.makeMove(m, ui);
            initPositions(pos);
            pos.unMakeMove(m, ui);
        }
    }
    node->setState(BookNode::INITIALIZED);
}

void
Book::setChildRefs(Position& pos) {
    const U64 hash = pos.bookHash();
    const auto it = bookNodes.find(hash);
    assert(it != bookNodes.end());
    std::shared_ptr<BookNode>& node = it->second;

    MoveList moves;
    MoveGen::pseudoLegalMoves(pos, moves);
    MoveGen::removeIllegal(pos, moves);
    UndoInfo ui;
    for (int i = 0; i < moves.size; i++) {
        pos.makeMove(moves[i], ui);
        U64 childHash = pos.bookHash();
        hashToParent.insert(std::make_pair(childHash, hash));
        auto it2 = bookNodes.find(childHash);
        if (it2 != bookNodes.end()) {
            auto child = it2->second;
            node->addChild(moves[i].getCompressedMove(), child);
            child->addParent(moves[i].getCompressedMove(), node);
        }
        pos.unMakeMove(moves[i], ui);
    }
}

void
Book::writeBackup(const BookNode& bookNode) {
    if (backupFile.empty())
        return;
    std::ofstream os;
    os.open(backupFile.c_str(), std::ios_base::out |
                                std::ios_base::binary |
                                std::ios_base::app);
    os.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    BookNode::BookSerializeData bsd;
    bookNode.serialize(bsd);
    os.write((const char*)&bsd.data[0], sizeof(bsd.data));
    os.close();
}

void
Book::printBookInfo(Position& pos, const std::vector<Move>& movePath) const {
    std::cout << TextIO::asciiBoard(pos);
    std::cout << TextIO::toFEN(pos) << std::endl;
    std::cout << num2Hex(pos.bookHash()) << std::endl;
    std::shared_ptr<BookNode> node = getBookNode(pos.bookHash());
    if (!node) {
        std::cout << "not in book" << std::endl;
        return;
    }
    {
        UndoInfo ui;
        std::string moves;
        Position tmpPos = TextIO::readFEN(TextIO::startPosFEN);
        for (const Move& m : movePath) {
            if (!moves.empty())
                moves += ' ';
            moves += TextIO::moveToString(tmpPos, m, false);
            tmpPos.makeMove(m, ui);
        }
        std::cout << (moves.empty() ? "root position" : moves) << std::endl;
    }
    {
        Position tmpPos;
        std::vector<Move> moveList;
        if (!getPosition(pos.bookHash(), tmpPos, moveList)) {
            std::cout << "error, getPosition failed" << std::endl;
            return;
        }
        UndoInfo ui;
        std::string moves;
        tmpPos = TextIO::readFEN(TextIO::startPosFEN);
        for (const Move& m : moveList) {
            if (!moves.empty())
                moves += ' ';
            moves += TextIO::moveToString(tmpPos, m, false);
            tmpPos.makeMove(m, ui);
        }
        std::cout << (moves.empty() ? "root position" : moves) << std::endl;
    }

    std::vector<Move> childMoves;
    getOrderedChildMoves(*node, childMoves);
    for (size_t mi = 0; mi < childMoves.size(); mi++) {
        const Move& childMove = childMoves[mi];
        auto it = node->getChildren().find(childMove.getCompressedMove());
        assert(it != node->getChildren().end());
        std::shared_ptr<BookNode> child = it->second;
        int negaMaxScore = child->getNegaMaxScore();
        if (pos.isWhiteMove())
            negaMaxScore = BookNode::negateScore(negaMaxScore);
        int expandCostW = node->getExpansionCost(bookData, child, true);
        int expandCostB = node->getExpansionCost(bookData, child, false);
        std::cout << std::setw(2) << mi << ' '
                  << std::setw(6) << TextIO::moveToString(pos, childMove, false) << ' '
                  << std::setw(6) << negaMaxScore << ' '
                  << std::setw(6) << expandCostW << ' '
                  << std::setw(6) << expandCostB << std::endl;
    }

    std::string moveS = node->getBestNonBookMove().isEmpty() ? "--" :
                        TextIO::moveToString(pos, node->getBestNonBookMove(), false);
    std::cout << "-- "
              << std::setw(6) << moveS << ' '
              << std::setw(6) << node->getSearchScore() * (pos.isWhiteMove() ? 1 : -1) << ' '
              << std::setw(6) << node->getExpansionCost(bookData, nullptr, true) << ' '
              << std::setw(6) << node->getExpansionCost(bookData, nullptr, false) << ' '
              << std::setw(8) << node->getSearchTime() << std::endl;
}

void
Book::getOrderedChildMoves(const BookNode& node, std::vector<Move>& moves) const {
    std::vector<std::pair<int,U16>> childMoves;
    for (const auto& e : node.getChildren()) {
        Move childMove;
        childMove.setFromCompressed(e.first);
        std::shared_ptr<BookNode> child = e.second;
        int score = -BookNode::negateScore(child->getNegaMaxScore());
        childMoves.push_back(std::make_pair(score,
                                            childMove.getCompressedMove()));
    }
    std::sort(childMoves.begin(), childMoves.end());
    moves.clear();
    for (const auto& e : childMoves) {
        Move m;
        m.setFromCompressed(e.second);
        moves.push_back(m);
    }
}

// ----------------------------------------------------------------------------

SearchRunner::SearchRunner(int instanceNo0, TranspositionTable& tt0)
    : instanceNo(instanceNo0), tt(tt0) {
}

Move
SearchRunner::analyze(const std::vector<Move>& gameMoves,
                      const std::vector<Move>& movesToSearch,
                      int searchTime) {
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    UndoInfo ui;
    std::vector<U64> posHashList(200 + gameMoves.size());
    int posHashListSize = 0;
    for (const Move& m : gameMoves) {
        posHashList[posHashListSize++] = pos.zobristHash();
        pos.makeMove(m, ui);
        if (pos.getHalfMoveClock() == 0)
            posHashListSize = 0;
    }

    if (movesToSearch.empty()) {
        MoveList legalMoves;
        MoveGen::pseudoLegalMoves(pos, legalMoves);
        MoveGen::removeIllegal(pos, legalMoves);
        Move bestMove;
        int bestScore = IGNORE_SCORE;
        if (legalMoves.size == 0) {
            if (MoveGen::inCheck(pos))
                bestScore = -SearchConst::MATE0;
            else
                bestScore = 0; // stalemate
        }
        bestMove.setScore(bestScore);
        return bestMove;
    }

    kt.clear();
    ht.init();
    Search::SearchTables st(tt, kt, ht, et);
    ParallelData pd(tt);
    Search sc(pos, posHashList, posHashListSize, st, pd, nullptr, treeLog);

    int minTimeLimit = searchTime;
    int maxTimeLimit = searchTime;
    sc.timeLimit(minTimeLimit, maxTimeLimit);

    MoveList moveList;
    for (const Move& m : movesToSearch)
        moveList.addMove(m.from(), m.to(), m.promoteTo());

    int maxDepth = -1;
    S64 maxNodes = -1;
    bool verbose = false;
    int maxPV = 1;
    bool onlyExact = true;
    int minProbeDepth = 1;
    Move bestMove = sc.iterativeDeepening(moveList, maxDepth, maxNodes, verbose, maxPV,
                                          onlyExact, minProbeDepth);
    return bestMove;
}

SearchScheduler::SearchScheduler()
    : stopped(false) {
}

SearchScheduler::~SearchScheduler() {
    stopWorkers();
}

void
SearchScheduler::addWorker(const std::shared_ptr<SearchRunner>& sr) {
    workers.push_back(sr);
}

void
SearchScheduler::startWorkers() {
    for (auto w : workers) {
        SearchRunner& sr = *w;
        auto thread = std::make_shared<std::thread>([this,&sr]() {
            workerLoop(sr);
        });
        threads.push_back(thread);
    }
}

void
SearchScheduler::stopWorkers() {
    {
        std::lock_guard<std::mutex> L(mutex);
        stopped = true;
        pendingCv.notify_all();
    }
    for (auto& t : threads) {
        t->join();
        t.reset();
    }
}

void
SearchScheduler::addWorkUnit(const WorkUnit& wu) {
    std::lock_guard<std::mutex> L(mutex);
    bool empty = pending.empty();
    pending.push_back(wu);
    if (empty)
        pendingCv.notify_all();
}

void
SearchScheduler::getResult(WorkUnit& wu) {
    std::unique_lock<std::mutex> L(mutex);
    while (complete.empty())
        completeCv.wait(L);
    wu = complete.front();
    complete.pop_front();
}

void
SearchScheduler::reportResult(const WorkUnit& wu) const {
    Position pos = TextIO::readFEN(TextIO::startPosFEN);
    UndoInfo ui;
    std::string moves;
    for (const Move& m : wu.gameMoves) {
        if (!moves.empty())
            moves += ' ';
        moves += TextIO::moveToString(pos, m, false);
        pos.makeMove(m, ui);
    }
    std::set<std::string> excluded;
    MoveList legalMoves;
    MoveGen::pseudoLegalMoves(pos, legalMoves);
    MoveGen::removeIllegal(pos, legalMoves);
    for (int i = 0; i < legalMoves.size; i++) {
        const Move& m = legalMoves[i];
        if (!contains(wu.movesToSearch, m))
            excluded.insert(TextIO::moveToString(pos, m, false));
    }
    std::string excludedS;
    for (const std::string& m : excluded) {
        if (!excludedS.empty())
            excludedS += ' ';
        excludedS += m;
    }

    int score = wu.bestMove.score();
    if (!pos.isWhiteMove())
        score = -score;

    std::string bestMove = TextIO::moveToString(pos, wu.bestMove, false);

    std::cout << std::setw(5) << std::right << wu.id << ' '
              << std::setw(6) << std::right << score << ' '
              << std::setw(6) << std::left  << bestMove << ' '
              << std::setw(6) << std::right << wu.searchTime << " : "
              << moves << " : "
              << excludedS << " : "
              << TextIO::toFEN(pos)
              << std::endl;
}

void
SearchScheduler::workerLoop(SearchRunner& sr) {
    while (true) {
        WorkUnit wu;
        {
            std::unique_lock<std::mutex> L(mutex);
            while (!stopped && pending.empty())
                pendingCv.wait(L);
            if (stopped)
                return;
            wu = pending.front();
            pending.pop_front();
        }
        wu.bestMove = sr.analyze(wu.gameMoves, wu.movesToSearch, wu.searchTime);
        wu.instNo = sr.instNo();
        {
            std::unique_lock<std::mutex> L(mutex);
            bool empty = complete.empty();
            complete.push_back(wu);
            if (empty)
                completeCv.notify_all();
        }
    }
}

} // Namespace BookBuild
