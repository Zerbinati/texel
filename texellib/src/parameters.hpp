/*
    Texel - A UCI chess engine.
    Copyright (C) 2012-2013  Peter Österlund, peterosterlund2@gmail.com

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
 * parameters.hpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#ifndef PARAMETERS_HPP_
#define PARAMETERS_HPP_

#include "util/util.hpp"
#include "piece.hpp"

#include <memory>
#include <map>
#include <string>
#include <cassert>


namespace EvaluateNS {
    void computeKnightMobility();
}


/** Handles all UCI parameters. */
class Parameters {
public:
    enum Type {
        CHECK,
        SPIN,
        COMBO,
        BUTTON,
        STRING
    };

    /** Base class for UCI parameters. */
    struct ParamBase {
        std::string name;
        Type type;

        ParamBase(const std::string& n, Type t) : name(n), type(t) { }

        virtual bool getBoolPar() const { assert(false); return false; }
        virtual int getIntPar() const { assert(false); return 0; }
        virtual std::string getStringPar() const { assert(false); return ""; }
        virtual void set(const std::string& value) { assert(false); }
    private:
        ParamBase(const ParamBase& other) = delete;
        ParamBase& operator=(const ParamBase& other) = delete;
    };

    /** A boolean parameter. */
    struct CheckParam : public ParamBase {
        bool value;
        bool defaultValue;

        CheckParam(const std::string& name, bool def)
            : ParamBase(name, CHECK) {
            this->value = def;
            this->defaultValue = def;
        }

        virtual bool getBoolPar() const { return value; }

        virtual void set(const std::string& value) {
            if (toLowerCase(value) == "true")
                this->value = true;
            else if (toLowerCase(value) == "false")
                this->value = false;
        }
    };

    class IntRef {
    public:
        IntRef(int& value) : valueP(&value) {}
        void set(int v) { *valueP = v; }
        int get() const { return *valueP; }
    private:
        int* valueP;
    };

    struct SpinParamBase : public ParamBase {
        SpinParamBase(const std::string& name) : ParamBase(name, SPIN) {}
        virtual int getDefaultValue() const = 0;
        virtual int getMinValue() const = 0;
        virtual int getMaxValue() const = 0;
    };

    /** An integer parameter. */
    template <typename Ref>
    struct SpinParamRef : public SpinParamBase {
        int minValue;
        int maxValue;
        Ref value;
        int defaultValue;

        SpinParamRef(const std::string& name, int minV, int maxV, int def, Ref valueR)
            : SpinParamBase(name), value(valueR) {
            this->minValue = minV;
            this->maxValue = maxV;
            this->value.set(def);
            this->defaultValue = def;
        }

        virtual int getIntPar() const { return value.get(); }

        virtual void set(const std::string& value) {
            int val;
            if (str2Num(value, val) && (val >= minValue) && (val <= maxValue))
                this->value.set(val);
        }

        int getDefaultValue() const { return defaultValue; }
        int getMinValue() const { return minValue; }
        int getMaxValue() const { return maxValue; }
    };

    struct SpinParam : public SpinParamRef<IntRef> {
        int value;
        SpinParam(const std::string& name, int minV, int maxV, int def)
            : SpinParamRef(name, minV, maxV, def, IntRef(value)) {
        }
    };

    /** A multi-choice parameter. */
    struct ComboParam : public ParamBase {
        std::vector<std::string> allowedValues;
        std::string value;
        std::string defaultValue;

        ComboParam(const std::string& name, const std::vector<std::string>& allowed,
                   const std::string& def)
            : ParamBase(name, COMBO) {
            this->allowedValues = allowed;
            this->value = def;
            this->defaultValue = def;
        }

        virtual std::string getStringPar() const { return value; }

        virtual void set(const std::string& value) {
            for (size_t i = 0; i < allowedValues.size(); i++) {
                const std::string& allowed = allowedValues[i];
                if (toLowerCase(allowed) == toLowerCase(value)) {
                    this->value = allowed;
                    break;
                }
            }
        }
    };

    /** An action parameter. */
    struct ButtonParam : public ParamBase {
        ButtonParam(const std::string& name)
            : ParamBase(name, BUTTON) { }

        virtual void set(const std::string& value) {
        }
    };

    /** A string parameter. */
    struct StringParam : public ParamBase {
        std::string value;
        std::string defaultValue;

        StringParam(const std::string& name, const std::string& def)
            : ParamBase(name, STRING) {
            this->value = def;
            this->defaultValue = def;
        }

