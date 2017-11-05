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
            str2Num(value, val);
            if ((val >= minValue) && (val <= maxValue))
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

    std::shared_ptr<ParamBase> getParam(const std::string& name) {
        return params[name];
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

    operator int() { return defaultValue; }
    void setValue(int v) { assert(false); }
    bool isUCI() const { return false; }
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

    operator int() { return  ref.get(); }
    void setValue(int v) { value = v; }
    bool isUCI() const { return true; }
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

const bool useUciParam = false;

extern int pieceValue[Piece::nPieceTypes];

DECLARE_PARAM_2REF(pV, 92, 0, 200, useUciParam);
DECLARE_PARAM_2REF(nV, 385, 0, 800, useUciParam);
DECLARE_PARAM_2REF(bV, 385, 0, 800, useUciParam);
DECLARE_PARAM_2REF(rV, 593, 0, 1200, useUciParam);
DECLARE_PARAM_2REF(qV, 1244, 0, 2400, useUciParam);
DECLARE_PARAM_2REF(kV, 9900, 9900, 9900, false); // Used by SEE algorithm but not included in board material sums

DECLARE_PARAM(pawnDoubledPenalty, 19, 0, 50, useUciParam);
DECLARE_PARAM(pawnIslandPenalty, 14, 0, 50, useUciParam);
DECLARE_PARAM(pawnIsolatedPenalty,  9, 0, 50, useUciParam);


#endif /* PARAMETERS_HPP_ */
