/*
	The MIT License (MIT)
	Copyright (c) 2020 nsweb
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

/*
	Please define FCHECK_IMPLEMENTATION before including this file in one C / C++ file to create the implementation.
	Should be C++11 compatible.
	FCHECK_USE_STDVECTOR : #define this to use std vectors, otherwise you need provide your own implementation of the Array macros
	FCHECK_SET_CONSTRAINT_SIZE : #define this if you implement your own Constraint-derived classes and need more space than default (see MaxConstraint)
	FCHECK_WITH_STATS : #define this to retrieve various stats about the search algorithm
*/

#ifdef FCHECK_USE_STDVECTOR
	#include <vector>
	#include <algorithm>

	#define FCHECK_Array_Size(a)				a.size()
	#define FCHECK_Array_PushBack(a, val)		a.push_back(val)
	#define FCHECK_Array_Clear(a)				a.clear()
	#define FCHECK_Array_Resize(a, s)			a.resize(s)
	#define FCHECK_Array_Reserve(a, s)			a.reserve(s)
	#define FCHECK_Array_Back(a)				a.back()
	#define FCHECK_Array_PopBack(a)				a.pop_back()
	#define FCHECK_Array_Insert(a, idx, val)	a.insert(a.begin() + idx, val)
	#define FCHECK_Array_Erase(a, first, last)	a.erase(a.begin() + (first), a.begin() + (last));
	#define FCHECK_Array_Sort(a, lambda)		std::sort(a.begin(), a.end(), lambda)
#endif

/**/
namespace fcheck
{
#ifdef FCHECK_USE_STDVECTOR
	template<typename T>
	using Array = std::vector<T>;
#endif
	using VarId = int;
	struct Var;
	class Assignment;
	class CSP;

#ifdef FCHECK_WITH_STATS
	/**
	 * Various statistics for the search algorithm.
	 */
	struct Stats
	{
		Stats() = default;

		unsigned long long validated_constraints = 0;
		unsigned long long applied_arcs = 0;
		unsigned long long assigned_vars = 0;
	};
#endif
	enum class DomainType : int
	{
		Values = 0,
		Ranges,
	};
	/** Domain represents either continuous ranges [min, max) of values or a set of values that a Var can take */
	struct Domain
	{
		Domain() = default;
		int Size() const;
		/** Remove any value different from 'val' in the domain */
		void Intersect(int val);
		/** Remove any value different from 'val0' or 'val1' in the domain */
		void Intersect(int val0, int val1);
		/** Remove any value outside of [rmin, rmax) in the domain */
		void IntersectRange(int rmin, int rmax);
		/** Remove 'val' value from the domain */
		void Exclude(int val);
		/** Remove any value >= rmax from the domain */
		void ExcludeSup(int rmax);
		/** Remove any value < rmin from the domain */
		void ExcludeInf(int rmin);

		DomainType type = DomainType::Values;
		Array<int> values;
	};
	/** Backup domain, that is used to restore variables domain when searching algo needs to backtrack */
	struct SavedDomain
	{
		SavedDomain() = default;

		VarId var_id;
		DomainType type = DomainType::Values;
		Array<int> values;
	};

	/** Set of SavedDomains for one step of the searching algo */
	struct SavedDomains
	{
		SavedDomains() = default;

		Array<SavedDomain> domains;
	};

	/**
	 * Represents a value of an instanced variable.
	 * The searching algo is finished when all variables have been instanced.
	 */
	struct InstVar
	{
		static const int UNASSIGNED = -INT_MAX;

		InstVar() = default;

		int value = InstVar::UNASSIGNED;
	};

	/**
	 * Base class for representing constraints on variables.
	 * You can define new derived constraints, but note that sizeof(YourNewConstraint) must be <= MAX_CONSTRAINT_SIZE.
	 * Increase this value if you have larger constraint classes.
	 */
	struct Constraint
	{
		enum class Eval : int
		{
			NA = 0,	// not applicable yet
			Passed,
			Failed
		};

