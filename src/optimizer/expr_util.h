#pragma once
#include "ast.h"

// The declaration at the root of an assignment target:  p.x.y  ->  decl of p.
// Returns nullptr for a field of `this` (never removable) or anything else.
inline DeclNode* root_decl(ExprNode* e)
{
    while (e)
    {
        if (auto* v = dynamic_cast<VarNameNode*>(e)) return v->decl;
        if (auto* f = dynamic_cast<FieldAccessNode*>(e)) { e = f->object.get(); continue; }
        return nullptr;
    }
    return nullptr;
}
