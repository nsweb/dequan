
#include <iostream>
#include <chrono>
#include <ratio>

#define FCHECK_USE_STDVECTOR
#define FCHECK_WITH_STATS
#define FCHECK_IMPLEMENTATION
//#define FCHECK_SET_CONSTRAINT_SIZE 64
#include "../fcheck.h"

//struct MyNewConstraint : public fcheck::Constraint
//{
//    MyNewConstraint()
//    {
//        static_assert(sizeof(MyNewConstraint) <= FCHECK_SET_CONSTRAINT_SIZE, "");
//    }
//    virtual void LinkVars(fcheck::Array<fcheck::Var>& vars) {}
//    virtual Eval Evaluate(const fcheck::Array<fcheck::InstVar>& inst_vars, fcheck::VarId last_assigned_vid) { return fcheck::Constraint::Eval::Passed;  }
//    virtual bool AplyArcConsistency(fcheck::Assignment& a, fcheck::VarId last_assigned_vid) { return true;  }
//
//    fcheck::Array<int> a0, a1;
//};


// https://en.wikipedia.org/wiki/Eight_queens_puzzle
bool NQueensTest(const int num_queen)
{
    std::cout << "\n----------------------------\n";
    std::cout << num_queen << "-queens test : ";

    fcheck::CSP csp;
    fcheck::Array<fcheck::VarId> qvars;
    qvars.resize(num_queen);

    for (int i = 0; i < num_queen; i++)
    {
        qvars[i] = csp.AddIntVar(0, num_queen);
    }

    for (int i = 0; i < num_queen; i++)
    {
        for (int j = i + 1; j < num_queen; j++)
        {
            csp.AddConstraint(fcheck::OpConstraint(qvars[i], qvars[j], fcheck::OpConstraint::Op::NotEqual, 0));
            csp.AddConstraint(fcheck::OpConstraint(qvars[i], qvars[j], fcheck::OpConstraint::Op::NotEqual, j - i));
            csp.AddConstraint(fcheck::OpConstraint(qvars[i], qvars[j], fcheck::OpConstraint::Op::NotEqual, i - j));
        }
    }
    csp.FinalizeModel();

    fcheck::Assignment a;
    a.Reset(csp);

    auto t1 = std::chrono::high_resolution_clock::now();

    bool success = csp.ForwardCheckingStep(a);

    auto t2 = std::chrono::high_resolution_clock::now();

    std::cout << (success ? "PASSED\n" : "FAILED\n");

    if (success)
    {
        for (int col_idx = 0; col_idx < num_queen; col_idx++)
        {
            int qrow_idx = a.GetInstVarValue(qvars[col_idx]);
            for (int row_idx = 0; row_idx < num_queen; row_idx++)
            {
                std::cout << (row_idx == qrow_idx ? "X " : "0 ");
            }
            std::cout << "\n";
        }
    }

    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "\nForwardCheckingStep took " << time_span.count() << " seconds.\n";

#ifdef FCHECK_WITH_STATS
    std::cout << "\napplied_arcs: " << a.stats.applied_arcs;
    std::cout << "\nassigned_vars: " << a.stats.assigned_vars;
    std::cout << "\nvalidated_constraints: " << a.stats.validated_constraints;
#endif

    return success;
}