		Constraint() = default;
		virtual void LinkVars(Array<Var>& vars) = 0;
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid) = 0;
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid) { return true; }
	};

	/**
	 * Can represent and store any Constraint derived class.
	 * This class exists to avoid heap allocations while still supporting polymorphism when storing all constraints.
	 * i.e. Array<GenericConstraint> instead of Array<Constraint*>
	 */
	struct GenericConstraint
	{
#ifdef FCHECK_SET_CONSTRAINT_SIZE
		static constexpr int MAX_CONSTRAINT_SIZE = FCHECK_SET_CONSTRAINT_SIZE;
#else
		struct MaxConstraint
		{
			virtual void A() {}
			Array<int> v;
		};
		static constexpr int MAX_CONSTRAINT_SIZE = sizeof(MaxConstraint);
#endif
		char buffer[MAX_CONSTRAINT_SIZE];

		Constraint* operator->()	{ return (Constraint*)buffer; }
		Constraint* get()			{ return (Constraint*)buffer; }
	};

	/** Constraint of the form : v0 (op) v1 + offset */
	struct OpConstraint : public Constraint
	{
		enum class Op : int
		{
			Equal = 0,	// ==
			NotEqual,	// !=
			SupEqual,	// >=
			Sup,		// >
			InfEqual,	// <=
			Inf		// <
		};

		OpConstraint(VarId _v0, VarId _v1, Op _op,int _offset) : v0(_v0), v1(_v1), op(_op), offset(_offset)
		{
			static_assert(sizeof(OpConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		}
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		VarId v0, v1;
		Op op = Op::Equal;
		int offset = 0;
	};

	/** Constraint of the form : v0 == v1 */
	struct EqualityConstraint : public Constraint
	{
		EqualityConstraint(VarId _v0, VarId _v1) : v0(_v0), v1(_v1)
		{
			static_assert(sizeof(EqualityConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		}
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		VarId v0, v1;
	};

	/** Constraint of the form : v0 == v1 || v0 == v2 */
	struct OrEqualityConstraint : public Constraint
	{
		OrEqualityConstraint(VarId _v0, VarId _v1, VarId _v2) : v0(_v0), v1(_v1), v2(_v2)
		{
			static_assert(sizeof(OrEqualityConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		}
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		VarId v0, v1, v2;
	};

	/** Constraint of the form : v0 == v1 + v2 - v3 */
	struct CombinedEqualityConstraint : public Constraint
	{
		CombinedEqualityConstraint(VarId _v0, VarId _v1, VarId _v2, VarId _v3) : v0(_v0), v1(_v1), v2(_v2), v3(_v3)
		{
			static_assert(sizeof(CombinedEqualityConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		VarId v0, v1, v2, v3;
	};

	/** Constraint of the form : (v0 >= min && v0 < max) || (v1 >= min && v1 < max) */
	struct OrRangeConstraint : public Constraint
	{
		OrRangeConstraint(VarId _v0, VarId _v1, int _min, int _max) : v0(_v0), v1(_v1), min(_min), max(_max)
		{
			static_assert(sizeof(OrRangeConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		}
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		VarId v0, v1;
		int min, max;
	};

	/** Add an all different constraints to a bunch of variables. */
	struct AllDifferentConstraint : public Constraint
	{
		AllDifferentConstraint(const Array<VarId>& vars) : alldiff_vars(vars)
		{
			static_assert(sizeof(AllDifferentConstraint) <= GenericConstraint::MAX_CONSTRAINT_SIZE, "");
		}
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid);
		virtual bool AplyArcConsistency(Assignment& a, VarId last_assigned_vid);

		Array<VarId> alldiff_vars;
	};

	/** Class for representing the different variables of the CSP to solve. */
	struct Var
	{
		Var() = default;

		static const VarId INVALID = -1;

		VarId var_id = Var::INVALID;
		/** All the constraints where the variable is referenced */
		Array<Constraint*> linked_constraints;
	};

	/**
	 * Assignment is a snapshot of the progression of the search algorithm.
	 * All working variables of the search algorithm goes here.
	 */
	class Assignment
	{
	public:
		Assignment();
		/** Reset the assignment and make it ready for a new solving. */
		void Reset(const CSP& csp);
		/** Whether all variables have been assigned. */
		bool IsComplete();
		int GetInstVarValue(VarId vid) const;
		const Domain& GetCurrentDomain(VarId vid) const;
		VarId NextUnassignedVar();
		void AssignVar(VarId vid, int val);
		void UnAssignVar(VarId vid);
		/** Try and validate that none of the passed constraints is violated. */
		bool ValidateVarConstraints(const Var& var) /*const*/;
		/** Ensure that the variable's domain has been backed up once in this step before we modify it. */
		void EnsureSavedDomain(VarId vid, const Domain& dom);
		/** Restore all the domains saved during this step, typically when a backtrack is needed. */
		void RestoreSavedDomainStep();

		/** Current number of assigned variables, the search algo is finished when all variables have been assigned */
		int assigned_var_count = 0;
		/** Current instanced values of the variables */
		Array<InstVar> inst_vars;
		/** Current domains, starting from the initial domains and progressively reduced by the searching algo */
		Array<Domain> current_domains;
		/** Backed up domains, used to restore domains if the searching algo needs to backtrack */
		Array<SavedDomains> saved_domains;
		/** Order in which variables will be processed for assignments */
		Array<VarId> assign_order;

#ifdef FCHECK_WITH_STATS
		Stats stats;
#endif
	};

	/**
	 * Class representing a Constraint Satisfying Problem.
	 * This is where you model the problem using variables and constraints.
	 * Once defined, CSP is static and never modified by the searching algorithm.
	 */
	class CSP
	{
	public:
		CSP() = default;

		/** Add an integer variable that can take any value in range [min, max). Including min but excluding max. */
		VarId AddIntVar(int min_val, int max_val);
		/** Add an integer variable with a custom domain. */
		VarId AddIntVar(const Domain& domain);
		/** Add an integer variable with a fixed value. */
		VarId AddFixedVar(int val);
		/** Add a boolean variable. That is, internally, an integer that can either be 0 or 1. */
		VarId AddBoolVar();
		/** Add a new constraint derived from the Constraint class */
		template <class T>
		void AddConstraint(const T& con);
		/** You need to call FinalizeModel() once all var and constraints have been added. */
		void FinalizeModel();
		/** Recursive method to solve the CSP. */
		bool ForwardCheckingStep(Assignment& a) const;

		/** All the variables in the model */
		Array<Var> vars;
		/** All the constraints in the model */
		Array<GenericConstraint> constraints;
		/** Domains of the variables, stored at the same index at the corresponding var */
		Array<Domain> domains;
	};

}; /*namespace fcheck*/

#ifdef FCHECK_IMPLEMENTATION

namespace fcheck
{
	Assignment::Assignment() {}

	void Assignment::Reset(const CSP& csp)
	{
		assigned_var_count = 0;

		FCHECK_Array_Clear(inst_vars);
		FCHECK_Array_Resize(inst_vars, FCHECK_Array_Size(csp.vars));

		current_domains = csp.domains;
		FCHECK_Array_Clear(saved_domains);
		FCHECK_Array_Reserve(saved_domains, FCHECK_Array_Size(csp.vars));

		// Compute order of assignements, smaller domains go first (especially constant variables)
		FCHECK_Array_Clear(assign_order);
		FCHECK_Array_Resize(assign_order, FCHECK_Array_Size(csp.vars));
		for (int d_idx = 0; d_idx < FCHECK_Array_Size(assign_order); d_idx++)
		{
			assign_order[d_idx] = d_idx;
		}

		FCHECK_Array_Sort(assign_order,
			[this](const VarId& a, const VarId& b) -> bool
			{
				int sa = current_domains[a].Size();
				int sb = current_domains[b].Size();
				if (sa == sb)
				{
					return a < b;
				}
				return sa < sb;
			});
	}

	bool Assignment::IsComplete()
	{
		return assigned_var_count == FCHECK_Array_Size(inst_vars);
	}

	const Domain& Assignment::GetCurrentDomain(VarId vid) const
	{
		return current_domains[vid];
	}
	int Assignment::GetInstVarValue(VarId vid) const
	{
		return inst_vars[vid].value;
	}

	VarId Assignment::NextUnassignedVar()
	{
		return assign_order[assigned_var_count];
	}

	void Assignment::AssignVar(VarId vid, int val)
	{
		inst_vars[vid].value = val;
		assigned_var_count++;
#ifdef FCHECK_WITH_STATS
		stats.assigned_vars++;
#endif
	}

	void Assignment::UnAssignVar(VarId vid)
	{
		inst_vars[vid].value = InstVar::UNASSIGNED;
		assigned_var_count--;
	}

	void Assignment::RestoreSavedDomainStep()
	{
		SavedDomains& domain_step = FCHECK_Array_Back(saved_domains);
		for (int d_idx = 0; d_idx < FCHECK_Array_Size(domain_step.domains); d_idx++)
		{
			VarId vid = domain_step.domains[d_idx].var_id;
			current_domains[vid].type = domain_step.domains[d_idx].type;
			current_domains[vid].values = domain_step.domains[d_idx].values;
		}
	}

	void Assignment::EnsureSavedDomain(VarId vid, const Domain& dom)
	{
		SavedDomains& domain_step = FCHECK_Array_Back(saved_domains);
		for (int d_idx = 0; d_idx < FCHECK_Array_Size(domain_step.domains); d_idx++)
		{
			if (domain_step.domains[d_idx].var_id == vid)
				return;
		}
		SavedDomain new_dom{ vid, dom.type, dom.values };
		FCHECK_Array_PushBack(domain_step.domains, std::move(new_dom));
	}

	VarId CSP::AddIntVar(const Domain& domain)
	{
		Var new_var = { (VarId)FCHECK_Array_Size(vars), {} };
		FCHECK_Array_PushBack(vars, new_var);
		FCHECK_Array_PushBack(domains, domain);

		return new_var.var_id;
	}
	VarId CSP::AddIntVar(int min_val, int max_val)
	{
		Domain new_dom = { DomainType::Ranges, {min_val, max_val} };
		return AddIntVar(new_dom);
	}
	VarId CSP::AddFixedVar(int val)
	{
		Domain new_dom = { DomainType::Values, {val} };
		return AddIntVar(new_dom);
	}
	VarId CSP::AddBoolVar()
	{
		Domain new_dom = { DomainType::Values, {0, 1} };
		return AddIntVar(new_dom);
	}
	template <class T>
	void CSP::AddConstraint(const T& con)
	{
		GenericConstraint gen_con;
		new(gen_con.get()) T(con);
		FCHECK_Array_PushBack(constraints, std::move(gen_con));
	}
	void CSP::FinalizeModel()
	{
		// Once we know that the constraints array won't change (and won't be reallocated),
		// we can link the constraint adresses with the variables.
		for (int c_idx = 0; c_idx < FCHECK_Array_Size(constraints); c_idx++)
		{
			constraints[c_idx]->LinkVars(vars);
		}
	}

	bool CSP::ForwardCheckingStep(Assignment& a) const
	{
		if (a.IsComplete())
		{
			return true;
		}

		// Add a new saved domain step
		FCHECK_Array_PushBack(a.saved_domains, std::move(SavedDomains()));

		VarId vid = a.NextUnassignedVar();
		const Domain& dom = a.GetCurrentDomain(vid);
		const Var& var = vars[vid];

		auto LambdaStep = [this, &a, &var](int val)->bool
		{
			a.AssignVar(var.var_id, val);
			if (a.ValidateVarConstraints(var))
			{
				bool success = true;
				for (int c_idx = 0; success && c_idx < FCHECK_Array_Size(var.linked_constraints); c_idx++)
				{
					// Restrict domain of other variables, by removing values that would violate linked constraints
					success &= var.linked_constraints[c_idx]->AplyArcConsistency(a, var.var_id);
				}
				if (success)
				{
					// This instanced variable did not violate any constraints, recurse and continue with next variable
					success = ForwardCheckingStep(a);
				}
				if (success)
				{
					return true;
				}
				else
				{
					a.UnAssignVar(var.var_id);
					// Restore saved domains since they may have been modified by applied arc consistencies or sub-steps
					a.RestoreSavedDomainStep();
				}
			}
			else
			{
				a.UnAssignVar(var.var_id);
			}

			return false;
		};

		bool found_result = false;
		if (dom.type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(dom.values) && !found_result; d_idx++)
			{
				int val = dom.values[d_idx];
				found_result = LambdaStep(val);
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(dom.values); r_idx += 2)
			{
				int min = dom.values[r_idx];
				int max = dom.values[r_idx + 1];
				for (int val = min; val < max && !found_result; val++)
				{
					found_result = LambdaStep(val);
				}
			}
		}
		if (found_result)
		{
			return true;
		}

		FCHECK_Array_PopBack(a.saved_domains);
		return false;
	}

	bool Assignment::ValidateVarConstraints(const Var& var) /*const*/
	{
		for (int c_idx = 0; c_idx < FCHECK_Array_Size(var.linked_constraints); c_idx++)
		{
#ifdef FCHECK_WITH_STATS
			stats.validated_constraints++;
#endif
			if (var.linked_constraints[c_idx]->Evaluate(inst_vars, var.var_id) == Constraint::Eval::Failed)
			{
				return false;
			}
		}

		return true;
	}
	void OpConstraint::LinkVars(Array<Var>& vars)
	{
		FCHECK_Array_PushBack(vars[v0].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v1].linked_constraints, this);
	}
	Constraint::Eval OpConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
		{
			switch (op)
			{
			case Op::Equal:
				if (inst_vars[v0].value == inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			case Op::NotEqual:
				if (inst_vars[v0].value != inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			case Op::SupEqual:
				if (inst_vars[v0].value >= inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			case Op::Sup:
				if (inst_vars[v0].value > inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			case Op::InfEqual:
				if (inst_vars[v0].value <= inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			case Op::Inf:
				if (inst_vars[v0].value < inst_vars[v1].value + offset)
					return Constraint::Eval::Passed;
				break;
			};

			return Constraint::Eval::Failed;
		}

		return Constraint::Eval::NA;
	}
	bool OpConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		auto DoCheck = [&a](VarId v0, int v0_val, int oth_val, Op op)->bool
		{
			Domain& dom = a.current_domains[v0];
			a.EnsureSavedDomain(v0, dom);

			switch (op)
			{
			case Op::Equal:
				dom.Intersect(oth_val);
				break;
			case Op::NotEqual:
				dom.Exclude(oth_val);
				break;
			case Op::SupEqual:
				dom.ExcludeInf(oth_val);
				break;
			case Op::Sup:
				dom.ExcludeInf(oth_val + 1);
				break;
			case Op::InfEqual:
				dom.ExcludeSup(oth_val + 1);
				break;
			case Op::Inf:
				dom.ExcludeSup(oth_val);
				break;
			};

			if (dom.values.empty())
			{
				// Domain wipe out
				return false;
			}
			return true;
		};

		// there are only two vars, and one has just been assigned
		int v0_val = a.inst_vars[v0].value;
		int v1_val = a.inst_vars[v1].value;

		if (v0_val == InstVar::UNASSIGNED)
		{
			return DoCheck(v0, v0_val, v1_val + offset, op);
		}
		if (v1_val == InstVar::UNASSIGNED)
		{
			Op op_rev = op;
			switch (op)
			{
			case Op::SupEqual:	op_rev = Op::InfEqual; break;
			case Op::Sup:		op_rev = Op::Inf; break;
			case Op::InfEqual:	op_rev = Op::SupEqual; break;
			case Op::Inf:		op_rev = Op::Sup; break;
			};
			return DoCheck(v1, v1_val, v0_val - offset, op_rev);
		}

		return true;
	}
	void EqualityConstraint::LinkVars(Array<Var>& vars)
	{
		FCHECK_Array_PushBack(vars[v0].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v1].linked_constraints, this);
	}
	Constraint::Eval EqualityConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
		{
			return inst_vars[v0].value == inst_vars[v1].value ? Constraint::Eval::Passed : Constraint::Eval::Failed;
		}

		return Constraint::Eval::NA;
	}
	bool EqualityConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		auto DoCheck = [&a](VarId v0, int v0_val, int oth_val)->bool
		{
			Domain& dom = a.current_domains[v0];
			a.EnsureSavedDomain(v0, dom);

			dom.Intersect(oth_val);
			if (dom.values.empty())
			{
				// Domain wipe out
				return false;
			}
			return true;
		};

		// there are only two vars, and one has just been assigned
		int v0_val = a.inst_vars[v0].value;
		int v1_val = a.inst_vars[v1].value;

		if (v0_val == InstVar::UNASSIGNED)
		{
			return DoCheck(v0, v0_val, v1_val);
		}
		if (v1_val == InstVar::UNASSIGNED)
		{
			return DoCheck(v1, v1_val, v0_val);
		}

		return true;
	}
	void OrEqualityConstraint::LinkVars(Array<Var>& vars)
	{
		FCHECK_Array_PushBack(vars[v0].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v1].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v2].linked_constraints, this);
	}
	Constraint::Eval OrEqualityConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED &&
			inst_vars[v2].value != InstVar::UNASSIGNED)
		{
			return	(inst_vars[v0].value == inst_vars[v1].value ||
					inst_vars[v0].value == inst_vars[v2].value) ? Constraint::Eval::Passed : Constraint::Eval::Failed;
		}

		return Constraint::Eval::NA;
	}
	bool OrEqualityConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		// there are three vars, we need two var assignments to apply arc consistency
		int v0_val = a.inst_vars[v0].value;
		int v1_val = a.inst_vars[v1].value;
		int v2_val = a.inst_vars[v2].value;

		if (v0_val == InstVar::UNASSIGNED && v1_val != InstVar::UNASSIGNED && v2_val != InstVar::UNASSIGNED)
		{
			Domain& dom = a.current_domains[v0];
			a.EnsureSavedDomain(v0, dom);

			dom.Intersect(v1_val, v2_val);

			if (dom.values.empty())
			{
				// Domain wipe out
				return false;
			}
		}

		return true;
	}
	void CombinedEqualityConstraint::LinkVars(Array<Var>& vars)
	{
		FCHECK_Array_PushBack(vars[v0].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v1].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v2].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v3].linked_constraints, this);
	}
	Constraint::Eval CombinedEqualityConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED &&
			inst_vars[v2].value != InstVar::UNASSIGNED &&
			inst_vars[v3].value != InstVar::UNASSIGNED)
		{
			return	(inst_vars[v0].value == inst_vars[v1].value + inst_vars[v2].value - inst_vars[v3].value) ?
				Constraint::Eval::Passed : Constraint::Eval::Failed;
		}

		return Constraint::Eval::NA;
	}
	bool CombinedEqualityConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		// there are four vars, we need two var assignments to apply arc consistency
		int v0_val = a.inst_vars[v0].value;
		int v1_val = a.inst_vars[v1].value;
		int v2_val = a.inst_vars[v2].value;
		int v3_val = a.inst_vars[v2].value;

		if (v0_val == InstVar::UNASSIGNED &&
			v1_val != InstVar::UNASSIGNED &&
			v2_val != InstVar::UNASSIGNED &&
			v3_val != InstVar::UNASSIGNED)
		{
			Domain& dom = a.current_domains[v0];
			a.EnsureSavedDomain(v0, dom);

			int comb_val = v1_val + v2_val - v3_val;
			dom.Intersect(comb_val);

			if (dom.values.empty())
			{
				// Domain wipe out
				return false;
			}
		}

		return true;
	}
	void OrRangeConstraint::LinkVars(Array<Var>& vars)
	{
		FCHECK_Array_PushBack(vars[v0].linked_constraints, this);
		FCHECK_Array_PushBack(vars[v1].linked_constraints, this);
	}
	Constraint::Eval OrRangeConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vid)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
		{
			return	(inst_vars[v0].value >= min && inst_vars[v0].value < max) ||
					(inst_vars[v1].value >= min && inst_vars[v1].value < max) ? Constraint::Eval::Passed : Constraint::Eval::Failed;
		}

		return Constraint::Eval::NA;
	}
	bool OrRangeConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
