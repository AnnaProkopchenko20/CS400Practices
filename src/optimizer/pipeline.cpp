#include "optimizer/pipeline.h"

#include "optimizer/effects.h"
#include "optimizer/fn_remover.h"
#include "optimizer/fn_use_counter.h"
#include "optimizer/var_remover.h"
#include "optimizer/var_use_counter.h"

OptStats remove_unused(ProgramNode& program, bool conservative)
{
    OptStats stats;

    while (true)
    {
        ++stats.rounds;

        // Effects depend on the current tree: recompute every round.
        EffectOracle oracle;
        oracle.conservative = conservative;
        oracle.analyse(program);

        // 1. unused variables (fresh counter every round)
        int vars = 0;
        {
            VariableUseCounter counter(oracle);
            program.accept(counter);
            auto dead = counter.dead();
            if (!dead.empty())
            {
                UnusedVariableRemover remover(dead);
                program.accept(remover);
                vars = remover.removed;
            }
        }

        // 2. unused functions, counted on the tree AFTER the variables went
        int fns = 0;
        {
            FunctionUseCounter counter;
            program.accept(counter);
            UnusedFunctionRemover remover(counter.calls);
            program.accept(remover);
            fns = remover.removed;
        }

        stats.removed_stmts += vars;
        stats.removed_fns   += fns;

        if (vars == 0 && fns == 0) break;   // a round removed nothing: stop
    }
    return stats;
}
