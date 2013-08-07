/*
    Texel - A UCI chess engine.
    Copyright (C) 2013  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * parallel.hpp
 *
 *  Created on: Jul 9, 2013
 *      Author: petero
 */

#ifndef PARALLEL_HPP_
#define PARALLEL_HPP_

#include "killerTable.hpp"
#include "history.hpp"
#include "transpositionTable.hpp"
#include "evaluate.hpp"
#include "searchUtil.hpp"

#include <memory>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>


class Search;
class SearchTreeInfo;

class ParallelData;
class SplitPoint;
class SplitPointMove;
class FailHighInfo;


class WorkerThread {
public:
    /** Constructor. Does not start new thread. */
    WorkerThread(int threadNo, ParallelData& pd, TranspositionTable& tt);

    /** Destructor. Waits for thread to terminate. */
    ~WorkerThread();

    /** Start thread. */
    void start();

    /** Tell thread to stop. */
    void stop(bool wait);

    /** Returns true if thread should stop searching. */
    bool shouldStop() const { return stopThread; }

    /** Return true if thread is running. */
    bool threadRunning() const { return thread != nullptr; }

    /** Return thread number. The first worker thread is number 1. */
    int getThreadNo() const { return threadNo; }

    /** For debugging. */
    double getPUseful() const { return pUseful; }

private:
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    /** Thread main loop. */
    void mainLoop();

    int threadNo;
    ParallelData& pd;
    std::shared_ptr<std::thread> thread;

    std::shared_ptr<Evaluate::EvalHashTables> et;
    std::shared_ptr<KillerTable> kt;
    std::shared_ptr<History> ht;
    TranspositionTable& tt;

    double pUseful; // Probability that thread is currently doing something useful, for debugging

    volatile bool stopThread;
};


/** Priority queue of pending search tasks. Handles thread safety. */
class WorkQueue {
    friend class ParallelTest;
public:
    /** Constructor. */
    WorkQueue(std::condition_variable& cv, FailHighInfo& fhInfo);

    /** Reset dynamic minimum split depth to default value. */
    void resetSplitDepth();

    /** Add SplitPoint to work queue. */
    void addWork(const std::shared_ptr<SplitPoint>& sp);

    /** Get best move for helper thread to work on. */
    std::shared_ptr<SplitPoint> getWork(int& moveNo, ParallelData& pd, int threadNo);

    /** A helper thread stopped working on a move before it was finished. */
    void returnMove(const std::shared_ptr<SplitPoint>& sp, int moveNo);

    /** Set which move number the SplitPoint owner is currently searching. */
    void setOwnerCurrMove(const std::shared_ptr<SplitPoint>& sp, int moveNo, int alpha);

    /** Cancel this SplitPoint and all children. */
    void cancel(const std::shared_ptr<SplitPoint>& sp);

    /** Set move to canceled after helper thread finished searching it. */
    void moveFinished(const std::shared_ptr<SplitPoint>& sp, int moveNo, bool cancelRemaining);

    /** Return probability that the best unstarted move needs to be searched.
     *  Also return the corresponding SplitPoint. */
    double getBestProbability(std::shared_ptr<SplitPoint>& bestSp) const;
    double getBestProbability() const;

    /** Return current dynamic minimum split depth. */
    int getMinSplitDepth() const;

private:
    /** Move sp to waiting if it has no unstarted moves. */
    void maybeMoveToWaiting(const std::shared_ptr<SplitPoint>& sp);

    /** Insert sp in queue. Notify condition variable if queue becomes non-empty. */
    void insertInQueue(const std::shared_ptr<SplitPoint>& sp);

    /** Recompute probabilities for "sp" and all children. Update "queue" and "waiting". */
    void updateProbabilities(const std::shared_ptr<SplitPoint>& sp);

    struct SplitPointCompare {
        bool operator()(const std::shared_ptr<SplitPoint>& a,
                        const std::shared_ptr<SplitPoint>& b) const;
    };

    /** Move sp and all its children from spSet to spVec. */
    static void removeFromSet(const std::shared_ptr<SplitPoint>& sp,
                              std::set<std::shared_ptr<SplitPoint>, SplitPointCompare>& spSet,
                              std::vector<std::shared_ptr<SplitPoint>>& spVec);

    /** Cancel "sp" and all children. Assumes mutex already locked. */
    void cancelInternal(const std::shared_ptr<SplitPoint>& sp);

