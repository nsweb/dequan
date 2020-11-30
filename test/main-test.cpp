
#include <iostream>
#include <chrono>
#include <ratio>

#define DEQUAN_USE_STDVECTOR
#define DEQUAN_WITH_STATS
#define DEQUAN_IMPLEMENTATION
//#define DEQUAN_SET_CONSTRAINT_SIZE 64
#include "../dequan.h"

//struct MyNewConstraint : public dequan::Constraint
//{
//    MyNewConstraint()
//    {
//        static_assert(sizeof(MyNewConstraint) <= DEQUAN_SET_CONSTRAINT_SIZE, "");
//    }
//    virtual void LinkVars(dequan::Array<dequan::Var>& vars) {}
//    virtual Eval Evaluate(const dequan::Array<dequan::InstVar>& inst_vars, dequan::VarId last_assigned_vid) { return dequan::Constraint::Eval::Passed;  }
//    virtual bool AplyArcConsistency(dequan::Assignment& a, dequan::VarId last_assigned_vid) { return true;  }
//
//    dequan::Array<int> a0, a1;
//};


// https://en.wikipedia.org/wiki/Eight_queens_puzzle
bool NQueensTest(const int num_queen)
{
    std::cout << "\n----------------------------\n";
    std::cout << num_queen << "-queens test : ";

    dequan::CSP csp;
    dequan::Array<dequan::VarId> qvars;
    qvars.resize(num_queen);

    for (int i = 0; i < num_queen; i++)
    {
        qvars[i] = csp.AddIntVar(0, num_queen);
    }

    for (int i = 0; i < num_queen; i++)
    {
        for (int j = i + 1; j < num_queen; j++)
        {
            csp.AddConstraint(dequan::OpConstraint(qvars[i], qvars[j], dequan::OpConstraint::Op::NotEqual, 0));
            csp.AddConstraint(dequan::OpConstraint(qvars[i], qvars[j], dequan::OpConstraint::Op::NotEqual, j - i));
            csp.AddConstraint(dequan::OpConstraint(qvars[i], qvars[j], dequan::OpConstraint::Op::NotEqual, i - j));
        }
    }
    csp.FinalizeModel();

    dequan::Assignment a;
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

#ifdef DEQUAN_WITH_STATS
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

    dequan::CSP csp;
    dequan::Array<dequan::VarId> vars;

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

    dequan::Array<dequan::VarId> alldiff_vars;
    for (int row_idx = 0; row_idx < num_row; row_idx++)
    {
        alldiff_vars.clear();
        for (int col_idx = 0; col_idx < num_row; col_idx++)
        {
            alldiff_vars.push_back(vars[row_idx * num_row + col_idx]);
        }
        csp.AddConstraint(dequan::AllDifferentConstraint(alldiff_vars));
    }
    for (int col_idx = 0; col_idx < num_row; col_idx++)
    {
        alldiff_vars.clear();
        for (int row_idx = 0; row_idx < num_row; row_idx++)
        {
            alldiff_vars.push_back(vars[row_idx * num_row + col_idx]);
        }
        csp.AddConstraint(dequan::AllDifferentConstraint(alldiff_vars));
    }
    csp.FinalizeModel();

    dequan::Assignment a;
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

#ifdef DEQUAN_WITH_STATS
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

    dequan::CSP csp;
    dequan::Array<dequan::VarId> vars;

    vars.resize(4);
    vars[0] = csp.AddIntVar(0, 10);
    vars[1] = csp.AddIntVar(0, 10);
    vars[2] = csp.AddFixedVar(6);
    vars[3] = csp.AddFixedVar(5);
    csp.AddConstraint(dequan::OpConstraint(vars[0], vars[2], dequan::OpConstraint::Op::Inf, 0));
    csp.AddConstraint(dequan::OpConstraint(vars[0], vars[3], dequan::OpConstraint::Op::SupEqual, 0));
    csp.AddConstraint(dequan::OpConstraint(vars[1], vars[2], dequan::OpConstraint::Op::InfEqual, 0));
    csp.AddConstraint(dequan::OpConstraint(vars[1], vars[3], dequan::OpConstraint::Op::Sup, 0));
    csp.FinalizeModel();

    dequan::Assignment a;
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

#ifdef DEQUAN_WITH_STATS
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