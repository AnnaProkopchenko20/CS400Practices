#include "string_state.h"
#pragma once
#include "assign_state.h"
#include "equals_state.h"
#include "ident_state.h"
#include "negation_state.h"
#include "number_state.h"
#include "start_state.h"
#include "minus_state.h"

inline std::unique_ptr<LexerState> makeState(StateId id)
{
    switch (id)
    {
        case StateId::Start:      return std::make_unique<StartState>();
        case StateId::Ident:      return std::make_unique<IdentState>();
        case StateId::Number:     return std::make_unique<NumberState>();
        case StateId::Assign:     return std::make_unique<AssignState>();
        case StateId::Equals:     return std::make_unique<EqualsState>();
        case StateId::Negation:   return std::make_unique<NegationState>();
        case StateId::MinusState: return std::make_unique<MinusState>();
        case StateId::String:     return std::make_unique<StringState>();
    }
    return nullptr;
}