    void printSpTree(std::ostream& os, const ParallelData& pd,
                     int threadNo, const std::shared_ptr<SplitPoint> selectedSp,
                     int selectedMove);
    void findLeaves(const std::shared_ptr<SplitPoint>& sp, std::vector<int>& parentThreads,
                    std::vector<std::shared_ptr<SplitPoint>>& leaves);

    /** Scoped lock that measures lock contention and adjusts minSplitDepth accordingly. */
    class Lock {
    public:
        Lock(const WorkQueue* wq0);
    private:
        const WorkQueue& wq;
        std::unique_lock<std::mutex> lock;
    };
    friend class Lock;

    mutable int minSplitDepth;      // Dynamic minimum split depth
    mutable U64 nContended;         // Number of times mutex has been contended
    mutable U64 nNonContended;      // Number of times mutex has not been contended

    std::condition_variable& cv;
    FailHighInfo& fhInfo;
    mutable std::mutex mutex;

    // SplitPoints with unstarted SplitPointMoves
    std::set<std::shared_ptr<SplitPoint>, SplitPointCompare> queue;

    // SplitPoints with no unstarted SplitPointMoves
    std::set<std::shared_ptr<SplitPoint>, SplitPointCompare> waiting;
};


/** Fail high statistics, for estimating SplitPoint usefulness probabilities. */
class FailHighInfo {
public:
    /** Constructor. */
    FailHighInfo();

    /** Return probability that move moveNo needs to be searched.
     * @param parentMoveNo  Move number of move leading to this position.
     * @param currMoveNo    Move currently being searched.
     * @param moveNo        Move number to get probability for.
     * @param allNode       True if this is an expected ALL node. */
    double getMoveNeededProbability(int parentMoveNo, int currMoveNo, int moveNo, bool allNode) const;

    /** Return probability that move moveNo needs to be searched in a PV node.
     * @param currMoveNo    Move currently being searched.
     * @param moveNo        Move number to get probability for.
     * @param allNode       True if this is an expected ALL node. */
    double getMoveNeededProbabilityPv(int currMoveNo, int moveNo) const;

    /** Add fail high information.
     * @param parentMoveNo  Move number of move leading to this position.
     * @param nSearched     Number of moves searched at this node.
     * @param failHigh      True if the node failed high.
     * @param allNode       True if this is an expected ALL node. */
    void addData(int parentMoveNo, int nSearched, bool failHigh, bool allNode);

    /** Add "alpha increased" information for PV nodes.
     * @param  nSearched     Number of moves searched at this node.
     * @param  alphaChanged  True if alpha increased after searching move. */
    void addPvData(int nSearched, bool alphaChanged);

    /** Rescale the counters so that future updates have more weight. */
    void reScale();

    /** Print object state to "os", for debugging. */
    void print(std::ostream& os) const;

private:
    void reScaleInternal(int factor);
    void reScalePv(int factor);

    inline int getNodeType(int moveNo, bool allNode) const;


    static const int NUM_NODE_TYPES = 4;
    static const int NUM_STAT_MOVES = 15;
    mutable std::mutex mutex;

    int failHiCount[NUM_NODE_TYPES][NUM_STAT_MOVES]; // [parentMoveNo>0?1:0][moveNo]
    int failLoCount[NUM_NODE_TYPES];                 // [parentMoveNo>0?1:0]
    int totCount;                                    // Sum of all counts

    int newAlpha[NUM_STAT_MOVES];
    int totPvCount;
};


/** Top-level parallel search data structure. */
class ParallelData {
public:
    /** Constructor. */
    ParallelData(TranspositionTable& tt);

    /** Create/delete worker threads so that there are numWorkers in total. */
    void addRemoveWorkers(int numWorkers);

    /** Start all worker threads. */
    void startAll();

    /** Stop all worker threads. */
    void stopAll();


    /** Return number of helper threads in use. */
    int numHelperThreads() const;

    /** Get number of nodes searched by all helper threads. */
    S64 getNumSearchedNodes() const;

    /** Add nNodes to total number of searched nodes. */
    void addSearchedNodes(S64 nNodes);


    /** For debugging. */
    const WorkerThread& getHelperThread(int i) const { return *threads[i]; }

    // Notified when wq becomes non-empty and when search should stop
    std::condition_variable cv;

    FailHighInfo fhInfo;

    WorkQueue wq;