#if 0
		auto DoCheck = [&a](VarId v0, int min, int max)->bool
		{
			Domain& dom = a.current_domains[v0];
			const SavedDomain& sav_dom = a.EnsureSavedDomain(v0, dom);

			dom.IntersectRange(min, max);

			if (dom.values.empty())
			{
				// Domain wipe out
				return false;
			}
			return true;
		};

		// there are only two vars, and one has just been assigned
		int v0_val = a.inst_vars[v0].value;
		int v1_val = a.inst_vars[v1].value;

		// Only restrict v0 domain if v1 range eq is not respected
		if (v0_val == InstVar::UNASSIGNED && (v1_val < min || v1_val >= max) )
		{
			return DoCheck(v0, min, max);
		}
		if (v1_val == InstVar::UNASSIGNED && (v0_val < min || v0_val >= max))
		{
			return DoCheck(v1, min, max);
		}

		return true;
#else
		return true;
#endif
	}
	void AllDifferentConstraint::LinkVars(Array<Var>& vars)
	{
		for (int v_idx = 0; v_idx < FCHECK_Array_Size(alldiff_vars); v_idx++)
		{
			FCHECK_Array_PushBack(vars[alldiff_vars[v_idx]].linked_constraints, this);
		}
	}
	Constraint::Eval AllDifferentConstraint::Evaluate(const Array<InstVar>& inst_vars, VarId last_assigned_vids)
	{
		int var_val = inst_vars[last_assigned_vids].value;
		for (int v_idx = 0; v_idx < FCHECK_Array_Size(alldiff_vars); v_idx++)
		{
			if (inst_vars[alldiff_vars[v_idx]].value == var_val && alldiff_vars[v_idx] != last_assigned_vids)
			{
				return Constraint::Eval::Failed;
			}
		}

		return Constraint::Eval::Passed;
	}
	bool AllDifferentConstraint::AplyArcConsistency(Assignment& a, VarId last_assigned_vid)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		int val = a.inst_vars[last_assigned_vid].value;
		for (int v_idx = 0; v_idx < FCHECK_Array_Size(alldiff_vars); v_idx++)
		{
			int vid = alldiff_vars[v_idx];
			if (a.inst_vars[vid].value == InstVar::UNASSIGNED)
			{
				Domain& dom = a.current_domains[vid];
				a.EnsureSavedDomain(vid, dom);
				dom.Exclude(val);

				if (dom.values.empty())
				{
					// Domain wipe out
					return false;
				}
			}
		}

		return true;
	}

	int Domain::Size() const
	{
		if (type == DomainType::Values)
		{
			return (int)FCHECK_Array_Size(values);
		}
		else
		{
			int size = 0;
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); r_idx += 2)
			{
				size += values[r_idx + 1] - values[r_idx];
			}
			return size;
		}
	}
	void Domain::Intersect(int val)
	{
		if (type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				if (val == values[d_idx])
				{
					FCHECK_Array_Clear(values);
					FCHECK_Array_PushBack(values, val);
					break;
				}
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); r_idx += 2)
			{
				if (values[r_idx] <= val && val < values[r_idx + 1])
				{
					type = DomainType::Values;
					FCHECK_Array_Clear(values);
					FCHECK_Array_PushBack(values, val);
					break;
				}
			}
		}
	}
	void Domain::Exclude(int val)
	{
		if (type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				if (val == values[d_idx])
				{
					FCHECK_Array_Erase(values, d_idx, d_idx + 1);
					break;
				}
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); r_idx += 2)
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				if (min <= val && val < max)
				{
					if (max - min <= 1)
					{
						FCHECK_Array_Erase(values, r_idx, r_idx + 2);
					}
					else
					{
						if (val == min)
						{
							values[r_idx] = val + 1;
						}
						else if (val + 1 == max)
						{
							values[r_idx + 1] = val;
						}
						else
						{
							values[r_idx + 1] = val;
							FCHECK_Array_Insert(values, (r_idx + 2), max);
							FCHECK_Array_Insert(values, (r_idx + 2), val + 1);
						}
					}
					break;
				}
			}
		}
	}
	void Domain::Intersect(int val0, int val1)
	{
		int write_idx = 0;
		if (type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				int val = values[d_idx];
				if (val0 == val)
				{
					values[write_idx++] = val0;
				}
				else if (val1 == val)
				{
					values[write_idx++] = val1;
				}
			}
		}
		else
		{
			type = DomainType::Values;
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); r_idx += 2)
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				if (min <= val0 && val0 < max)
				{
					values[write_idx++] = val0;
				}
				if (min <= val1 && val1 < max)
				{
					values[write_idx++] = val1;
				}
			}
		}
		FCHECK_Array_Erase(values, write_idx, FCHECK_Array_Size(values));
	}
	void Domain::IntersectRange(int rmin, int rmax)
	{
		if (type == DomainType::Values)
		{
			int write_idx = 0;
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				int val = values[d_idx];
				if (rmin <= val && val < rmax)
				{
					values[write_idx++] = val;
				}
			}
			FCHECK_Array_Erase(values, write_idx, FCHECK_Array_Size(values));
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); )
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				int new_min = min > rmin ? min : rmin;
				int new_max = max < rmax ? max : rmax;
				if (new_max > new_min)
				{
					values[r_idx] = new_min;
					values[r_idx + 1] = new_max;
					r_idx += 2;
				}
				else
				{
					FCHECK_Array_Erase(values, r_idx, r_idx + 2);
				}
			}
		}
	}
	void Domain::ExcludeSup(int rmax)
	{
		if (type == DomainType::Values)
		{
			int write_idx = 0;
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				int val = values[d_idx];
				if (val < rmax)
				{
					values[write_idx++] = val;
				}
			}
			FCHECK_Array_Erase(values, write_idx, FCHECK_Array_Size(values));
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); )
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				int new_max = max < rmax ? max : rmax;
				if (new_max > min)
				{
					values[r_idx + 1] = new_max;
					r_idx += 2;
				}
				else
				{
					FCHECK_Array_Erase(values, r_idx, r_idx + 2);
				}
			}
		}
	}
	void Domain::ExcludeInf(int rmin)
	{
		if (type == DomainType::Values)
		{
			int write_idx = 0;
			for (int d_idx = 0; d_idx < FCHECK_Array_Size(values); d_idx++)
			{
				int val = values[d_idx];
				if (val >= rmin)
				{
					values[write_idx++] = val;
				}
			}
			FCHECK_Array_Erase(values, write_idx, FCHECK_Array_Size(values));
		}
		else
		{
			for (int r_idx = 0; r_idx < FCHECK_Array_Size(values); )
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				int new_min = min > rmin ? min : rmin;
				if (max > new_min)
				{
					values[r_idx] = new_min;
					r_idx += 2;
				}
				else
				{
					FCHECK_Array_Erase(values, r_idx, r_idx + 2);
				}
			}
		}
	}

}; /*namespace fcheck*/

#endif // FCHECK_IMPLEMENTATION