        virtual std::string getStringPar() const { return value; }

        virtual void set(const std::string& value) {
            this->value = value;
        }
    };

    /** Get singleton instance. */
    static Parameters& instance();

    void getParamNames(std::vector<std::string>& parNames) {
        parNames.clear();
        for (const auto& p : params)
            parNames.push_back(p.first);
    }

    std::shared_ptr<ParamBase> getParam(const std::string& name) const {
        auto it = params.find(toLowerCase(name));
        if (it == params.end())
            return nullptr;
        return it->second;
    }

    bool getBoolPar(const std::string& name) const {
        return params.find(toLowerCase(name))->second->getBoolPar();
    }

    int getIntPar(const std::string& name) const {
        return params.find(toLowerCase(name))->second->getIntPar();
    }
    std::string getStringPar(const std::string& name) const {
        return params.find(toLowerCase(name))->second->getStringPar();
    }

    void set(const std::string& name, const std::string& value) {
        auto it = params.find(toLowerCase(name));
        if (it == params.end())
            return;
        it->second->set(value);
    }

    void addPar(const std::shared_ptr<ParamBase>& p) {
        assert(params.find(toLowerCase(p->name)) == params.end());
        params[toLowerCase(p->name)] = p;
    }

private:
    Parameters();

    std::map<std::string, std::shared_ptr<ParamBase> > params;
};

// ----------------------------------------------------------------------------

class IntPairRef {
public:
    IntPairRef(int& value1, int& value2) : value1P(&value1), value2P(&value2) {}
    void set(int v) { *value1P = v; *value2P = v; }
    int get() const { return *value1P; }
private:
    int* value1P;
    int* value2P;
};

/** Param can be either a UCI parameter or a compile time constant. */
template <int defaultValue, int minValue, int maxValue, bool uci, typename Ref> class Param;

template <int defaultValue, int minValue, int maxValue, typename Ref>
class Param<defaultValue, minValue, maxValue, false, Ref> {
public:
    Param() : ref(value) {}
    Param(int& r1, int& r2) : ref(r1, r2) { ref.set(defaultValue); }

    operator int() const { return defaultValue; }
    void setValue(int v) { assert(false); }
    void registerParam(const std::string& name, Parameters& pars) {}
private:
    int value = defaultValue;
    Ref ref;
};

template <int defaultValue, int minValue, int maxValue, typename Ref>
class Param<defaultValue, minValue, maxValue, true, Ref> {
public:
    Param() : ref(value) {}
    Param(int& r1, int& r2) : ref(r1, r2) { ref.set(defaultValue); }

    operator int() const { return  ref.get(); }
    void setValue(int v) { value = v; }
    void registerParam(const std::string& name, Parameters& pars) {
        pars.addPar(std::make_shared<Parameters::SpinParamRef<Ref>>(name, minValue, maxValue,
                                                                    defaultValue, ref));
    }
private:
    int value = defaultValue;
    Ref ref;
};

#define DECLARE_PARAM(name, defV, minV, maxV, uci) \
    typedef Param<defV,minV,maxV,uci,Parameters::IntRef> name##ParamType; \
    extern name##ParamType name;

#define DECLARE_PARAM_2REF(name, defV, minV, maxV, uci) \
    typedef Param<defV,minV,maxV,uci,IntPairRef> name##ParamType; \
    extern name##ParamType name;

#define DEFINE_PARAM(name) \
    name##ParamType name;

#define DEFINE_PARAM_2REF(name, ref1, ref2) \
    name##ParamType name(ref1, ref2);


#define REGISTER_PARAM(varName, uciName) \
    varName.registerParam(uciName, *this);

// ----------------------------------------------------------------------------

class ParamModifiedListener {
public:
    virtual void modified() = 0;
};

class TableSpinParam;

class ParamTableBase : public ParamModifiedListener {
public:
    int getMinValue() const { return minValue; }
    int getMaxValue() const { return maxValue; }

    void addDependent(ParamModifiedListener* dep) { dependent.push_back(dep); }

protected:
    ParamTableBase(bool uci0, int minVal0, int maxVal0) :
        uci(uci0), minValue(minVal0), maxValue(maxVal0) {}
    void registerParamsN(const std::string& name, Parameters& pars,
                         int* table, int* parNo, int N);
    void modifiedN(int* table, int* parNo, int N);

    const bool uci;
    const int minValue;
    const int maxValue;

    std::vector<std::shared_ptr<TableSpinParam>> params;
    std::vector<ParamModifiedListener*> dependent;
};