    // Move played in Search::iterativeDeepening before calling negaScout. For debugging.
    Move topMove;

private:
    /** Vector of helper threads. Master thread not included. */
    std::vector<std::shared_ptr<WorkerThread>> threads;

    TranspositionTable& tt;

    std::atomic<S64> totalHelperNodes; // Number of nodes searched by all helper threads
};


/** SplitPoint does not handle thread safety, WorkQueue must do that.  */
class SplitPoint {
    friend class ParallelTest;
public:
    /** Constructor. */
    SplitPoint(int threadNo,
               const std::shared_ptr<SplitPoint>& parentSp, int parentMoveNo,
               const Position& pos, const std::vector<U64>& posHashList,
               int posHashListSize, const SearchTreeInfo& sti,
               const KillerTable& kt, const History& ht,
               int alpha, int beta, int ply);

    /** Add a child SplitPoint */
    void addChild(const std::weak_ptr<SplitPoint>& child);

    /** Add a move to the SplitPoint. */
    void addMove(int moveNo, const SplitPointMove& spMove);

    /** Assign sequence number. */
    void setSeqNo();

    /** compute pSpUseful and pnextMoveUseful. */
    void computeProbabilities(const FailHighInfo& fhInfo);

    /** Get parent SplitPoint, or null for root SplitPoint. */
    std::shared_ptr<SplitPoint> getParent() const;

    /** Get children SplitPoints. */
    const std::vector<std::weak_ptr<SplitPoint>>& getChildren() const;


    U64 getSeqNo() const { return seqNo; }
    double getPSpUseful() const { return pSpUseful; }
    double getPNextMoveUseful() const { return pNextMoveUseful; }
    const History& getHistory() const { return ht; }
    const KillerTable& getKillerTable() const { return kt; }
    const SplitPointMove& getSpMove(int moveNo) const { return spMoves[moveNo]; }

    /** Return probability that moveNo needs to be searched. */
    double getPMoveUseful(const FailHighInfo& fhInfo, int moveNo) const;

    void getPos(Position& pos, const Move& move) const;
    void getPosHashList(const Position& pos, std::vector<U64>& posHashList,
                        int& posHashListSize) const;
    const SearchTreeInfo& getSearchTreeInfo() const { return searchTreeInfo; }
    int getAlpha() const { return alpha; }
    int getBeta() const { return beta; }
    int getPly() const { return ply; }


    /** Get index of first unstarted move. Mark move as being searched. */
    int getNextMove();

    /** A helper thread stopped working on a move before it was finished. */
    void returnMove(int moveNo);

    /** Set which move number the SplitPoint owner is currently searching. */
    void setOwnerCurrMove(int moveNo, int alpha);

    /** Cancel this SplitPoint and all children. */
    void cancel();

    /** Return true if this SplitPoint has been canceled. */
    bool isCanceled() const;

    /** Set move to canceled after helper thread finished searching it. */
    void moveFinished(int moveNo, bool cancelRemaining);

    /** Return true if there are moves that have not been started to be searched. */
    bool hasUnStartedMove() const;

    /** Return true if there are moves that have not been finished (canceled) yet. */
    bool hasUnFinishedMove() const;

    /** Return true if this SplitPoint is an ancestor to "sp". */
    bool isAncestorTo(const SplitPoint& sp) const;

    /** Return true if some other thread is helping the SplitPoint owner. */
    bool hasHelperThread() const;

    /** Return true if the held SplitPoint is an estimated ALL node. */
    bool isAllNode() const;

    /** Return true if beta > alpha + 1. */
    bool isPvNode() const;

    /** Thread that created this SplitPoint. */
    int owningThread() const { return threadNo; }

    /** Print object state to "os", for debugging. */
    void print(std::ostream& os, int level, const FailHighInfo& fhInfo) const;

    /** For debugging. */
    int getParentMoveNo() const { return parentMoveNo; }

    /** For debugging. */
    int getCurrMoveNo() const { return currMoveNo; }

    /** Get index of first unstarted move, or -1 if there is no unstarted move. */
    int findNextMove() const;

private:
    SplitPoint(const SplitPoint&) = delete;
    SplitPoint& operator=(const SplitPoint&) = delete;

    /** Return probability that moveNo needs to be searched, by calling corresponding
     * function in fhInfo. */
    double getMoveNeededProbability(const FailHighInfo& fhInfo, int moveNo) const;

