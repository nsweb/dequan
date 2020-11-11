
#include <iostream>
#include <string>
#include <chrono>
#include <ratio>

#define FCHECK_USE_STDVECTOR
#define FCHECK_WITH_STATS
#define FCHECK_IMPLEMENTATION
#include "../fcheck.h"


template<typename ... Args>
std::string string_format(const char* format, Args ... args)
{
    size_t size = snprintf(nullptr, 0, format, args ...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format, args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

// https://en.wikipedia.org/wiki/Eight_queens_puzzle
bool NQueensTest2(const int num_queen)
{
    std::cout << "----------------------------\n";
    std::cout << num_queen << "-queens test2 : ";

    fcheck::CSP csp;
    //fcheck::Domain raw_dom;
    fcheck::Array<std::string> var_names;
    fcheck::Array<fcheck::VarId> qvars;

    var_names.resize(num_queen);
    qvars.resize(num_queen);

    for (int i = 0; i < num_queen; i++)
    {
        var_names[i] = string_format("q%d", i);
    }

    for (int i = 0; i < num_queen; i++)
    {
        qvars[i] = csp.AddIntVar2(var_names[i].c_str(), 0, num_queen);
    }

    for (int i = 0; i < num_queen; i++)
    {
        for (int j = i + 1; j < num_queen; j++)
        {
            csp.PushConstraint2(fcheck::OpConstraint2(qvars[i], qvars[j], fcheck::OpConstraint2::Op::NotEqual, 0));
            csp.PushConstraint2(fcheck::OpConstraint2(qvars[i], qvars[j], fcheck::OpConstraint2::Op::NotEqual, j - i));
            csp.PushConstraint2(fcheck::OpConstraint2(qvars[i], qvars[j], fcheck::OpConstraint2::Op::NotEqual, i - j));
        }
    }

    fcheck::Assignment a;
    a.Reset2(csp.domains2);

    auto t1 = std::chrono::high_resolution_clock::now();

    bool success = csp.ForwardCheckingStep2(a);

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
    std::cout << "\nForwardCheckingStep2 took " << time_span.count() << " seconds.\n";

    return success;
}


int main()
{
    NQueensTest2(8);
}