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
	C++ 11
*/

#ifdef FCHECK_USE_STDVECTOR
#include <vector>
#endif

/**/
namespace fcheck
{
#ifdef FCHECK_USE_STDVECTOR
	template<class T>
	using Array = std::vector<T>;
#endif

	using VarId = int;
	struct Var;
	class Assignment;
	class CSP;

#ifdef FCHECK_WITH_STATS
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
	struct Domain
	{
		Domain() = default;
		void Intersect(int val);
		void Intersect(int val0, int val1);
		void IntersectRange(int min, int max);
		void Exclude(int val);

		DomainType type = DomainType::Values;
		Array<int> values;
	};

	struct SavedDomain
	{
		SavedDomain() = default;

		VarId var_id;
		DomainType type = DomainType::Values;
		Array<int> values;
	};

	struct SavedDomainStep
	{
		SavedDomainStep() = default;

		int step = 0;
		Array<SavedDomain> domains;
	};

	struct InstVar
	{
		static const int UNASSIGNED = -INT_MAX;

		InstVar() = default;

		int value = InstVar::UNASSIGNED;
	};

	struct Constraint
	{
		enum class Eval : int
		{
			NA = 0,	// not applicable yet
			Passed,
			Failed
		};
		static const int MAX_CONSTRAINT_SIZE = 24;