    /** Remove null entries from children vector. */
    void cleanUpChildren();

    const Position pos;
    const std::vector<U64> posHashList; // List of hashes for previous positions up to the last "zeroing" move.
    const int posHashListSize;
    const SearchTreeInfo searchTreeInfo;   // For ply-1
    const KillerTable& kt;
    const History& ht;

    int alpha;
    const int beta;
    const int ply;

    double pSpUseful;       // Probability that this SplitPoint is needed. 100% if parent is null.
    double pNextMoveUseful; // Probability that next unstarted move needs to be searched.

    const int threadNo;     // Owning thread
    const std::shared_ptr<SplitPoint> parent;
    const int parentMoveNo; // Move number in parent SplitPoint that generated this SplitPoint
    std::vector<std::weak_ptr<SplitPoint>> children;

    static U64 nextSeqNo;
    U64 seqNo;      // To break ties when two objects have same priority. Set by addWork() under lock
    int currMoveNo;
    std::vector<SplitPointMove> spMoves;

    bool canceled;
};


/** Represents one move at a SplitPoint. */
class SplitPointMove {
public:
    /** Constructor */
    SplitPointMove(const Move& move, int lmr, int depth,
                   int captSquare, bool inCheck);

    const Move& getMove() const { return move; }
    int getLMR() const { return lmr; }
    int getDepth() const { return depth; }
    int getRecaptureSquare() const { return captSquare; }
    bool getInCheck() const { return inCheck; }

    bool isCanceled() const { return canceled; }
    void setCanceled(bool value) { canceled = value; }

    bool isSearching() const { return searching; }
    void setSearching(bool value) { searching = value; }

private:
    Move move;      // Position defined by sp->pos + move
    int lmr;        // Amount of LMR reduction
    int depth;
    int captSquare; // Recapture square, or -1 if no recapture
    bool inCheck;   // True if side to move is in check

    bool canceled;  // Result is no longer needed
    bool searching; // True if currently searched by a helper thread
};


/** Handle a SplitPoint object using RAII. */
class SplitPointHolder {
public:
    /** Constructor. */
    SplitPointHolder(ParallelData& pd, std::vector<std::shared_ptr<SplitPoint>>& spVec);

    /** Destructor. Cancel SplitPoint. */
    ~SplitPointHolder();

    /** Set the SplitPoint object. */
    void setSp(const std::shared_ptr<SplitPoint>& sp);

    /** Add a move to the SplitPoint. */
    void addMove(int moveNo, const SplitPointMove& spMove);

    /** Add SplitPoint to work queue. */
    void addToQueue();

    /** Set which move number the SplitPoint owner is currently searching. */
    void setOwnerCurrMove(int moveNo, int alpha);

    /** For debugging. */
    int getSeqNo() const { return sp->getSeqNo(); }

    /** Return true if the held SplitPoint is an estimated ALL node. */
    bool isAllNode() const;

    /** Return true if some other thread is helping the help SplitPoint. */
    bool hasHelperThread() const { return sp->hasHelperThread(); }

private:
    SplitPointHolder(const SplitPointHolder&) = delete;
    SplitPointHolder& operator=(const SplitPointHolder&) = delete;

    ParallelData& pd;
    std::vector<std::shared_ptr<SplitPoint>>& spVec;
    std::shared_ptr<SplitPoint> sp;
    enum class State { EMPTY, CREATED, QUEUED } state;
};


inline bool
WorkQueue::SplitPointCompare::operator()(const std::shared_ptr<SplitPoint>& a,
                                         const std::shared_ptr<SplitPoint>& b) const {
    int pa = (int)(a->getPNextMoveUseful() * 100);
    int pb = (int)(b->getPNextMoveUseful() * 100);
    if (pa != pb)
        return pa > pb;
    if (a->getPly() != b->getPly())
        return a->getPly() < b->getPly();
    return a->getSeqNo() < b->getSeqNo();
}

inline int
WorkQueue::getMinSplitDepth() const {
    return minSplitDepth;
}

inline int
FailHighInfo::getNodeType(int moveNo, bool allNode) const {
    if (moveNo == 0)
        return allNode ? 0 : 3;
    else if (moveNo > 0)
        return 1;
    else
        return 2;
}

inline bool
SplitPoint::isCanceled() const {
    return canceled;
}

#endif /* PARALLEL_HPP_ */