bool SudokuTest()
{
    const int num_row = 9;
    const int _ = 0;
    const int sudoku_init[num_row * num_row] =
    {
        _, _, 3,  _, 2, _,  6, _, _,
        9, _, _,  3, _, 5,  _, _, 1,
        _, _, 1,  8, _, 6,  4, _, _,

        _, _, 8,  1, _, 2,  9, _, _,
        7, _, _,  _, _, _,  _, _, 8,
        _, _, 6,  7, _, 8,  2, _, _,

        _, _, 2,  6, _, 9,  5, _, _,
        8, _, _,  2, _, 3,  _, _, 9,
        _, _, 5,  _, 1, _,  3, _, _
    };

    std::cout << "\n----------------------------\n";
    std::cout << num_row << "-sudoku test : ";

    fcheck::CSP csp;
    fcheck::Array<fcheck::VarId> vars;

    vars.resize(num_row * num_row);

    for (int row_idx = 0; row_idx < num_row; row_idx++)
    {
        for (int col_idx = 0; col_idx < num_row; col_idx++)
        {
            if (sudoku_init[row_idx * num_row + col_idx] == _)
            {
                vars[row_idx * num_row + col_idx] = csp.AddIntVar(1, num_row + 1);
            }
            else
            {
                vars[row_idx * num_row + col_idx] = csp.AddFixedVar(sudoku_init[row_idx * num_row + col_idx]);
            }
        }
    }

    fcheck::Array<fcheck::VarId> alldiff_vars;
    for (int row_idx = 0; row_idx < num_row; row_idx++)
    {
        alldiff_vars.clear();
        for (int col_idx = 0; col_idx < num_row; col_idx++)
        {
            alldiff_vars.push_back(vars[row_idx * num_row + col_idx]);
        }
        csp.AddConstraint(fcheck::AllDifferentConstraint(alldiff_vars));
    }
    for (int col_idx = 0; col_idx < num_row; col_idx++)
    {
        alldiff_vars.clear();
        for (int row_idx = 0; row_idx < num_row; row_idx++)
        {
            alldiff_vars.push_back(vars[row_idx * num_row + col_idx]);
        }
        csp.AddConstraint(fcheck::AllDifferentConstraint(alldiff_vars));
    }
    csp.FinalizeModel();

    fcheck::Assignment a;
    a.Reset(csp);

    auto t1 = std::chrono::high_resolution_clock::now();

    bool success = csp.ForwardCheckingStep(a);

    auto t2 = std::chrono::high_resolution_clock::now();

    std::cout << (success ? "PASSED\n" : "FAILED\n");

    if (success)
    {
        for (int row_idx = 0; row_idx < num_row; row_idx++)
        {
            for (int col_idx = 0; col_idx < num_row; col_idx++)
            {
                int val = a.GetInstVarValue(vars[row_idx * num_row + col_idx]);
                std::cout << val << " ";
            }
            std::cout << "\n";
        }
    }

    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "\nForwardCheckingStep took " << time_span.count() << " seconds.\n";

#ifdef FCHECK_WITH_STATS
    std::cout << "\napplied_arcs: " << a.stats.applied_arcs;
    std::cout << "\nassigned_vars: " << a.stats.assigned_vars;
    std::cout << "\nvalidated_constraints: " << a.stats.validated_constraints;
#endif

    return success;
}

bool OpInequalityTest()
{
    std::cout << "\n----------------------------\n";
    std::cout << "OpInequality test : ";

    fcheck::CSP csp;
    fcheck::Array<fcheck::VarId> vars;

    vars.resize(4);
    vars[0] = csp.AddIntVar(0, 10);
    vars[1] = csp.AddIntVar(0, 10);
    vars[2] = csp.AddFixedVar(6);
    vars[3] = csp.AddFixedVar(5);
    csp.AddConstraint(fcheck::OpConstraint(vars[0], vars[2], fcheck::OpConstraint::Op::Inf, 0));
    csp.AddConstraint(fcheck::OpConstraint(vars[0], vars[3], fcheck::OpConstraint::Op::SupEqual, 0));
    csp.AddConstraint(fcheck::OpConstraint(vars[1], vars[2], fcheck::OpConstraint::Op::InfEqual, 0));
    csp.AddConstraint(fcheck::OpConstraint(vars[1], vars[3], fcheck::OpConstraint::Op::Sup, 0));
    csp.FinalizeModel();

    fcheck::Assignment a;
    a.Reset(csp);

    auto t1 = std::chrono::high_resolution_clock::now();

    bool success = csp.ForwardCheckingStep(a);

    auto t2 = std::chrono::high_resolution_clock::now();

    std::cout << (success ? "PASSED\n" : "FAILED\n");

    if (success)
    {
        std::cout << "Var0 = " << a.GetInstVarValue(vars[0]) << "\n";
        std::cout << "Var1 = " << a.GetInstVarValue(vars[1]) << "\n";
    }

    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    std::cout << "\nForwardCheckingStep took " << time_span.count() << " seconds.\n";

#ifdef FCHECK_WITH_STATS
    std::cout << "\napplied_arcs: " << a.stats.applied_arcs;
    std::cout << "\nassigned_vars: " << a.stats.assigned_vars;
    std::cout << "\nvalidated_constraints: " << a.stats.validated_constraints;
#endif

    return success;
}


int main()
{
    OpInequalityTest();
    NQueensTest(8);
    SudokuTest();
}