		Constraint() = default;
		virtual void LinkVars(Array<Var>& vars) = 0;
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars) = 0;
		virtual bool Evaluate(const Array<InstVar>& inst_vars) = 0;
		virtual bool AplyArcConsistency(Assignment& a) { return true; }
	};

	struct GenericConstraint
	{
		char buffer[Constraint::MAX_CONSTRAINT_SIZE];

		Constraint* operator->()	{ return (Constraint*)buffer; }
		Constraint* get()			{ return (Constraint*)buffer; }
	};

	struct OpConstraint : public Constraint
	{
		enum class Op : int
		{
			Equal = 0,	// ==
			NotEqual	// !=
			//SupEqual,	// >=
			//Sup,		// >
			//InfEqual,	// <=
			//Inf		// <
		};

		OpConstraint(VarId _v0, VarId _v1, Op _op,int _offset) : v0(_v0), v1(_v1), op(_op), offset(_offset)
		{
			static_assert(sizeof(OpConstraint) <= Constraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars);
		virtual bool Evaluate(const Array<InstVar>& inst_vars);
		virtual bool AplyArcConsistency(Assignment& a);

		VarId v0, v1;
		Op op = Op::Equal;
		int offset = 0;
	};
	struct EqualityConstraint : public Constraint
	{
		EqualityConstraint(VarId _v0, VarId _v1) : v0(_v0), v1(_v1)
		{
			static_assert(sizeof(EqualityConstraint) <= Constraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars);
		virtual bool Evaluate(const Array<InstVar>& inst_vars);
		virtual bool AplyArcConsistency(Assignment& a);

		VarId v0, v1;
	};
	struct OrEqualityConstraint : public Constraint
	{
		OrEqualityConstraint(VarId _v0, VarId _v1, VarId _v2) : v0(_v0), v1(_v1), v2(_v2)
		{
			static_assert(sizeof(OrEqualityConstraint) <= Constraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars);
		virtual bool Evaluate(const Array<InstVar>& inst_vars);
		virtual bool AplyArcConsistency(Assignment& a);

		VarId v0, v1, v2;
	};
	struct CombinedEqualityConstraint : public Constraint
	{
		CombinedEqualityConstraint(VarId _v0, VarId _v1, VarId _v2, VarId _v3) : v0(_v0), v1(_v1), v2(_v2), v3(_v3)
		{
			static_assert(sizeof(OrEqualityConstraint) <= Constraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars);
		virtual bool Evaluate(const Array<InstVar>& inst_vars);
		virtual bool AplyArcConsistency(Assignment& a);

		VarId v0, v1, v2, v3;
	};
	struct OrRangeConstraint : public Constraint
	{
		OrRangeConstraint(VarId _v0, VarId _v1, int _min, int _max) : v0(_v0), v1(_v1), min(_min), max(_max)
		{
			static_assert(sizeof(OrEqualityConstraint) <= Constraint::MAX_CONSTRAINT_SIZE, "");
		};
		virtual void LinkVars(Array<Var>& vars);
		virtual Eval TryEvaluate(const Array<InstVar>& inst_vars);
		virtual bool Evaluate(const Array<InstVar>& inst_vars);
		virtual bool AplyArcConsistency(Assignment& a);

		VarId v0, v1;
		int min, max;
	};

	struct Var
	{
		Var() = default;

		static const VarId INVALID = -1;

		VarId var_id = Var::INVALID;
		Array<Constraint*> linked_constraints;
	};

	class Assignment
	{
	public:
		Assignment();
		void Reset(const CSP& csp);
		bool IsComplete();
		int GetInstVarValue(VarId vid) const;
		const Domain& GetCurrentDomain(VarId vid) const;
		VarId NextUnassignedVar();
		void AssignVar(VarId vid, int val);
		void UnAssignVar(VarId vid);
		bool ValidateConstraints(const Array<Constraint*>& constraints) /*const*/;
		const SavedDomain& EnsureSavedDomain(VarId vid, const Domain& dom);
		void RestoreSavedDomainStep();

		int assigned_var_count = 0;
		Array<InstVar> inst_vars;
		Array<Domain> current_domains;			// domains, changed on the go by the forward-checking algo
		Array<SavedDomainStep> saved_domains;	// backup_domains to restore after a forward-checking step

#ifdef FCHECK_WITH_STATS
		Stats stats;
#endif
	};

	class CSP
	{
	public:
		CSP() = default;

		VarId AddIntVar(const char* name_id, const Domain& domain);
		VarId AddIntVar(const char* name_id, int min_val, int max_val);
		VarId AddBoolVar(const char* name_id);
		template <class T>
		void PushConstraint(const T& con);
		void LinkConstraints();

		bool ForwardCheckingStep(Assignment& a) const;

		// Static parameters, unaffected by searching algo
		Array<Var> vars;
		Array<GenericConstraint> constraints;
		Array<Domain> domains;
		Array<const char*> var_names;
	};

}; /*namespace fcheck*/

#ifdef FCHECK_IMPLEMENTATION

namespace fcheck
{
	Assignment::Assignment() {}

	void Assignment::Reset(const CSP& csp)
	{
		assigned_var_count = 0;

		inst_vars.clear();
		inst_vars.resize(csp.domains.size());

		current_domains = csp.domains;
		saved_domains.clear();
	}

	bool Assignment::IsComplete() { return assigned_var_count == inst_vars.size(); }

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
		for (int idx = assigned_var_count; idx < inst_vars.size(); idx++)
		{
			if (inst_vars[idx].value == InstVar::UNASSIGNED)
			{
				return (VarId)idx;
			}
		}
		return (VarId)inst_vars.size();
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
		SavedDomainStep& domain_step = saved_domains.back();
		for (int d_idx = 0; d_idx < domain_step.domains.size(); d_idx++)
		{
			VarId vid = domain_step.domains[d_idx].var_id;
			current_domains[vid].type = domain_step.domains[d_idx].type;
			current_domains[vid].values = domain_step.domains[d_idx].values;
		}
	}

	const SavedDomain& Assignment::EnsureSavedDomain(VarId vid, const Domain& dom)
	{
		SavedDomainStep& domain_step = saved_domains.back();
		for (int d_idx = 0; d_idx < domain_step.domains.size(); d_idx++)
		{
			if (domain_step.domains[d_idx].var_id == vid)
				return domain_step.domains[d_idx];
		}
		domain_step.domains.push_back(SavedDomain{ vid, dom.type, dom.values });
		return domain_step.domains.back();
	}

	VarId CSP::AddIntVar(const char* name_id, const Domain& domain)
	{
		Var new_var = { (VarId)vars.size(), {} };
		vars.push_back(new_var);
		domains.push_back(domain);
		var_names.push_back(name_id);

		return new_var.var_id;
	}
	VarId CSP::AddIntVar(const char* name_id, int min_val, int max_val)
	{
		Domain new_dom = { DomainType::Ranges, {min_val, max_val+1} };
		return AddIntVar(name_id, new_dom);
	}
	VarId CSP::AddBoolVar(const char* name_id)
	{
		Var new_var = { (VarId)vars.size(), {} };
		Domain new_dom = { DomainType::Values, {0, 1} };
		vars.push_back(new_var);
		domains.push_back(new_dom);
		var_names.push_back(name_id);

		return new_var.var_id;
	}
	template <class T>
	void CSP::PushConstraint(const T& con)
	{
		GenericConstraint gen_con;
		new(gen_con.get()) T(con);
		constraints.push_back(std::move(gen_con));
	}
	void CSP::LinkConstraints()
	{
		for (int c_idx = 0; c_idx < constraints.size(); c_idx++)
		{
			constraints[c_idx]->LinkVars(vars);
		}
	}

	bool CSP::ForwardCheckingStep(Assignment& a) const
	{
		if (a.IsComplete())
			return true;

		// add a new saved domain step
		a.saved_domains.push_back(SavedDomainStep());

		VarId vid = a.NextUnassignedVar();
		const Domain& dom = a.GetCurrentDomain(vid);
		const Var& var = vars[vid];

		auto LambdaStep = [this, &a, &var](int val)->bool
		{
			a.AssignVar(var.var_id, val);
			if (a.ValidateConstraints(var.linked_constraints))
			{
				bool success = true;
				for (int c_idx = 0; success && c_idx < var.linked_constraints.size(); c_idx++)
				{
					success &= var.linked_constraints[c_idx]->AplyArcConsistency(a);
				}
				if (success)
				{
					success = ForwardCheckingStep(a);
				}
				if (success)
				{
					return true;
				}
				else
				{
					a.UnAssignVar(var.var_id);
					// Restore saved domains
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
			for (int d_idx = 0; d_idx < dom.values.size() && !found_result; d_idx++)
			{
				int val = dom.values[d_idx];
				found_result = LambdaStep(val);
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < dom.values.size(); r_idx += 2)
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

		a.saved_domains.pop_back();
		return false;
	}

	//////////////////////////////////////////////////////
	bool Assignment::ValidateConstraints(const Array<Constraint*>& constraints) /*const*/
	{
		for (int c_idx = 0; c_idx < constraints.size(); c_idx++)
		{
#ifdef FCHECK_WITH_STATS
			stats.validated_constraints++;
#endif
			if (constraints[c_idx]->TryEvaluate(inst_vars) == Constraint::Eval::Failed)
			{
				return false;
			}
		}

		return true;
	}
	void OpConstraint::LinkVars(Array<Var>& vars)
	{
		vars[v0].linked_constraints.push_back(this);
		vars[v1].linked_constraints.push_back(this);
	}
	Constraint::Eval OpConstraint::TryEvaluate(const Array<InstVar>& inst_vars)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
			return Evaluate(inst_vars) ? Constraint::Eval::Passed : Constraint::Eval::Failed;

		return Constraint::Eval::NA;
	}
	bool OpConstraint::Evaluate(const Array<InstVar>& inst_vars)
	{
		switch (op)
		{
		case Op::Equal:
			if (inst_vars[v0].value == inst_vars[v1].value + offset)
				return true;
			break;
		case Op::NotEqual:
			if (inst_vars[v0].value != inst_vars[v1].value + offset)
				return true;
			break;
		//case Op::SupEqual:
		//	if (inst_vars[v0].value >= inst_vars[v1].value + offset)
		//		return true;
		//	break;
		//case Op::Sup:
		//	if (inst_vars[v0].value > inst_vars[v1].value + offset)
		//		return true;
		//	break;
		//case Op::InfEqual:
		//	if (inst_vars[v0].value <= inst_vars[v1].value + offset)
		//		return true;
		//	break;
		//case Op::Inf:
		//	if (inst_vars[v0].value < inst_vars[v1].value + offset)
		//		return true;
		//	break;
		};

		return false;
	}
	bool OpConstraint::AplyArcConsistency(Assignment& a)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		auto DoCheck = [&a](VarId v0, int v0_val, int oth_val, Op op)->bool
		{
			Domain& dom = a.current_domains[v0];
			/*const SavedDomain& sav_dom =*/ a.EnsureSavedDomain(v0, dom);

			if (op == Op::Equal)
			{
				dom.Intersect(oth_val);
			}
			else // Op::NotEqual
			{
				dom.Exclude(oth_val);
			}

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
			return DoCheck(v1, v1_val, v0_val - offset, op);
		}

		return true;
	}
	void EqualityConstraint::LinkVars(Array<Var>& vars)
	{
		vars[v0].linked_constraints.push_back(this);
		vars[v1].linked_constraints.push_back(this);
	}
	Constraint::Eval EqualityConstraint::TryEvaluate(const Array<InstVar>& inst_vars)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
			return Evaluate(inst_vars) ? Constraint::Eval::Passed : Constraint::Eval::Failed;

		return Constraint::Eval::NA;
	}
	bool EqualityConstraint::Evaluate(const Array<InstVar>& inst_vars)
	{
		return inst_vars[v0].value == inst_vars[v1].value;
	}
	bool EqualityConstraint::AplyArcConsistency(Assignment& a)
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
		vars[v0].linked_constraints.push_back(this);
		vars[v1].linked_constraints.push_back(this);
		vars[v2].linked_constraints.push_back(this);
	}
	Constraint::Eval OrEqualityConstraint::TryEvaluate(const Array<InstVar>& inst_vars)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED &&
			inst_vars[v2].value != InstVar::UNASSIGNED)
			return Evaluate(inst_vars) ? Constraint::Eval::Passed : Constraint::Eval::Failed;

		return Constraint::Eval::NA;
	}
	bool OrEqualityConstraint::Evaluate(const Array<InstVar>& inst_vars)
	{
		return	inst_vars[v0].value == inst_vars[v1].value ||
				inst_vars[v0].value == inst_vars[v2].value;
	}
	bool OrEqualityConstraint::AplyArcConsistency(Assignment& a)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		// there are three vars, and one has just been assigned
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
		vars[v0].linked_constraints.push_back(this);
		vars[v1].linked_constraints.push_back(this);
		vars[v2].linked_constraints.push_back(this);
		vars[v3].linked_constraints.push_back(this);
	}
	Constraint::Eval CombinedEqualityConstraint::TryEvaluate(const Array<InstVar>& inst_vars)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED &&
			inst_vars[v2].value != InstVar::UNASSIGNED &&
			inst_vars[v3].value != InstVar::UNASSIGNED)
			return Evaluate(inst_vars) ? Constraint::Eval::Passed : Constraint::Eval::Failed;

		return Constraint::Eval::NA;
	}
	bool CombinedEqualityConstraint::Evaluate(const Array<InstVar>& inst_vars)
	{
		return	inst_vars[v0].value == inst_vars[v1].value + inst_vars[v2].value - inst_vars[v3].value;
	}
	bool CombinedEqualityConstraint::AplyArcConsistency(Assignment& a)
	{
#ifdef FCHECK_WITH_STATS
		a.stats.applied_arcs++;
#endif
		// there are three vars, and one has just been assigned
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
		vars[v0].linked_constraints.push_back(this);
		vars[v1].linked_constraints.push_back(this);
	}
	Constraint::Eval OrRangeConstraint::TryEvaluate(const Array<InstVar>& inst_vars)
	{
		if (inst_vars[v0].value != InstVar::UNASSIGNED &&
			inst_vars[v1].value != InstVar::UNASSIGNED)
			return Evaluate(inst_vars) ? Constraint::Eval::Passed : Constraint::Eval::Failed;

		return Constraint::Eval::NA;
	}
	bool OrRangeConstraint::Evaluate(const Array<InstVar>& inst_vars)
	{
		return	(inst_vars[v0].value >= min && inst_vars[v0].value < max) ||
				(inst_vars[v1].value >= min && inst_vars[v1].value < max);
	}
	bool OrRangeConstraint::AplyArcConsistency(Assignment& a)
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

	void Domain::Intersect(int val)
	{
		if (type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < values.size(); d_idx++)
			{
				if (val == values[d_idx])
				{
					values.clear();
					values.push_back(val);
					break;
				}
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < values.size(); r_idx += 2)
			{
				if (values[r_idx] <= val && val < values[r_idx + 1])
				{
					type = DomainType::Values;
					values.clear();
					values.push_back(val);
					break;
				}
			}
		}
	}
	void Domain::Exclude(int val)
	{
		if (type == DomainType::Values)
		{
			for (int d_idx = 0; d_idx < values.size(); d_idx++)
			{
				if (val == values[d_idx])
				{
					values.erase(values.begin() + d_idx);
					break;
				}
			}
		}
		else
		{
			for (int r_idx = 0; r_idx < values.size(); r_idx += 2)
			{
				int min = values[r_idx];
				int max = values[r_idx + 1];
				if (min <= val && val < max)
				{
					if (max - min <= 1)
					{
						values.erase(values.begin() + r_idx, values.begin() + r_idx + 2);
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
							values.insert(values.begin() + (r_idx + 2), max);
							values.insert(values.begin() + (r_idx + 2), val + 1);
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
			for (int d_idx = 0; d_idx < values.size(); d_idx++)
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
			for (int r_idx = 0; r_idx < values.size(); r_idx += 2)
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
		values.erase(values.begin() + write_idx, values.end());
	}
	void Domain::IntersectRange(int rmin, int rmax)
	{
		if (type == DomainType::Values)
		{
			int write_idx = 0;
			for (int d_idx = 0; d_idx < values.size(); d_idx++)
			{
				int val = values[d_idx];
				if (rmin <= val && val < rmax)
				{
					values[write_idx++] = val;
				}
			}
			values.erase(values.begin() + write_idx, values.end());
		}
		else
		{
			for (int r_idx = 0; r_idx < values.size(); )
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
					values.erase(values.begin() + r_idx, values.begin() + r_idx + 2);
				}
			}
		}
	}

}; /*namespace fcheck*/

#endif // FCHECK_IMPLEMENTATION