class TableSpinParam : public Parameters::SpinParamBase {
public:
    TableSpinParam(const std::string& name, ParamTableBase& owner, int defaultValue);

    int getDefaultValue() const { return defaultValue; }
    int getMinValue() const { return owner.getMinValue(); }
    int getMaxValue() const { return owner.getMaxValue(); }
    int getIntPar() const { return value; }

    void set(const std::string& value);

private:
    ParamTableBase& owner;
    const int defaultValue;
    int value;
};

template <int N>
class ParamTable : public ParamTableBase {
public:
    ParamTable(int minVal, int maxVal, bool uci,
               std::initializer_list<int> data,
               std::initializer_list<int> parNo);

    int operator[](int i) const { return table[i]; }
    const int* getTable() const { return table; }

    void registerParams(const std::string& name, Parameters& pars) {
        registerParamsN(name, pars, table, parNo, N);
    }

    void modified() { modifiedN(table, parNo, N); }

private:
    int table[N];
    int parNo[N];
};

template <int N>
class ParamTableMirrored : public ParamModifiedListener {
public:
    ParamTableMirrored(ParamTable<N>& orig0) : orig(orig0) {
        orig.addDependent(this);
        modified();
    }
    int operator[](int i) const { return table[i]; }
    const int* getTable() const { return table; }
    void modified() {
        for (int i = 0; i < N; i++)
            table[i] = orig[N-1-i];
    }
private:
    int table[N];
    ParamTable<N>& orig;
};

template <int N>
class ParamTableEv : public ParamTable<N> {
public:
    ParamTableEv(int minVal, int maxVal, bool uci,
                 std::initializer_list<int> data,
                 std::initializer_list<int> parNo);

    void modified();
};


inline
TableSpinParam::TableSpinParam(const std::string& name, ParamTableBase& owner0, int defaultValue0)
    : SpinParamBase(name), owner(owner0), defaultValue(defaultValue0), value(defaultValue0) {
}

inline void
TableSpinParam::set(const std::string& value) {
    int val;
    if (str2Num(value, val) && (val >= getMinValue()) && (val <= getMaxValue())) {
        this->value = val;
        owner.modified();
    }
}

template <int N>
ParamTable<N>::ParamTable(int minVal0, int maxVal0, bool uci0,
                          std::initializer_list<int> table0,
                          std::initializer_list<int> parNo0)
    : ParamTableBase(uci0, minVal0, maxVal0) {
    assert(table0.size() == N);
    assert(parNo0.size() == N);
    for (int i = 0; i < N; i++) {
        table[i] = table0.begin()[i];
        parNo[i] = parNo0.begin()[i];
    }
}

template <int N>
ParamTableEv<N>::ParamTableEv(int minVal0, int maxVal0, bool uci0,
                              std::initializer_list<int> table0,
                              std::initializer_list<int> parNo0)
    : ParamTable<N>(minVal0, maxVal0, uci0, table0, parNo0) {
}

template <int N>
void ParamTableEv<N>::modified() {
    ParamTable<N>::modified();
    EvaluateNS::computeKnightMobility();
}

// ----------------------------------------------------------------------------

const bool useUciParam = false;

extern int pieceValue[Piece::nPieceTypes];


// Evaluation parameters

DECLARE_PARAM_2REF(pV, 100, 1, 200, useUciParam);
DECLARE_PARAM_2REF(nV, 385, 1, 800, useUciParam);
DECLARE_PARAM_2REF(bV, 385, 1, 800, useUciParam);
DECLARE_PARAM_2REF(rV, 600, 1, 1200, useUciParam);
DECLARE_PARAM_2REF(qV, 1215, 1, 2400, useUciParam);
DECLARE_PARAM_2REF(kV, 9900, 9900, 9900, false); // Used by SEE algorithm but not included in board material sums

DECLARE_PARAM(pawnIslandPenalty,        9, 0, 50, useUciParam);
DECLARE_PARAM(pawnBackwardPenalty,      17, 0, 50, useUciParam);
DECLARE_PARAM(pawnSemiBackwardPenalty1, 6, -50, 50, useUciParam);
DECLARE_PARAM(pawnSemiBackwardPenalty2, 2, -50, 50, useUciParam);
DECLARE_PARAM(pawnGuardedPassedBonus,   4, 0, 50, useUciParam);
DECLARE_PARAM(pawnRaceBonus,            170, 0, 1000, useUciParam);
DECLARE_PARAM(passedPawnEGFactor,       62, 1, 128, useUciParam);

DECLARE_PARAM(QvsRMBonus1,         29, -100, 100, useUciParam);
DECLARE_PARAM(QvsRMBonus2,         15, -100, 100, useUciParam);
DECLARE_PARAM(knightVsQueenBonus1, 125, 0, 200, useUciParam);
DECLARE_PARAM(knightVsQueenBonus2, 251, 0, 600, useUciParam);
DECLARE_PARAM(knightVsQueenBonus3, 357, 0, 800, useUciParam);
DECLARE_PARAM(krkpBonus,           193, 0, 800, useUciParam);
DECLARE_PARAM(krpkbBonus,          38,  -200, 200, useUciParam);
DECLARE_PARAM(krpkbPenalty,        40,  0, 128, useUciParam);
DECLARE_PARAM(krpknBonus,          187, 0, 400, useUciParam);

DECLARE_PARAM(pawnTradePenalty,    62, 0, 100, useUciParam);
DECLARE_PARAM(pieceTradeBonus,     9, 0, 100, useUciParam);
DECLARE_PARAM(pawnTradeThreshold,  365, 100, 1000, useUciParam);
DECLARE_PARAM(pieceTradeThreshold, 738, 10, 1000, useUciParam);

DECLARE_PARAM(threatBonus1,     63, 5, 500, useUciParam);
DECLARE_PARAM(threatBonus2,     1192, 100, 10000, useUciParam);

DECLARE_PARAM(rookHalfOpenBonus,     18, 0, 100, useUciParam);
DECLARE_PARAM(rookOpenBonus,         19, 0, 100, useUciParam);
DECLARE_PARAM(rookDouble7thRowBonus, 74, 0, 100, useUciParam);
DECLARE_PARAM(trappedRookPenalty,    71, 0, 200, useUciParam);

DECLARE_PARAM(bishopPairPawnPenalty, 5, 0, 10, useUciParam);
DECLARE_PARAM(trappedBishopPenalty,  83, 0, 300, useUciParam);
DECLARE_PARAM(oppoBishopPenalty,     83, 0, 128, useUciParam);

DECLARE_PARAM(kingSafetyHalfOpenBCDEFG1, 19, 0, 100, useUciParam);
DECLARE_PARAM(kingSafetyHalfOpenBCDEFG2, -11, -50, 100, useUciParam);
DECLARE_PARAM(kingSafetyHalfOpenAH1,     18, 0, 100, useUciParam);
DECLARE_PARAM(kingSafetyHalfOpenAH2,     11, 0, 100, useUciParam);
DECLARE_PARAM(kingSafetyWeight1,         33, -50, 200, useUciParam);
DECLARE_PARAM(kingSafetyWeight2,         -40, -50, 200, useUciParam);
DECLARE_PARAM(kingSafetyWeight3,         8, -50, 200, useUciParam);
DECLARE_PARAM(kingSafetyWeight4,         1, -50, 200, useUciParam);
DECLARE_PARAM(kingSafetyThreshold,       45, 0, 200, useUciParam);
DECLARE_PARAM(knightKingProtectBonus,    16, -50, 50, useUciParam);
DECLARE_PARAM(bishopKingProtectBonus,    19, -50, 50, useUciParam);
DECLARE_PARAM(pawnStormBonus,            12, 0, 20, useUciParam);

DECLARE_PARAM(pawnLoMtrl,          500, 0, 10000, useUciParam);
DECLARE_PARAM(pawnHiMtrl,          3204, 0, 10000, useUciParam);
DECLARE_PARAM(minorLoMtrl,         1116, 0, 10000, useUciParam);
DECLARE_PARAM(minorHiMtrl,         3742, 0, 10000, useUciParam);
DECLARE_PARAM(castleLoMtrl,        712, 0, 10000, useUciParam);
DECLARE_PARAM(castleHiMtrl,        7884, 0, 10000, useUciParam);
DECLARE_PARAM(queenLoMtrl,         4498, 0, 10000, useUciParam);
DECLARE_PARAM(queenHiMtrl,         6531, 0, 10000, useUciParam);
DECLARE_PARAM(passedPawnLoMtrl,    767, 0, 10000, useUciParam);
DECLARE_PARAM(passedPawnHiMtrl,    2512, 0, 10000, useUciParam);
DECLARE_PARAM(kingSafetyLoMtrl,    945, 0, 10000, useUciParam);
DECLARE_PARAM(kingSafetyHiMtrl,    3571, 0, 10000, useUciParam);
DECLARE_PARAM(oppoBishopLoMtrl,    752, 0, 10000, useUciParam);
DECLARE_PARAM(oppoBishopHiMtrl,    3386, 0, 10000, useUciParam);
DECLARE_PARAM(knightOutpostLoMtrl, 168, 0, 10000, useUciParam);
DECLARE_PARAM(knightOutpostHiMtrl, 538, 0, 10000, useUciParam);

extern ParamTable<64>         kt1b, kt2b, pt1b, pt2b, nt1b, nt2b, bt1b, bt2b, qt1b, qt2b, rt1b;
extern ParamTableMirrored<64> kt1w, kt2w, pt1w, pt2w, nt1w, nt2w, bt1w, bt2w, qt1w, qt2w, rt1w;

extern ParamTable<64> knightOutpostBonus;
extern ParamTable<64> protectedPawnBonus, attackedPawnBonus;

extern ParamTable<15> rookMobScore;
extern ParamTable<14> bishMobScore;
extern ParamTableEv<28> knightMobScore;
extern ParamTable<28> queenMobScore;

extern ParamTable<36> connectedPPBonus;
extern ParamTable<8> passedPawnBonusX, passedPawnBonusY;
extern ParamTable<10> ppBlockerBonus;
extern ParamTable<8> candidatePassedBonus;

extern ParamTable<16> majorPieceRedundancy;
extern ParamTable<5> QvsRRBonus;
extern ParamTable<7> RvsMBonus, RvsMMBonus;
extern ParamTable<4> bishopPairValue;
extern ParamTable<7> rookEGDrawFactor;

extern ParamTable<9> pawnShelterTable, pawnStormTable;
extern ParamTable<10> kingAttackWeight;
extern ParamTable<5> kingPPSupportK;
extern ParamTable<8> kingPPSupportP;

extern ParamTable<8> pawnDoubledPenalty;
extern ParamTable<8> pawnIsolatedPenalty;


// Search parameters

DECLARE_PARAM(aspirationWindow, 15, 1, 100, useUciParam);
DECLARE_PARAM(rootLMRMoveCount, 2, 0, 100, useUciParam);

DECLARE_PARAM(razorMargin1, 86, 1, 500, useUciParam);
DECLARE_PARAM(razorMargin2, 353, 1, 1000, useUciParam);

DECLARE_PARAM(reverseFutilityMargin1, 204, 1, 1000, useUciParam);
DECLARE_PARAM(reverseFutilityMargin2, 420, 1, 1000, useUciParam);
DECLARE_PARAM(reverseFutilityMargin3, 533, 1, 2000, useUciParam);
DECLARE_PARAM(reverseFutilityMargin4, 788, 1, 3000, useUciParam);

DECLARE_PARAM(futilityMargin1,  61, 1,  500, useUciParam);
DECLARE_PARAM(futilityMargin2, 144, 1,  500, useUciParam);
DECLARE_PARAM(futilityMargin3, 268, 1, 1000, useUciParam);
DECLARE_PARAM(futilityMargin4, 334, 1, 1000, useUciParam);

DECLARE_PARAM(lmpMoveCountLimit1,  3, 1, 256, useUciParam);
DECLARE_PARAM(lmpMoveCountLimit2,  6, 1, 256, useUciParam);
DECLARE_PARAM(lmpMoveCountLimit3, 12, 1, 256, useUciParam);
DECLARE_PARAM(lmpMoveCountLimit4, 24, 1, 256, useUciParam);

DECLARE_PARAM(lmrMoveCountLimit1,  3, 1, 256, useUciParam);
DECLARE_PARAM(lmrMoveCountLimit2, 12, 1, 256, useUciParam);

DECLARE_PARAM(quiesceMaxSortMoves, 8, 0, 256, useUciParam);
DECLARE_PARAM(deltaPruningMargin, 152, 0, 1000, useUciParam);


// Time management parameters

DECLARE_PARAM(timeMaxRemainingMoves, 45, 2, 200, useUciParam);
DECLARE_PARAM(bufferTime, 1000, 1, 10000, useUciParam);
DECLARE_PARAM(minTimeUsage, 85, 1, 100, useUciParam);
DECLARE_PARAM(maxTimeUsage, 400, 100, 1000, useUciParam);
DECLARE_PARAM(timePonderHitRate, 35, 1, 100, useUciParam);


#endif /* PARAMETERS_HPP_ */
